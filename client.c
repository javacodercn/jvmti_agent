#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include "errno.h"
#include "log.h"

#define BUFF_SIZE 128

extern int log_level;

int main(int argc, const char* argv[]) {
	int fd =  socket(AF_INET, SOCK_STREAM, 0);
	if(fd == -1) {
		_LOG_(LOG_ERROR, "socket() error %s\n", strerror(errno));
		return -1;
	}
	struct sockaddr_in addr;
    memset(&addr, 0, sizeof(struct sockaddr_in));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    addr.sin_port = htons(8000);
	socklen_t address_len = sizeof(struct sockaddr_in);
    int ret = connect(fd, (const struct sockaddr *)&addr, address_len);
	if(ret == -1) {
		_LOG_(LOG_ERROR, "connect error %s\n", strerror(errno));
		return -1;
	}
	char cmd[12];
	char buff[BUFF_SIZE + 1];
	memset(cmd, 0, 12);
	printf("input your command[bye for  disconnect, return for continue, className#field for monitor new field modification]\n", cmd);
	scanf("%s", cmd);
	while(strcmp(cmd, "bye") != 0) {
		if(strcmp(cmd, "\n") != 0) {
			send(fd, cmd, strlen(cmd), 0);
		}
		memset(buff, 0, BUFF_SIZE+1);
		int n = recv(fd, buff, BUFF_SIZE, 0);
		if(n == -1) {
			_LOG_(LOG_ERROR, "connect error %s\n", strerror(errno));
			return -1;
		}
		buff[n]='\0';
		printf("-------------------------\n");
		printf("%s", buff);
		printf("-------------------------\n");
	
		memset(cmd, 0, 12);
		printf("input your command[bye for  disconnect, return for continue, className#field for monitor new field modification]\n", cmd);
		scanf("%s", cmd);		
	}
	send(fd, cmd, strlen(cmd), 0);
	
	ret = close(fd);
	if(ret == -1) {
		_LOG_(LOG_ERROR, "connect error %s\n", strerror(errno));
		return -1;
	}
	return 0;
}