#ifndef NETWORK_HEADER_FILE
#define NETWORK_HEADER_FILE

int network_init(int port);

typedef char* (*gatherJvmInfo)(char* param, ssize_t);

int network_accept();

void network_close() ;

#endif