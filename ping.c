#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <resolv.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <netdb.h>
#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
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
int main(int count, char *argv[])
{
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
    while (1)
    {
        gettimeofday(&start, 0);

        // Send the packet using sendto() for sending datagrams.
        int bytes_sent = sendto(sock, packet, ICMP_HDRLEN + datalen, 0, (struct sockaddr *)&dest_in, sizeof(dest_in));

        if (bytes_sent == -1)
        {
            fprintf(stderr, "sendto() failed with error: %d", errno);
            return -1;
        }

        // bzero(packet, IP_MAXPACKET); //Commented this zeroing of transmit buffer
        socklen_t len = sizeof(dest_in);
        ssize_t bytes_received = -1;
        // using different buffer for receiving the response
        while ((bytes_received = recvfrom(sock, data, sizeof(data), 0, (struct sockaddr *)&dest_in, &len)))
        {
            if (bytes_received > 0)
            {
                // Check the IP header
                struct iphdr *iphdr = (struct iphdr *)data;
                struct icmphdr *icmphdr = (struct icmphdr *)(data + (iphdr->ihl * 4));
                // printf("%ld bytes from %s\n", bytes_received, inet_ntoa(dest_in.sin_addr));
                // icmphdr->type
                gettimeofday(&end, 0);
                unsigned long microseconds = (end.tv_sec - start.tv_sec) * 1000.0f + (end.tv_usec - start.tv_usec);
                printf("Successfuly received one packet: IP: %s, Packet seq number: %d , Time that the ping took: %ld microseconds\n", argv[1], seq++, microseconds);
                break;
            }
        }
        sleep(1);
    }
    // Close the raw socket descriptor.
    close(sock);
    return 0;
}

// Compute checksum (RFC 1071).
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