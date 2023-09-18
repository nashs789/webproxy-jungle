/* $begin tinymain */
/*
 * tiny.c - A simple, iterative HTTP/1.0 Web server that uses the
 *     GET method to serve static and dynamic content.
 *
 * Updated 11/2019 droh
 *   - Fixed sprintf() aliasing issue in serve_static(), and clienterror().
 */
#include "csapp.h"

void doit(int fd);
void read_requesthdrs(rio_t *rp);
int parse_uri(char *uri, char *filename, char *cgiargs);
void serve_static(int fd, char *filename, int filesize);
void get_filetype(char *filename, char *filetype);
void serve_dynamic(int fd, char *filename, char *cgiargs);
void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg);

void serve_dynamic(int fd, char *filename, char *cgiargs) {
    char buf[MAXLINE], *emptylist[] = { NULL };
    
    /* Return first part of HTTP response */
    sprintf(buf, "HTTP/1.0 200 OK\r\n");
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Server: Tiny Web Server\r\n");
    Rio_writen(fd, buf, strlen(buf));
    
    if (Fork() == 0) { /* Child */
        /* Real server would set all CGI vars here */
        setenv("QUERY_STRING", cgiargs, 1);
        Dup2(fd, STDOUT_FILENO);         /* Redirect stdout to client */
        Execve(filename, emptylist, environ); /* Run CGI program */
    }
    
    Wait(NULL); /* Parent waits for and reaps child */
}

void serve_static(int fd, char *filename, int filesize) {
    int srcfd;
    char *srcp, filetype[MAXLINE], buf[MAXBUF];

    /* Send response headers to client */
    get_filetype(filename, filetype);
    sprintf(buf, "HTTP/1.1 200 OK\r\n");
    sprintf(buf, "%sServer: Tiny Web Server\r\n", buf);
    sprintf(buf, "%sConnection: close\r\n", buf);
    sprintf(buf, "%sContent-length: %d\r\n", buf, filesize);
    sprintf(buf, "%sContent-type: %s\r\n\r\n", buf, filetype);
    Rio_writen(fd, buf, strlen(buf));
    printf("Response headers:\n");
    printf("%s", buf);

    /* Send response body to client */
    srcfd = Open(filename, O_RDONLY, 0);
    srcp = Mmap(0, filesize, PROT_READ, MAP_PRIVATE, srcfd, 0);
    Close(srcfd);
    Rio_writen(fd, srcp, filesize);
    Munmap(srcp, filesize);
}

/*
* get_filetype - Derive file type from filename
*/
void get_filetype(char *filename, char *filetype) {
    if (strstr(filename, ".html")){
        strcpy(filetype, "text/html");
    } else if (strstr(filename, ".gif")) {
        strcpy(filetype, "image/gif");
    } else if (strstr(filename, ".png")) {
        strcpy(filetype, "image/png");
    } else if (strstr(filename, ".jpg")){
        strcpy(filetype, "image/jpeg");
    } else {
        strcpy(filetype, "text/plain");
    }
}

int parse_uri(char *uri, char *filename, char *cgiargs) {
    char *ptr;

    /*
       ⚙︎ Function    : strstr
       ⚙︎ Arguments   : const char *haystack, const char *needle
       ⚙︎ Return      : char *
       ⚙︎ Description : 주어진 문자열에서 특정 부분 문자열의 위치를 찾는 함수
                       - haystack: 검색 대상이 되는 문자열
                       - needle  : 찾고자 하는 부분 문자열
    */
    if(!strstr(uri, "cgi-bin")) {  /* Static content */
        strcpy(cgiargs, "");
        strcpy(filename, ".");
        strcat(filename, uri);

        if (uri[strlen(uri)-1] == '/'){
          strcat(filename, "home.html");
        }

        return 1; 
    } else {  /* Dynamic content */
        /*
        ⚙︎ Function    : index
        ⚙︎ Arguments   : const char *s, int c
        ⚙︎ Return      : char *
        ⚙︎ Description : 문자열에서 특정 문자의 첫 번째 등장 위치를 찾는 함수
                        - s: 검색할 문자열
                        - c: 검색할 문자
        */
        ptr = index(uri, '?');
        if (ptr) {
            strcpy(cgiargs, ptr+1);
            *ptr = '\0'; 
        } else {
            strcpy(cgiargs, "");
            strcpy(filename, ".");
            strcat(filename, uri);
            return 0;
        }
    }
}

void read_requesthdrs(rio_t *rp) {
    char buf[MAXLINE];
    
    Rio_readlineb(rp, buf, MAXLINE);
    while(strcmp(buf, "\r\n")) {
        Rio_readlineb(rp, buf, MAXLINE);
        printf("%s", buf);
    }

    return;
}

void clienterror(int fd, char *cause, char *errnum,char *shortmsg, char *longmsg) {
    char buf[MAXLINE], body[MAXBUF];

    /* Build the HTTP response body */
    sprintf(body, "<html><title>Tiny Error</title>");
    sprintf(body, "%s<body bgcolor=""ffffff"">\r\n", body);
    sprintf(body, "%s%s: %s\r\n", body, errnum, shortmsg);
    sprintf(body, "%s<p>%s: %s\r\n", body, longmsg, cause);
    sprintf(body, "%s<hr><em>The Tiny Web server</em>\r\n", body);

    /* Print the HTTP response */
    sprintf(buf, "HTTP/1.1 %s %s\r\n", errnum, shortmsg);
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Content-type: text/html\r\n");
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Content-length: %d\r\n\r\n", (int)strlen(body));
    Rio_writen(fd, buf, strlen(buf));
    Rio_writen(fd, body, strlen(body));
}

void doit(int fd) {
    int is_static;
    struct stat sbuf;
    char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE], filename[MAXLINE], cgiargs[MAXLINE];
    rio_t rio; // fd, cnt, bufptr, buf[]

    Rio_readinitb(&rio, fd);
    Rio_readlineb(&rio, buf, MAXLINE);
    printf("Request headers: \n");
    printf("%s", buf);
    sscanf(buf, "%s %s %s", method, uri, version);

    /*
       ⚙︎ Function    : strcasecmp
       ⚙︎ Arguments   : const char *str1, const char *str2
       ⚙︎ Return      : int
       ⚙︎ Description : 문자열을 대소문자 구분 없이 비교하는 함수입니다.
                       == 0: 동일한 문자
                        > 0: str1이 사전적으로 뒤
                        < 0: str1이 사전적으로 앞 
    */

    // Tiny Server는 HTTP Method: GET만 구현 하기로 규약
    if(strcasecmp(method, "GET")) {
        clienterror(fd, method, "501", "Not Implemented", "Tiny does not implement this method");
        return;
    }

    read_requesthdrs(&rio);
    is_static = parse_uri(uri, filename, cgiargs);

    /*
      ⚙︎ Function    : stat
      ⚙︎ Arguments   : const char *path, struct stat *buf
      ⚙︎ Return      : int
      ⚙︎ Description : 파일의 메타데이터(파일 크기, 소유자, 권한 등)를 가져오는 함수
                      == 0 : 파일을 찾음
                      == -1: 파일을 찾지 못함
    */
    if(stat(filename, &sbuf) < 0){
        clienterror(fd, filename, "404", "Not Found", "Tiny couldn't find this file");
        return;
    }

    if(is_static){
        if(!(S_ISREG(sbuf.st_mode) || S_IRUSR & sbuf.st_mode)){
            clienterror(fd, filename, "403", "Forbidden", "Tiny couldn't read the file");
            return;
        }
        serve_static(fd, filename, sbuf.st_size);
    } else {
        if(!(S_ISREG(sbuf.st_mode) || !(S_IXUSR & sbuf.st_mode))){
            clienterror(fd, filename, "403", "Forbidden", "Tiny couldn't run the CGI program");
            return;
        }
        serve_dynamic(fd, filename, cgiargs);
    }
}

int main(int argc, char **argv) {
    int listenfd, connfd;
    char hostname[MAXLINE], port[MAXLINE];
    socklen_t clientlen;
    struct sockaddr_storage clientaddr;

    /* Check command line args */
    if (argc != 2) {
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
        exit(1);
    }

    listenfd = Open_listenfd(argv[1]);

    while (1) {
      clientlen = sizeof(clientaddr);
      connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);  // line:netp:tiny:accept
      Getnameinfo((SA *)&clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE, 0);
      printf("Accepted connection from (%s, %s)\n", hostname, port);
      doit(connfd);   // line:netp:tiny:doit
      Close(connfd);  // line:netp:tiny:close
    }
}
