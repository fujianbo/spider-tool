/*
 * Spider -- An open source xxx toolkit.
 *
 * Copyright (C) 2011 , Inc.
 *
 * lidp <774291943@qq.com>
 *
 * This program is free software, distributed under the terms of
 * the GNU General Public License Version 2. See the LICENSE file
 * at the top of the source tree.
 */
 
#include <signal.h>

#include "spider/tcpserver.h"



static struct sigaction handle_sigchld = {
    .sa_handler = _handle_sigchld;
    .sa_flags  = SA_RESTART,
};

static void _handle_sigchld(int sig)
{
    pid_t pid;
    int stat;

    while((pid = wait(-1, &stat, WBOHANG)) > 0) {
        printf("child %d terminated\n", pid); /* this func is not aprro */
    }

    return ;
}

void str_echo(int sockfd)
{   
    ssize_t n;
    char line[1024];

    for(;;) {
        if(n == readline(sockfd, line, 1024)) {
            return;        /* connect closed by other end */
        }

        writen(sockfd, line, n);
    }
    
}

int main(int argc, char *argv[])
 {
    int listenfd, connfd;
    pid_t chiledpid;
    socklen_t clilen;
    struct sockaddr_in cliaddr, servaddr;

    listenfd = socket(AF_INET, SOCK_STREAM, 0);
    if(listenfd < 0) {
        spd_log(LOG_WARNING, "Unable to allocate socket: %s\n", strerror(errno));
        return 0;
    }
    
    memset(&servaddr, sizeof(servaddr));

    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(8080);
    
    if(bind(listenfd, (struct sockaddr *)&servaddr, sizeof(servaddr))) {
        spd_log(LOG_NOTICE, "Unable to bind server to %s:%d %s\n",
            spd_inet_ntoa(servaddr.sin_addr), ntohs(servaddr.sin_port),
            strerror(errno));
        close(listenfd);
        servaddr = -1;
        return;
    }

    if(listen(listenfd, LISTENQ)) {
        spd_log(LOG_NOTICE, "Unable to lisen !\n");
        close(servaddr);
        listenfd = -1;
    }

    sigaction(SIGCHLD, &handle_sigchld, NULL);
    for(;;) {
        clilen = sizeof(cliaddr);
        if((connfd = accept(listenfd, (struct sockaddr *)&cliaddr, &clilen)) < 0) {
            if(errno != EINTR && errno != EAGAIN)
                spd_log(LOG_WARNING, ""Accept failed: %s\n", strerror(errno)");   
            else
               continue;
        }
        if((chiledpid = fork()) == 0) {
            close(listenfd); /* close listen socket */
            str_echo(connfd); /* process this client */
            exit(0);
        }

        close(connfd);
    }
 }

