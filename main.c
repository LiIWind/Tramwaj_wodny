#include "common.h"

int main(){
	printf("Start systemu tramwaju wodnego\n");

	//uruchomienie procesu kapitana
	pid_t pid_kapitan = fork();
	if (pid_kapitan == 0) {
		//proces potomny
		execl("./kapitan", "kapitan", NULL);
		perror("Blad uruchomienia kapitana");
		exit(1);
	}

	pid_t pid_dyspozytor = fork();
        if (pid_dyspozytor == 0) {
                //proces potomny
                execl("./dyspozytor", "dyspozytor", NULL);
                perror("Blad uruchomienia dyspozytora");
                exit(1);
        }

	wait(NULL);
	wait(NULL);

	printf("Koniec symulacji\n");
	return 0;
}
