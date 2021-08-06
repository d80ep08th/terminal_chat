#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>

#define LENGTH 2048

// Global variables
volatile sig_atomic_t flag = 0;
int sockfd = 0;
char name[32];

void owrite_str_std_op() {
  printf("%s", "> ");
  fflush(stdout);
}

void str_trim_lf (char* arr, int length) {
  int i;
  for (i = 0; i < length; i++) { // loop till it finds \n in the string and replace it with \0
    if (arr[i] == '\n') {
      arr[i] = '\0';// \0 indicates the terminaton of a character string
      break;
    }
  }
}

void validate_JOIN (char* arr, int length) {
  int i;
  for (i = 0; i < length; i++) { // loop till it finds "JOIN" as the first four characters in the string
    if (arr[i] == 'J' && arr[i] == 'O' && arr[i] == 'I' && arr[i] == 'N') {

      /*
      if first four words are JOIN
      the string after JOIN is ROOM
          //if ROOM has no error
                // if ROOM exists
                      connect to chatroom ROOM
                      as the USERNAME
                //else
                      create new chatroom
                      as the USERNAME

      the code for client and server works the same in a ROOM
      but  now the server creates rooms before a client can connect
      and  if a client is trying to connect to an existing room , then it doesnt create a room

      creating a new room is same as creating a new client


      */




      break;
    }
  }
}

void catch_ctrl_c_and_exit(int sig) {
    flag = 1;
}

void send_msg_handler() {                 //Send Message to the chatroom
  char message[LENGTH] = {};
	char buffer[LENGTH + 32] = {};

  while(1) {                                  //SO that it keeps spinning for every thread in every client spinner
  	owrite_str_std_op();
    fgets(message, LENGTH, stdin);
    str_trim_lf(message, LENGTH);

    if (strcmp(message, "exit") == 0) {
			break;
    } else {
      sprintf(buffer, "%s:", name);
      sprintf(buffer, "%s\n", message);
      send(sockfd, buffer, strlen(buffer), 0);
    }

		bzero(message, LENGTH);
    bzero(buffer, LENGTH + 32);
  }
  catch_ctrl_c_and_exit(2);
}


void recv_msg_handler() {                   //Recieve message from the CHATROOM
	char message[LENGTH] = {};
  //char message_by[32];

  while (1) {                               //so that it keeps spinning for every thread in every client spinner
		int receive = recv(sockfd, message, LENGTH, 0);
    if (receive > 0) {
      //sprintf()
      printf("%s", message);
      owrite_str_std_op();
    } else if (receive == 0) {
			break;
    } else {
			// -1
		}
		memset(message, 0, sizeof(message));
  }
}

int main(int argc, char **argv){
	if(argc != 2){
		printf("Usage: %s <port>\n", argv[0]);
		return EXIT_FAILURE;
	}

	char *ip = "127.0.0.1";
	int port = atoi(argv[1]);

	signal(SIGINT, catch_ctrl_c_and_exit);

	printf("JOIN {ROOMNAME} {USERNAME}: ");  //JUST ENTER THE JOIN COMMAND HERE
  fgets(name, 32, stdin);              //which triggers the birth or inititiation in a specific room
  //scanf("%s",name);

    str_trim_lf(name, strlen(name));


    if (strlen(name) > 32 || strlen(name) < 2 ){
      printf("Name must be less than 30 and more than 2 characters and should start with .\n");
      return EXIT_FAILURE;
	   }

     struct sockaddr_in server_addr;

	/* Socket settings */
      sockfd = socket(AF_INET, SOCK_STREAM, 0);
      server_addr.sin_family = AF_INET;
      server_addr.sin_addr.s_addr = inet_addr(ip);
      server_addr.sin_port = htons(port);


  // Connect to Server
    int err = connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr));

  if (err == -1) {
		printf("ERROR: connect\n");
		return EXIT_FAILURE;
	}

	// Send name
	send(sockfd, name, 32, 0);

	printf("=== WELCOME TO THE CHATROOM ===\n");
  printf("%s has joined\n", name);


  //thread to send message in the CHATROOM
	pthread_t send_msg_thread;

            if(pthread_create(&send_msg_thread, NULL, (void *) send_msg_handler, NULL) != 0){
          		printf("ERROR: pthread\n");
              return EXIT_FAILURE;
          	}

  //thread that recieves message from the CHATROOM
	pthread_t recv_msg_thread;

        if(pthread_create(&recv_msg_thread, NULL, (void *) recv_msg_handler, NULL) != 0){
      		printf("ERROR: pthread\n");
      		return EXIT_FAILURE;
      	}

      	while (1){
      		if(flag){
      			printf("\nBye\n");
      			break;
          }
      	}

	close(sockfd);

	return EXIT_SUCCESS;
}
