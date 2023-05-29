#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/wait.h>
#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h> // gettimeofday()
#include <sys/types.h>
#include <unistd.h>

// IPv4 header len without options
#define IP4_HDRLEN 20
// ICMP header len for echo req
#define ICMP_HDRLEN 8
// Checksum algo
unsigned short calculate_checksum(unsigned short *paddress, int len);
#define SOURCE_IP "127.0.0.1"
#define DESTINATION_IP "127.0.0.1"
#define SOURCE_PORT 3000
// run 2 programs using fork + exec
// command: make clean && make all && ./partb
int main(int count, char *argv[])
{
    char *args[2];
    // compiled watchdog.c by makefile
    args[0] = "./watchdog";
    args[1] = "./new_ping";

    if (fork() == 0)
    {
        execvp(args[0], args);
    }
    else
    {
        execvp(args[1], args);
    }
    // create a socket for communicating with receiver using TCP protocol
    sleep(1);
    int tcpSock; // socket descriptor
    if ((tcpSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1)
    {
        printf("Could not create socket : %d", errno);
        return -1;
    }

    struct sockaddr_in server_address; // struct of the socket
    memset(&server_address, 0, sizeof(server_address));

    server_address.sin_family = AF_INET; // Ipv4
    server_address.sin_port = htons(SOURCE_PORT);

    int rval = inet_pton(AF_INET, (const char *)SOURCE_IP, &server_address.sin_addr);

    if (rval <= 0)
    {
        printf("inet_pton() faild");
        return -1;
    }

    // Make a connnection to the Receiver server
    int connectResult = connect(tcpSock, (struct sockaddr *)&server_address, sizeof(server_address));
    if (connectResult == -1)
    { // connection faild - print a message and close socket
        printf("connect() failed with error code %d\n", errno);
        close(tcpSock);
        return -1;
    }
    

    // If we are here - connection with receiver has established!!!
    int seq = 1;
    struct icmp icmphdr; // ICMP-header
    char data[IP_MAXPACKET] = "This is the ping.\n";
    int datalen = strlen(data) + 1;

    //===================
    // ICMP header
    //===================
    // Message Type (8 bits): ICMP_ECHO_REQUEST
    icmphdr.icmp_type = ICMP_ECHO;
    // Message Code (8 bits): echo request
    icmphdr.icmp_code = 0;
    // Identifier (16 bits): some number to trace the response.
    // It will be copied to the response packet and used to map response to the request sent earlier.
    // Thus, it serves as a Transaction-ID when we need to make "ping"
    icmphdr.icmp_id = 18;
    // Sequence Number (16 bits): starts at 0
    icmphdr.icmp_seq = 0;
    // ICMP header checksum (16 bits): set to 0 not to include into checksum calculation
    icmphdr.icmp_cksum = 0;
    // Combine the packet
    char packet[IP_MAXPACKET];
    // Next, ICMP header
    memcpy((packet), &icmphdr, ICMP_HDRLEN);
    // After ICMP header, add the ICMP data.
    memcpy(packet + ICMP_HDRLEN, data, datalen);
    // Calculate the ICMP header checksum
    icmphdr.icmp_cksum = calculate_checksum((unsigned short *)(packet), ICMP_HDRLEN + datalen);
    memcpy((packet), &icmphdr, ICMP_HDRLEN);
    struct sockaddr_in dest_in;
    memset(&dest_in, 0, sizeof(struct sockaddr_in));
    dest_in.sin_family = AF_INET;
    // The port is irrelant for Networking and therefore was zeroed.
    dest_in.sin_addr.s_addr = inet_addr(argv[1]);
    // Create raw socket for IP-RAW (make IP-header by yourself)

    int sock = -1;
    if ((sock = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP)) == -1)
    {
        fprintf(stderr, "socket() failed with error: %d", errno);
        fprintf(stderr, "To create a raw socket, the process needs to be run by Admin/root user.\n\n");
        return -1;
    }

    struct timeval start, end;
    char *watchdogMsg;
    int byteRecieved = 0;

    while (byteRecieved != 8)
    {

        gettimeofday(&start, 0);

        // Send the packet using sendto() for sending datagrams.
        int bytes_sent = sendto(sock, packet, ICMP_HDRLEN + datalen, 0, (struct sockaddr *)&dest_in, sizeof(dest_in));
        if (bytes_sent == -1)
        {
            fprintf(stderr, "sendto() failed with error: %d", errno);
            return -1;
        }

        socklen_t len = sizeof(dest_in);
        int bytes_received = -1;
        while ((bytes_received = recvfrom(sock, data, sizeof(data), 0, (struct sockaddr *)&dest_in, &len)))
        {
            if (bytes_received > 0)
            {
                struct iphdr *iphdr = (struct iphdr *)data;
                struct icmphdr *icmphdr = (struct icmphdr *)(data + (iphdr->ihl * 4));
                gettimeofday(&end, 0);
                int byteSent = send(tcpSock, "ICMP Recived", sizeof("ICMP Recived"), 0);
                if (byteSent == -1)
                {
                    close(tcpSock);
                    close(sock);
                    return -1;
                }
                else if (byteSent == 0)
                {
                    printf("peer has closed the TCP connection prior to send(). \n");
                }
                int byteRecieved = recv(tcpSock, &watchdogMsg, sizeof(watchdogMsg), 0);
                if ((byteRecieved) < 0)
                {
                    printf("recv failed with error code : %d", errno);
                    close(tcpSock);
                    close(sock);
                    return -1;
                }

                byteRecieved = recv(tcpSock, &watchdogMsg, sizeof(watchdogMsg), 0); /// error
                if ((byteRecieved) < 0)
                {
                    printf("recv failed with error code : %d", errno);
                    close(tcpSock);
                    close(sock);
                    return -1;
                }

                unsigned long microseconds = (end.tv_sec - start.tv_sec) * 1000.0f + (end.tv_usec - start.tv_usec);
                if (byteRecieved == 3)
                    printf("Successfuly received one packet: IP: %s, Packet seq number: %d , Time that the ping took: %ld microseconds\n", argv[1], seq++, microseconds);
                break;
            }
        }
        sleep(1);
    }
    close(sock);
    close(tcpSock);
    return 0;
}

unsigned short calculate_checksum(unsigned short *paddress, int len)
{
    int nleft = len;
    int sum = 0;
    unsigned short *w = paddress;
    unsigned short answer = 0;

    while (nleft > 1)
    {
        sum += *w++;
        nleft -= 2;
    }

    if (nleft == 1)
    {
        *((unsigned char *)&answer) = *((unsigned char *)w);
        sum += answer;
    }

    // add back carry outs from top 16 bits to low 16 bits
    sum = (sum >> 16) + (sum & 0xffff); // add hi 16 to low 16
    sum += (sum >> 16);                 // add carry
    answer = ~sum;                      // truncate to 16 bits

    return answer;
}
