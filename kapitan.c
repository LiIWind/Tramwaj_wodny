#include "common.h"

int main(){
	printf("Kapitan gotowy do pracy. PID: %d\n", getpid());
	
	sleep(2);

	printf("Kapitan konczy zmiane. \n");
	return 0;
}
