#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>
#include <arpa/inet.h>
#include "include/http.h"
#include "include/log.h"
#include "include/opt.h"
#include "include/color.h"
#include "include/utils.h"
#include <string.h>
#include <getopt.h>

int main(int argc, char *argv[])
{
    signal(SIGPIPE, SIG_IGN);

    print_program_name();

    // handling options
    int port = 8080;
    char *log_level_string = getenv("LOG_LEVEL");
    if (log_level_string == NULL)
    {
        log_level_string = "INFO";
    }
    int c;
    while ((c = getopt_long(argc, argv, "p:l:h", opts, NULL)) != -1)
    {
        switch (c)
        {
        case 'p':
            port = atoi(optarg);
            break;
        case 'l':
            log_level_string = optarg;
            if (strcmp(log_level_string, "DEBUG") == 0)
            {
                change_log_level(LOG_DEBUG);
            }
            else if (strcmp(log_level_string, "INFO") == 0)
            {
                change_log_level(LOG_INFO);
            }
            else if (strcmp(log_level_string, "WARN") == 0)
            {
                change_log_level(LOG_WARN);
            }
            else if (strcmp(log_level_string, "ERROR") == 0)
            {
                change_log_level(LOG_ERROR);
            }
            else
            {
                fprintf(stderr, "Unknown log leve %s\nuse %s -h\n", optarg, argv[0]);
                exit(1);
            }
            break;
        case 'h':
            usage(argv[0]);
            return 0;
        case '?':
            return 1; /* getopt already printed the error */
        }
    }

    if (port <= 0 || port > 65535)
    {
        fprintf(stderr, "Invalid port: %s\n", argv[1]);
        exit(EXIT_FAILURE);
    }


    // initializing the variables
    int server_fd, client_fd;
    struct sockaddr_in address;
    socklen_t addr_len = sizeof(address);

    // creating the socket
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0)
    {
        perror("error creating socket");
        exit(EXIT_FAILURE);
    }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);

    // binding the socket
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0)
    {
        perror("bind");
        exit(EXIT_FAILURE);
    }

    // listening
    if (listen(server_fd, 512) < 0)
    {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    LOG_I("Server is listening on port %s%d%s", ANSI_BOLD ANSI_FG_CYAN, port, ANSI_RESET);

    while (1)
    {
        // accepting a connection
        client_fd = accept(server_fd, (struct sockaddr *)&address, &addr_len);
        if (client_fd < 0)
        {
            LOG_E("Error accepting connection");
            continue;
        }
        int *pfd = malloc(sizeof *pfd);
        if (pfd == NULL)
        {
            perror("Couldn't allocate memory for client fd");
            return 1;
        }

        *pfd = client_fd;
        pthread_t tid;
        int ret = pthread_create(&tid, NULL, handle_request, pfd);
        if (ret != 0)
        {
            fprintf(stderr, "pthread_create failed: %s\n", strerror(ret));
            return 1;
        }
        pthread_detach(tid);
    }

    close(server_fd);
    return 0;
}
