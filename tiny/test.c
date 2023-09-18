#include <stdio.h>
#include <stdlib.h>
#include <unistd.h> // sleep 함수를 사용하기 위해

int main() {
    const char *uri = "https://example.com/page?param=value";
    char *ptr = index(uri, '?');

    if (ptr != NULL) {
        printf("Question mark found at position %ld\n", ptr - uri);
    } else {
        printf("Question mark not found in the URI.\n");
    }
}

// #include <stdio.h>
// #include <stdlib.h>
// #include "csapp.c"

// int open_clientfd(char *hostname, char *port){
//     int clientfd;
//     struct addrinfo hints, *listp, *p;
    
//     memset(&hint, 0, sizeof(hints));
//     hints.ai_socktype = SOCK_STREAM;
//     hints.ai_flags = AI_NUMERICSERV | AI_ADDRCONFIG;
//     Getaddrinfo(hostname, port, &hints, &listp);
    
//     for(p = listp; p; p = p -> ai_next){
//         if((clientfd = socket(p -> ai_family, p -> ai_socktype, p -> ai_protocol)) < 0){
//             continue;
//         }

//         if(connect(clientfd, p -> ai_addr, p -> ai_addrlen) != -1){
//             break;
//         }

//         Close(clientfd);
//     }

//     Freeaddrinfo(listp);

//     if(!p){
//         return -1;
//     } else {
//         return clientfd;
//     }
// }

// int open_listenfd(char *port){
//     struct addrinfo hints, *listp, *p;
//     int listenfd, optval = 1;

//     memset(&hints, 0, sizeof(hints));
//     hints.ai_socktype = SOCK_STREAM;
//     hints.ai_flags = AI_PASSIVE | AI_ADDRCONFIG | AI_NUMERICSERV;
//     Getaddrinfo(NULL, port, &hints, &listp);

//     for(p = listp; p; p = p -> ai_next){
//         if((listenfd = socket(p -> ai_family, p -> ai_socktype, p -> ai_protocol)) < 0){
//             continue;
//         }

//         Setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, (const void *)&optval, sizeof(int));

//         if(bind(listenfd, p -> ai_addr, p -> ai_addrlen) == 0){
//             break;
//         }

//         Close(listenfd);

//         Freeaddrinfo(listp);

//         if(!p){
//             return -1;
//         }

//         if(listen(listenfd, LISTENQ) < 0){
//             Close(listenfd);
//             return -1;
//         }
//         return listenfd;
//     }
// }

// int main(void) {
//     struct addrinfo *p, *listp, hints;
//     char buf[MAXLINE];
//     int rc, flags;
//     char *domain = "www.twitter.com";

//     /* Get a list of addrinfo records */
//     memset(&hints, 0, sizeof(hints));
//     hints.ai_flags = AI_CANONNAME;
//     hints.ai_family = AF_INET;       /* IPv4 only */
//     hints.ai_socktype = SOCK_STREAM; /* Connections only */
//     Getaddrinfo(domain, NULL, &hints, &listp);

//     /* Walk the list and display each IP address */
//     flags = NI_NUMERICHOST; /* Display address string instead of domain name */
//     for (p = listp; p != NULL; p = p -> ai_next) {
//         Getnameinfo(p -> ai_addr, p -> ai_addrlen, buf, MAXLINE, NULL, 0, flags);

//         if(p -> ai_canonname){
//             printf("%s\n", p -> ai_canonname);
//         }

//         printf("%s\n", buf);
//     }
//     /* Clean up */
//     Freeaddrinfo(listp);
//     exit(0);
// }