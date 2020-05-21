#pragma warning(disable:4996)

#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#define HEIGHT 6
#define WIDTH 7
#define DEPTH 4
#define UNKNOWN -10
//Print the board state
void printBoard(char playfield[HEIGHT][WIDTH]) {
	printf("  1234567\n");
	for (int i = HEIGHT - 1; i >= 0; i--) {
		printf("%d %.*s\n", i, WIDTH, playfield[i]);
	}
	return;
}
//Check if game is won
bool winCondition(char playfield[HEIGHT][WIDTH], int* heights, int move) {
	int height = heights[move] - 1;
	char player = playfield[height][move];
	bool win = false;
	//vertical win
	if (height >= 3) {
		if (playfield[height][move] == player && playfield[height - 1][move] == player &&
			playfield[height - 2][move] == player && playfield[height - 3][move] == player) {
			win = true;
		}
	}
	//Horisontal win
	for (int offset = 0; offset < 4; offset++) {
		if (move - offset < 0 || move - offset + 3 > WIDTH - 1) continue;
		if (playfield[height][move - offset] == player && playfield[height][move - offset + 1] == player &&
			playfield[height][move - offset + 2] == player && playfield[height][move - offset + 3] == player) {
			win = true;
		}
	}
	//diagonal win
	for (int offset = 0; offset < 4; offset++) {
		if (move - offset < 0 || move - offset + 3 > WIDTH - 1) continue;
		if (playfield[height - offset][move - offset] == player && playfield[height - offset + 1][move - offset + 1] == player &&
			playfield[height - offset + 2][move - offset + 2] == player && playfield[height - offset + 3][move - offset + 3] == player) win = true;
		if (playfield[height + offset][move - offset] == player && playfield[height + offset - 1][move - offset + 1] == player &&
			playfield[height + offset - 2][move - offset + 2] == player && playfield[height + offset - 3][move - offset + 3] == player) win = true;
	}
	return win;
}
//Create board and heights from moves list
int simulateMoves(char playfield[HEIGHT][WIDTH], int heights[WIDTH], int* moves, int* nMoves) {
	int i = 0;
	char player = 'X';
	while (moves[i] != UNKNOWN) {
		playfield[heights[moves[i]]][moves[i]] = player;
		heights[moves[i]] += 1;
		i++;
		if (player == 'O') player = 'X';
		else player = 'O';
	}
	*nMoves = i;
	return moves[i - 1];
}
//Get grade by calculating child grades
double getGradeFromKids(int myId, int depth, char playfield[HEIGHT][WIDTH], int* heights, int move, char player, int* requestsDue, int* moves, int n) {
	//If I reached DEPTH just return simple grade
	if (depth == DEPTH) {
		if (winCondition(playfield, heights, move)) {
			if (player == 'O') return 1;
			else return -1;
		}
		else return 0;
	}
	//If game is won with this move just return winner
	else if(winCondition(playfield, heights, move)) {
		if (player == 'O') return 1;
		else return -1;
	}
	//Else init variables
	MPI_Status otherStatus;
	char nextPlayer;
	double childGrades[WIDTH];
	int outsourced[WIDTH];
	double grade = 0;
	int legalMoves = 7, flag = 0, messageType = 0, requestAnswer = 0;
	//Assign next player
	if (player == 'O') nextPlayer = 'X';
	else nextPlayer = 'O';
	//Check all WIDTH moves
	for (int i = 0; i < WIDTH;) {
		outsourced[i] = -1;
		//Don't check illegal moves
		if (heights[i] >= HEIGHT) {
			childGrades[i] = UNKNOWN;
			i++;
			legalMoves--;
			continue;
		}
		//Check this move
		else {
			//IRECV task requests, do your own work
			MPI_Iprobe(MPI_ANY_SOURCE, 300, MPI_COMM_WORLD, &flag, &otherStatus);
			if (flag) {
				//SOMEONE WANTS WORK
				MPI_Recv((void*)&messageType, 1, MPI_INT, otherStatus.MPI_SOURCE, 300, MPI_COMM_WORLD, &otherStatus);
				if (i - 1 < WIDTH && *requestsDue > 0) {
					//There is work, give it to them
					requestAnswer = 1;
					MPI_Send((void*)&requestAnswer, 1, MPI_INT, otherStatus.MPI_SOURCE, 400, MPI_COMM_WORLD);
					*requestsDue -= 1;
					//Send moves
					moves[n] = i;
					MPI_Send((void*)moves, HEIGHT * WIDTH, MPI_INT, otherStatus.MPI_SOURCE, depth + 1, MPI_COMM_WORLD);
					moves[n] = UNKNOWN;
					//This move will have to be received
					outsourced[i] = otherStatus.MPI_SOURCE;
					i++;
				}
				else {
					//No more work to give out
					requestAnswer = 0;
					MPI_Send((void*)&requestAnswer, 1, MPI_INT, otherStatus.MPI_SOURCE, 400, MPI_COMM_WORLD);
					*requestsDue -= 1;
				}
			}
			else {
				//DO MY WORK
				playfield[heights[i]][i] = nextPlayer;
				heights[i] += 1;
				moves[n] = i;
				childGrades[i] = getGradeFromKids(myId, depth + 1, playfield, heights, i, nextPlayer, requestsDue, moves, n + 1);
				moves[n] = UNKNOWN;
				heights[i] -= 1;
				playfield[heights[i]][i] = '.';
				i++;
			}
		}
	}
	for (int i = 0; i < WIDTH; i++) {
		if(childGrades[i] != UNKNOWN && outsourced[i] == -1) grade += childGrades[i];
		//Get grades from outsourced tasks
		else if (outsourced[i] != -1) {
			MPI_Recv((void*)&childGrades[i], 1, MPI_DOUBLE, outsourced[i], 500, MPI_COMM_WORLD, &otherStatus);
			if(childGrades[i] != UNKNOWN) grade += childGrades[i];
			else printf("\n\nTHIS IS A HUGE PROBLEM!!!! %f\n\n", childGrades[i]);
		}
	}
	fflush(stdout);
	return grade / legalMoves;
}

void bossMan(int worldSize, int myId) {
	//Initialize variables
	bool gameOn = true;
	int move = 0;
	int heights[WIDTH];
	double moveGrades[WIDTH];
	char playfield[HEIGHT][WIDTH];
	int* moves = malloc(sizeof(int) * WIDTH * HEIGHT);
	int nMoves = 0, requestType = 0, answerType = 0, best = 0;
	char jobFlag = 'S';
	double grade = 0;
	MPI_Status status;
	//Initialize playfield, grades & heights
	for (int j = 0; j < WIDTH; j++) {
		heights[j] = 0;
		moveGrades[j] = UNKNOWN;
		for (int i = 0; i < HEIGHT; i++) {
			playfield[i][j] = '.';
		}
	}
	for (int i = 0; i < WIDTH * HEIGHT; i++) {
		moves[i] = UNKNOWN;
	}

	//Play the game
	while (gameOn) {
		printBoard(playfield);
		printf("Enter move: ");
		fflush(stdout);
		scanf("%d", &move);
		move -= 1;
		//Detect illegal moves
		if (move < 0 || move > WIDTH - 1 || heights[move] >= HEIGHT) {
			printf("Illegal move\n");
		}
		else {
			//apply player move
			playfield[heights[move]][move] = 'X';
			heights[move] += 1;
			moves[nMoves] = move;
			nMoves++;
			//Check if game is won
			if (winCondition(playfield, heights, move)) {
				printBoard(playfield);
				printf("You win!\n");
				fflush(stdout);
				gameOn = false;
			}
			//Check if game is a draw
			else {
				bool draw = true;
				for (int i = 0; i < WIDTH; i++) {
					if (heights[i] != HEIGHT) draw = false;
				}
				if (draw) {
					printBoard(playfield);
					printf("Game is a draw!\n");
					fflush(stdout);
					gameOn = false;
				}
			}
			//DO AI MOVE HERE
			if (gameOn) jobFlag = 'T';
			else jobFlag = 'E';
			//Let everyone know a new turn started, or that the game is over
			//printf("At Bcast! I am %d\n", myId);
			fflush(stdout);
			MPI_Bcast((void*)&jobFlag, 1, MPI_CHAR, 0, MPI_COMM_WORLD);
			if (gameOn) {
				//GIVE OUT TASKS HERE
				for (int i = 0; i < WIDTH;) {
					//If column i is full, skip this task!
					if (heights[i] >= HEIGHT) {
						i++;
						continue;
					}
					//Receive requests
					MPI_Recv((void*)&requestType, 1, MPI_INT, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
					if (requestType == 1) {
						//Tell worker there is work
						answerType = 1;
						MPI_Send((void*)&answerType, 1, MPI_INT, status.MPI_SOURCE, status.MPI_TAG, MPI_COMM_WORLD);
						//Send worker a task
						moves[nMoves] = i;
						MPI_Send((void*)moves, WIDTH * HEIGHT, MPI_INT, status.MPI_SOURCE, 1, MPI_COMM_WORLD);
						moves[nMoves] = UNKNOWN;
						i++;
					}
					else if(requestType == 2){
						//Receive results
						MPI_Recv((void*)&grade, 1, MPI_DOUBLE, status.MPI_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
						moveGrades[status.MPI_TAG] = grade;
						//printf("Got grade %f from %d for move %d\n", grade, status.MPI_SOURCE, status.MPI_TAG);
					}
				}
				//TELL EVERYONE THERE IS NO WORK
				for (int i = 0; i < worldSize - 1;) {
					//Receive request
					MPI_Recv((void*)&requestType, 1, MPI_INT, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
					//
					if (requestType == 1) {
						answerType = 0;
						//tell there is no job
						MPI_Send((void*)&answerType, 1, MPI_INT, status.MPI_SOURCE, status.MPI_TAG, MPI_COMM_WORLD);
						i++;
					}
					else if (requestType == 2) {
						//Receive results
						MPI_Recv((void*)&grade, 1, MPI_DOUBLE, status.MPI_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
						moveGrades[status.MPI_TAG] = grade;
						//printf("Got grade %f from %d for move %d\n", grade, status.MPI_SOURCE, status.MPI_TAG);
					}
				}
				//Perform best move
				best = -1;
				for (int i = 0; i < WIDTH; i++) {
					if (moveGrades[i] > moveGrades[best] && heights[i] < HEIGHT) {
						best = i;
						//printf("New best move = %d\n", best);
					}
					fflush(stdout);
				}
				playfield[heights[best]][best] = 'O';
				heights[best] += 1;
				moves[nMoves] = best;
				nMoves++;
				//Reset move grades
				for (int i = 0; i < WIDTH; i++) {
					moveGrades[i] = 0;
				}
				//Check if game is won
				if (winCondition(playfield, heights, best)) {
					printBoard(playfield);
					printf("You lose!\n");
					fflush(stdout);
					gameOn = false;
				}
				//Check if game is a draw
				else {
					bool draw = true;
					for (int i = 0; i < WIDTH; i++) {
						if (heights[i] != HEIGHT) draw = false;
					}
					if (draw) {
						printBoard(playfield);
						printf("Game is a draw!\n");
						fflush(stdout);
						gameOn = false;
					}
				}
				//OVDJE JAVI SVIMA DA NEMA POSLA AKO SI IZGUBIO
				if (gameOn == false) {
					jobFlag = 'E';
					//Let everyone know that the game is over
					MPI_Bcast((void*)&jobFlag, 1, MPI_CHAR, 0, MPI_COMM_WORLD);
				}
			}
		}
	}
	free(moves);
	return;
}

void workerMan(int worldSize, int myId) {
	//Initialize variables
	int* moves = malloc(sizeof(int) * WIDTH * HEIGHT);
	char jobFlag = 'S';
	int jobAvailable = 1, jobRequest = 1, depth = 0, move = 0, requestsDue = 0, requestBuf = 0, requestAnswer = 0, nMoves = 0;
	double turnGrade = 0;
	char playfield[HEIGHT][WIDTH];
	int heights[WIDTH];
	MPI_Status status, requestStatus;
	MPI_Request* requests = malloc(sizeof(MPI_Request) * worldSize);
	//Work untile the game is ending -> 'E'
	while (jobFlag != 'E') {
		//printf("At Bcast! I am %d\n", myId);
		fflush(stdout);
		//Receive game status
		MPI_Bcast((void*) &jobFlag, 1, MPI_CHAR, 0, MPI_COMM_WORLD);
		if (jobFlag == 'E') break;
		//Get task
		requestsDue = worldSize - 2;
		while (jobAvailable != 0) {
			//Ask for work
			jobRequest = 1;
			MPI_Send((void*)&jobRequest, 1, MPI_INT, 0, 0, MPI_COMM_WORLD);
			//Receive work
			MPI_Recv((void*)&jobAvailable, 1, MPI_INT, 0, 0, MPI_COMM_WORLD, &status);
			if (jobAvailable != 0) {
				//Work to be received from bossMan
				MPI_Recv((void*)moves, HEIGHT * WIDTH, MPI_INT, 0, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
				fflush(stdout);
				//Initialize playfield & heights
				for (int j = 0; j < WIDTH; j++) {
					heights[j] = 0;
					for (int i = 0; i < HEIGHT; i++) {
						playfield[i][j] = '.';
					}
				}
				//Extract board state
				move = simulateMoves(playfield, heights, moves, &nMoves);
				depth = status.MPI_TAG;
				turnGrade = 0;
				//Get grade from child grades! Work and share work!
				turnGrade = getGradeFromKids(myId, depth, playfield, heights, move, 'O', &requestsDue, moves, nMoves);
				//Return grade to boss
				jobRequest = 2;
				MPI_Send((void*)&jobRequest, 1, MPI_INT, 0, 0, MPI_COMM_WORLD);
				MPI_Send((void*)&turnGrade, 1, MPI_DOUBLE, 0, move, MPI_COMM_WORLD);
			}
			else {
				//printf("No more boss work to do! I am %d\n", myId);
				fflush(stdout);
				//Ask other workers for work
				jobRequest = 1;
				for (int i = 1; i < worldSize; i++) {
					if (myId != i) {
						MPI_Isend((void*)&jobRequest, 1, MPI_INT, i, 300, MPI_COMM_WORLD, &requests[i]);
					}
				}
				//printf("Asked others for work! I am %d\n", myId);
				fflush(stdout);
				//Receive due requests AND job answers
				int i = 1, jobAnswer = 0;
				if (i == myId) i++;
				//printf("Entering while! I am %d\n", myId);
				fflush(stdout);
				while (requestsDue > 0 || i < worldSize) {
					//Test to see what kind of message there is to receive
					MPI_Probe(MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &requestStatus);
					//If it is a work request say NO WORK to give
					if (requestStatus.MPI_TAG == 300) {
						//printf("Requests due %d, I am %d\n",requestsDue, myId);
						fflush(stdout);
						MPI_Recv((void*)&requestBuf, 1, MPI_INT, requestStatus.MPI_SOURCE, 300, MPI_COMM_WORLD, &requestStatus);
						requestsDue--;
						requestAnswer = 0;
						MPI_Send((void*)&requestAnswer, 1, MPI_INT, requestStatus.MPI_SOURCE, 400, MPI_COMM_WORLD);
					}
					//If it is a answer containing possible work, handle it
					else if (requestStatus.MPI_TAG == 400) {
						//printf("i is %d of %d, I am %d\n", i, worldSize, myId);
						fflush(stdout);
						MPI_Recv((void*)&jobAnswer, 1, MPI_INT, requestStatus.MPI_SOURCE, 400, MPI_COMM_WORLD, &requestStatus);
						if (jobAnswer == 1) {
							//Receive work and do it
							MPI_Recv((void*)moves, HEIGHT * WIDTH, MPI_INT, requestStatus.MPI_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &requestStatus);
							//Initialize playfield & heights
							for (int j = 0; j < WIDTH; j++) {
								heights[j] = 0;
								for (int i = 0; i < HEIGHT; i++) {
									playfield[i][j] = '.';
								}
							}
							move = simulateMoves(playfield, heights, moves, &nMoves);
							depth = requestStatus.MPI_TAG;
							turnGrade = 0;
							char player;
							int specialRequests = 0;
							if (nMoves % 2 == 0) player = 'O';
							else player = 'X';
							//WTF IS THIS
							//printf("Task takeover stats!!!! player is %c, depth is %d\n", player, depth);
							//printBoard(playfield);
							for (int i = 0; i < WIDTH; i++) //printf("%d ", heights[i]);
							//printf("\n");
							for (int i = 0; i < WIDTH * HEIGHT; i++) //printf("%d ", moves[i]);
							//printf("\n");
							fflush(stdout);

							turnGrade = getGradeFromKids(myId, depth, playfield, heights, move, player, &specialRequests, moves, nMoves);
							MPI_Send((void*)&turnGrade, 1, MPI_DOUBLE, requestStatus.MPI_SOURCE, 500, MPI_COMM_WORLD);
							//printf("Did work for %d, I am %d, gave grade %f\n", requestStatus.MPI_SOURCE, myId, turnGrade);
							fflush(stdout);
						}
						else {
							//printf("No work from %d! I am %d\n", i, myId);
							fflush(stdout);
						}
						i++;
						if (i == myId) i++;
					}
					else printf("WTF how did this happen? %d\n", requestStatus.MPI_TAG);	
				}
				//printf("Request phase over! I am %d\n", myId);
				fflush(stdout);
			}
		}
		jobAvailable = 1;
	}
	//printf("Game is over, no tasks to get! I am %d\n", myId);
	fflush(stdout);
	free(moves);
	free(requests);
	return;
}

int main(int argc, char**argv) {

	MPI_Init(NULL, NULL);
	int worldSize, myId;;
	MPI_Comm_size(MPI_COMM_WORLD, &worldSize);
	MPI_Comm_rank(MPI_COMM_WORLD, &myId);
	if (worldSize > 1) {
		if (myId == 0) {
			bossMan(worldSize, myId);
		}
		else {
			workerMan(worldSize, myId);
		}
	}
	else {
		//DO WORK ALONE HERE
		printf("WORK ALONE NOT IMPLEMENTED YET!\n");
	}
	MPI_Finalize();
}