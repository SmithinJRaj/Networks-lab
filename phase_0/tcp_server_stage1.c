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

void strrev(char * s)
{
    char temp;
    for(int i = 0, j = strlen(s)-2; i<j; i++,j--)
    {
        temp = s[i];
        s[i] = s[j];
        s[j] = temp;
    }
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

        while(1)
        {
            char buff[BUFF_SIZE];
            memset(buff, 0, BUFF_SIZE);

            ssize_t read_n = recv(conn_sock_fd, buff, sizeof(buff), 0);

            if(read_n < 0)
            {
                printf("[INFO] Error occured. Closing Server\n");
                close(conn_sock_fd);
                break;
            }
            else if(read_n == 0)
            {
                printf("[INFO] Client Disconnected. Closing Server\n");
                close(conn_sock_fd);
                break;
            }

            printf("[CLIENT MESSAGE] %s", buff);

            strrev(buff);

            send(conn_sock_fd, buff, read_n, 0);
        }
    }
}