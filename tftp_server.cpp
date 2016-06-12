/*
 * tftp_server.cpp
 *
 *  Created on: Jun 12, 2016
 *      Author: olga
 */

#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <string.h>
#include <iostream>
#include <netinet/in.h> // sockaddr_in
#include "tftp_server.h"

using namespace std;


int handle_WRQ(int conn, char *buf, int buf_size, struct sockaddr_in *client)
{
    struct timeval timeout;
    fd_set rfds;
    int retval;
    char buffer[MAX_MESSAGE_LENGTH] = {0};
    bool more_blocks = true;
    int block_number = 0;
    int timeoutExpiredCount = 0;
    string delimeter = "\0";
    int send_try = -1;
    int write_try = -1;

    size_t pos = 0;

    // read and parse WRQ packet
    if (buf_size < 4) {
        // WRQ packet too short
        return 0;
    }
    WRQ_packet wrq(ntohs(*(uint16_t*)buf), 0, buf+OP_SIZE);

    // search for null terminator starting from file_name field until previous
    // to last byte in buf.
    pos = (wrq.file_name).find( delimeter , buf_size-OP_SIZE-1);
    if (!pos) {
        // no zero terminator found
    	cout << "couldn't find file_name NULL terminator" << endl;
        return 0;
    }
    wrq.transmission_mode = pos+1;
    // search for null terminator starting from transmission_mode field until
    // last byte in buf.
    pos = (wrq.file_name).find(delimeter, buf_size-OP_SIZE-(wrq.transmission_mode.c_str()-wrq.file_name.c_str()));
    if (!pos) {
        cout << "couldn't find transmission_mode NULL terminator" << endl;
        return 0;
    }

    cout << "IN:WRQ," << wrq.file_name << "," << wrq.transmission_mode << endl;

    FILE *output = fopen(wrq.file_name.c_str(), "w");
    if (!output) {
        // fopen failed
        perror("TTFTP_ERROR");
        return 0;
    }

    ACK_packet ack(htons(OP_ACK), 0);
    cout << "OUT:ACK,0" << endl;
    send_try = sendto(conn, &ack, sizeof(ack), 0, (struct sockaddr*)client, sizeof(struct sockaddr_in));
    if (send_try == -1) {
        perror("TTFTP_ERROR");
        fclose(output);
        return 0;
    }

    do
    {
        int size_to_write = 512;
        do
        {
            int bad_data = 0;
            int wrong_block = 0;
            do
            {
                // TODO: Wait WAIT_FOR_PACKET_TIMEOUT to see if something appears
                // for us at the socket (we are waiting for DATA)
                timeout.tv_sec = WAIT_FOR_PACKET_TIMEOUT;
                timeout.tv_usec = 0;
                // init select() fd list
                FD_ZERO(&rfds);
                FD_SET(conn, &rfds);
                retval = select(conn+1, &rfds, NULL, NULL, &timeout);
                if (retval == -1) { // if select() failed
                    // TODO: perhaps we should continue with the loop..
                    fclose(output);
                    perror("TTFTP_ERROR");
                    return 0;
                }
                if (retval)// if there was something at the socket and
                    // we are here not because of a timeout
                {
                    // TODO: Read the DATA packet from the socket (at
                    // least we hope this is a DATA packet)
                    struct sockaddr_in src_addr;
                    socklen_t addrlen = sizeof(struct sockaddr);
                    int recv_retval = recvfrom(conn, buffer, MAX_MESSAGE_LENGTH, 0, (struct sockaddr*)&src_addr, &addrlen);
                    if (recv_retval == -1) { //recvfrom failed
                        fclose(output);
                        perror("TTFTP_ERROR");
                        return 0;
                    }
                    //check src_addr is valid.
                    if (client->sin_port != src_addr.sin_port || client->sin_addr.s_addr != src_addr.sin_addr.s_addr){
                    	continue;
                    }

                    //check header in buffer is valid, right block number, right opcode.
                    uint16_t opcode = ntohs(*(uint16_t*)buffer);
                    if (opcode != OP_DATA) {
                        bad_data = 1;
                        break;
                    }
                    uint16_t rcv_block_num = ntohs(*(uint16_t*)(buffer + 2));
                    if (rcv_block_num != block_number+1) {
                        wrong_block = 1;
                        cout << "FLOWERROR: Wrong incoming block number: " << rcv_block_num;
                        cout << ", is not expected block number: " << block_number+1 << endl;
                        break;
                    }

                    if (recv_retval < 516) { //this is the last one.
                        more_blocks = false;
                        size_to_write = recv_retval - 4;
                    }
                    cout << "IN:DATA," << rcv_block_num << "," << recv_retval << endl;
                    block_number++;
                    break;
                }
                else // Time out expired while waiting for data
                    // to appear at the socket
                {
                    //Send another ACK for the last packet
                    //send ack
                    ack.head.opcode = htons(OP_ACK);
                    ack.head.block_number = htons(block_number);
                    send_try = sendto(conn, &ack, sizeof(ack), 0, (struct sockaddr*)client, sizeof(struct sockaddr_in));
                    if (send_try == -1) {
                        perror("TTFTP_ERROR");
                        fclose(output);
                        return 0;
                    }
                    cout << "OUT:ACK," << block_number << endl;
                    timeoutExpiredCount++;
                }

                if (timeoutExpiredCount>= NUMBER_OF_FAILURES)
                {
                    // FATAL ERROR BAIL OUT
                	cout << "FLOWERROR: There were more than upper limit: " << NUMBER_OF_FAILURES;
                	cout << " time outs, FATAL error." << endl;
                    fclose(output);
                    return 0;
                }
            }while (true); // Continue while we didn't receive data

            if (bad_data == 1) // We got something else but DATA
            {
                // FATAL ERROR BAIL OUT
            	cout << "FLOWERROR: Received bad packet type, not DATA, FATAL error." << endl;
                fclose(output);
                return 0;
            }
            if (wrong_block == 1) // The incoming block number is not what we have
                // expected, i.e. this is a DATA pkt but the block number
                // in DATA was wrong (not last ACK’s block number + 1)
            {
                // FATAL ERROR BAIL OUT
            	cout << "FLOWERROR: Wrong incoming block number: " << block_number;
            	cout << " FATAL error." << endl;
                fclose(output);
                return 0;
            }

        } while (false);

        timeoutExpiredCount = 0;
        // write next bulk of data
        write_try = fwrite(buffer + 4, 1, size_to_write, output);
        if (write_try != size_to_write) {
            perror("TTFTP_ERROR");
            fclose(output);
            return 0;
        }

        cout << "WRITING: " << size_to_write << endl;

        //send ack
        ack.head.opcode = htons(OP_ACK);
        ack.head.block_number = htons(block_number);
        send_try = sendto(conn, &ack, sizeof(ack), 0, (struct sockaddr*)client, sizeof(struct sockaddr_in));
        if (send_try == -1) {
            perror("TTFTP_ERROR");
            fclose(output);
            return 0;
        }
        cout << "OUT:ACK," << block_number << endl;

    } while (more_blocks); // Have blocks left to be read from client (not end of transmission)
    fclose(output);
    return 1;
}



int main(int argc, const char * argv[])
{
	//check input arguments
	if (argc != 2){
        fprintf(stderr, "Bad parameters: %s\n", argv[0]);
        exit(-1);
	}
    if (MIN_PORT > atoi(argv[1]) || MAX_PORT < atoi(argv[1]) ) {
        fprintf(stderr, "Usage: %s port\n", argv[0]);
        exit(-1);
    }

    //define all needed variables
    int port = atoi(argv[1]);
    int recv_len = 0;
    int sock = -1;
    short opcode = 0;
    int bind_try = -1;
    struct sockaddr_in listen_addr, peer_addr;
    socklen_t addrlen = sizeof(struct sockaddr);

    char buf[MAX_MESSAGE_LENGTH] = {0};

    // create an UDP socket
    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock == -1) {
        perror("TTFTP_ERROR");
        return -1;
    }

    // configure listening ip & port
    memset(&listen_addr, 0, sizeof(struct sockaddr));
    listen_addr.sin_family = AF_INET;
    listen_addr.sin_port = htons(port);
    listen_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    //assign address to socket
    bind_try = bind(sock, (struct sockaddr*)&listen_addr, sizeof(struct sockaddr));
    if (bind_try == -1) {
        perror("TTFTP_ERROR");
        return -1;
    }

    //until the end of times
    for (;;) {
        addrlen = sizeof(struct sockaddr);
        recv_len = recvfrom(sock, buf, sizeof(buf), 0, (struct sockaddr*)&peer_addr, &addrlen);
        if (recv_len == -1) {
            perror("TTFTP_ERROR");
            return -1;
        }
        if (recv_len < 2) {
            // we at least want the opcode
            continue;
        }
        opcode = ntohs(*(uint16_t*)buf);
        if (opcode == OP_WRQ) {
            if (handle_WRQ(sock, buf, recv_len, &peer_addr)) {
            	cout << "RECVOK" << endl;
            } else {
            	cout << "RECVFAIL" << endl;
            }
        } else {
        	cout << "FLOWERROR: Bad packet opcode: " << opcode << endl;
        }
    }

    return 0;
}

