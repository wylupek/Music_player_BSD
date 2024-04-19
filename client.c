#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <fcntl.h>
#include <pthread.h>
#include <cjson/cJSON.h>

#define MAX_JSON_SIZE 8192
#define TCP_PORT 8888
#define TCP_ADDRESS "192.168.68.228"

void *sockThreadRecv(void *arg) {
    printf("\nReceiving...\n");
    char client_message[MAX_JSON_SIZE];
    int newSocket = *((int *)arg);

    /* Receiving TCP messages */
    for (;;) {
        if (recv(newSocket, client_message, 8192, 0) < 1) {
            break;
        }
        printf("Message:\n%s\n", client_message);
        memset(client_message, 0, sizeof (client_message));    
    }
}

int main() {
    char message[1000];
    int clientSocket;
    struct sockaddr_in serverAddr;
    socklen_t addr_size;

    /* Configure socket */
    clientSocket = socket(PF_INET, SOCK_STREAM, 0);
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(TCP_PORT);
    serverAddr.sin_addr.s_addr = inet_addr(TCP_ADDRESS);
    memset(serverAddr.sin_zero, '\0', sizeof serverAddr.sin_zero);
    addr_size = sizeof serverAddr;
    connect(clientSocket, (struct sockaddr *) &serverAddr, addr_size);

    
    /* Receiving thread */
    pthread_t thread_id1;
    if (pthread_create(&thread_id1, NULL, sockThreadRecv, &clientSocket) != 0) {
        printf("Failed to create recv thread\n");
    }
    pthread_detach(thread_id1);
    usleep(500000);

    /* Send loop */
    // if(send(clientSocket , "AQ;test1.mp3;Song 1;Artist 1\n" , strlen("AQ;test1.mp3;Song 1;Artist 1") , 0) < 0) {
    //     printf("Send failed\n");
    // }
    // sleep(1);
    int msg_scanf_size;
    for(;;) {
        printf("Data to send: ");
        msg_scanf_size=scanf("%s",message);
        char *s;
        s=strstr(message,"exit");
        if(s != NULL) {
            printf("Exiting\n");
            break;
        }
        if(send(clientSocket , message , strlen(message) , 0) < 0) {
            printf("Send failed\n");
        }
        memset(message, 0, sizeof (message));
    }
    close(clientSocket);
  return 0;
}
