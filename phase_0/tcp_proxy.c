#include <arpa/inet.h>
#include <asm-generic/socket.h>
#include <bits/posix1_lim.h>
#include <bits/sockaddr.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/epoll.h>

#define PORT 8080
#define BUFF_SIZE 10000
#define MAX_ACCEPT_BACKLOG 5
#define MAX_EPOLL_EVENTS 10
#define UPSTREAM_PORT 3000
#define MAX_SOCKS 10

int listen_socket_fd, epoll_fd;
struct epoll_event events[MAX_EPOLL_EVENTS];
int route_table[MAX_SOCKS][2], route_table_size = 0;

int create_loop()
{
    int epoll_fd = epoll_create1(0);
    return epoll_fd;
}

void loop_attach(int epoll_fd, int fd, int events)
{
    struct epoll_event event;
    event.events = events;
    event.data.fd = fd;
    epoll_ctl(epoll_fd, EPOLL_CTL_ADD,fd,&event);
}

int create_server() 
{
    int listen_socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    int enable  = 1;
    setsockopt(listen_socket_fd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int));
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(PORT);

    bind(listen_socket_fd, (struct sockaddr *)&server_addr, sizeof(server_addr));

    listen(listen_socket_fd, MAX_ACCEPT_BACKLOG);
    printf("[INFO] Server listening on port %d\n", PORT);
    return listen_socket_fd;
}

int connect_upstream()
{
    int upstream_sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in upstream_addr;
    upstream_addr.sin_family = AF_INET;
    upstream_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    upstream_addr.sin_port = htons(UPSTREAM_PORT);
    connect(upstream_sock_fd, (struct sockaddr *)&upstream_addr, sizeof(upstream_addr));
    return upstream_sock_fd;
}

void accept_connection(int listen_socket_fd)
{
    struct sockaddr_in client_addr;
    socklen_t client_addr_len = sizeof(client_addr);

    int conn_sock_fd = accept(listen_socket_fd, (struct sockaddr *)&client_addr, &client_addr_len);
    loop_attach(epoll_fd, conn_sock_fd, EPOLLIN);

    int upstream_sock_fd = connect_upstream();
    loop_attach(epoll_fd, upstream_sock_fd, EPOLLIN);

    route_table[route_table_size][0] = conn_sock_fd;
    route_table[route_table_size][1] = upstream_sock_fd;
    route_table_size++;

    printf("[INFO] Client connected to server\n");
}

void handle_client(int conn_sock_fd)
{
    char buff[BUFF_SIZE];
    memset(buff, 0, BUFF_SIZE);

    int read_n = recv(conn_sock_fd, buff, sizeof(buff), 0);

    int upstream_sock_fd;
    int ind =0;
    for(int i=0; i<route_table_size; i++)
    {
        if(route_table[i][0] == conn_sock_fd)
        {
            upstream_sock_fd = route_table[i][1];
            ind = i;
        }
    }

    if(read_n <= 0)
    {
        route_table[ind][0] = -1;
        route_table[ind][1] = -1;
        close(upstream_sock_fd);
        close(conn_sock_fd);
        return;
    }

    printf("[CLIENT MESSAGE] %s", buff);

    int bytes_written = 0;
    int message_len = read_n;
    while(bytes_written < message_len)
    {
        int n = send(upstream_sock_fd, buff + bytes_written, message_len - bytes_written, 0);
        bytes_written +=n;
    }
}

void handle_upstream(int upstream_sock_fd)
{
    char buff[BUFF_SIZE];
    memset(buff, 0, BUFF_SIZE);

    int read_n = recv(upstream_sock_fd, buff, sizeof(buff), 0);

    int client_sock_fd;
    int ind = 0;
    for(int i=0; i<route_table_size; i++)
    {
        if(route_table[i][1] == upstream_sock_fd)
        {
            ind = i;
            client_sock_fd = route_table[i][0];
        }
    }

    if(read_n <= 0)
    {
        route_table[ind][0] = -1;
        route_table[ind][1] = -1;
        close(client_sock_fd);
        close(upstream_sock_fd);
        return;
    }

    printf("[CLIENT MESSAGE] %s", buff);

    int bytes_written = 0;
    int message_len = read_n;
    while(bytes_written < message_len)
    {
        int n = send(client_sock_fd, buff + bytes_written, message_len - bytes_written, 0);
        bytes_written +=n;
    }
}

void loop_run(int epoll_fd)
{
    struct epoll_event events[MAX_EPOLL_EVENTS];
    while(1)
    {
        printf("[DEBUG] Epoll wait\n");
        int n_ready_fds = epoll_wait(epoll_fd, events, MAX_EPOLL_EVENTS, -1);
        for(int i=0; i<n_ready_fds; i++)
        {
            int conn_sock_fd = events[i].data.fd;

            int is_client = 0;
            for(int i=0; i<route_table_size; i++)
            {
                if(route_table[i][0] == conn_sock_fd)
                {
                    is_client = 1;
                }
            }
            if(conn_sock_fd == listen_socket_fd)
            {
                accept_connection(listen_socket_fd);
            }
            else if(is_client)
            {
                handle_client(conn_sock_fd);
            }
            else
            {
                handle_upstream(conn_sock_fd);
            }
        }
    }
}

int main()
{
    listen_socket_fd = create_server();

    epoll_fd = create_loop();

    loop_attach(epoll_fd, listen_socket_fd, EPOLLIN);

    loop_run(epoll_fd);
}