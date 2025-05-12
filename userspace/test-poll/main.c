#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdint.h>
#include <poll.h>


int main(){
	//poll input (pty) and normally alaway return POLLOUT
	struct pollfd test[] = {
		{.fd = STDIN_FILENO,.events = POLLOUT},
	};

	if(poll(test,sizeof(test)/sizeof(*test),0) != 1){
		printf("poll test failed\n");
		return 1;
	}

	struct pollfd wait_for_input[] = {
		{.fd = STDIN_FILENO,.events = POLLIN},
	};

	poll(wait_for_input,1,-1);

	return 0;
}