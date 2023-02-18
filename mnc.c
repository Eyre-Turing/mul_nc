#include <unistd.h>
#include <getopt.h>
#include <string.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <pthread.h>

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>

#define LISTEN_BACKLOG  50
#define READ_BUFF_SIZE  1024

void usage(FILE *fp, const char *self)
{
    fprintf(fp, "Usage: %s [-p <port>] [-e <file>] [-c <command>]\n"
                "  -a <addr>     Address\n"
                "  -U <path>     Use unix socket. Not implemented, just declared\n"
                "  -p <port>     Port\n"
                "  -e <file>     Exec file, whenever a client connected or connected to a server\n"
                "  -c <command>  Run command by /bin/sh -c, whenever a client connected or connected to a server\n"
                "  -l            Run as server(listening), otherwise run as a client(connecting)\n"
                "  -d            Run as a daemon service\n"
                "Note: If set -e and -c at the same time, only exec file, do not run command\n",
                self);
}

void *read_server(void *arg)
{
    int fd = *((int *) arg);
    char buff[READ_BUFF_SIZE];
    ssize_t n;
    while ((n = read(fd, buff, READ_BUFF_SIZE - 1)) > 0) {
        buff[n] = '\0';
        fprintf(stdout, "%s", buff);
        fflush(stdout);
    }
    return NULL;
}

int main(int argc, char *argv[])
{
    int serv_fd, cli_fd, opt, port = 0, as_listening = 0, as_service = 0, s;
    char *tag_host, *port_str = NULL, *exec_file = NULL, *run_command = NULL, *line = NULL, *unix_sock = NULL;
    struct sockaddr_in my_addr, peer_addr;
    socklen_t peer_addr_size;
    struct addrinfo hints, *result, *rp;
    pid_t pid;
    size_t len = 0;
    ssize_t read_c;
    pthread_t thread;

    // 解析参数
    while ((opt = getopt(argc, argv, "ha:U:p:e:c:ld")) != -1) {
        switch (opt) {
        case 'h':
            usage(stdout, argv[0]);
            return 0;
            break;
        case 'a':
            tag_host = optarg;
            break;
        case 'U':
            unix_sock = optarg;     // 当前只是声明出来，还没实现unix socket，后面实现
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
        default:
            usage(stderr, argv[0]);
            return 1;
        }
    }

    if (as_service) {
        daemon(1, 0);   // 不要切换当前目录到根目录，要不然很容易在调用命令时由于路径问题坑人
    }

    // 创建套接字
    serv_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (serv_fd == -1) {
        perror("socket");
        return 1;
    }

    if (!as_listening) {    // 如果作为客户端运行
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
                // dup2(serv_fd, STDERR_FILENO);    // 这个操作不要做，要不然当调用脚本时有调试也会被重定向，服务后期会死得很惨
                if (exec_file) {
                    execl(exec_file, exec_file, NULL);
                } else if (run_command) {
                    execl("/bin/sh", "/bin/sh", "-c", run_command, NULL);
                }
                _exit(0);
            }
            wait(NULL);
        }
        pthread_create(&thread, NULL, read_server, (void *) &serv_fd);
        while ((read_c = getline(&line, &len, stdin)) != -1) {
            write(serv_fd, line, strlen(line));
        }
        free(line);
        close(serv_fd);
        return 0;
    }

    // 以下是作为服务端运行的逻辑

    // 绑定端口
    my_addr.sin_family = AF_INET;
    my_addr.sin_port = htons(port);
    my_addr.sin_addr.s_addr = inet_addr("0.0.0.0");
    if (bind(serv_fd, (struct sockaddr *) &my_addr, sizeof(my_addr)) == -1) {
        perror("bind");
        close(serv_fd);
        return 1;
    }

    // 开始监听客户端连接
    if (listen(serv_fd, LISTEN_BACKLOG) == -1) {
        perror("listen");
        close(serv_fd);
        return 1;
    }

    signal(SIGCHLD, SIG_IGN);   // 防止产生僵尸进程

    // 开始循环
    while (1) {
        // 等待连接
        peer_addr_size = sizeof(peer_addr);
        cli_fd = accept(serv_fd, (struct sockaddr *) &peer_addr, &peer_addr_size);
        if (cli_fd == -1) {
            perror("accept");
            continue;
        }

        // 创建子进程执行程序
        pid = fork();
        if (pid == -1) {
            perror("fork");
            continue;
        }
        if (pid == 0) {
            // daemon(0, 1);    // 这个操作不要做，要不然会导致守护进程过多，会产生太多垃圾进程需要手动清理
            dup2(cli_fd, STDIN_FILENO);
            dup2(cli_fd, STDOUT_FILENO);
            // dup2(cli_fd, STDERR_FILENO);     // 这个操作不要做，要不然当调用脚本时有调试也会被重定向，服务后期会死得很惨
            if (exec_file) {
                execl(exec_file, exec_file, NULL);
            } else if (run_command) {
                execl("/bin/sh", "/bin/sh", "-c", run_command, NULL);
            }
            _exit(0);
        }
        close(cli_fd);
    }
    close(serv_fd);
    return 0;
}
