#include "common.h"

int main(){
        printf("Dyzpozytor obserwuje. PID: %d\n", getpid());

        sleep(2);

        printf("Dyspozytor konczy zmiane. \n");
        return 0;
}

