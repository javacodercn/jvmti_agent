#include <sys/types.h>
#include <sys/socket.h>
#include <errno.h>
#include <string.h>
#include <arpa/inet.h>
#include "log.h"
#include "network.h"

int daemonfd ;

extern int log_level;

#define RECV_BUFF_SIZE 1024

int network_init(int port) {
	daemonfd = socket(AF_INET, SOCK_STREAM, 0);
	if(daemonfd == -1) {
		_LOG_(LOG_ERROR, "socket() error %s\n", strerror(errno));
		return -1;
	}
	
	int flag = 1;
	int ret = setsockopt(daemonfd, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(int));
	if(ret == -1) {
		_LOG_(LOG_ERROR, "setsockopt error %s\n", strerror(errno));
		return -1;
	}
	
	struct sockaddr_in server_addr;
	server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    bzero(&(server_addr.sin_zero), 8);
    socklen_t address_len = sizeof(struct sockaddr_in);
	ret = bind(daemonfd, (const struct sockaddr*)&server_addr,
              address_len);
	if(ret == -1) {
		_LOG_(LOG_ERROR, "bind error %s\n", strerror(errno));
		return -1;
	}
	
	_LOG_(LOG_DEBUG, "%s %d\n", "bind to success, port:", port);
	
	ret = listen(daemonfd, 5);
	if(ret == -1) {
		_LOG_(LOG_ERROR, "listen error %s\n", strerror(errno));
		return -1;
	}
	return 0;
}

char* readFromFd(int fd, char* buff, size_t sz, ssize_t* ret) {
	memset(buff, 0, RECV_BUFF_SIZE + 1);
	*ret = recv(fd, buff, RECV_BUFF_SIZE, 0);
	if(*ret == -1) {
		_LOG_(LOG_ERROR, "recv error %s\n", strerror(errno));
		return ;
	}
	_LOG_(LOG_DEBUG, "receive content: %s\n", buff);
	return buff;
}

int network_accept(gatherJvmInfo callBack) {
	struct sockaddr client_addr;
	socklen_t address_len = sizeof(struct sockaddr_in);
	int fd = accept(daemonfd, &client_addr,
              &address_len);
	if(fd == -1) {
		_LOG_(LOG_ERROR, "accept error %s\n", strerror(errno));
		return -1;
	}
	
	char buffer[RECV_BUFF_SIZE + 1];
	ssize_t ret;
	while(strcmp(readFromFd(fd, buffer, RECV_BUFF_SIZE, &ret), "bye")!=0) {
		char* content = NULL;
		if(callBack != NULL) {
			content = callBack(buffer, ret);
		} else {
			content = "no data";
		}
		
		ret = send(fd, content, strlen(content),0);
		if(ret == -1) {
			_LOG_(LOG_ERROR, "send error %s\n", strerror(errno));
			return -1;
		}
	}
	ret = close(fd);
	if(ret == -1) {
		_LOG_(LOG_ERROR, "close error %s\n", strerror(errno));
		return -1;
	}
	return 0;
}

void network_close() {
	int ret = close(daemonfd);
	if(ret == -1) {
		_LOG_(LOG_ERROR, "close error %s\n", strerror(errno));
		return ;
	}
}
