/*
 * adder.c - a minimal CGI program that adds two numbers together
 */
/* $begin adder */
#include "csapp.h"

int main(void) {
    char *buf, *p;
    char arg1[MAXLINE], arg2[MAXLINE], content[MAXLINE];
    int n1=0, n2=0;
    
    /* Extract the two arguments */
    if ((buf = getenv("QUERY_STRING")) != NULL) {
        // p = strchr(buf, '&');
        // *p = '\0';
        // strcpy(arg1, buf);
        // strcpy(arg2, p+1);
        // n1 = atoi(arg1);
        // n2 = atoi(arg2);
        sscanf(buf, "a=%d&b=%d", &n1, &n2);
    }

    /* Make the response body */
    sprintf(content, "QUERY_STRING=%s", buf);
    sprintf(content, "Welcome to add.com: ");
    sprintf(content, "%sTHE Internet addition portal.\r\n<p>", content);
    sprintf(content, "%s<!DOCTYPE html><html><head><title>Adder Form</title></head><body>", content);
    sprintf(content, "%s<form action=\"adder\" method=\"GET\">", content);
    sprintf(content, "%s<input type=\"number\" id=\"a\" name=\"a\" required>", content);
    sprintf(content, "%s<input type=\"number\" id=\"b\" name=\"b\" required>", content);
    sprintf(content, "%s<input type=\"submit\" value=\"Add\"></form>", content);
    sprintf(content, "%s</body></html>", content);
    sprintf(content, "%sThe answer is: %d + %d = %d\r\n<p>",
            content, n1, n2, n1 + n2);
    sprintf(content, "%sThanks for visiting!\r\n", content);

    /* Generate the HTTP response */
    printf("Connection: close\r\n");
    printf("Content-length: %d\r\n", (int)strlen(content));
    printf("Content-type: text/html\r\n\r\n");
    printf("%s", content);
    fflush(stdout);
    exit(0);
}
/* $end adder */
