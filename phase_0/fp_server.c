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

#define PORT 8080
#define BUFF_SIZE 10000
#define MAX_ACCEPT_BACKLOG 5

void write_to_file(int conn_sock_fd)
{
    char buffer[BUFF_SIZE];
    ssize_t bytes_received;

    FILE *fp;
    const char* filename ="t2.txt";
    fp = fopen(filename, "w");
    if(fp == NULL)
    {
        perror("[-]Error in creating file");
        exit(EXIT_FAILURE);
    }

    printf("[INFO] Receiving data from client...\n");
    while((bytes_received = recv(conn_sock_fd, buffer, sizeof(buffer), 0)) > 0)
    {
        printf("[FILE DATA] %s", buffer);
        fprintf(fp, "%s", buffer);
        memset(buffer, 0, sizeof(buffer));
    }

    if(bytes_received < 0)
    {
        perror("[-]Error in receiving data");
    }

    fclose(fp);
    printf("[INFO] Data written to file successfully.\n");
}

int main()
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
    while(1)
    {
        struct sockaddr_in client_addr;
        socklen_t client_addr_len;

        int conn_sock_fd = accept(listen_socket_fd, (struct sockaddr *)&client_addr, &client_addr_len);
        printf("[INFO] Client connected to server\n");

        write_to_file(conn_sock_fd);
    }
}