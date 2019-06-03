#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "get_num.h"
#include "my_signal.h"
#include "my_socket.h"
#include "set_timer.h"

int debug = 0;

char *interface     = NULL;
int   cap_file_size = 1;            /* 1MB */
int   filecount     = 10;           /* total 10 files */
char *cap_filename  = "a.cap.";     /* cap filename: a.cap.0, a.cap.1, ... a.cap.9 */
int   snaplen       = 68;           /* tcpdump snaplen: 68 bytes */
long  threshold     = 90*1024*1024; /* if 90MB/s, stop tcpdump and exit */
char *ip_address    = NULL;         /* remote ip address */
int   port          = 0;            /* remote port number */
int   read_count_interval = 1;      /* 1 sec */
int   socket_read_bufsize = 64*1024; /* 64kB read */
pid_t tcpdump_pid;

volatile sig_atomic_t has_alarm = 0;

int usage()
{
    char msg[] = "Usage: cap-read [-i interface] [-C cap_file_size(MB)]\n"
                 "       [-W filecount]  [-w cap_filename] [-s snaplen]\n"
                 "       [-t threshold (Bytes)] [-I read_count_interval ]\n"
                 "       [-b socket_read_bufsize] ip_address port\n"
                 "-t and -b: 4k == 4*1024, 4m == 4*1024*1024";
    fprintf(stderr, "%s\n", msg);

    return 0;
}

void sig_alarm(int signo)
{
    has_alarm = 1;
    return;
}

void sig_term_int(int signo)
{
    kill(tcpdump_pid, SIGTERM);
    exit(0);
    return;
}

int do_tcpdump()
{
    char b_cap_file_size[16];
    char b_filecount[16];
    char b_snaplen[16];
    char *my_argv[1024];

    int my_argc = 0;
    my_argv[my_argc++] = "tcpdump";

    /* -i interface */
    my_argv[my_argc++] = "-nn";
    if (interface != NULL) {
        my_argv[my_argc++] = "-i";
        my_argv[my_argc++] = interface;
    }

    /* -C cap_file_size */
    my_argv[my_argc++] = "-C";
    snprintf(b_cap_file_size, sizeof(b_cap_file_size), "%d", cap_file_size);
    my_argv[my_argc++] = b_cap_file_size;

    /* -W filecount */
    my_argv[my_argc++] = "-W";
    snprintf(b_filecount, sizeof(b_filecount), "%d", filecount);
    my_argv[my_argc++] = b_filecount;

    /* -s snaplen */
    my_argv[my_argc++] = "-s";
    snprintf(b_snaplen, sizeof(b_snaplen), "%d", snaplen);
    my_argv[my_argc++] = b_snaplen;

    /* -w capfile */
    my_argv[my_argc++] = "-w";
    my_argv[my_argc++] = cap_filename;
    
    my_argv[my_argc++] = (char *) 0;

    //close(STDIN_FILENO);
    //close(STDOUT_FILENO);
    //close(STDERR_FILENO);
    if (execvp("tcpdump", my_argv) < 0) {
        err(1, "execvp tcpdump");
    }

    return 0;
}

int main(int argc, char *argv[])
{
    int c, n;
    int sockfd;
    char *socket_read_buf = NULL;
    long read_byte_size = 0;

    while ( (c = getopt(argc, argv, "dhi:C:W:w:s:t:I:b:")) != -1) {
        switch (c) {
            case 'd':
                debug = 1;
                break;
            case 'h':
                usage();
                exit(0);
            case 'i':
                interface = optarg;
                break;
            case 'C':
                cap_file_size = strtol(optarg, NULL, 0);
                break;
            case 'W':
                filecount = strtol(optarg, NULL, 0);
                break;
            case 'w':
                cap_filename = optarg;
                break;
            case 's':
                snaplen = strtol(optarg, NULL, 0);
                break;
            case 't':
                threshold = get_num(optarg);
                break;
            case 'I':
                read_count_interval = strtol(optarg, NULL, 0);
                break;
            case 'b':
                socket_read_bufsize = get_num(optarg);
                break;
            default:
                break;
        }
    }
    argc -= optind;
    argv += optind;
    ip_address = argv[0];
    port       = strtol(argv[1], NULL, 0);

    if ( (tcpdump_pid = fork()) < 0) {
        err(1, "fork for tcpdump");
    }
    if (tcpdump_pid == 0) { /* child */
        do_tcpdump();
        exit(0);
    }
    /* parent */
    
    my_signal(SIGINT,  sig_term_int);
    my_signal(SIGTERM, sig_term_int);
    my_signal(SIGALRM, sig_alarm);
    set_timer(read_count_interval, 0, read_count_interval, 0);

    socket_read_buf = malloc(socket_read_bufsize);
    if (socket_read_buf == NULL) {
        err(1, "malloc for socket_read_buf");
    }
    sockfd = tcp_socket();
    if (connect_tcp(sockfd, ip_address, port) < 0) {
        errx(1, "connect_tcp");
    }

    for ( ; ; ) {
        if (has_alarm) {
            if (read_byte_size < threshold) {
                kill(tcpdump_pid, SIGTERM);
                exit(0);
            }
            has_alarm = 0;
            read_byte_size = 0;
        }
        n = read(sockfd, socket_read_buf, socket_read_bufsize);
        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            else {
                err(1, "read");
            }
        }
        if (n == 0) { /* EOF */
           kill(tcpdump_pid, SIGTERM);
           exit(0);
        }
        read_byte_size += n;
    }

    return 0;
}
