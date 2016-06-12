/*
 * tftp_server.h
 *
 *  Created on: Jun 12, 2016
 *      Author: olga
 */


#ifndef TFTP_SERVER_H_
#define TFTP_SERVER_H_


#include <sys/types.h>
#include <sys/socket.h>

using namespace std;

enum {
    OP_RRQ = 1,		//Read request
    OP_WRQ,			//Write request
    OP_DATA,		//DATA
    OP_ACK,			//Acknowledgment
    OP_ERROR,		//Error
};

#define OP_SIZE 2
#define MAX_MESSAGE_LENGTH 516

struct packet_head {
    short opcode;
    short block_number;
} __attribute__((packed));


typedef struct packet_head packet_head;

class WRQ_packet{
public:
	WRQ_packet(short opcode_, short blockNumber_, string fileName) : file_name(fileName){
		head.opcode = opcode_;
		head.block_number = blockNumber_;
	}
	packet_head head;
	string file_name;
	string transmission_mode;
};

class ACK_packet{
public:
	ACK_packet(short opcode_, short blockNumber_) {
		head.opcode = opcode_;
		head.block_number = blockNumber_;
	}
	packet_head head;
};

class DATA_packet{
public:
	packet_head head;
	string data;
};



const int WAIT_FOR_PACKET_TIMEOUT = 3;
const int NUMBER_OF_FAILURES = 7;
const int MAX_PORT = 65536;
const int MIN_PORT = 1000;





#endif /* TFTP_SERVER_H_ */
