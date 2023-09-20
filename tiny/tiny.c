#include "csapp.h"

void doit(int fd);
void read_requesthdrs(rio_t *rp);
int parse_uri(char *uri, char *filename, char *cgiargs);
void serve_static(int fd, char *filename, int filesize, char *method);
void get_filetype(char *filename, char *filetype);
void serve_dynamic(int fd, char *filename, char *cgiargs, char *method);
void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg);

int main(int argc, char **argv) {
    int listenfd, connfd;
    char hostname[MAXLINE], port[MAXLINE];
    socklen_t clientlen;
    struct sockaddr_storage clientaddr;

    if(argc != 2){
	    fprintf(stderr, "usage: %s <port>\n", argv[0]);
	    exit(1);
    }

    listenfd = Open_listenfd(argv[1]);
    
    while(1){
	    clientlen = sizeof(clientaddr);
	    connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);
        Getnameinfo((SA *) &clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE, 0);
        printf("Accepted connection from (%s, %s)\n", hostname, port);
	    doit(connfd);
	    Close(connfd);
    }
}

/*
 * doit - handle one HTTP request/response transaction
 */
void doit(int fd) {
    int is_static;
    struct stat sbuf;
    char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
    char filename[MAXLINE], cgiargs[MAXLINE];
    rio_t rio;

    /* Read request line and headers */
    Rio_readinitb(&rio, fd);
    if(!Rio_readlineb(&rio, buf, MAXLINE)) {
        return;
    }

    printf("%s", buf);
    sscanf(buf, "%s %s %s", method, uri, version);
    if(!(strcasecmp(method, "GET")==0||strcasecmp(method,"HEAD")==0)) {
        clienterror(fd, method, "501", "Not Implemented", "Tiny does not implement this method");
        return;
    }
    read_requesthdrs(&rio);

    is_static = parse_uri(uri, filename, cgiargs);
    if(stat(filename, &sbuf) < 0) {
	    clienterror(fd, filename, "404", "Not found", "Tiny couldn't find this file");
	    return;
    }

    if(is_static) { /* Serve static content */          
	    if(!(S_ISREG(sbuf.st_mode)) || !(S_IRUSR & sbuf.st_mode)) {
	        clienterror(fd, filename, "403", "Forbidden", "Tiny couldn't read the file");
	        return;
	    }

	    serve_static(fd, filename, sbuf.st_size,method);
    } else { /* Serve dynamic content */
	    if (!(S_ISREG(sbuf.st_mode)) || !(S_IXUSR & sbuf.st_mode)) {
	        clienterror(fd, filename, "403", "Forbidden", "Tiny couldn't run the CGI program");
	        return;
	    }
	    serve_dynamic(fd, filename, cgiargs, method);
    }
}

/*
 * read_requesthdrs - read HTTP request headers
 */
void read_requesthdrs(rio_t *rp) {
    char buf[MAXLINE];

    Rio_readlineb(rp, buf, MAXLINE);
    printf("%s", buf);

    while(strcmp(buf, "\r\n")) {
	    Rio_readlineb(rp, buf, MAXLINE);
	    printf("%s", buf);
    }

    return;
}

/*
 * parse_uri - parse URI into filename and CGI args
 *             return 0 if dynamic content, 1 if static
 */
int parse_uri(char *uri, char *filename, char *cgiargs) {
    char *ptr;

    if (!strstr(uri, "cgi-bin")) {  /* Static content */
	    strcpy(cgiargs, "");
	    strcpy(filename, ".");
	    strcat(filename, uri);

	    if (uri[strlen(uri)-1] == '/'){
	        strcat(filename, "home.html");
        }

	    return 1;
    } else {  /* Dynamic content */
	    ptr = index(uri, '?');

	    if(ptr){
	        strcpy(cgiargs, ptr+1);
	        *ptr = '\0';
	    } else {
	        strcpy(cgiargs, "");
        }

        strcpy(filename, ".");
	    strcat(filename, uri);
	    return 0;
    }
}

/*
 * serve_static - copy a file back to the client 
 */
void serve_static(int fd, char *filename, int filesize, char *method) {
    int srcfd;
    char *srcp, filetype[MAXLINE], buf[MAXBUF], *fbuf;

    get_filetype(filename, filetype);
    sprintf(buf, "HTTP/1.0 200 OK\r\n");
    sprintf(buf, "%sServer: Tiny Web Server\r\n", buf);
    sprintf(buf, "%sConnection: close\r\n", buf);
    sprintf(buf, "%sContent-length: %d\r\n", buf, filesize);
    sprintf(buf, "%sContent-type: %s\r\n\r\n", buf, filetype); // 빈 줄 한 개가 헤더를 종료하고 있음
    Rio_writen(fd, buf, strlen(buf));
    printf("Response headers:\n");
    printf("%s",buf);

    if(strcasecmp(method,"HEAD")==0){
        return;
    }
    srcfd = Open(filename, O_RDONLY, 0);
    srcp = Mmap(0, filesize, PROT_READ, MAP_PRIVATE, srcfd, 0);
    Close(srcfd);     
    Rio_writen(fd, srcp, filesize);
    Munmap(srcp, filesize);

    // fbuf = malloc(filesize);
    // Rio_readn(srcfd, fbuf, filesize);
    // Close(srcfd);
    // Rio_writen(fd, fbuf, filesize);
    // free(fbuf);
}

/*
 * get_filetype - derive file type from file name
 */
void get_filetype(char *filename, char *filetype) 
{
    if (strstr(filename, ".html"))
	    strcpy(filetype, "text/html");
    else if (strstr(filename, ".gif"))
	    strcpy(filetype, "image/gif");
    else if (strstr(filename, ".png"))
	    strcpy(filetype, "image/png");
    else if (strstr(filename, ".jpg"))
	    strcpy(filetype, "image/jpeg");
    else if (strstr(filename,".mp4"))
        strcpy(filetype,"video/mp4");
    else
	    strcpy(filetype, "text/plain");
}  

/*
 * serve_dynamic - run a CGI program on behalf of the client
 */
void serve_dynamic(int fd, char *filename, char *cgiargs, char *method) {
    char buf[MAXLINE], *emptylist[] = { NULL };

    /* Return first part of HTTP response */
    sprintf(buf, "HTTP/1.0 200 OK\r\n"); 
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Server: Tiny Web Server\r\n");
    Rio_writen(fd, buf, strlen(buf));
  
    if (Fork() == 0) { /* Child */ 
	    setenv("QUERY_STRING", cgiargs, 1);
	    setenv("REQUEST_METHOD", method, 1);
	    Dup2(fd, STDOUT_FILENO);
	    Execve(filename, emptylist, environ);
    }
    Wait(NULL);
}

/*
 * clienterror - returns an error message to the client
 */
void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg) {
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

void echo(int connfd){
    size_t n;
    char buf[MAXLINE];
    rio_t rio;
    Rio_readinitb(&rio,connfd);
    while((n=Rio_readlineb(&rio,buf,MAXLINE))!=0){
        printf("server received %d bytes\n",(int)n);
        Rio_writen(connfd,buf,n);
    }
}

/*
    ⚙︎ function   : memcpy(destination, source, size_t)
    ⚙︎ Header     : <string.h>
    ⚙︎ parameter  : - destination: 복사할 데이터가 위치할 메모리주소를 가르키는 포인터
                   - source     : 복사할 데이터가 위치한 메모리주소를 가르키는 포인터
                   - size_t     : 복사할 데이터의 길이 (Bytes)
    ⚙︎ description: source에 있는 원본 데이터를 size_t만큼 복사해 destination 주소로 복사
    ⚙︎ caution    : size_t 가 char* 인 경우에는 문자열의 끝을 알리는 "\0" 까지 복사해야 하기 때문에 길이 + 1을 해준다.
                   desination과 source의 메모리 주소는 겹치면 안된다.

    ⚙︎ Function    : strstr
    ⚙︎ Arguments   : const char *haystack, const char *needle
    ⚙︎ Return      : char *
    ⚙︎ Description : 주어진 문자열에서 특정 부분 문자열의 위치를 찾는 함수
                    - haystack: 검색 대상이 되는 문자열
                    - needle  : 찾고자 하는 부분 문자열

    ⚙︎ Function    : index
    ⚙︎ Arguments   : const char *s, int c
    ⚙︎ Return      : char *
    ⚙︎ Description : 문자열에서 특정 문자의 첫 번째 등장 위치를 찾는 함수
                    - s: 검색할 문자열
                    - c: 검색할 문자

    ⚙︎ Function    : strcasecmp
    ⚙︎ Arguments   : const char *str1, const char *str2
    ⚙︎ Return      : int
    ⚙︎ Description : 문자열을 대소문자 구분 없이 비교하는 함수입니다.
                    == 0: 동일한 문자
                    > 0: str1이 사전적으로 뒤
                    < 0: str1이 사전적으로 앞 

    ⚙︎ Function    : stat
    ⚙︎ Arguments   : const char *path, struct stat *buf
    ⚙︎ Return      : int
    ⚙︎ Description : 파일의 메타데이터(파일 크기, 소유자, 권한 등)를 가져오는 함수
                    == 0 : 파일을 찾음
                    == -1: 파일을 찾지 못함

    ⚙︎ Function    : pthread_create
    ⚙︎ Header      : <pthread.h>
    ⚙︎ Arguments   : pthread_t *thread, const pthread_attr_t *attr, void *(*start_routine) (void *), void *arg
    ⚙︎ Return      : int
    ⚙︎ Description : 병렬로 실행할 코드 블록을 다른 스레드에서 실행
                  - thread: 새로 생성된 스레드의 식별자를 저장할 포인터 (제어 및 추적)
                  - attr: 스레드의 속성을 지정하는데 사용되는 pthread_attr_t 타입의 포인터 (NULL 허용)
                  - start_routine: 새로운 스레드에서 실행할 함수를 가리키는 포인터로 스레드가 시작될 때 호출
                  - arg: start_routine 함수에 전달될 인수를 가리키는 포인터
*/
