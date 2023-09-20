#include <stdio.h>
#include "csapp.h"

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400
#define LRU_MAGIC_NUMBER 9999
#define CACHE_OBJS_COUNT 10

static const char *user_agent_hdr = "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 Firefox/10.0.3\r\n";
static const char *conn_hdr = "Connection: close\r\n";
static const char *prox_hdr = "Proxy-Connection: close\r\n";
static const char *host_hdr_format = "Host: %s\r\n";
static const char *requestlint_hdr_format = "GET %s HTTP/1.0\r\n";
static const char *endof_hdr = "\r\n";

static const char *connection_key = "Connection";
static const char *user_agent_key= "User-Agent";
static const char *proxy_connection_key = "Proxy-Connection";
static const char *host_key = "Host";
static const char *new_version = "HTTP/1.0";

//void *thread(void *vargp);
void doit(int connfd);
void parse_uri(char *uri,char *hostname,char *path,int *port);
void build_http_header(char *http_header,char *hostname,char *path,int port,rio_t *client_rio);
int connect_endServer(char *hostname,int port,char *http_header);

int main(int argc, char **argv){
    int listenfd, connfd;
    socklen_t clientlen;
    char hostname[MAXLINE], port[MAXLINE];
    struct sockaddr_storage clientaddr;

    if(argc != 2){
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
        exit(1);
    }   

    listenfd = Open_listenfd(argv[1]);

    printf("##### listen start #####\n");
    printf("hostname = %s\nport = %s \n", argv[0], argv[1]);

    while(1){
        clientlen = sizeof(clientaddr);
	      connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);
        Getnameinfo((SA *) &clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE, 0);
        printf("Accepted connection from (%s, %s)\n", hostname, port);
	      doit(connfd);
	      Close(connfd);
    }
}

void doit(int connfd) {
    int is_static, end_serverfd, port;
    char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE], hostname[MAXLINE], path[MAXLINE], endserver_http_header[MAXLINE];
    rio_t rio, server_rio;

    /* Read request line and headers */
    Rio_readinitb(&rio, connfd);
    Rio_readlineb(&rio, buf, MAXLINE);
    sscanf(buf, "%s %s %s", method, uri, version);

    printf("\n##### doit - buf print #####\n");
    printf("method = %s\n", method);
    printf("uri = %s\n", uri);
    printf("version = %s\n", version);

    if(strcasecmp(method, "GET")){
        printf("Proxy does not implement the method");
        return;
    }

    parse_uri(uri, hostname, path, &port);
    
    printf("\n##### doit - uri parsing #####\n");
    printf("hostname = %s\n", hostname);
    printf("path = %s\n", path);
    printf("port = %d\n", port);

    build_http_header(endserver_http_header, hostname, path, port, &rio);
    end_serverfd = connect_endServer(hostname, port, endserver_http_header);

    if(end_serverfd < 0){
        printf("connection failed\n");
        return;
    }

    Rio_readinitb(&server_rio, end_serverfd);
    Rio_writen(end_serverfd, endserver_http_header, strlen(endserver_http_header));
    size_t n;

    while((n = Rio_readlineb(&server_rio, buf, MAXLINE)) != 0){
        // printf("proxy received %d bytes, then send\n", n);
        Rio_writen(connfd, buf, n);
    }
    Close(end_serverfd);
}

/*Connect to the end server*/
inline int connect_endServer(char *hostname, int port, char *http_header){
    char portStr[100];

    sprintf(portStr, "%d", port);

    return Open_clientfd(hostname, portStr);
}

void build_http_header(char *http_header, char *hostname, char *path, int port, rio_t *client_rio) {
    char buf[MAXLINE], request_hdr[MAXLINE], other_hdr[MAXLINE], host_hdr[MAXLINE];
    
    printf("\n#### build_http_header - print #####\n");
    printf("request_hdr = %s\n", request_hdr);
    printf("requestlint_hdr_format = %s\n", requestlint_hdr_format);
    printf("path = %s\n", path);

    sprintf(request_hdr, requestlint_hdr_format, path);

    while(Rio_readlineb(client_rio, buf, MAXLINE) > 0){
        if(strcmp(buf, endof_hdr) == 0){    // EOF header 요청은 언제나 \r\n 이기 때문에
            break;
        }

        if(!strncasecmp(buf, host_key, strlen(host_key))){
            strcpy(host_hdr, buf);
            continue;
        }

        if(!strncasecmp(buf, connection_key, strlen(connection_key))
         &&!strncasecmp(buf, proxy_connection_key, strlen(proxy_connection_key))
         &&!strncasecmp(buf, user_agent_key, strlen(user_agent_key))){

            strcat(other_hdr, buf); 
        }
    }

    if(strlen(host_hdr) == 0){
        sprintf(host_hdr, host_hdr_format, hostname);
    }

    sprintf(http_header, "%s%s%s%s%s%s%s", request_hdr
                                         , host_hdr
                                         , conn_hdr
                                         , prox_hdr
                                         , user_agent_hdr
                                         , other_hdr
                                         , endof_hdr);
    return;
}

/*parse the uri to get hostname,file path ,port*/
/* 
  Before Parsing: GET http://localhost:8000/ HTTP/1.1
  After Parsing:
                - hostname: localhost
                - path: /
                - port: 8000
*/
void parse_uri(char *uri, char *hostname, char *path, int *port) {
    char *pos = strstr(uri, "//");

    *port = 80;
    pos = pos != NULL ? pos + 2 : uri;  // NULL: 해당되는 위치가 없음, pos + 2: // 뒤에서 부터 시작(https://locahost -> locahost)

    char *pos2 = strstr(pos, ":");

    // sscanf 에서 %s는 '\0'을 구분으로 읽기 때문에 넣어줌
    if(pos2 != NULL) {
        *pos2 = '\0';                            // 1. pos, pos2를 ':' 기준으로 분리 hostname:port
        sscanf(pos, "%s", hostname);             // 2. pos: ':' 앞에 있는 hostname
        sscanf(pos2 + 1, "%d%s", port, path);    // 3. pos: ':' 뒤에 있는 port번호
    } else {
        pos2 = strstr(pos, "/");

        if(pos2 != NULL) {                      // 1. pos, pos2를 ':' 기준으로 분리 hostname/{path}
            sscanf(pos2, "%s", path);           // 2. pps2: '/' 뒤에 있는 path
            *pos2 = '\0'; 
        } 

        sscanf(pos, "%s", hostname);            // 3. pos: '/' 앞에 있는 hostname
        // 4. port가 없는 uri parsing이기 때문에 80 port 그대로 이용
    }
    
    return;
}