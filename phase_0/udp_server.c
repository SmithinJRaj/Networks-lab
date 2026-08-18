#include <arpa/inet.h>
#include <asm-generic/socket.h>
#include <bits/posix1_lim.h>
#include <bits/pthreadtypes.h>
#include <bits/sockaddr.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <pthread.h>

#define PORT 8080
#define BUFF_SIZE 10000

typedef struct{
    char message[BUFF_SIZE];
    struct sockaddr_in client_addr;
    int sockfd;
    socklen_t addr_len;
} client_data_t;

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

void* handle_client(void* arg)
{
    client_data_t* data = (client_data_t*)arg;
    printf("[CLIENT MESSAGE] %s", data->message);

    strrev(data->message);
    sendto(data->sockfd, data->message, strlen(data->message), 0, (struct sockaddr*)&(data->client_addr), data->addr_len);

    free(data);
    pthread_exit(NULL);
}

int main()
{
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    pthread_t thread_id;

    struct sockaddr_in addr,client_addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(PORT);

    bind(sockfd, (struct sockaddr *)&addr, sizeof(addr));

    printf("[INFO] server listening on port %d\n", PORT);

    char buffer[BUFF_SIZE];
    while(1)
    {
        socklen_t len = sizeof(client_addr);

        ssize_t n = recvfrom(sockfd, buffer, BUFF_SIZE, 0, (struct sockaddr*)&client_addr, &len);
        buffer[n] = '\0';
        
        client_data_t* data = (client_data_t*)malloc(sizeof(client_data_t));
        strcpy(data->message, buffer);
        data->client_addr = client_addr;
        data->sockfd = sockfd;
        data->addr_len = len;

        if(pthread_create(&thread_id, NULL, handle_client, (void *)data)!=0)
        {
            perror("Failed to create thread");
            free(data);
        }
        pthread_detach(thread_id);  
    }
    return 0;
}