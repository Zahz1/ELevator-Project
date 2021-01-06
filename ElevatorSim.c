//needed libraries
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <pthread.h>
#include <unistd.h>

#define CAPACITY 10
#define PICKUPT 2
#define ELEVATORMOVET 5
#define GENCHANCE 5 //% chance a job will generate, rand() mod 100, if(result < GenChance)
//elevator struct
struct Elevator {
    int currFloor; //current floor elevator is on
    int** activeQueue; //array of active jobs
    int numJobs; //number of jobs
    int nPeople; //number of people in the elevator
    int status; //0 = down, 1 = up, 2 = stop
    int waitTime; //total wait time
};
//setting up elevator
void setUpElevator(struct Elevator *e){
    e->currFloor = 1; //currently in elevator lobby, floor 1
    e->activeQueue = NULL; //active queue set to null
    e->numJobs = 0; //number of jobs set to 0
    e->nPeople = 0; //number of people in elevator set to 0
    e->status = 2; //elevator status set to stopped
    e->waitTime = 0; //wait time set to 0
}


//globals
//done queue is the jobs finished, available queue are the jobs that havent been npicked up
int **doneQueue, **AvailableQueue;
//doneNum is the number of jobs finished
int *doneNum, *availableNum;
int simTime = 0; //simulation time set to 0
struct Elevator e;
int genInterval = 5;
int TOPFLOOR = 8;
//naming file
char logFile[13] = {'e','l','e','v','a','t','o','r','.','l','o','g', '\0'};
//floor thread initializers, lock, and barrier
pthread_barrier_t   barrier;
pthread_mutex_t lockMain = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t signal = PTHREAD_COND_INITIALIZER;

//pthread_mutex_t lockAQ = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t lockAQ;

//dynamicMemory dynamically adds a job to a job array
int** dynamicallyAdd(int** A, int* aNum, int* job){
    int i;
    int **newA;
    if((*aNum) == 0){
        newA = malloc(sizeof(int*));
        newA[0] = job;
    } else {
        newA = malloc(sizeof(int*)*((*aNum)+1));
        for(i = 0; i < (*aNum); i++){
            newA[i] = A[i];
        }
        newA[(*aNum)] = job;
    }
    (*aNum)++;
    return newA;
    //make sure to update aN after use of function
}
// dynamically adds a integer to a job
int* dynamicallyAddInt(int* A, int* aNum, int newInt){
    int i;
    int *newA;
    if((*aNum) == 0){
        newA = malloc(sizeof(int));
        newA[0] = newInt;
    }else{
        newA = malloc(sizeof(int)*((*aNum)+1));
        for(i = 0; i < (*aNum); i++){
            newA[i] = A[i];
        }
        newA[(*aNum)] = newInt;
    }
    (*aNum)++;
    return newA;
    //make sure to update aN after use of function
}



//change number of people of elevator before calling this function
//removes index from job array
int** dynamicallyRemoveIndex(int** A, int* aNum, int jobIndex){
    int i, newANum;
    int **newA = NULL;
    if(jobIndex < (*aNum) && jobIndex >= 0){
        if((*aNum) == 0 || (*aNum) == 1){
            (*aNum)--;
            return NULL;
        } else {
            newANum = (*aNum) - 1;
            newA = malloc(sizeof(int*)*(newANum));
            for(i = 0; i < jobIndex+1; i++){
                newA[i] = A[i];
            }
            for(i = jobIndex+1; i < (*aNum); i++){
                newA[i-1] = A[i];
            }
        }
        (*aNum) = newANum;
        return newA;
    }else{
        printf("\ndynamicallyRemoveIndex::ERROR\n");
    }
    return A;
    //make sure to update aN after use of function
}

//prints lists of jobs aka array
void printList(int** A, int n){
    int i;
    for(i = 0; i < n; i++){
        printf("%d %d %d %d\n",A[i][0],A[i][1],A[i][2],A[i][3]);
    }
}

//drop off function, removes those jobs from active queue
void dropOff(struct Elevator *e, int currFloor, int inTime){
    //search Active to find all that need to be droped off
    //remove from Active and put in DoneA with End Time
    FILE *out;
    out = fopen(logFile, "a");
    int i;
    int numOfIndex = 0;
    int* removeIndexList = NULL;
    if (e->numJobs > 0){
        for(i = 0; i < e->numJobs; i++){
            if(e->activeQueue[i][3] == currFloor){ //assuming position 3 is end floor
                //this means this floor is their destination
                removeIndexList = dynamicallyAddInt(removeIndexList,&numOfIndex,i);
            }
        }
        //loop through indexes
        for(i = 0; i < numOfIndex; i++){ //this loop adds to doneA list and stores time of departure in position 4
            int itemsInJob = 5;
            int *editedJob = dynamicallyAddInt(e->activeQueue[removeIndexList[i]], &itemsInJob,inTime);
            doneQueue = dynamicallyAdd(doneQueue,doneNum,editedJob);
        }

        for(i = numOfIndex-1; i >= 0; i--){ //this loop removes indexes from the elevator list, removing the larger ones first
            printf("Time %d: The elevator dropped off %d passengers at floor %d.\n",simTime,e->activeQueue[removeIndexList[i]][1], e->currFloor);
            fprintf(out,"Time %d: The elevator dropped off %d passengers at floor %d.\n",simTime,e->activeQueue[removeIndexList[i]][1], e->currFloor);
            e->nPeople = e->nPeople - e->activeQueue[removeIndexList[i]][1];
            e->activeQueue  = dynamicallyRemoveIndex(e->activeQueue, &e->numJobs, removeIndexList[i]);
        }

    }
    fclose(out);
}

//int waitingCheckM[TOPFLOOR][2];
int** waitingCheckM;             //colm 1 will be job that are waiting and going down
                                //colm 2 will be job thar are waiting and going up
//pickup function, adds job to active queue and processes them
bool pickUp(struct Elevator *e, int currFloor){
    //search Available to find all that and going in same direction
    //remove from Available and put in Active if there is enough space
    //add 2 to elevator wait time
    if(e->nPeople < CAPACITY && *availableNum > 0){
        int i;
        FILE *out;
        out = fopen(logFile, "a");
        int numOfIndex = 0;
        int* possibleIndexList = NULL;
        for(i = 0; i < *availableNum; i++){
            if((AvailableQueue[i][2] == currFloor) && (AvailableQueue[i][1] <= (CAPACITY-e->nPeople))){ //assuming position 1 is starting floor
                //this means this is their start floor and their group can fit in the elevator
                if((AvailableQueue[i][2] > AvailableQueue[i][3]) && (e->status == 0 || e->status == 2)){ //going down
                    possibleIndexList = dynamicallyAddInt(possibleIndexList,&numOfIndex,i);
                    break;
                }
                //if the available queue job is going in the direction of the elevator or if the elevator is stopped
                if((AvailableQueue[i][2] < AvailableQueue[i][3]) && (e->status == 1 || e->status == 2)){ //going up
                    //pick up the job
                    possibleIndexList = dynamicallyAddInt(possibleIndexList,&numOfIndex,i);
                    break;
                }
            }
        }
        //we now need to loop through the list of indexes and decide which ones can fit in the Elevator
        int tempPeopleCount = e->nPeople;
        int removeNum = 0;
        int* removeIndexList = NULL;

        for(i = 0; i < numOfIndex; i++){ //getting final list of people to remove
            if(tempPeopleCount == CAPACITY){
                break;
            }

            if(tempPeopleCount < CAPACITY){
                removeIndexList = dynamicallyAddInt(removeIndexList,&removeNum, possibleIndexList[i]);
                tempPeopleCount = tempPeopleCount + AvailableQueue[possibleIndexList[i]][1];
            } else {
                continue;
            }
        }
        //put final people on elevator
        for(int i = removeNum-1; i >= 0; i--){
            printf("Time %d: The elevator picked up %d passengers at floor %d.\n",simTime, AvailableQueue[removeIndexList[i]][1], e->currFloor);
            fprintf(out,"Time %d: The elevator picked up %d passengers at floor %d.\n",simTime, AvailableQueue[removeIndexList[i]][1], e->currFloor);
            //update waitingCheckM
            if (AvailableQueue[removeIndexList[i]][2] > AvailableQueue[removeIndexList[i]][3]){
                waitingCheckM[e->currFloor-1][0] = 0;
            } else {
                waitingCheckM[e->currFloor-1][1] = 0;
            }
            int temp4 = 4;
            int* jobToAdd = dynamicallyAddInt(AvailableQueue[removeIndexList[i]], &temp4, simTime);
            e->activeQueue = dynamicallyAdd(e->activeQueue, &e->numJobs, jobToAdd);
            e->nPeople = e->nPeople + AvailableQueue[removeIndexList[i]][1];
            AvailableQueue = dynamicallyRemoveIndex(AvailableQueue, availableNum, removeIndexList[i]); //mutex on this one
        }
        fclose(out);
        if (removeNum > 0){
            return true;
        }
    }
    return false;
}
//move up one floor
void moveUp(struct Elevator *e){
    e->status = 1;
    e->currFloor++;
    e->waitTime = e->waitTime + 5;
}
//move down one floor
void moveDown(struct Elevator *e){
    e->status = 0;
    e->currFloor--;
    e->waitTime = e->waitTime + 5;
}

//the algorithm chooses whe nto move up and when to move down FIFO CSCAN algorithm
void moveElevator(struct Elevator *e, int** Available, int* aNum){
    FILE *out;
    out = fopen(logFile, "a");
    //elevator is not moving and doesnt have jobs to finish
    if(e->waitTime == 0){
        //if elevator is stopped and there are no people in the elevator, or there are no jobs
        if((e->status == 2 && e->nPeople == 0) || e->numJobs == 0){
            //we want to look at available jobs
            int target;
            //if there is a job avaiable
            if (*aNum > 0){
                target = Available[0][2];
            } else {
                target =-1;
            }
            //if elevator is going up and the number of avaiable jobs is not zero and the target is not equal to the floor currently on
            if((e->currFloor == 1 && *aNum != 0) && (target != e->currFloor)){
                //move elevator up one floor
                moveUp(e);
                printf("Going up.\n");
                fprintf(out,"Going up.\n");
                //of the elevator has reached top floor and the number of jobs avaiable is not equal to zero, and the target job is not on the top floor
            } else if ((e->currFloor == TOPFLOOR && *aNum != 0) && (target != e->currFloor)){
                //move elevator down one floor
                moveDown(e);
                printf("Going down.\n");
                fprintf(out,"Going down.\n");
            //else
            } else {
                //if the number of avaiable jobs is not equal to 0
                if(*aNum != 0){
                    //if the target is more than the current floor
                    if(target > e->currFloor){
                        //move up one floor
                        moveUp(e);
                        printf("Going up.\n");
                        fprintf(out,"Going up.\n");
                        //if the target is less than the current floor
                    }else if (target < e->currFloor){
                        //go down one floor
                        moveDown(e);
                        printf("Going down.\n");
                        fprintf(out,"Going down.\n");
                    //else there are no jobs
                    }else{
                        //elevator status is now stopped
                        e->status = 2;
                        printf("Stopped\n");
                        fprintf(out,"Stopped.\n");
                    }
                //else there are no jobs
                }else{
                    //elevator status is now stopped
                    e->status = 2;
                    printf("Stopped\n");
                    fprintf(out,"Stopped.\n");
                }
            }
        }else{
            //if people are on the elevator and elevator is going up
            if(e->status == 1){
                //move up one floor
                moveUp(e);
                printf("Going up.\n");
                fprintf(out,"Going up.\n");
                //if the status is going down
            }else if (e->status == 0){
                //move down one floor
                moveDown(e);
                printf("Going down.\n");
                fprintf(out,"Going down.\n");
            }else{
                //the elevator goes in direction of person inside
                if(e->activeQueue[0][2] < e->activeQueue[0][3]){
                    moveUp(e);
                    printf("Going up.\n");
                    fprintf(out,"Going up.\n");
                }else{
                    moveDown(e);
                    printf("Going down.\n");
                    fprintf(out,"Going down.\n");
                }
            }
        }
    }
    fclose(out);
}

//randomized job generation
int* generateJob(int currFloor, int upOrDown){ //1 if up, 0 if down
    //set up [time][number of people][currFloor][end floor]
    int* newJob = malloc(sizeof(int)*4);
    newJob[0] = simTime;
    newJob[1] = (rand() % CAPACITY)+1;
    newJob[2] = currFloor;
    if(upOrDown == 1){
        newJob[3] = (rand() % (TOPFLOOR + 1 - (currFloor + 1))) + (currFloor + 1); // range between (currFloor + 1) and top floor
        //newJob[3] = currFloor+1;
    }else{
        newJob[3] = (rand() % (currFloor-1)) +1; // range between (1 and (currFloor-1))
        //newJob[3] =currFloor-1;
    }
    return newJob;
}

bool running = false;
//the threads for the floors
void *threadFloorFunction(void *floor){
    int *myFloor = (int *) floor;
    bool did_PickUp;
    //while simulation is runnninng
    while (running) {
        //if the elevator isnt moving and the elevator is at the threads floor than the thread does the pick up and drop off function
        if (e.currFloor == *myFloor && e.waitTime == 0) {
            dropOff(&e,*myFloor,simTime);
            pthread_mutex_lock(&lockAQ);
            did_PickUp = pickUp(&e,*myFloor);
            pthread_mutex_unlock(&lockAQ);
            //drop off function
            if (did_PickUp){ // adding wait time here allows for proper synchronization
                e.waitTime = e.waitTime + 2;
            }
        }
        //random chance the thread will generate a call for its floor
        if (simTime % genInterval == 0){
            if ((rand()%100) < GENCHANCE){
                int* newJob = NULL;
                int randUpDown = (rand()%2);
                if(!(*myFloor == TOPFLOOR && randUpDown == 1) && !(*myFloor == 1 && randUpDown == 0)) {
                    if(waitingCheckM[*myFloor-1][randUpDown] != 1) {
                        newJob = generateJob(*myFloor, randUpDown);
                        waitingCheckM[*myFloor - 1][randUpDown] = 1;
                    }
                }
                if (newJob != NULL){
                    FILE *out;
                    out = fopen(logFile, "a");
                    pthread_mutex_lock(&lockAQ);
                    printf("Time %d: Call received at floor %d with destination to floor %d.\n",simTime,newJob[2], newJob[3]);
                    fprintf(out,"Time %d: Call received at floor %d with destination to floor %d.\n",simTime,newJob[2], newJob[3]);
                    //add to available jobs
                    AvailableQueue = dynamicallyAdd(AvailableQueue, availableNum, newJob);
                    pthread_mutex_unlock(&lockAQ);
                    fclose(out);
                }
            }
        }
        pthread_barrier_wait(&barrier);
        pthread_cond_wait(&signal, &lockMain);
        pthread_mutex_unlock(&lockMain);
    }
    //call move after pick up and drop off and make pick up and drop of available depending on wait time
    return 0;
}

int floori;
int *floorList;
int totalTime = 180;

//initializes data structures and all necessary setups
void init(){
    //int waitingCheckM[TOPFLOOR][2];
    waitingCheckM = malloc(sizeof(int*)*TOPFLOOR);
    for (int i = 0; i < TOPFLOOR; ++i) {
        waitingCheckM[i] = malloc(sizeof(int)*2);
    }
    floorList = malloc(sizeof(int)*TOPFLOOR);
    srand(time(NULL));
    setUpElevator(&e);
    doneNum = malloc(sizeof(int));
    *doneNum = 0;
    availableNum = malloc(sizeof(int));
    *availableNum = 0;
    if (pthread_mutex_init(&lockAQ, NULL) != 0) {
        printf("\n mutex init has failed\n");
        return;
    }
    for(floori = 0; floori < TOPFLOOR; floori++){
        floorList[floori] = floori+1;
    }
    pthread_barrier_init (&barrier, NULL, TOPFLOOR+1);
    for (int i = 0; i < TOPFLOOR; ++i) {
        for (int j = 0; j < 2; ++j) {
            waitingCheckM[i][j] = 0;
        }
    }
}
// run function runs the threads for each floor and the loop required to run the simulation
void run(){
    pthread_t threads[TOPFLOOR];
    int t;
    int pt;
    running = true;
    for(pt = 0; pt < TOPFLOOR; pt++){
        pthread_create(&threads[pt],NULL,threadFloorFunction,&floorList[pt]);
    }
    for(t = 0; t <= totalTime; t++){
        simTime = t;
        pthread_barrier_wait (&barrier);
        moveElevator(&e, AvailableQueue, availableNum);
        if (e.waitTime != 0){
            e.waitTime--;
        }
        
        sleep(1);
        pthread_mutex_lock(&lockMain);
        pthread_cond_broadcast(&signal);
        pthread_mutex_unlock(&lockMain);
    }
    running = false;
    for(pt = 0; pt < TOPFLOOR; pt++){
        pthread_join(threads[pt], NULL);
    }
}

//prints counters: average wait time, average turn around time, number serviced
void printCounters(){
    //job - [0] = call made, [4] = job picked up, [5] = job finished
    int numberServed = *doneNum;
    int totalStart = 0, totalPickUp = 0, totalDropOff = 0;
    for (int i = 0; i < *doneNum; i++) {
        totalStart = totalStart + doneQueue[i][0];
        totalPickUp = totalPickUp + doneQueue[i][4];
        totalDropOff = totalDropOff + doneQueue[i][5];
    }
    double averageWaitTime = ((double)totalPickUp - (double)totalStart) /((double)numberServed);
    double averageTurnAroundTime = ((double)totalDropOff - (double)totalStart)/((double)numberServed);

    printf("Number Serviced = %d people\n",numberServed);
    printf("Average Wait Time = %f seconds\n", averageWaitTime);
    printf("Average Turnaround Time = %f seconds\n", averageTurnAroundTime);
}

int main(int argc, char *argv[]){
    TOPFLOOR = atoi(argv[1]);
    genInterval = atoi(argv[2]);
    totalTime = atoi(argv[3]);

    init();
    run();
    printCounters();

    pthread_mutex_destroy(&lockMain);
    pthread_mutex_destroy(&lockAQ);

    return 0;
}
