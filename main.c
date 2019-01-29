#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "log.h"
#include "network.h"

extern int log_level;

int main(int argc, char ** argv)
{
	network_init(8000);
	_LOG_(LOG_ERROR, "%s\n", "333333333333333");
	network_accept();
	_LOG_(LOG_ERROR, "%s\n","4444444444444444");
	network_close();
}
