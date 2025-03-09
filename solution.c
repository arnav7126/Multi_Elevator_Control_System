#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <math.h>
#include <stdbool.h>
#include <sys/types.h>
#include <stdlib.h>
#include <sys/msg.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <ctype.h>
#include <limits.h>
#include <pthread.h>

#define MAX_NEW_REQUESTS 30
#define ELEVATOR_MAX_CAP 20
#define ALLOWED_PASSENGER_LIMIT 5
#define MAX_NO_OF_ELEVATORS 100
#define MAX_NO_OF_PASSENGERS 3000
#define PERMS 0666

pthread_mutex_t lock;
bool isCorrectAuthString = false;

typedef struct PassengerRequest
{
    int requestId;
    int startFloor;
    int requestedFloor;
} PassengerRequest;

typedef struct MainSharedMemory
{
    char authStrings[MAX_NO_OF_ELEVATORS][ELEVATOR_MAX_CAP + 1];
    char elevatorMovementInstructions[MAX_NO_OF_ELEVATORS];
    PassengerRequest newPassengerRequests[MAX_NEW_REQUESTS];
    int elevatorFloors[MAX_NO_OF_ELEVATORS];
    int droppedPassengers[1000];
    int pickedUpPassengers[1000][2];
} MainSharedMemory;

typedef struct TurnChangeResponse
{
    long mtype;
    int turnNumber;
    int newNumberOfPassengers;
    int errorOccurred;
    int finished;
} TurnChangeResponse;

typedef struct TurnChangeRequest
{
    long mtype;
    int droppedPassengersCount;
    int pickedUpPassengersCount;
} TurnChangeRequest;

typedef struct SolverRequest
{
    long mtype;
    int elevatorNumber;
    char authStringGuess[ELEVATOR_MAX_CAP + 1];
} SolverRequest;

typedef struct SolverResponse
{
    long mtype;
    int guessIsCorrect;
} SolverResponse;

typedef struct Passenger
{
    int passengerId[MAX_NO_OF_PASSENGERS];
    int selectedElevator[MAX_NO_OF_PASSENGERS];
    char stageOfPassenger[MAX_NO_OF_PASSENGERS];
    int startFloor[MAX_NO_OF_PASSENGERS];
    int requestedFloor[MAX_NO_OF_PASSENGERS];
} Passenger;

typedef struct Elevator
{
    int currentFloor[MAX_NO_OF_ELEVATORS];
    char elevatorDirection[MAX_NO_OF_ELEVATORS];
    int currentPassengersInElevator[MAX_NO_OF_ELEVATORS];
    int authFlag[MAX_NO_OF_ELEVATORS];
    int initialPassengersInElevator[MAX_NO_OF_ELEVATORS];
} Elevator;

typedef struct SolverRange
{
    int startOfRange;
    int endOfRange;
    int solverId;
    int elevatorNumber;
    MainSharedMemory *sharedMemoryPtr;
    int *messageQueIds;
    Elevator *elevators;
} SolverRange;

int power(int base, int exponent)
{
    int ans = 1;
    for (int i = 0; i < exponent; i++)
    {
        ans *= base;
    }
    return ans;
}

void authStringGuess(int index, char *authString, int lengthOfAuthString)
{
    const char allowedLetters[] = "abcdef";
    for (int i = 0; i < lengthOfAuthString; i++)
    {
        authString[i] = 'a';
    }
    authString[lengthOfAuthString] = '\0';

    for (int i = lengthOfAuthString - 1; i >= 0; i--)
    {
        authString[i] = allowedLetters[index % 6];
        index /= 6;

        if (index == 0)
        {
            break;
        }
    }
}

void *threadFunction(void *arg)
{
    SolverRange *solverRangeIndex = (SolverRange *)arg;
    SolverRequest request = {0};
    MainSharedMemory *sharedMemoryPtr = solverRangeIndex->sharedMemoryPtr;
    int *messageQueIds = solverRangeIndex->messageQueIds;
    int authStringLength = solverRangeIndex->elevators->initialPassengersInElevator[solverRangeIndex->elevatorNumber];
    char authString[ELEVATOR_MAX_CAP + 1];
    request.mtype = 2;
    request.elevatorNumber = solverRangeIndex->elevatorNumber;

    if (msgsnd(messageQueIds[solverRangeIndex->solverId - 1], &request, sizeof(request) - sizeof(request.mtype), 0) == -1)
    {
        perror("could not send target message to solver");
        pthread_exit(NULL);
    }
    for (int i = solverRangeIndex->startOfRange; i <= solverRangeIndex->endOfRange; i++)
    {
        if (isCorrectAuthString)
        {
            pthread_exit(NULL);
        }

        authStringGuess(i, authString, authStringLength);

        SolverRequest guess = {0};
        guess.mtype = 3;
        strncpy(guess.authStringGuess, authString, ELEVATOR_MAX_CAP);

        if (msgsnd(messageQueIds[solverRangeIndex->solverId - 1], &guess, sizeof(guess) - sizeof(guess.mtype), 0) == -1)
        {
            perror("could not send auth string guess to solver");
            pthread_exit(NULL);
        }

        SolverResponse solverResponse;
        if (msgrcv(messageQueIds[solverRangeIndex->solverId - 1], &solverResponse, sizeof(solverResponse) - sizeof(solverResponse.mtype), 4, 0) != -1)
        {
            if (solverResponse.guessIsCorrect == 1)
            {
                pthread_mutex_lock(&lock);
                if (!isCorrectAuthString)
                {
                    isCorrectAuthString = true;
                    strncpy(sharedMemoryPtr->authStrings[solverRangeIndex->elevatorNumber], authString, ELEVATOR_MAX_CAP);
                }
                pthread_mutex_unlock(&lock);
                pthread_exit(NULL);
            }
        }
    }
    pthread_exit(NULL);
}

int main()
{
    FILE *inputFile = fopen("input.txt", "r");
    if (inputFile == NULL)
    {
        perror("could not open input file");
        return -1;
    }

    int noOfElevators, noOfFloors, noOfSolvers, turnNoLastReq;
    fscanf(inputFile, "%d", &noOfElevators);
    fscanf(inputFile, "%d", &noOfFloors);
    fscanf(inputFile, "%d", &noOfSolvers);
    fscanf(inputFile, "%d", &turnNoLastReq);

    key_t sharedMemoryKey, mainMessageQueKey;
    fscanf(inputFile, "%d", &sharedMemoryKey);
    fscanf(inputFile, "%d", &mainMessageQueKey);

    printf("no of elevators: %d\n", noOfElevators);
    printf("no of floors: %d\n", noOfFloors);
    printf("no of solvers: %d\n", noOfSolvers);
    printf("turn no of last request: %d\n", turnNoLastReq);
    printf("shared memory key: %d\n", sharedMemoryKey);
    printf("main message que key: %d\n", mainMessageQueKey);

    key_t individualSolverKeys[noOfSolvers];

    for (int i = 0; i < noOfSolvers; i++)
    {
        fscanf(inputFile, "%d", &individualSolverKeys[i]);
        // printf("solver %d key: %d\n", i + 1, individualSolverKeys[i]);
    }

    int solverMessageQueIds[noOfSolvers];
    for (int i = 0; i < noOfSolvers; i++)
    {
        if ((solverMessageQueIds[i] = msgget(individualSolverKeys[i], PERMS)) == -1)
        {
            perror("could not connect to solver message que");
            return -2;
        }
        // printf("connected to solver %d message que with id: %d\n", i + 1, solverMessageQueIds[i]);
    }

    fclose(inputFile);

    int shmId;
    if ((shmId = shmget(sharedMemoryKey, sizeof(MainSharedMemory), PERMS)) == -1)
    {
        perror("could not connect to shared memory");
        return -3;
    }

    // printf("connected to shared memory with id: %d\n", shmId);

    MainSharedMemory *mainShmPtr;
    mainShmPtr = (MainSharedMemory *)shmat(shmId, NULL, 0);
    if (mainShmPtr == (void *)-1)
    {
        perror("could not attach shared memory");
        return -4;
    }

    // printf("attached to shared memory\n");

    int mainMessageQueId;
    if ((mainMessageQueId = msgget(mainMessageQueKey, PERMS)) == -1)
    {
        perror("could not connect to main message que");
        return -5;
    }

    // printf("connected to main message que with id: %d\n", mainMessageQueId);

    Passenger passengers;
    Elevator elevators;

    for (int i = 0; i < MAX_NO_OF_PASSENGERS; i++)
    {
        passengers.passengerId[i] = -1;
        passengers.selectedElevator[i] = -1;
        passengers.stageOfPassenger[i] = 'u';
        passengers.startFloor[i] = -1;
        passengers.requestedFloor[i] = -1;
    }

    for (int i = 0; i < noOfElevators; i++)
    {
        elevators.currentFloor[i] = 0;
        elevators.elevatorDirection[i] = 's';
        elevators.currentPassengersInElevator[i] = 0;
        elevators.authFlag[i] = 0;
        elevators.initialPassengersInElevator[i] = 0;
    }
    // printf("segments initilaised successfully\n");

    // printf("Size of MainSharedMemory: %lu bytes\n", sizeof(MainSharedMemory));

    int noOfPassengersIndex = 0;
    TurnChangeResponse currentTurnResponse;
    currentTurnResponse.turnNumber = 0;
    TurnChangeRequest currentTurnRequest;

    while (currentTurnResponse.turnNumber <= turnNoLastReq)
    {
        if (msgrcv(mainMessageQueId, &currentTurnResponse, sizeof(currentTurnResponse) - sizeof(currentTurnResponse.mtype), 2, 0) == -1)
        {
            perror("could not receive message from helper");
            return -6;
        }
        if (currentTurnResponse.errorOccurred)
        {
            perror("error occured from main helper");
            return -7;
        }
        if (currentTurnResponse.finished)
        {
            printf("finished");
            return 0;
        }
        for (int i = 0; i < currentTurnResponse.newNumberOfPassengers; i++)
        {
            PassengerRequest currentRequest = mainShmPtr->newPassengerRequests[i];
            passengers.passengerId[noOfPassengersIndex] = currentRequest.requestId;
            passengers.startFloor[noOfPassengersIndex] = currentRequest.startFloor;
            passengers.requestedFloor[noOfPassengersIndex] = currentRequest.requestedFloor;
            passengers.stageOfPassenger[noOfPassengersIndex] = 'u';
            noOfPassengersIndex++;
        }

        //  for(int i=0; i<noOfPassengersIndex; i++){
        //      printf("passenger id: %d startOfRange floor: %d endOfRange floor: %d\n", passengers.passengerId[i], passengers.startFloor[i], passengers.requestedFloor[i]);
        //  }
        //  return 0;

        for (int i = 0; i < noOfElevators; i++)
        {
            elevators.elevatorDirection[i] = 's';
            mainShmPtr->elevatorMovementInstructions[i] = elevators.elevatorDirection[i];
        }

        currentTurnRequest.mtype = 1;
        currentTurnRequest.droppedPassengersCount = 0;
        currentTurnRequest.pickedUpPassengersCount = 0;
        if (msgsnd(mainMessageQueId, &currentTurnRequest, sizeof(currentTurnRequest) - sizeof(currentTurnRequest.mtype), 0) == -1)
        {
            perror("could not send message to helper");
            return -8;
        }
    }

    // for (int i = 0; i < noOfPassengersIndex; i++)
    // {
    //     printf("passenger id: %d\n", passengers.passengerId[i]);
    // }

    // printf("testttttttttttttt\n");

    bool areAllPassengersDropped = false;
    while (!areAllPassengersDropped)
    {
        areAllPassengersDropped = true;
        for (int i = 0; i < noOfPassengersIndex; i++)
        {
            if (passengers.stageOfPassenger[i] != 'c')
            {
                areAllPassengersDropped = false;
                break;
            }
        }
        if (areAllPassengersDropped)
            break;

        // TurnChangeResponse currentTurnResponse;
        if (msgrcv(mainMessageQueId, &currentTurnResponse, sizeof(currentTurnResponse) - sizeof(currentTurnResponse.mtype), 2, 0) == -1)
        {
            perror("could not receive message from helper");
            return -9;
        }

        if (currentTurnResponse.errorOccurred)
        {
            perror("error detected from main helper");
            return -10;
        }
        if (currentTurnResponse.finished)
        {
            printf("finished\n");
            break;
        }
        int currentTurnNumber = currentTurnResponse.turnNumber;
        int newNumberOfPassengers = currentTurnResponse.newNumberOfPassengers;

        // debug to check new requests number
        printf("current turn number: %d\n", currentTurnNumber);
        // printf("new requests: %d\n", newNumberOfPassengers);

        for (int i = 0; i < noOfElevators; i++)
        {
            elevators.initialPassengersInElevator[i] = elevators.currentPassengersInElevator[i];
        }

        int numberOfDroppedPassengers = 0;
        int numberOfPickedUpPassengers = 0;

        for (int i = 0; i < noOfElevators; i++)
        {
            elevators.currentFloor[i] = mainShmPtr->elevatorFloors[i];
            // printf("elevator %d current floor: %d\n", i, elevators.currentFloor[i]);
        }

        for (int i = 0; i < noOfElevators; i++)
        {
            for (int j = 0; j < noOfPassengersIndex; j++)
            {
                if (passengers.selectedElevator[j] == i &&
                    passengers.stageOfPassenger[j] == 'p' &&
                    passengers.requestedFloor[j] == elevators.currentFloor[i])
                {
                    passengers.stageOfPassenger[j] = 'c';
                    mainShmPtr->droppedPassengers[numberOfDroppedPassengers] = passengers.passengerId[j];
                    elevators.currentPassengersInElevator[i]--;
                    passengers.selectedElevator[j] = -1;
                    numberOfDroppedPassengers++;
                    printf("dropped passenger id: %d\n", passengers.passengerId[j]);
                }
            }
        }

        for (int i = 0; i < noOfElevators; i++)
        {
            if (elevators.initialPassengersInElevator[i] == 0)
            {
                if (elevators.currentFloor[i] == noOfFloors - 1)
                {
                    elevators.elevatorDirection[i] = 'd';
                }
                else if (elevators.currentFloor[i] == 0)
                {
                    elevators.elevatorDirection[i] = 'u';
                }
            }
            else
            {
                pthread_t threads[noOfSolvers];
                isCorrectAuthString = false;

                if (pthread_mutex_init(&lock, NULL) != 0)
                {
                    perror("could not initialise mutex");
                    return -11;
                }

                SolverRange individualSolverRange[noOfSolvers];
                int totalPossibilities = power(6, elevators.initialPassengersInElevator[i]);
                int rangeSize = totalPossibilities / noOfSolvers;

                for (int j = 0; j < noOfSolvers; j++)
                {
                    individualSolverRange[j].solverId = j + 1;
                    individualSolverRange[j].elevatorNumber = i;
                    individualSolverRange[j].startOfRange = j * rangeSize;
                    individualSolverRange[j].endOfRange = (j + 1) * rangeSize - 1;
                    individualSolverRange[j].sharedMemoryPtr = mainShmPtr;
                    individualSolverRange[j].messageQueIds = solverMessageQueIds;
                    individualSolverRange[j].elevators = &elevators;

                    if (j == noOfSolvers - 1)
                    {
                        individualSolverRange[j].endOfRange = totalPossibilities - 1;
                    }
                }

                for (int j = 0; j < noOfSolvers; j++)
                {
                    pthread_create(&threads[j], NULL, threadFunction, &individualSolverRange[j]);
                }

                for (int j = 0; j < noOfSolvers; j++)
                {
                    pthread_join(threads[j], NULL);
                }

                pthread_mutex_destroy(&lock);

                if (elevators.currentFloor[i] == noOfFloors - 1)
                {
                    elevators.elevatorDirection[i] = 'd';
                }
                else if (elevators.currentFloor[i] == 0)
                {
                    elevators.elevatorDirection[i] = 'u';
                }
            }
            mainShmPtr->elevatorMovementInstructions[i] = elevators.elevatorDirection[i];
        }

        // Pick up passengers
        for (int i = 0; i < noOfPassengersIndex; i++)
        {
            if (passengers.stageOfPassenger[i] != 'u')
                continue;

            char passengerStartTowardsRequestDirection = (passengers.requestedFloor[i] > passengers.startFloor[i]) ? 'u' : 'd';
            int leastCrowdedElevator = -1;
            int minNumberOfPeopleInElevator = INT_MAX;

            for (int j = 0; j < noOfElevators; j++)
            {
                if (elevators.currentPassengersInElevator[j] < ALLOWED_PASSENGER_LIMIT &&
                    (elevators.elevatorDirection[j] == passengerStartTowardsRequestDirection || elevators.elevatorDirection[j] == 's') &&
                    passengers.startFloor[i] == elevators.currentFloor[j])
                {
                    if (elevators.currentPassengersInElevator[j] < minNumberOfPeopleInElevator)
                    {
                        leastCrowdedElevator = j;
                        minNumberOfPeopleInElevator = elevators.currentPassengersInElevator[j];
                    }
                }
            }
            if (leastCrowdedElevator != -1)
            {
                passengers.stageOfPassenger[i] = 'p';
                passengers.selectedElevator[i] = leastCrowdedElevator;
                elevators.currentPassengersInElevator[leastCrowdedElevator]++;
                mainShmPtr->pickedUpPassengers[numberOfPickedUpPassengers][0] = passengers.passengerId[i];
                mainShmPtr->pickedUpPassengers[numberOfPickedUpPassengers++][1] = leastCrowdedElevator;
                printf("picked up passenger id: %d by elevator: %d on floor %d\n", passengers.passengerId[i], leastCrowdedElevator, elevators.currentFloor[leastCrowdedElevator]);
            }
        }

        // if (elevators.elevatorDirection[i] == 'u' && elevators.currentFloor[i] < noOfFloors - 1)
        // {
        //     elevators.currentFloor[i]++;
        // }
        // else if (elevators.elevatorDirection[i] == 'd' && elevators.currentFloor[i] > 0)
        // {
        //     elevators.currentFloor[i]--;
        // }

        // if (elevators.currentFloor[i] == 0)
        // {
        //     elevators.elevatorDirection[i] = 'u';
        // }
        // else if (elevators.currentFloor[i] == noOfFloors - 1)
        // {
        //     elevators.elevatorDirection[i] = 'd';
        // }
        // mainShmPtr->elevatorMovementInstructions[i] = elevators.elevatorDirection[i];
        //}

        // for (int i = 0; i < noOfPassengersIndex; i++)
        // {
        //     printf("%c ", passengers.stageOfPassenger[i]);
        // }
        // printf("\n");

        // for (int i = 0; i < noOfElevators; i++)
        // {
        //     printf("%d ", assignedPassengerCount[i]);
        // }
        // printf("\n");

        // TurnChangeRequest currentTurnRequest;
        currentTurnRequest.mtype = 1;
        currentTurnRequest.droppedPassengersCount = numberOfDroppedPassengers;
        currentTurnRequest.pickedUpPassengersCount = numberOfPickedUpPassengers;
        if (msgsnd(mainMessageQueId, &currentTurnRequest, sizeof(currentTurnRequest) - sizeof(currentTurnRequest.mtype), 0) == -1)
        {
            perror("could not send turn change request");
            return -12;
        }
    }
    printf("FINISHEDDDDDDDDDDDDDDDDDDDDDDD\n");
    
    // Ensure the shared memory is detached and removed
    if (shmdt(mainShmPtr) == -1) {
        perror("Error detaching shared memory in solution");
    }
    
    if (shmctl(shmId, IPC_RMID, NULL) == -1) {
        perror("Error removing shared memory in solution");
    }
    
    // Ensure message queue is removed
    if (msgctl(mainMessageQueId, IPC_RMID, NULL) == -1) {
        perror("Error removing main message queue in solution");
    }


    return 0;
}
