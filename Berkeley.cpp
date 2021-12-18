#include<stdio.h>
#include<unistd.h>
#include<iostream>
#include<arpa/inet.h>
#include<netinet/in.h>
#include<stdlib.h>
#include<time.h>
#include<pthread.h>
#include<strings.h>
#include<queue>
#include <bits/stdc++.h>
using namespace std;

void *getClockDifference(void *args);
void requestForProcessesClockValue();
void waitForDeamonRequest();
void sendSynchronisedTimeStamps();
void *sendTimeToProcess(void *args);

const int BUFFER_SIZE = 200;
const int MAX_LISTEN_QUEUE_SIZE = 100;
const char *SEND_CLOCK_VALUE = "SEND_CLOCK_VALUE";
int* processes;
int* processClocks;
bool amIdeamon = false;
int clockCounter = 0;
int totalNumberOfProcesses = 0;
int portNumber = 8080;
int sum_diff = 0;
int avg_diff = 0;

int main(int argc,char* argv[]){
  if(argc < 2){
    printf("Please enter the totalNumber of processes and process number and restart again\n");
    exit(1);
  }

  totalNumberOfProcesses = atoi(argv[1]);
  int processNumber = atoi(argv[2]);
  portNumber = portNumber + (processNumber - 1) * 10;

  processClocks = (int*)malloc(sizeof(int)*totalNumberOfProcesses);
  processes = (int*)malloc(sizeof(int)*totalNumberOfProcesses);


  //Process with processNumber = 1, acts as the daemon
  if(processNumber == 1){
    amIdeamon = true;
  }
  time_t t;
  srand((unsigned) time(&t));
  clockCounter = rand() % 50;

  if(amIdeamon){
    printf("I'm deamon and I will start the synchronisation process\n");
    printf("The clock counter before synchronisation is : %d\n",clockCounter);
    int i;
    for(i = 0 ;i<totalNumberOfProcesses;i++){
      processes[i] = i+1;
    }
    requestForProcessesClockValue();
    sendSynchronisedTimeStamps();
    clockCounter = clockCounter + avg_diff;
    printf("The clock counter after synchronisation is : %d\n", clockCounter);

  }else{
    printf("I'm not deamon and will run on portNumber : %d\n",portNumber);
    printf("The clock counter before synchronisation is : %d\n",clockCounter);
    waitForDeamonRequest();
  }

  return 0;
}

/*
  Daemon executes this method and requests for clock values of peers to
  calculate the difference
*/
void requestForProcessesClockValue(){

  pthread_t threads[totalNumberOfProcesses-1];
  int i;
  processClocks[0] = clockCounter;

  for(i = 2; i<=totalNumberOfProcesses;i++){
    // printf("%d\n",i);
    pthread_create(&threads[i-2],NULL,&getClockDifference,&processes[i-1]);
  }

  for(i=2; i<=totalNumberOfProcesses;i++){
    pthread_join(threads[i-2],NULL);
  }
}

/*
  Once avg is calculated the daemon broadcasts the offset
*/
void sendSynchronisedTimeStamps() {

  avg_diff = sum_diff / totalNumberOfProcesses + 1;

  pthread_t threads[totalNumberOfProcesses-1];
  int i;
  for(i = 2; i<=totalNumberOfProcesses;i++){
    pthread_create(&threads[i-2],NULL,&sendTimeToProcess,&processes[i-1]);
  }

  for(i=2; i<=totalNumberOfProcesses;i++){
    pthread_join(threads[i-2],NULL);
  }
}

/*
  It connects to socket of peer process and send the offset
*/
void *sendTimeToProcess(void  *args){
  int processNumber = *(int *)args;
  // printf("Process Number : %d\n", processNumber);
  int socketfd;
  // char buffer[BUFFER_SIZE] = {0};
  struct sockaddr_in serverAddress;
  socketfd = socket(AF_INET,SOCK_STREAM,0);

  serverAddress.sin_family = AF_INET;
  serverAddress.sin_port = htons(8080 + (processNumber - 1 )* 10);
  inet_pton(AF_INET,"127.0.0.1",&serverAddress.sin_addr);
  connect(socketfd,(struct sockaddr *)&serverAddress,sizeof(serverAddress));
  printf("Sending the synchronised TimeStamp to processNumber : %d \n",processNumber);
  string token = "CLOCK_ADJUSTMENT";

  send(socketfd,token.c_str(),token.size(),0);

  //Sending time adjustment
  int clock_adjustment = 0;

  clock_adjustment = clockCounter - processClocks[processNumber-1] + avg_diff;

  string message = std::to_string(clock_adjustment);
  send(socketfd,message.c_str(),message.size(),0);

  close(socketfd);
  return NULL;
}

/*
  Daemon sends request to each process and then computes the avg clock offset
*/
void *getClockDifference(void  *args){
  int processNumber = *(int *)args;
  // printf("Process Number : %d\n", processNumber);
  int socketfd;
  char buffer[BUFFER_SIZE] = {0};
  struct sockaddr_in serverAddress;
  socketfd = socket(AF_INET,SOCK_STREAM,0);

  serverAddress.sin_family = AF_INET;
  serverAddress.sin_port = htons(8080 + (processNumber - 1 )* 10);
  inet_pton(AF_INET,"127.0.0.1",&serverAddress.sin_addr);
  connect(socketfd,(struct sockaddr *)&serverAddress,sizeof(serverAddress));
  // printf("Please send me your clock counter number, processNumber : %d \n",processNumber);
  string token = "SEND_CLOCK_VALUE";
  send(socketfd,token.c_str(),token.size(),0);

  bzero(buffer,BUFFER_SIZE);
  read(socketfd,buffer,BUFFER_SIZE);
  std::cout << "Process Clock for Process No "<< processNumber << " is : " << buffer << '\n';
  //get the difference and also store the clockDifference
  processClocks[processNumber-1] = atoi(buffer);
  int diff = processClocks[processNumber-1] - clockCounter;
  sum_diff = sum_diff + diff;
  close(socketfd);
  return NULL;
}

/*
  It waits for daemon to start the synchronisation process and sends the
  requested data and gets the clock adjustment data as response.
  It uses the adjustment data to adjust its clock
*/
void waitForDeamonRequest(){
  struct sockaddr_in serverAddress;
  int addresslen = sizeof(serverAddress);
  int socketfd = socket(AF_INET, SOCK_STREAM, 0);
	char buffer[BUFFER_SIZE] = {0};

  //binding mediator to socket
  serverAddress.sin_family = AF_INET;
  serverAddress.sin_port = htons(portNumber);
  serverAddress.sin_addr.s_addr = INADDR_ANY;

  //Binding socket to the port no defined in PORT
  if( bind(socketfd, (struct sockaddr *)&serverAddress, sizeof(serverAddress)) == 0){ // returns -1 if failed
    printf("Binded Socket Successfully to the port number %d \n",portNumber);
  } else{
    printf("Binding Socket Failed...\n Try After 10-15s \n");
    printf("Sometimes this happens when we stop server and start it immediately\n");
    exit(1);
  }

  //Listen for new connections with queue size of 250
  if(listen(socketfd,MAX_LISTEN_QUEUE_SIZE) < 0){
    printf("Listener failed");
    exit(1);
  }

  bool exit = true;
  while(true && exit){
    int newSocketFd;
    struct sockaddr_in clientAddress;
    newSocketFd = accept(socketfd,(struct sockaddr *)&clientAddress,(socklen_t*)&addresslen);
    bzero(buffer,BUFFER_SIZE);
    read(newSocketFd,buffer,BUFFER_SIZE);
    // std::cout << buffer << '\n';

    //if buffer == SEND_CLOCK_VALUE then send your clock value and wait for update
    if(strcmp(buffer,SEND_CLOCK_VALUE) == 0){
      string token = std::to_string(clockCounter);
      printf("Sending my clock counter : %d\n", clockCounter);
      send(newSocketFd,token.c_str(),token.size(),0);
      bzero(buffer,BUFFER_SIZE);
    }else{
      exit = false;
      read(newSocketFd,buffer,BUFFER_SIZE);
      // std::cout << buffer << '\n';
      int clock_adjustment = atoi(buffer);
      std::cout << "Clock Adjustment : " << clock_adjustment << '\n';
      //now read for the time difference;

      //Add the value to the clockCounter to get synchronised clock.
      clockCounter = clockCounter + clock_adjustment;
      printf("The clock counter after synchronisation is : %d\n", clockCounter );
    }

    close(newSocketFd);
  }


  close(socketfd);
}
