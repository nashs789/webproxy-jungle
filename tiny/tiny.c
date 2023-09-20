/*
    Tiny Server 과제 내용
    1. 11.6c -
    2. 11.7  -
    3. 11.9  -
    4. 11.10 -
    5. 11.11 -
*/
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

void doit(int fd) {
    int is_static;
    struct stat sbuf;
    char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
    char filename[MAXLINE], cgiargs[MAXLINE];
    rio_t rio;

    // Client로 부터 받은 Request 앍오드림
    Rio_readinitb(&rio, fd);
    Rio_readlineb(&rio, buf, MAXLINE);
    sscanf(buf, "%s %s %s", method, uri, version);

    // Tiny Server는 HTTP Method GET, HEAD만 지원한다.
    if(!(strcasecmp(method, "GET") == 0 || strcasecmp(method, "HEAD") == 0)) {
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
	    if(!(S_ISREG(sbuf.st_mode)) || !(S_IRUSR & sbuf.st_mode)) {    // 일반 파일(파일 모드) && 읽기 권한 
	        clienterror(fd, filename, "403", "Forbidden", "Tiny couldn't read the file");
	        return;
	    }

	    serve_static(fd, filename, sbuf.st_size,method);
    } else { /* Serve dynamic content */
	    if (!(S_ISREG(sbuf.st_mode)) || !(S_IXUSR & sbuf.st_mode)) {    // 일반 파일(파일 모드) && 실행 권한 
	        clienterror(fd, filename, "403", "Forbidden", "Tiny couldn't run the CGI program");
	        return;
	    }
	    serve_dynamic(fd, filename, cgiargs, method);
    }
}

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

int parse_uri(char *uri, char *filename, char *cgiargs) {
    char *ptr;

    // path: cgi-bin/{????} 밑에 있는 컨텐츠는 동적 컨텐츠
    if(!strstr(uri, "cgi-bin")) {     // 정적 컨텐츠인 케이스
	    strcpy(cgiargs, "");          // 파라미터 없음
	    strcpy(filename, ".");        // 요청한 filename이 현재 디렉토리(./)를 찾을 수 있도록 '.' 추가
	    strcat(filename, uri);        // ex) {filename}/{index.html}

        // root인 경우 home.html 파일을 내려준다.
	    if (uri[strlen(uri)-1] == '/'){     
	        strcat(filename, "home.html");
        }

	    return 1;
    } else {                          // 동적 컨텐츠인 케이스 
	    ptr = index(uri, '?');        // Query String을 구분짓는 '?' 기준으로 포인터를 갖는다

	    if(ptr){                      // Query String이 있는 케이스
	        strcpy(cgiargs, ptr + 1); // 파라미터 값 복사
	        *ptr = '\0';
	    } else {
	        strcpy(cgiargs, "");      // 파라미터 없음
        }

        strcpy(filename, ".");
	    strcat(filename, uri);
	    return 0;
    }
}

void serve_static(int fd, char *filename, int filesize, char *method) {
    int srcfd;
    char *srcp, filetype[MAXLINE], buf[MAXBUF], *fbuf;

    get_filetype(filename, filetype);
    // Client에게 보내는 Response Header(헤더 마지막 부분은 \r\n 일 것)
    sprintf(buf, "HTTP/1.0 200 OK\r\n");
    sprintf(buf, "%sServer: Tiny Web Server\r\n", buf);
    sprintf(buf, "%sConnection: close\r\n", buf);
    sprintf(buf, "%sContent-length: %d\r\n", buf, filesize);
    sprintf(buf, "%sContent-type: %s\r\n\r\n", buf, filetype);
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

void serve_dynamic(int fd, char *filename, char *cgiargs, char *method) {
    char buf[MAXLINE], *emptylist[] = { NULL };

    // Client에게 보내는 Response Header
    sprintf(buf, "HTTP/1.0 200 OK\r\n"); 
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Server: Tiny Web Server\r\n");
    Rio_writen(fd, buf, strlen(buf));
  
    // 자식 프로세스를 생성 후 CGI 스크립트 실행
    if(Fork() == 0){ 
        // setenv를 통해서 Query String(파라미터) 와 HTTP Method 전달
	    setenv("QUERY_STRING", cgiargs, 1);
	    setenv("REQUEST_METHOD", method, 1);
	    Dup2(fd, STDOUT_FILENO);    // 현재 소켓 연결중인 fd -> 자식 프로세스의 표준 출력으로 변경
	    Execve(filename, emptylist, environ);    // CGI 스크립트 생성
    }

    Wait(NULL);    // 자식 프로세스 실행 종료 대기 후 자원 정리
}

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

    while((n = Rio_readlineb(&rio, buf, MAXLINE)) != 0){
        printf("server received %d bytes\n", (int)n);
        Rio_writen(connfd, buf, n);
    }
}