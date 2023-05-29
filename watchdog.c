#include <stdio.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <errno.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/tcp.h>
#include <time.h>
#include <sys/time.h> // gettimeofday()

#define SERVER_PORT 3000              // port for making connection
#define SERVER_IP_ADDRESS "127.0.0.1" // local IP

int main()
{
    int listeningSocket = -1;
    listeningSocket = socket(AF_INET, SOCK_STREAM, 0); // The listining socket descriptor
    if (listeningSocket == -1)
    {
        printf("Could not create socket : %d", errno);
        return -1;
    }

    int enableReuse = 1;
    int ret = setsockopt(listeningSocket, SOL_SOCKET, SO_REUSEADDR, &enableReuse, sizeof(int)); // enable reuse of the socket on its port
    if (ret < 0)
    {
        printf("setsocketpot() failed with error code : %d", errno);
        return -1;
    }

    struct sockaddr_in server_address; // struct of the socket
    memset(&server_address, 0, sizeof(server_address));

    server_address.sin_family = AF_INET;          // use IPv4 address
    server_address.sin_port = htons(SERVER_PORT); //
    server_address.sin_addr.s_addr = INADDR_ANY;  // any IP at this port

    int bindResult = bind(listeningSocket, (struct sockaddr *)&server_address, sizeof(server_address)); // iund the port with address and port
    if (bindResult == -1)
    {
        printf("bind() failed with error code : %d\n", errno);
        // close socket
        close(listeningSocket);
        return -1;
    }
    int listenResult = listen(listeningSocket, 4); // start listining for incoming sockets
    if (listenResult == -1)
    {
        printf("listen() failed with error code : %d", errno);
        // close the socket
        close(listeningSocket);
    }

    struct sockaddr_in clientAddress; // A new socket for the tcp connection with client
    socklen_t clientAddressLen = sizeof(clientAddress);

    memset(&clientAddress, 0, sizeof(clientAddress));
    clientAddressLen = sizeof(clientAddress);

    int clientSocket = accept(listeningSocket, (struct sockaddr *)&clientAddress, &clientAddressLen); // accept a new connection from the sender
    if (clientSocket == -1)
    {
        printf("listen() failed with error code : %d", errno);
        close(listeningSocket);
        return -1;
    }
    int byteRecieved = 0;
    //printf("%s",argv[1]);
    struct timeval start, end;
    float timer = 0;
    int sizeOfMsg = 0;
    int byteSent = 0;
    while (timer < 10)
    {
        timer = 0;
        gettimeofday(&start, 0);
        //sleep(10);//check the watchdog
        byteRecieved = recv(clientSocket, &sizeOfMsg, sizeof(sizeOfMsg), 0);
        if ((byteRecieved) < 0)
        {
            printf("recv failed with error code : %d", errno);
            close(listeningSocket);
            close(clientSocket);
            return -1;
        }
        else
        {
            byteSent = send(clientSocket, "ok", sizeof("ok"), 0);
        }
        if (byteSent == -1)
        {
            printf("send() failed with error code : %d ", errno);
            close(listeningSocket);
            close(clientSocket);
            return -1;
        }
        else if (byteSent == 0)
        {
            printf("peer has closed the TCP connection prior to send(). \n");
        }
        gettimeofday(&end, 0);
        timer = (end.tv_sec - start.tv_sec) + (end.tv_usec - start.tv_usec) / 1e6;
        sleep(1);
    }
    byteSent = send(clientSocket, "timeout", sizeof("timeout"), 0);
    printf("Server cannot be reached. 10 sec passed.\n");
    close(listeningSocket);
    close(clientSocket);
    return 0;
}