#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "csapp.h"
#include "cache.h"

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400


/* You won't lose style points for including this long line in your code */
static const char *user_agent_hdr = "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 Firefox/10.0.3\r\n";
static void test_ParseHostnamePath();
static void test_ExtractPort();

void ProcessTask(void *context);
void DealWithProxyRequest(int connfd);

static void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg);
static void ParseHostnamePath(char *url, int url_length, char *hostname, char *path, int len);
static int ExtractPort(char *hostname, int host_len, char *port, int port_len);
static void ProxyRequestServer(rio_t *rio, int client_fd, char *hostname, size_t host_len, char *path, size_t path_len);
static void ProxyRespondClient(int connfd, int client_fd, char *url, size_t url_len);
static int IsNeedForward(const char *request_header, size_t header_len);
static inline void SendClientCache(int connfd, char *cache_object, size_t object_len);


int main(int argc, char *argv[])
{
    if (argc != 2) {
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
        exit(1);
    }
    printf("proxy is listening on port: %s\n\n", argv[1]);

    int listenfd = Open_listenfd(argv[1]);
    socklen_t clientlen;
    struct sockaddr_storage clientaddr;

    CacheInit();
    int *connfd;
    pthread_t tid;

    while (1) {
        clientlen = sizeof(clientaddr);
        connfd = (int *) Malloc(sizeof(int));
        *connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);
        Pthread_create(&tid, NULL, ProcessTask, connfd);
    }

    return 0;
}

void ProcessTask(void *context)
{
    int connfd = *((int *)context);
    Pthread_detach(pthread_self());
    Free(context);

    DealWithProxyRequest(connfd);

    Close(connfd);
}

/**
 * @brief client connected to the proxy, proxy need to send a request to server,
 * then forward response received from server back to client.
 * @param connfd used by connection between client and proxy
 */
void DealWithProxyRequest(int connfd)
{
    rio_t rio;
    Rio_readinitb(&rio, connfd);
    char buf[MAXLINE];

    if (!Rio_readlineb(&rio, buf, MAXLINE)) {
        return;
    }
    printf("%s", buf);

    char method[MAXLINE] = {0};
    char url[MAXLINE] = {0};
    char version[MAXLINE] = {0};
    sscanf(buf, "%s %s %s", method, url, version);

    int res = strcasecmp(method, "GET");
    if (res != 0) {
        clienterror(connfd, method, "501", "Not implemented", "Tiny does not implement this method");
        return;
    }

    char hostname[MAXLINE];
    char path[MAXLINE];
    ParseHostnamePath(url, MAXLINE, hostname, path, MAXLINE);

    size_t object_len;
    char *object;
    if ((object = FindObejct(url, &object_len)) != NULL) {
        SendClientCache(connfd, object, object_len);
        return;
    }

    char port[MAXLINE];
    int is_include_port = ExtractPort(hostname, MAXLINE, port, MAXLINE);
    if (is_include_port == 0) {
        strcpy(port, "80");
    }

    // proxy connect to server
    int client_fd = Open_clientfd(hostname, port);
    if (client_fd < 0) {
        return;
    }

    /* proxy send request to server */ 
    ProxyRequestServer(&rio, client_fd, hostname, strlen(hostname), path, strlen(path));

    /* proxy read response from server, then forward response back to client */
    ProxyRespondClient(connfd, client_fd, url, strlen(url));

    close(client_fd);
}

/**
 * @param url[in]: e.g. http://www.cmu.edu/hub/index.html
 * @param url_length: the length of url-array
 * @param hostname[out]: www.cmu.edu
 * @param path[out]: /hub/index.html
 * @param len: the length of hostname-array and path-array
 */
static void ParseHostnamePath(char *url, int url_length, char *hostname, char *path, int len)
{
    char prefix[] = "http://";
    const int prefix_length = 7;
    int hostname_length = 0;
    int path_length = 0;
    int end_parse_hostname = 0;

    for (int i = 0; i < url_length; i++) {
        if (i < prefix_length) {
            assert(prefix[i] == url[i]);
            continue;
        }

        if (!end_parse_hostname) {
            if (url[i] == '\0' || url[i] == '/') {
                path[path_length] = '/';
                path_length++;
                path[path_length] = '\0';
                end_parse_hostname = 1;
                continue;
            }
            hostname[hostname_length] = url[i];
            hostname_length++;
            hostname[hostname_length] = '\0';
            continue;
        }

        if (url[i] == '\0') {
            break;
        }
        path[path_length] = url[i];
        path_length++;
        path[path_length] = '\0';
    }
}

static void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg) 
{
    char buf[MAXLINE];

    /* Print the HTTP response headers */
    sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Content-type: text/html\r\n\r\n");
    Rio_writen(fd, buf, strlen(buf));

    /* Print the HTTP response body */
    sprintf(buf, "<html><title>Tiny Error</title>");
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "<body bgcolor=""ffffff"">\r\n");
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "%s: %s\r\n", errnum, shortmsg);
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "<p>%s: %s\r\n", longmsg, cause);
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "<hr><em>The Tiny Web server</em>\r\n");
    Rio_writen(fd, buf, strlen(buf));
}

/**
 * if hostname specifys a port, cut the port from hostname
 * @return 1 if hostname specify the port, otherwise 0
 */
static int ExtractPort(char *hostname, int host_len, char *port, int port_len)
{
    for (int i = 0; i < host_len; i++) {
        if (hostname[i] == ':') {
            strcpy(port, hostname + i + 1);
            hostname[i] = '\0';
            return 1;
        }

        if (hostname[i] == '\0') {
            return 0;
        }
    }
    return 0;
}

/**
 * @param rio used to read request headers from client
 * @param client_fd used by connection between proxy and server
 */
static void ProxyRequestServer(rio_t *rio, int client_fd, char *hostname, size_t host_len, char *path, size_t path_len)
{
    char buf[MAXLINE];
    // send HTTP request line
    strcpy(buf, "GET ");
    strcat(buf, path);
    strcat(buf, " HTTP/1.0\r\n");
    Rio_writen(client_fd, buf, strlen(buf));

    // send HTTP request headers
    strcpy(buf, "Host: ");
    strcat(buf, hostname);
    strcat(buf, "\r\n");
    Rio_writen(client_fd, buf, strlen(buf));

    strcpy(buf, user_agent_hdr);
    Rio_writen(client_fd, buf, strlen(buf));

    strcpy(buf, "Connection: close\r\n");
    Rio_writen(client_fd, buf, strlen(buf));

    strcpy(buf, "Proxy-Connection: close\r\n");
    Rio_writen(client_fd, buf, strlen(buf));

    // forward additional request headers sended by client to server
    do {
        Rio_readlineb(rio, buf, MAXLINE);
        if (IsNeedForward(buf, strlen(buf)) == 1) {
            Rio_writen(client_fd, buf, strlen(buf));
        }
    } while (strcmp(buf, "\r\n") != 0);

    strcpy(buf, "\r\n");
    Rio_writen(client_fd, buf, strlen(buf));
}

/**
 * @param connfd used by connection between client and proxy
 * @param client_fd used by connection between proxy and server
 */
static void ProxyRespondClient(int connfd, int client_fd, char *url, size_t url_len)
{
    char buf[MAXLINE];
    ssize_t n;
    ssize_t sum = 0;

    char *cache = (char *)Malloc(MAX_OBJECT_SIZE);
    char *temp = cache;
    int can_cache = 1;

    // read HTTP response
    while ((n = Rio_readn(client_fd, buf, MAXLINE)) > 0) {
        if (can_cache) {
            sum += n;
            if (sum <= MAX_OBJECT_SIZE) {
                memcpy(temp, buf, n);
                temp += n;
            } else {
                can_cache = 0;
            }
        }
        
        Rio_writen(connfd, buf, n);
    }

    if (can_cache) {
        CacheObject(url, url_len, cache, sum);
    }

    Free(cache);
}

/**
 * @brief if request header contains one of ["Host", "User-Agent", "Connection", "Proxy-Connection"],
 * no need to forward to server, return 0, otherwise return 1.
 */
static int IsNeedForward(const char *request_header, size_t header_len)
{
    if (strstr(request_header, "Host") ||
        strstr(request_header, "User-Agent") ||
        strstr(request_header, "Connection") ||
        strstr(request_header, "Proxy-Connection")) {
        return 0;
    }
    
    return 1;
}

static inline void SendClientCache(int connfd, char *cache_object, size_t object_len)
{
    Rio_writen(connfd, cache_object, object_len);
}

static void test_ParseHostnamePath()
{
    /* test1 */
    char url[] = "http://baidu.com";
    int url_length = strlen(url) + 1;  // include '\0'

    int len = 10;
    char hostname[len];
    char path[len];

    ParseHostnamePath(url, url_length, hostname, path, len);

    assert(strcmp(hostname, "baidu.com") == 0);
    assert(strcmp(path, "/") == 0);

    /* test2 */
    char url2[] = "http://baidu.com/dir/a.html";
    int url_length2 = strlen(url2) + 1;

    ParseHostnamePath(url2, url_length2, hostname, path, len);
    assert(strcmp(hostname, "baidu.com") == 0);
    assert(strcmp(path, "/dir/a.html") == 0);

    printf("test passed\n");
}

static void test_ExtractPort()
{
    char hostname[MAXLINE] = "www.baidu.com:15213";
    char port[MAXLINE];
    ExtractPort(hostname, MAXLINE, port, MAXLINE);
    assert(strcmp(port, "15213") == 0);
}