#include <unistd.h>
#include <getopt.h>
#include <string.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <pthread.h>

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>

#define LISTEN_BACKLOG  25
#define READ_BUFF_SIZE  1024

void usage(FILE *fp, const char *self)
{
    fprintf(fp, "Usage: %s [-Uld] [-a <addr>] [-p <port>] [-e <file>] [-c <command>]\n"
                "  -a <addr>     Address. If set -U, please specify unix socket file path\n"
                "  -U            Use unix socket\n"
                "  -p <port>     Port\n"
                "  -e <file>     Exec file, whenever a client connected or connected to a server\n"
                "  -c <command>  Run command by /bin/sh -c, whenever a client connected or connected to a server\n"
                "  -l            Run as server(listening), otherwise run as a client(connecting)\n"
                "  -d            Run as a daemon service\n"
                "  -k            Accept multiple connections in listen mode\n"
                "Note: If set -e and -c at the same time, only exec file, do not run command.\n"
                "      If use -l but no specify -e or -c, it will run as nc.\n",
                self);
}

struct read_server_arg {
    int fd;
    int peer_online;
};

void *read_service(void *arg)
{
    struct read_server_arg *arg_t = (struct read_server_arg *) arg;
    int fd = arg_t->fd;
    char buff[READ_BUFF_SIZE];
    ssize_t n;
    while ((n = read(fd, buff, READ_BUFF_SIZE - 1)) > 0) {
        buff[n] = '\0';
        fprintf(stdout, "%s", buff);
        fflush(stdout);
    }
    arg_t->peer_online = 0;
    return NULL;
}

int main(int argc, char *argv[])
{
    int serv_fd, cli_fd, opt, port = 0, as_listening = 0, as_service = 0, use_unix_socket = 0, keep_open = 0, s;
    char *tag_host = NULL, *port_str = NULL, *exec_file = NULL, *run_command = NULL, *line = NULL;
    struct sockaddr *my_addr = NULL, *peer_addr = NULL;
    struct sockaddr_in my_addr_in, peer_addr_in;
    struct sockaddr_un my_addr_un, peer_addr_un;
    socklen_t my_addr_size, peer_addr_size;
    struct addrinfo hints, *result, *rp;
    pid_t pid;
    size_t len = 0;
    ssize_t read_c;
    pthread_t thread;
    struct read_server_arg read_server_arg_t;

    // ????????????
    while ((opt = getopt(argc, argv, "ha:Up:e:c:ldk")) != -1) {
        switch (opt) {
        case 'h':
            usage(stdout, argv[0]);
            return 0;
            break;
        case 'a':
            tag_host = optarg;
            break;
        case 'U':
            use_unix_socket = 1;
            break;
        case 'p':
            if (strspn(optarg, "0123456789") != strlen(optarg) || (port = atoi(optarg), port > 65535)) {
                fprintf(stderr, "port must be a 0~65535 number\n");
                return 1;
            }
            port_str = optarg;
            break;
        case 'e':
            exec_file = optarg;
            break;
        case 'c':
            run_command = optarg;
            break;
        case 'l':
            as_listening = 1;
            break;
        case 'd':
            as_service = 1;
            break;
        case 'k':
            keep_open = 1;
            break;
        default:
            usage(stderr, argv[0]);
            return 1;
        }
    }

    if (as_service) {
        daemon(1, 0);   // ???????????????????????????????????????????????????????????????????????????????????????????????????
    }

    // ???????????????
    if (use_unix_socket) {
        serv_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    } else {
        serv_fd = socket(AF_INET, SOCK_STREAM, 0);
    }
    if (serv_fd == -1) {
        perror("socket");
        return 1;
    }

    if (!as_listening) {    // ???????????????????????????
        if (!tag_host) {
            fprintf(stderr, "you have to specify host address.\n");
            close(serv_fd);
            return 1;
        }
        if (use_unix_socket) {
            my_addr_un.sun_family = AF_UNIX;
            strncpy(my_addr_un.sun_path, tag_host, sizeof(my_addr_un.sun_path) - 1);
            if (connect(serv_fd, (struct sockaddr *) &my_addr_un, sizeof(my_addr_un)) == -1) {
                perror("connect");
                close(serv_fd);
                return 1;
            }
        } else {
            memset(&hints, 0, sizeof(hints));
            hints.ai_family = AF_INET;
            hints.ai_socktype = SOCK_STREAM;
            hints.ai_flags = 0;
            hints.ai_protocol = 0;
            s = getaddrinfo(tag_host, port_str, &hints, &result);
            if (s != 0) {
                fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(s));
                close(serv_fd);
                return 1;
            }
            for (rp = result; rp; rp = rp->ai_next) {
                if (connect(serv_fd, rp->ai_addr, rp->ai_addrlen) != -1) {
                    break;
                }
                perror("connect");
            }
            freeaddrinfo(result);
            if (rp == NULL) {
                fprintf(stderr, "could not connect\n");
                close(serv_fd);
                return 1;
            }
        }
        if (exec_file || run_command) {
            pid = fork();
            if (pid == -1) {
                perror("fork");
                close(serv_fd);
                return 1;
            }
            if (pid == 0) {
                dup2(serv_fd, STDIN_FILENO);
                dup2(serv_fd, STDOUT_FILENO);
                // dup2(serv_fd, STDERR_FILENO);    // ????????????????????????????????????????????????????????????????????????????????????????????????????????????
                if (exec_file) {
                    execl(exec_file, exec_file, NULL);
                } else if (run_command) {
                    execl("/bin/sh", "/bin/sh", "-c", run_command, NULL);
                }
                _exit(0);
            }
            wait(NULL);
        }
        read_server_arg_t.fd = serv_fd;
        read_server_arg_t.peer_online = 1;
        pthread_create(&thread, NULL, read_service, (void *) &read_server_arg_t);
        while ((read_c = getline(&line, &len, stdin)) != -1) {
            if (read_server_arg_t.peer_online == 0) {
                fprintf(stderr, "peer has performed an orderly shutdown.\n");
                break;
            }
            write(serv_fd, line, strlen(line));
        }
        free(line);
        close(serv_fd);
        return 0;
    }

    // ???????????????????????????????????????

    // ????????????
    if (use_unix_socket) {
        if (!tag_host) {
            fprintf(stderr, "you have to specify unix socket file path.\n");
            close(serv_fd);
            return 1;
        }
        my_addr_un.sun_family = AF_UNIX;
        strncpy(my_addr_un.sun_path, tag_host, sizeof(my_addr_un.sun_path) - 1);
        my_addr = (struct sockaddr *) &my_addr_un;
        my_addr_size = sizeof(my_addr_un);
    } else {
        my_addr_in.sin_family = AF_INET;
        my_addr_in.sin_port = htons(port);
        if (tag_host) {
            my_addr_in.sin_addr.s_addr = inet_addr(tag_host);
        } else {
            my_addr_in.sin_addr.s_addr = inet_addr("0.0.0.0");
        }
        my_addr = (struct sockaddr *) &my_addr_in;
        my_addr_size = sizeof(my_addr_in);
    }

    // TIME_WAIT??????????????????????????????????????????????????????socket??????
    s = 1;
    setsockopt(serv_fd, SOL_SOCKET, SO_REUSEADDR,  &s, sizeof(s));

    if (bind(serv_fd, my_addr, my_addr_size) == -1) {
        perror("bind");
        close(serv_fd);
        return 1;
    }

    // ???????????????????????????
    if (listen(serv_fd, LISTEN_BACKLOG) == -1) {
        perror("listen");
        close(serv_fd);
        return 1;
    }

    signal(SIGCHLD, SIG_IGN);   // ????????????????????????

    // ????????????
    while (serv_fd >= 0) {
        // ????????????
        if (use_unix_socket) {
            peer_addr = (struct sockaddr *) &peer_addr_un;
            peer_addr_size = sizeof(peer_addr_un);
        } else {
            peer_addr = (struct sockaddr *) &peer_addr_in;
            peer_addr_size = sizeof(peer_addr_in);
        }
        cli_fd = accept(serv_fd, peer_addr, &peer_addr_size);
        if (cli_fd == -1) {
            perror("accept");
            continue;
        }

        if (!keep_open) {
            close(serv_fd);
            serv_fd = -1;
        }

        if (!exec_file && !run_command) {
            // ??????????????????????????????????????????????????????nc???????????????????????????????????????????????????????????????
            // ??????????????????????????????????????????????????????????????????????????????????????????????????????????????????

            read_server_arg_t.fd = cli_fd;
            read_server_arg_t.peer_online = 1;
            pthread_create(&thread, NULL, read_service, (void *) &read_server_arg_t);
            while ((read_c = getline(&line, &len, stdin)) != -1) {
                if (read_server_arg_t.peer_online == 0) {
                    fprintf(stderr, "peer has performed an orderly shutdown.\n");
                    break;
                }
                write(cli_fd, line, strlen(line));
            }
            free(line);
            line = NULL;

            // ??????????????????????????????while
            close(cli_fd);
            continue;
        }

        // ???????????????????????????
        pid = fork();
        if (pid == -1) {
            perror("fork");
            continue;
        }
        if (pid == 0) {
            // daemon(0, 1);    // ????????????????????????????????????????????????????????????????????????????????????????????????????????????
            dup2(cli_fd, STDIN_FILENO);
            dup2(cli_fd, STDOUT_FILENO);
            // dup2(cli_fd, STDERR_FILENO);     // ????????????????????????????????????????????????????????????????????????????????????????????????????????????
            if (exec_file) {
                execl(exec_file, exec_file, NULL);
            } else if (run_command) {
                execl("/bin/sh", "/bin/sh", "-c", run_command, NULL);
            }
            _exit(0);
        }
        close(cli_fd);
    }
    if (serv_fd >= 0) {
        close(serv_fd);
    }
    return 0;
}
