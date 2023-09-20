/*
    Web Proxy 과제 내용
    1.
    2.
    3.
*/

#include <stdio.h>
#include "csapp.h"

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

void *thread(void *vargp);
void doit(int connfd);
void parse_uri(char *uri,char *hostname,char *path,int *port);
void build_http_header(char *http_header,char *hostname,char *path,int port,rio_t *client_rio);
int connect_endServer(char *hostname,int port,char *http_header);
void cache_init();
int cache_find(char *url);
int cache_eviction();
void cache_LRU(int index);
void cache_uri(char *uri, char *buf);
void writePre(int i);
void writeAfter(int i);
void readerPre(int i);
void readerAfter(int i);

/*
    sem_t: 세마포어 표현 데이터 타입
    - 다중 스레드를 사용하기 때문에 공유 자원에 대한 제어 및 동기화(프로세스 간에도 가능)
    - sem_init, sem_wait, sem_post, sem_destroy 등을 사용해서 조작
*/
typedef struct {
    char cache_obj[MAX_OBJECT_SIZE];
    char cache_url[MAXLINE];
    int LRU;                            // Least Recently Used 가장 최근에 사용된 것
    int isEmpty;                        // 0: 비어있지 않음, 1: 비어있음

    int readCnt;                        // 현재 캐시 객체를 '읽고' 있는 Thread의 수
    sem_t rdcntmutex;
    sem_t wmutex;

    int writeCnt;                       // 현재 캐시 객체를 '쓰고' 있는 Thread의 수
    sem_t wtcntMutex;
    sem_t queue;
} cache_block;

typedef struct {
    cache_block cacheobjs[CACHE_OBJS_COUNT];
    int cache_num;
} Cache;

Cache cache;

int main(int argc, char **argv){
    pthread_t tid;
    int listenfd, connfd;
    socklen_t clientlen;
    char hostname[MAXLINE], port[MAXLINE];
    struct sockaddr_storage clientaddr;
    cache_init();

    if(argc != 2){
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
        exit(1);
    }   

    /*
        Signal(시그널 종류, 핸들러) 함수
        - 프로세스에게 특정 이벤트나 프로세스 or dnsdudcpwpdp 조건이 발생했음을 알리는 비동기적 신호를 보냄
        - SIGPIPE: 시그널은 파이프(pipe) 또는 소켓(socket)과 같은 통신 채널을 사용할 때 발생할 수 있는 시그널
        - SIG_IGN: 무시
    */
    Signal(SIGPIPE, SIG_IGN);
    listenfd = Open_listenfd(argv[1]);

    while(1){
        clientlen = sizeof(clientaddr);
	    connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);
        Getnameinfo((SA *) &clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE, 0);
        
        printf("Accepted connection from (%s, %s)\n", hostname, port);
        /*
            Thread 생성
            - create 후 thead는 독립 실행 후 start_routine(밑에서는 thread 함수) 실행 후 종료 되면서
              Thread도 종료된다.
            - Thread가 종료 되면서 사용한 자원과 메모리는 자동으로 해제된다. but Thread 동적 할당시 명시적인 해제가 필요
            - 스레드의 메모리는 회수되지 않는다. (joinable 상태로 남아있기 때문에 main Thread가 기다림)
                - Pthread_join을 통해서 해당 Thread 종료시 회수
                - Pthread_detach를 통해서 main Thread로 부터 분리되어 종료 시점에 자원을 반환한다.
        */
        Pthread_create(&tid, NULL, thread, (void *)connfd);
    }
}

void *thread(void *vargp) {
    int connfd = (int)vargp;
    
    Pthread_detach(pthread_self());    // pthread_self() 현재 함수의 Thread ID를 반환
    doit(connfd);
    Close(connfd);
}

void doit(int connfd) {
    int is_static, end_serverfd, port, cache_index;
    char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE], hostname[MAXLINE];
    char cachebuf[MAX_OBJECT_SIZE], path[MAXLINE], endserver_http_header[MAXLINE];
    char url_store[100];
    rio_t rio, server_rio;

    // Client로 부터 받은 Request 앍오드림
    Rio_readinitb(&rio, connfd);
    Rio_readlineb(&rio, buf, MAXLINE);
    sscanf(buf, "%s %s %s", method, uri, version);    // method, uri, version 초기화
    strcpy(url_store, uri);                           // 받은 uri, caching이 대어 있는지 확인하기 위해 보관

    if(strcasecmp(method, "GET")){
        printf("Proxy does not implement the method");
        return;
    }

    // 캐시 리스트 서치 후 요청과 일치하는 url이 있다면 캐시에서 값 출력 해준다.
    // 1. 읽기 작업 하기 위해서 동기화
    // 2. Client로 캐시에 있던 데이터 전송
    // 3. 읽기 작업 종료 동기화
    // 4. LRU 값 변경
    if((cache_index = cache_find(url_store)) != -1){ 
        readerPre(cache_index);
        Rio_writen(connfd, cache.cacheobjs[cache_index].cache_obj, strlen(cache.cacheobjs[cache_index].cache_obj));
        readerAfter(cache_index);
        cache_LRU(cache_index);

        return;
    }

    parse_uri(uri, hostname, path, &port);

    build_http_header(endserver_http_header, hostname, path, port, &rio);
    // build_http_header를 통해 만들어진 request header를 이용해서 end server와 연결 (1. 서버와의 연결)
    end_serverfd = connect_endServer(hostname, port, endserver_http_header);

    if(end_serverfd < 0){
        printf("connection failed\n");
        return;
    }

    Rio_readinitb(&server_rio, end_serverfd);
    Rio_writen(end_serverfd, endserver_http_header, strlen(endserver_http_header));    // 2. HTTP Request 전송
    
    size_t n;
    int sizebuf = 0;

    // 3. HTTP Resonse 읽어오고(HTML - body) 응답을 Client로 전송
    while((n = Rio_readlineb(&server_rio, buf, MAXLINE)) != 0){
        sizebuf += n;

        if(sizebuf < MAX_OBJECT_SIZE){
            strcat(cachebuf, buf);
        }

        // printf("proxy received %d bytes, then send\n", n); // byte 확인
        Rio_writen(connfd, buf, n);
    }
    Close(end_serverfd);

    // 위 while 에서 누적한 bytes가 정해놓은 값보다 작은 경우에 caching
    if(sizebuf < MAX_OBJECT_SIZE){
        cache_uri(url_store, cachebuf);
    }
}

inline int connect_endServer(char *hostname, int port, char *http_header){
    char portStr[100];

    sprintf(portStr, "%d", port);

    return Open_clientfd(hostname, portStr);
}

void build_http_header(char *http_header, char *hostname, char *path, int port, rio_t *client_rio) {
    char buf[MAXLINE], request_hdr[MAXLINE], other_hdr[MAXLINE], host_hdr[MAXLINE];

    // Request Header: GET {path} HTTP/1.0 ex) [Method Path Version]
    sprintf(request_hdr, requestlint_hdr_format, path);

    /*
        Request Header에서 필드 값에 따라서 처리
        ex) Fields
        - Host: localhost:8000 (내 Tiny Server거 8000 port 사용)
        - User-Agent: curl/7.81.0
        - Accept: *\/*
        - Proxy-Connection: Keep-Alive
    */
    while(Rio_readlineb(client_rio, buf, MAXLINE) > 0){
        if(strcmp(buf, endof_hdr) == 0){    // EOF Header 요청은 언제나 \r\n 이기 때문에
            break;
        }

        if(!strncasecmp(buf, host_key, strlen(host_key))){
            strcpy(host_hdr, buf);
            continue;
        }

        // 해당 조건의 필드 정의는 위에 상수로 되어있음(해당 필드는 other header에 넣지 않겠다)
        if(!strncasecmp(buf, connection_key, strlen(connection_key))
         &&!strncasecmp(buf, proxy_connection_key, strlen(proxy_connection_key))
         &&!strncasecmp(buf, user_agent_key, strlen(user_agent_key))){

            strcat(other_hdr, buf); 
        }
    }

    if(strlen(host_hdr) == 0){
        sprintf(host_hdr, host_hdr_format, hostname);
    }

    /*  Reqeust Header 최종 형태
        ex)
        - GET / HTTP/1.0
        - Host: localhost:8000
        - Connection: close
        - Proxy-Connection: close
        - User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 Firefox/10.0.3
    */
    sprintf(http_header, "%s%s%s%s%s%s%s", request_hdr
                                         , host_hdr
                                         , conn_hdr
                                         , prox_hdr
                                         , user_agent_hdr
                                         , other_hdr
                                         , endof_hdr);
    return;
}

/* 
  Before Parsing: GET http://localhost:8000/ HTTP/1.1
  After Parsing:
                ex)
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

void cache_init(){
    int i;
    cache.cache_num = 0;

    for(i = 0; i < CACHE_OBJS_COUNT; i++){
        cache.cacheobjs[i].LRU = 0;
        cache.cacheobjs[i].isEmpty = 1;
        /*
            Sem_init(공유할 세마포어 포인터, pshared, value)
            - pshared: 0: 프로세스, 1: 쓰레드 간 자원을 공유
            - value: 동시점에 접근 가능한 쓰레드(프로세스) 개수

            description: 밑에서 Sem_init 호출부를 보면 전부 0(쓰레드)로 서정되어 있고, 쓰레드가 동시에 작업하지 못하도록 되어있음
        */
        Sem_init(&cache.cacheobjs[i].wmutex, 0, 1);
        Sem_init(&cache.cacheobjs[i].rdcntmutex, 0, 1);
        cache.cacheobjs[i].readCnt = 0;
        cache.cacheobjs[i].writeCnt = 0;
        Sem_init(&cache.cacheobjs[i].wtcntMutex, 0, 1);
        Sem_init(&cache.cacheobjs[i].queue, 0, 1);
    }
}

int cache_find(char *url){
    int i;
    
    for(i = 0; i < CACHE_OBJS_COUNT; i++){
        readerPre(i);

        // 비어있지 않음 && 들어온 url과 동일한 값이 cache에 존재하는경우
        if((cache.cacheobjs[i].isEmpty == 0) && (strcmp(url, cache.cacheobjs[i].cache_url) == 0)){
            break;
        }

        readerAfter(i);
    }
    
    // 비어있지는 않으나 caching 되어있지 않아서 찾지 못한 경우
    if(i >= CACHE_OBJS_COUNT){
        return -1;
    }

    return i;
}

// buf: Client에 전달한 Response 복사한 거
void cache_uri(char *uri, char *buf){
    int i = cache_eviction();    // cache 교체할 인덱스

    writePre(i);
    strcpy(cache.cacheobjs[i].cache_obj, buf);
    strcpy(cache.cacheobjs[i].cache_url, uri);
    cache.cacheobjs[i].isEmpty = 0;
    writeAfter(i);
    cache_LRU(i);
}

// 캐시 교체 인덱스 서치 함수
int cache_eviction(){
    int min = LRU_MAGIC_NUMBER;
    int minindex = 0;
    int i;

    for(i = 0; i < CACHE_OBJS_COUNT; i++){
        readerPre(i);

        // 해당 캐시가 비어있다면 종료
        if(cache.cacheobjs[i].isEmpty == 1){
            minindex = i;
            readerAfter(i);
            break;
        }

        // 현재 캐시의 LRU 값이 최저 값이 아니라면 최소 값 갱신(캐시 교체를 위한 인덱스)
        if(cache.cacheobjs[i].LRU < min){
            minindex = i;
        }

        readerAfter(i);
    }

    return minindex;
}

void cache_LRU(int index){
    int i;

    writePre(index);
    cache.cacheobjs[index].LRU = LRU_MAGIC_NUMBER;
    writeAfter(index);

    for(i = 0; i < CACHE_OBJS_COUNT; i++) {
        // 캐시가 비어있지 않고 && 현재 처리중인 캐시가 아닌 경우
        if(cache.cacheobjs[i].isEmpty == 0 && i != index){
            writePre(i);
            cache.cacheobjs[i].LRU--;
            writeAfter(i);
        }        
    }
}

/*
    P(sem_wait): 1. 세마포어의 값 확인(wtcntMutex)
                 2. 세마포어의 값이 양수인경우 함수 빠져나감(다른 Thread가 사용중)
                 3. 세마포어가 0인 경우 양수가 될 때까지 해당 Thread 블록
    V(sem_post): 1. 세마포어의 값을 1 증가
                 2. 다른 Thread가 대기 상태라면 그 중 하나 깨워준다.
*/
void writePre(int i){
    P(&cache.cacheobjs[i].wtcntMutex);    // writeCnt에 대해 조작하기 위해서 wait
    cache.cacheobjs[i].writeCnt++;        // 작업중인 Thread 수 증가

    if(cache.cacheobjs[i].writeCnt == 1) {// 작업중인 Thread 본인 제외 없다면
        P(&cache.cacheobjs[i].queue);     // queue를 얻어야 쓰기 가능 wait
    }

    V(&cache.cacheobjs[i].wtcntMutex);    // writeCnt 조작 필요 없기 때문에 헤제 post
    P(&cache.cacheobjs[i].wmutex);        // '쓰기' 작업에 대한 블록(다른 Thread가 작업 못하도록) wait
}

void writeAfter(int i){
    V(&cache.cacheobjs[i].wmutex);        // '쓰기' 작업 끝 post
    P(&cache.cacheobjs[i].wtcntMutex);    // writeCnt에 대해 조작하기 위해서 wait
    cache.cacheobjs[i].writeCnt--;        // 작업중인 Thread 수 감소
    
    if(cache.cacheobjs[i].writeCnt == 0){ // 작업중인 Thread 없다면
        V(&cache.cacheobjs[i].queue);     // queue를 얻어야 쓰기 가능 post
    }

    V(&cache.cacheobjs[i].wtcntMutex);    // writeCnt에 대한 조삭 해제 post
}

void readerPre(int i){
    P(&cache.cacheobjs[i].queue);
    P(&cache.cacheobjs[i].rdcntmutex);
    cache.cacheobjs[i].readCnt++;

    if(cache.cacheobjs[i].readCnt == 1) {
        P(&cache.cacheobjs[i].wmutex);
    }
    
    V(&cache.cacheobjs[i].rdcntmutex);
    V(&cache.cacheobjs[i].queue);
}

void readerAfter(int i){
    P(&cache.cacheobjs[i].rdcntmutex);
    cache.cacheobjs[i].readCnt--;
    
    if(cache.cacheobjs[i].readCnt == 0){
        V(&cache.cacheobjs[i].wmutex);
    }

    V(&cache.cacheobjs[i].rdcntmutex);
}