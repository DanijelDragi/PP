#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <Windows.h>

#define NUMBER_OF_MEALS 4

void printTabs(int i) {
	for (int j = 0; j < i; j++) printf("\t");
	return;
}

int main(int argc, char**argv) {

	MPI_Init(NULL, NULL);
	int n;
	MPI_Comm_size(MPI_COMM_WORLD, &n);
	int i;
	MPI_Comm_rank(MPI_COMM_WORLD, &i);

	//Initialize variables
	srand((unsigned int) time(NULL) + i);
	int forkL = 0, forkR = 0, reqL = 0, reqR = 0, cleanL = 0, cleanR = 0, mealsHad = 0, flag = 0;
	char* send = malloc(sizeof(char)), * recv = malloc(sizeof(char));
	MPI_Status status;
	MPI_Request request;
	if (i == 0) forkR = 1;
	if (i < n-1) forkL = 1;

	//Loop for eating and thinking!
	while (mealsHad < NUMBER_OF_MEALS) {
		//Misli
		printTabs(i);
		printf("Mislim.\n");
		fflush(stdout);
		for (int j = 0; j < 10; j++) {
			//Obraduj zahtjeve dok mislis
			MPI_Irecv((void*)recv, 1, MPI_CHAR, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &request);
			Sleep(rand() % 400 + 200);
			MPI_Test(&request, &flag, &status);
			if (flag) {
				if (*recv == 'Z') {
					if (status.MPI_SOURCE == (i + n - 1) % n && forkR == 1) {
						forkR = 0;
						*send = 'O';
						MPI_Send((void*)send, 1, MPI_CHAR, (i + n - 1) % n, 0, MPI_COMM_WORLD);
					}
					if (status.MPI_SOURCE == (i + 1) % n && forkL == 1) {
						if (cleanL) reqL = 1;
						else {
							forkL = 0;
							*send = 'O';
							MPI_Send((void*)send, 1, MPI_CHAR, (i + 1) % n, 0, MPI_COMM_WORLD);
						}
					}
				}
				else if (*recv == 'O') {
					if (status.MPI_SOURCE == (i + 1) % n) {
						forkL = 1;
						cleanL = 1;
					}
					else if (status.MPI_SOURCE == (i + n - 1) % n) {
						forkR = 1;
						cleanR = 1;
					}
				}
				else printf("WTF HOW DID I GET HERE, while thinking\n");
			}
			else {
				MPI_Cancel(&request);
			}
		}
		//Skupi obje vilice
		while (forkL != 1 || forkR != 1) {
			if (forkL != 1) {
				printTabs(i);
				printf("Trazim vilicu L\n");
				fflush(stdout);
				*send = 'Z';
				MPI_Send((void*)send, 1, MPI_CHAR, (i + 1) % n, 0, MPI_COMM_WORLD);
			}
			//Cekam vilicu, odgovaram na poruke
			while (forkL != 1) {
				MPI_Recv((void*)recv, 1, MPI_CHAR, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
				//Sredi odgovor na zahtjev
				if (*recv == 'O') {
					if (status.MPI_SOURCE == (i + 1) % n) {
						forkL = 1;
						cleanL = 1;
					}
					else printf("WTF HOW DID I GET HERE, GOT O from wrong source\n");
				}
				//Zabiljezi zahtjev, odgovori ako treba
				else if (*recv == 'Z') {
					if (status.MPI_SOURCE == (i + n - 1) % n && forkR == 1) {
						if (cleanR) reqR = 1;
						else {
							forkR = 0;
							*send = 'O';
							MPI_Send((void*)send, 1, MPI_CHAR, (i + n - 1) % n, 0, MPI_COMM_WORLD);
						}
					}
					else printf("WTF HOW DID I GET HERE, GOT Z from wrong source\n");
				}
				else printf("WTF HOW DID I GET HERE, got message not O not Z\n");
			}
			if (forkR != 1) {
				printTabs(i);
				printf("Trazim vilicu R\n");
				*send = 'Z';
				MPI_Send((void*)send, 1, MPI_CHAR, (i +n - 1) % n, 0, MPI_COMM_WORLD);
			}
			//Cekam vilicu, odgovaram na poruke
			while (forkR != 1) {
				MPI_Recv((void*)recv, 1, MPI_CHAR, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
				//Sredi odgovor na zahtjev
				if (*recv == 'O') {
					if (status.MPI_SOURCE == (i + n - 1) % n) {
						forkR = 1;
						cleanR = 1;
					}
					else printf("WTF HOW DID I GET HERE, GOT O from wrong source\n");
				}
				//Zabiljezi zahtjev, odgovori ako treba
				else if (*recv == 'Z') {
					if (status.MPI_SOURCE == (i + 1) % n && forkL == 1) {
						if (cleanL) reqL = 1;
						else {
							forkL = 0;
							*send = 'O';
							MPI_Send((void*)send, 1, MPI_CHAR, (i + 1) % n, 0, MPI_COMM_WORLD);
						}
					}
					else printf("WTF HOW DID I GET HERE, GOT Z from wrong source\n");
				}
				else printf("WTF HOW DID I GET HERE, got message not O not Z\n");
			}
		}
		//Jedi
		mealsHad += 1;
		cleanL = 0;
		cleanR = 0;
		printTabs(i);
		printf("Jedem.\n");
		fflush(stdout);
		//Odgovori na zahtjeve ako ih ima
		if (reqL == 1) {
			*send = 'O';
			MPI_Send((void*)send, 1, MPI_CHAR, (i + n - 1) % n, 0, MPI_COMM_WORLD);
			reqL = 0;
		}
		if (reqR == 1) {
			*send = 'O';
			MPI_Send((void*)send, 1, MPI_CHAR, (i + 1) % n, 0, MPI_COMM_WORLD);
			reqR = 0;
		}
	}
	*send = 'O';
	MPI_Send((void*)send, 1, MPI_CHAR, (i + n - 1) % n, 0, MPI_COMM_WORLD);
	MPI_Send((void*)send, 1, MPI_CHAR, (i + 1) % n, 0, MPI_COMM_WORLD);
	// Finalize the MPI environment.
	MPI_Finalize();
}