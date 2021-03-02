#include <unistd.h>
#include <stdio.h>
#include <pthread.h>	//for pthread
#include <assert.h>
#include <stdlib.h>		// for exit
#include <semaphore.h>	//for semaphore
#include <string.h>
#include <time.h>		//for gettimeofday()
#include <errno.h>		//for trywait- errno
#define N 256

//For obstacle
typedef struct Data {
	char* obsName; 			//name for print
	int obsIndex;			//save the specific obstacle`s index of the same type.
	long int startObsTime;	//(startObs)
	long int endObsTime;	//(endObs)
}Data;

typedef struct trainee {
	char* name;
	int percent; 		//affects time
	sem_t sem_trainee; 	//"nivzarut" semaphore.
	long int enter;		//(enter) start time
	long int exit;		//(exit) time of the end of the last obstacle.
	Data* obsDataArr;	//array to mark obstacle's completion and statistics. 
}trainee;

typedef struct waitingTrainee {
	trainee* waitingMember;
	struct waitingTrainee* prev;
	struct waitingTrainee* next;
}waitingTrainee;

waitingTrainee *W_Head = NULL, *W_Tail = NULL;	//global waiting list.

typedef struct obstacle {
	char* name;		//obstacle`s name.
	int num;		//number of obstacle from this type.
	int avgTime;	//Average time to cross this obstacle.
	sem_t* semArr;	//obstacles`s semaphores array
}obstacle;

obstacle* obstacleArr; 		//global array of obstacle

sem_t waitingListMutex;		//semaphore of waiting trainees for atomic action

static int numObs;		//save the num of obstacles
int active_members = 0;

//declaration:
int getNumOfLines(FILE* file);
void GetObstaclesFromFile(FILE* f_course, char** obsArrNames);
trainee** GetTeamFromFile(FILE* f_team, int numTeam, char** obsArrNames);
void freeMember(trainee* temp);
void printData(int numTeam, trainee** traineesArr);
void* runCourse(void* t);
void runObsTimer(trainee* member, int currObs_j, int obsType_i);
void Add_To_WaitingList(trainee* waitMember);
void Wakes_Up_WaitingList();

int main(int argc, char* argv[]) {

	int i, numTeam;
	FILE* f_course, *f_team;
	char** obsArrNames;		//array for all the obstacles`s names, for print.
	pthread_t* threads;
	int* ans; 				//array of ansers from create thread.
	trainee** traineesArr;	//array that save all the trainees in the team.

	sem_init(&waitingListMutex, 0, 1);
	if (argc != 3) {
		printf("\nThe number of parameters worng.\n");
		exit(EXIT_FAILURE);
	}

	//open files:
	f_course = fopen(argv[1], "r");
	if (f_course == NULL) {
		perror("ERROR - can`t open course-desc.txt file\n");
	}
	f_team = fopen(argv[2], "r");
	if (f_team == NULL) {
		fclose(f_course);
		perror("ERROR - can`t open team-desc file\n");
	}

	numObs = getNumOfLines(f_course);
	numTeam = getNumOfLines(f_team);
	active_members = numTeam;
	assert(obsArrNames = (char**)malloc(sizeof(char*)*numObs));
	GetObstaclesFromFile(f_course, obsArrNames);
	traineesArr = GetTeamFromFile(f_team, numTeam, obsArrNames);

	assert(ans = (int*)malloc((numTeam) * sizeof(int)));

	//create array of threads.
	assert(threads = (pthread_t*)malloc(numTeam * sizeof(pthread_t)));
	for (i = 0; i<numTeam; i++) {
		ans[i] = pthread_create(&threads[i], NULL, runCourse, (void*)traineesArr[i]);
		if (ans[i] != 0) {
			perror("create thread failed.");
			exit(EXIT_FAILURE);
		}
	}

	for (i = 0; i<numTeam; i++)
		pthread_join(threads[i], NULL);

	//print all data after all the trainee finished the course.
	printData(numTeam, traineesArr);

	//free all memmory:
	for (i = 0; i<numObs; i++) {
		free(obstacleArr[i].name);
		free(obstacleArr[i].semArr);
		free(obsArrNames[i]);
	}
	free(obstacleArr);
	free(obsArrNames);
	for (i = 0; i< numTeam; i++) {
		freeMember(traineesArr[i]);
	}
	free(traineesArr);
	free(threads);
	free(ans);

	fclose(f_course);
	fclose(f_team);
	return 0;
}

int getNumOfLines(FILE* file) {
	int num;
	fscanf(file, "%d", &num);
	if (num <= 0 || num == EOF) {
		perror("ERROR - Incorrect reading value\n");
		return 0;
	}
	return num;
}

void GetObstaclesFromFile(FILE* f_course, char** obsArrNames) {
	char tempName[N];
	int tmpNum, tmpTime;
	int i = 0, j;

	//create global array of all obstacles.
	assert(obstacleArr = (obstacle*)malloc(sizeof(obstacle)*numObs));

	while (fscanf(f_course, "%s%d%d", tempName, &tmpNum, &tmpTime) != EOF) {

		assert(obstacleArr[i].name = (char*)malloc(sizeof(char) * (strlen(tempName) + 1)));
		strcpy(obstacleArr[i].name, tempName);
		/*-save obstacles`s names-*/
		assert(obsArrNames[i] = (char*)malloc((strlen(tempName) + 1) * sizeof(char)));
		strcpy(obsArrNames[i], tempName);
		obstacleArr[i].num = tmpNum;
		obstacleArr[i].avgTime = tmpTime;
		assert(obstacleArr[i].semArr = (sem_t*)malloc(sizeof(sem_t)*obstacleArr[i].num));
		//Initializing obstacles`s semaphores (mutex type)
		for (j = 0; j<obstacleArr[i].num; j++) {
			sem_init(&(obstacleArr[i].semArr[j]), 0, 1);
		}
		i++;
	}
}

trainee** GetTeamFromFile(FILE* f_team, int numTeam, char** obsArrNames) {
	char tempName[N], tmpObsName[N];
	trainee** traineeArr;
	trainee* member;
	Data* tempData;
	int i, traineeInd = 0, tmpPrc, len, k;

	assert(traineeArr = (trainee**)malloc(sizeof(trainee*) * numTeam));

	//read all file until EOF and get trainee`s data		
	while (traineeInd<numTeam) {

		fscanf(f_team, "%s %d", tempName, &tmpPrc);
		assert(member = (trainee*)malloc(sizeof(trainee)));
		assert(member->name = (char*)malloc(sizeof(char) * (strlen(tempName) + 1)));
		strcpy(member->name, tempName);

		assert(tempData = (Data*)malloc(sizeof(Data)*numObs));
		/*-set names of obs according to order in ObsFile.-*/
		k = 0;
		for (i = 0; i<numObs; i++) {
			len = strlen(obsArrNames[k]);
			assert(tempData[i].obsName = (char*)malloc(sizeof(char) * (len + 1)));
			strcpy(tempData[i].obsName, obsArrNames[k]);
			k++;
			//the key to know if the trinee try it yet or NOT..
			tempData[i].obsIndex = -1;
		}
		member->obsDataArr = tempData;
		traineeArr[traineeInd] = member;
		traineeArr[traineeInd]->percent = tmpPrc;
		sem_init(&traineeArr[traineeInd]->sem_trainee, 0, 0);

		traineeInd++;
	}
	return traineeArr;
}

//free memory of a trainee
void freeMember(trainee* temp) {
	Data* tempD;
	int i;
	tempD = temp->obsDataArr;
	//free each cell in obsDataArr.
	for (i = 0; i<numObs; i++) {
		free(tempD[i].obsName);
	}
	free(temp->obsDataArr);
	free(temp->name);
	free(temp);
}

/*(duration)= startObd-endObs.
(elapsed)= enter-exit.
(active)= sum of duration time in course.
(wait)= elapsed-active.*/
void printData(int numTeam, trainee** traineesArr) {
	Data* tempD;
	int i = 0, j = 0;
	long int duration, elapsed, active, wait;

	for (i = 0; i<numTeam; i++) {
		printf("\n%s results:\n", traineesArr[i]->name);
		printf("\tenter: %ld\n", traineesArr[i]->enter);
		//print data on each one of the obstacles of a trainee.
		active = 0;
		tempD = traineesArr[i]->obsDataArr;
		for (j = 0; j<numObs; j++) {
			duration = tempD[j].endObsTime - tempD[j].startObsTime;
			printf("\t%d.%8s  obs: %d start: %ld finish: %ld duration: %ld\n", j, tempD[j].obsName, tempD[j].obsIndex, tempD[j].startObsTime, tempD[j].endObsTime, duration);
			active = active + duration;
		}
		elapsed = traineesArr[i]->exit - traineesArr[i]->enter;
		wait = elapsed - active;
		printf("\texit: %ld\telapsed: %ld\tactive: %ld\twait: %ld\n", traineesArr[i]->exit, elapsed, active, wait);
	}
}

void runObsTimer(trainee* member, int currObs_j, int obsType_i) {
	struct timeval startObs, endObs;
	double workoutTime = 0.0;
	double r = (1 + (rand() % 10) / 100.0);
	double perc = member->percent / 100.0;
	Data* t;
	// save the number of the obstacle from this type.
	t = member->obsDataArr;
	t[obsType_i].obsIndex = currObs_j;

	workoutTime += obstacleArr[obsType_i].avgTime;
	workoutTime += workoutTime*r;
	if (member->percent < 0) {
		workoutTime += workoutTime * perc;
	}
	else if (member->percent > 0) {
		workoutTime -= workoutTime * perc;
	}
	gettimeofday(&startObs, NULL); // get the start time of this "obs".
	t[obsType_i].startObsTime = startObs.tv_sec * 1000 + startObs.tv_usec / 1000;
	usleep(workoutTime * 1000);
	gettimeofday(&endObs, NULL); // get the finish time of this "obs".
	t[obsType_i].endObsTime = endObs.tv_sec * 1000 + endObs.tv_usec / 1000;
}

/*Each trainee tries to catch an obstacle that he has not tried yet,
if the obstacle is available he start.
Otherwise, waiting in the waiting_list until someone wakes him up to ` again.
*/
void* runCourse(void* t) {
	struct timeval start, end;
	trainee* threadTrainee = (trainee*)t; //casting
	Data* traineeObstacles = threadTrainee->obsDataArr;
	obstacle* tempObsType;
	int i = 0, j, count = 0;

	gettimeofday(&start, NULL);// get the enter time.
	threadTrainee->enter = start.tv_sec * 1000 + start.tv_usec / 1000;

	while (count < numObs) {
		for (i = 0; i< numObs; i++) {
			if (traineeObstacles[i].obsIndex == -1) {
				tempObsType = &obstacleArr[i];
				for (j = 0; j<tempObsType->num; j++) {

					if (sem_trywait(&tempObsType->semArr[j]) == -1 && errno == EAGAIN) {
						continue;
					}
					// if the trainee succed, sent to the obstacle..
					else {
						runObsTimer(threadTrainee, j, i);
						count++;

						sem_post(&tempObsType->semArr[j]);		//Releases to other trainee.																															   //now this trainee wakes up everyone in the waiting list
						sem_wait(&waitingListMutex);			//for atomic action.
						Wakes_Up_WaitingList();
						sem_post(&waitingListMutex);			//for atomic action.
						break; // move to try next type of obstacle
					}
				}
			}
		}
		if (count != numObs && active_members > 1) {
			sem_wait(&waitingListMutex);
			Add_To_WaitingList(threadTrainee);
			sem_post(&waitingListMutex);
			sem_wait(&threadTrainee->sem_trainee);
		}
	}
	gettimeofday(&end, NULL); // get the start time.
	threadTrainee->exit = end.tv_sec * 1000 + end.tv_usec / 1000;
}

/*If a trainee has failed to catch an obstacle he didn`t try yet,
then he joins to waiting list (to the END).*/
void Add_To_WaitingList(trainee* waitMember) {
	waitingTrainee* member;
	waitingTrainee* temp2;
	trainee* tmpMember;

	assert(member = (waitingTrainee*)malloc(sizeof(waitingTrainee)));

	member->waitingMember = waitMember;
	if (W_Head == NULL) {//if waiting list is empty, add as first node.
		W_Head = member;
		member->next = NULL;
		member->prev = NULL;
		W_Tail = member;
	}
	//add to the end of the list.
	else {
		temp2 = W_Tail;
		temp2->next = member;
		member->next = NULL;
		member->prev = temp2;
		W_Tail = member;
	}
	active_members--;
}

/*If the obstacle is available and the trainee succeeded to catch it,
then he wakes up the all waiting list*/
void Wakes_Up_WaitingList() {
	waitingTrainee* temp;
	waitingTrainee* temp2;
	trainee* tmpT;

	//remove each node from the head of the list
	while (W_Head != NULL) {
		temp = W_Head;
		//in case there is only one in the list
		if (temp->next == NULL) {
			tmpT = temp->waitingMember;
			sem_post(&tmpT->sem_trainee);
			W_Head = NULL;
			free(temp);
		}
		else {
			temp2 = temp->next;
			temp2->prev = NULL;
			W_Head = temp2;
			temp->next = NULL;
			tmpT = temp->waitingMember;
			sem_post(&tmpT->sem_trainee);
			free(temp);
		}
		active_members++;
	}
	W_Tail = NULL;
}




