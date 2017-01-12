/**
 * @file    daemon.c
 * @author  Gyeongwon Kim
 */

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "daemon.h"

/**
 * @brief   make daemon process
 * @details When daemonize() called, the caller process will duplicate itself.
 * Parent process terminates without waiting for its child. Child process calls
 * setsid() to become session leader and call fork(). Then, it will become the
 * child process of init process, whose pid is 1.
 */
int daemonize (void)
{
    pid_t pid;

    pid = fork();
    if (pid > 0){   // Parent Process
        _exit(0);   // Use more primitive function
    }
    else if (pid < 0){           // fork() failed
        fprintf(stderr, "first fork() failed: %s\n", strerror(errno));
        return -1;
    }

    // Now, Child Process
    if (setsid() == -1){    // setsid() failed
        fprintf(stderr, "setsid() failed: %s\n", strerror(errno));
        return -2;
    }

    // Optional parts below

    // One more step to prevent the process from acquiring control terminal
    pid = fork();
    if (pid > 0){
        _exit(0);
    }
    else if (pid < 0){
        fprintf(stderr, "second fork() failed: %s\n", strerror(errno));
        return -3;
    }

    // Change directory to root
    if (chdir("/") != 0){
        fprintf(stderr, "chdir() failed: %s\n", strerror(errno));
        return -4;
    }

    // In daemon, SIGHUP is unproducible. It can be used to record some logs.
    signal(SIGHUP,SIG_IGN);

    // Daemon process does not control terminal. Hence, it is recommended to
    // close all standard file descriptors.
    // For safety, reopen fds as /dev/null
    stdin = freopen("/dev/null", "r", stdin);
    stdout = freopen("/dev/null", "w", stdout);
    stderr = freopen("/dev/null", "w", stderr);

    stdin = freopen("stdin", "r", stdin);
    stdout = freopen("stdout", "w", stdout);
    stderr = freopen("stderr", "w", stderr);
    return 0;
}
