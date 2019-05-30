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

volatile sig_atomic_t has_alarm = 0;

int usage()
{
    char msg[] = "Usage: cap-read [-i interface] [-C cap_file_size(MB)] [-W filecount]  [-w cap_filename] [-s snaplen] [-t threshold (Bytes)] [-I read_count_interval ] [-b socket_read_bufsize] ip_address port";
    fprintf(stderr, "%s\n", msg);

    return 0;
}

int start_tcpdump(char *tcpdump_command)
{
    fprintf(stderr, "start tcpdump in background: %s\n", tcpdump_command);
    system(tcpdump_command);
    fprintf(stderr, "tcpdump is running in background\n");

    return 0;
}

void sig_alarm(int signo)
{
    has_alarm = 1;
    return;
}

void sig_int(int signo)
{
    system("pkill tcpdump");
    exit(0);
    return;
}

int create_tcpdump_command(char *command_buf)
{
    /* tcpdump command example:
     * tcpdump -nn -i exp0 -C 1 -W 10 -s 68 -w a.cap.
     */
    
    char tmp[16];

    /* command base */
    strcat(command_buf, "tcpdump -nn");
    if (interface != NULL) {
        strcat(command_buf, " -i ");
        strcat(command_buf, interface);
    }
    /* -C cap_file_size */
    strcat(command_buf, " -C ");
    snprintf(tmp, sizeof(tmp), "%d", cap_file_size);
    strcat(command_buf, tmp);

    /* -W filecount */
    strcat(command_buf, " -W ");
    snprintf(tmp, sizeof(tmp), "%d", filecount);
    strcat(command_buf, tmp);

    /* -s snaplen */
    strcat(command_buf, " -s ");
    snprintf(tmp, sizeof(tmp), "%d", snaplen);
    strcat(command_buf, tmp);

    /* -w cap_filename*/
    strcat(command_buf, " -w ");
    strcat(command_buf, cap_filename);

    strcat(command_buf, " < /dev/null > /dev/null 2>&1 &");
    return 0;
}

int main(int argc, char *argv[])
{
    char tcpdump_command[1024];
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
                socket_read_bufsize = strtol(optarg, NULL, 0);
                break;
            default:
                break;
        }
    }
    argc -= optind;
    argv += optind;
    ip_address = argv[0];
    port       = strtol(argv[1], NULL, 0);
    create_tcpdump_command(tcpdump_command);
    if (debug) {
        printf("tcpdump command: %s\n", tcpdump_command);
        exit(1);
    }
    system(tcpdump_command);
    
    my_signal(SIGALRM, sig_alarm);
    my_signal(SIGINT,  sig_int);
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
                system("pkill tcpdump");
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
           system("pkill tcpdump");
           exit(0);
        }
        read_byte_size += n;
    }

    return 0;
}
