#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <pthread.h>
#include <unistd.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>

#include <ctype.h>
#include <stdatomic.h>


// Constants
#define MAX_CLIENTS 1000
#define MAX_LINE_LENGTH 20000 // 20k bytes including the \n
#define DEFAULT_PORT 1234
// ASCII string of 1-20 characters
#define MAX_NAME_LENGTH 20
#define MAX_ROOMNAME_LENGTH 20

// For the pre-threading
#define SBUFSIZE 16
#define NTHREADS 4		// Number of worker threads by default


// Typedefs

// Client struct
typedef struct
{
    int clientfd;   // File descriptor for the client's connection
    int identifier; // Client identifier
    int joined;     // Boolean that says whether a client has joined a room 1 if yes 0 if no
    char roomname[MAX_ROOMNAME_LENGTH]; // The chat room a client is part of
    char username[MAX_NAME_LENGTH]; // String name of the user
} cli_linked_list;


// Buffer for pre-threading
typedef struct
{
    cli_linked_list **buf;
	int n;			/* Maximum number of slots */
	int front;		/* buf[(front+1)%n] is the first item */
	int rear;		/* buf[rear%n] is the last item */
	int slots;		/* Counts avalible slots */
	pthread_mutex_t mutex;	/* Protects access to buf */
	pthread_cond_t not_empty;
	pthread_cond_t not_full;
} shared_buffer_t;


// Mapping of Functions
void add_in_Q(cli_linked_list *client);  // Given: Client struct pointer. Effects: Adds a client the the client_list.

void remove_from_Q(cli_linked_list *client); // Given: Client struct pointer. Effects: Removes a client the the client_list.

// Given: Desired size of the bounded buffer and a pointer to the shared_buffer_t pointer.
// Effects:Initializes a bounded buffer for holding connection requests.
void start_buffer(shared_buffer_t *sp, int n);

// Given: Buffer pointer and item to be inserted.. Effects:Inserts item into the rear of the buffer.
void insert_in_Buffer(shared_buffer_t *sp, cli_linked_list *item);

 // Given: Buffer pointer. Effects: Removes first item from buffer.
cli_linked_list* remove_from_buffer(shared_buffer_t *sp);

// Given: 2 strings.. Effects:Concatenates two strings and returns the result.
char* concat(const char *s1, const char *s2);

// Given: Input string.. Effects: Strips newlines and return carriages from an input string by replacing them with nullbytes.
void strip_CR_NL(char *str);

// Given:Port number CLI argument. Effects: Starts the chat server and serves clients.
//int main(int argc, char **argv);

 // Given: Effects:  Services client request with worker thread.
static void* new_thread(void *vargp);

// Given:  String message and connection file descriptor. Effects:  Sends a string message to a single client via client's connection file descriptor.
void msg_described_client( char *msg, int connfd);

// Given:String message and cli_linked_list of who that message is from. Effects:Sends a string message to all other clients in the same room as from_client.
void msg_every_client_same_room(char *msg, cli_linked_list *from_client);

// Given:cli_linked_list of the client sending a message. Effects: Services the client's requests.
static void serve_request_of_client(cli_linked_list *from_client);




// Globals
shared_buffer_t shared_buffer; // Shared buffer of cli_linked_list pointers
cli_linked_list *client_list[MAX_CLIENTS] = {0}; // Array to hold all the currently connected clients (NULL initalized)
pthread_mutex_t client_list_mutex;
atomic_uint num_clients = 0;
int next_identifier = 1; // Number we use to get a unique identifier for an incoming new client



void add_in_Q(cli_linked_list *client)
{
    //char name;
    pthread_mutex_lock(&client_list_mutex);
                      for (int i = 0; i < MAX_CLIENTS; ++i)
                      {
                          if (client_list[i] == NULL) // Looking for a NULL slot
                          {
                              client_list[i] = client;
                              //strcpy(client->username, name);
                              printf("[SERVER]\n Client connected with identifier: %d and fd: %d\n", client->identifier, client->clientfd);
                              //sprintf(buff_out, "%s has joined\n", cli->name);
                              //printf("[SERVER]\n\n\n Client \"%s\" joined the server\n", buff_out);
                              num_clients++;
                              break;
                          }
                      }
    pthread_mutex_unlock(&client_list_mutex);
}


void remove_from_Q(cli_linked_list *client)
{
    pthread_mutex_lock(&client_list_mutex);
    for (int i = 0; i < MAX_CLIENTS; ++i)
    {
        if (client_list[i] != NULL)
        {
            if(client_list[i]->identifier == client->identifier)
            {
                free(client);
                client_list[i] = NULL;
                num_clients--;
            }
        }

    }
    pthread_mutex_unlock(&client_list_mutex);
}

void start_buffer(shared_buffer_t *sp, int n)
{
    sp->buf = malloc(n * sizeof(cli_linked_list *));
    for (int i = 0; i < n; ++i)
    {
        (sp->buf)[i] = NULL;
    }
	sp->n = n;				/* Buffer holds n items */
	sp->front = sp->rear = 0;		/* Empty iff front == rear */
	pthread_mutex_init(&sp->mutex, NULL);	/* Default mutex for locking */
	sp->slots = 0;				/* All slots available */
	pthread_cond_init(&sp->not_full, NULL);
	pthread_cond_init(&sp->not_empty, NULL);
}

void insert_in_Buffer(shared_buffer_t *sp, cli_linked_list *item)
{
	pthread_mutex_lock(&sp->mutex);		/* Lock the buffer */
	while (sp->slots == sp->n) {		/* Wait for available slot */
		pthread_cond_wait(&sp->not_full, &sp->mutex);
	}
	sp->buf[(++sp->rear) % (sp->n)] = item;	/* Insert the item */
	sp->slots = sp->slots + 1;
	pthread_cond_signal(&sp->not_empty);
	pthread_mutex_unlock(&sp->mutex);	/* Unlock the buffer */

}

cli_linked_list* remove_from_buffer(shared_buffer_t *sp)
{
	cli_linked_list *item;
	pthread_mutex_lock(&sp->mutex);		/* Lock the buffer */
	while (sp->slots == 0) {		/* Wait for available item */
		pthread_cond_wait(&sp->not_empty, &sp->mutex);
	}
	item = sp->buf[(++sp->front) % (sp->n)];/* Remove the item */
	sp->slots = sp->slots - 1;
	pthread_cond_signal(&sp->not_full);	/* Announce available slot */
	pthread_mutex_unlock(&sp->mutex);	/* Unlock the buffer */
	return item;
}

char* concat(const char *s1, const char *s2)
{
    char *result = malloc(strlen(s1) + strlen(s2) + 1); // +1 for the null-terminator
    if (result == NULL)
    {
        printf("concat(): failed to allocate memory.\n");
        exit(EXIT_FAILURE);
    }
    strcpy(result, s1);
    strcat(result, s2);
    return result;
}

void strip_CR_NL(char *str)
{
    while (*str != '\0') {
        if (*str == '\r' || *str == '\n') {
            *str = '\0';
        }
        str++;
    }
}

int main(int argc, char **argv)
{
    // Process argument
    if (argc > 2)
    {
        printf("error: server requires a single argument for the desired port number\n");
        printf("usage: ./server [port]\n");
        exit(EXIT_FAILURE);
    }
    unsigned int port;
    if (argc == 1) // If no port number is specified
    {
        port = 1234;
        printf("Started server on default port: %u\n", port);
    }
    else
    {
        port = atoi(argv[1]);
        if (port <= 1023 || port > 65535) {
            printf("error: specify port number greater than 1023\n");
            exit(EXIT_FAILURE);
        }
        printf("Started server on port: %u\n", port);
    }

    // Ignore SIGPIPE
	signal(SIGPIPE, SIG_IGN);

    // Initalize mutex for adding clients to the client_list
    pthread_mutex_init(&client_list_mutex, NULL);



    /* Setup server */

    /* Pre-threading setup */
    pthread_t tid;
    /* Client settings */
    int connfd = 0;
    struct sockaddr_in cli_addr;
    /* Server settings */
    int listenfd = 0;
    struct sockaddr_in serv_addr;
    int opt = 1;

    // Create socket file descriptor for the server
        if ((listenfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) // IPv4, TCP, protocol value 0
        {
            printf("socket creation failed\n");
            exit(EXIT_FAILURE);
        }

        // Forcefully attaching socket to the port
        if (setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)))
        {
            printf("setsockopt error\n");
            exit(EXIT_FAILURE);
        }
        if (setsockopt(listenfd, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt)))
        {
            printf("setsockopt error\n");
            exit(EXIT_FAILURE);
        }


    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr.sin_port = htons(port);

    // Attach socket to the desired port, need to cast sockaddr_in to generic struture sockaddr
    //bind(listenfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)
    if (bind(listenfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
    {
        printf("socket bind failed\n");
        exit(EXIT_FAILURE);
    }

    // Wait for the clients to make a connection
    if (listen(listenfd, MAX_CLIENTS) < 0)
    {
        printf("listen error\n");
        exit(EXIT_FAILURE);
    }
    printf("=== WELCOME TO THE CHATROOM ===\n");


	start_buffer(&shared_buffer, SBUFSIZE); // Create bounded buffer for our worker threads
	for (int i = 0; i < NTHREADS; i++) // Spawn worker threads
    {
		pthread_create(&tid, NULL, new_thread, NULL);
    }




//starts spinning
    /* Handle clients */
    while(1)
    {
        // Create a connected descriptor that can be used to communicate with the client
        socklen_t client_addr_len = sizeof(cli_addr);
        if ((connfd = accept(listenfd, (struct sockaddr *)&cli_addr, &client_addr_len)) < 0)
        {
            printf("accept error\n");
            exit(EXIT_FAILURE);
        }

        /* Check if max clients is reached */
        if (num_clients == MAX_CLIENTS) // Reject connection if the client list is full
        {
            printf("[SERVER]\n Max clients reached, connection rejected\n");
            close(connfd);  // will close the connection
            continue;       /* will get out of the while loop if Max clients are reached   */
        }

        // Initialize a client struct + attributes, and add it to the list of clients
        cli_linked_list *client = calloc(1, sizeof(cli_linked_list));
        client->clientfd = connfd;                //CLIENT FILE DESCRIPTOR
        client->identifier = next_identifier++;    //IDENTIFIER
        client->joined = 0;

        // ADDS CLIENT TO QUEUE
        add_in_Q(client);

        if (num_clients > NTHREADS) // WIf the number of clients is exceeding the default amount of worker threads
        {
            pthread_create(&tid, NULL, new_thread, NULL);
        }

        // Handle the client by inserting the connfd into the bounded buffer (Done by the main thread)
        insert_in_Buffer(&shared_buffer, client);
    }

    return(0);
}

void *new_thread(void *vargp)
{
	(void) vargp; // Avoid message about unused arguments
    pthread_detach(pthread_self()); // No return values
	while(1)
    {
        // Remove next-to-be-serviced client from bounded buffer
        cli_linked_list *from_client = remove_from_buffer(&shared_buffer);
        int connfd = from_client->clientfd;

		// Service client
        serve_request_of_client(from_client);
        char *leave_msg = concat(from_client->username, " has left\n");
        printf("%s has left \n", from_client->username);
        msg_every_client_same_room(leave_msg, from_client);
        remove_from_Q(from_client);
        //if the client is the last client in the room then delete the room
		close(connfd);
	}

  return NULL;
}

void msg_described_client( char *msg, int connfd)
{
    if (write(connfd, msg, strlen(msg)) < 0) {
        printf("Write message to failed\n");
        exit(EXIT_FAILURE);
    }
}

void msg_every_client_same_room(char *msg, cli_linked_list *from_client)
{
    int connfd; // Holds the client fd's for each client iterated throug in the client_list;
    int from_id = from_client->identifier;
    char *from_roomname = from_client->roomname;
    char *end_seq = "\r\n";     // im gonna make that into "\n"

    pthread_mutex_lock(&client_list_mutex);
    for (int i = 0; i < MAX_CLIENTS; ++i)
    {

                  // Looking for a non-NULL slots, don't send to the client that sent it
                  //, and only send to clients from the same room

        if (( client_list[i] != NULL ) && ( client_list[i]->identifier != from_id ) && ( !strcmp(client_list[i]->roomname, from_roomname) ))

        {
            connfd = client_list[i]->clientfd;

            if (write(connfd, msg, strlen(msg)) < 0)
            {
                printf("Write message to all failed (msg)\n");
                exit(EXIT_FAILURE);
            }
            // Write \r\n
            if (write(connfd, end_seq, strlen(end_seq)) < 0)
            {
                printf("Write message to all failed (end_seq)\n");
                exit(EXIT_FAILURE);
            }
        }
    }
    pthread_mutex_unlock(&client_list_mutex);
}

void serve_request_of_client(cli_linked_list *from_client)
{
    int valread;
    int from_connfd = from_client->clientfd;
    int from_id = from_client->identifier;
    //char from_name[MAX_NAME_LENGTH] = from_client->username;
    int from_joined = from_client->joined;
    char msg_buffer[MAX_LINE_LENGTH] = {0}; // Holds a client message

    printf("[SERVER]\n Client \"%d\" joined the server\n", from_id);
    msg_described_client("For telnet clients ==> Connect to a room using this format \"JOIN {ROOMNAME} {USERNAME}\" : \n", from_connfd);

    // JOIN ROOMNAME USERNAME

    //printf("[SERVER]\n\n Client \"%s\" joined the server\n", from_client->username);
    // Continously read messages from the client
    while ((valread = read(from_connfd, msg_buffer, MAX_LINE_LENGTH)) > 0)
    {
        strip_CR_NL(msg_buffer); // Strip return carriage and new line from terminal entered message
        msg_buffer[MAX_LINE_LENGTH - 1] = '\0';

        if (!from_joined)  //from_joined = 0
        {
            // Delimit the input string use strtok() to look for JOIN {ROOMNAME} {USERNAME}<NL>
            int i = 1;
            // strncpy the message buffer since strtok() modifies the msg_buffer
            char msg_buffer_cpy[MAX_LINE_LENGTH] = {0};

            //copying the message buffer
            strncpy(msg_buffer_cpy, msg_buffer, MAX_LINE_LENGTH);
            //gets first token
            char *p = strtok(msg_buffer_cpy, " ");


            if (p == NULL) // Ignore blank input, or else a segfault will occur in the strcmp() below
            {
                //skip the iteration
                continue;
            }

                      if (!strcmp(p, "JOIN")) // first token is same as JOIN then dont enter
                      {
                                while (p) {
                                    p = strtok(NULL, " ");
                                    i = i + 1;          // i counts tokens divided by a ""
                                    //JOIN ROOMNAME USERNAME
                                    //====|========|========
                                    //i=1, JOIN || i=2,  ROOMNAME || i=3, USERNAME

                                                  if( strlen(p) < 2 || strlen(p) >= 20-1)
                                                  {

                                                          if (i == 2) // ROOMNAME
                                                          {
                                                            if( strlen(p) < 2 || strlen(p) >= 20-1)
                                                            {
                                                              strncpy(from_client->roomname, p, MAX_ROOMNAME_LENGTH);
                                                              //save roomname
                                                              from_client->roomname[MAX_ROOMNAME_LENGTH - 1] = '\0';
                                                            }
                                                            else
                                                            {
                                                              printf("the roomname is too small or too large, only 20chars please\n");
                                                              msg_described_client("ERROR: the roomname is too small or too large, only 20chars please \n", from_connfd);
                                                              break;
                                                            }
                                                          }
                                                          else if (i == 3) // USERNAME
                                                          {

                                                            if( strlen(p) < 2 || strlen(p) >= 20-1)
                                                            {
                                                              strncpy(from_client->username, p, MAX_NAME_LENGTH);
                                                              from_client->roomname[MAX_NAME_LENGTH - 1] = '\0';
                                                            }
                                                            else
                                                            {
                                                              printf("the username is too small or too large, only 20chars please\n");
                                                              msg_described_client("ERROR: the username is too small or too large, only 20chars please \n", from_connfd);
                                                              break;
                                                            }
                                                          }
                                                  }

                                  }

                          //if while(p) ends without discovering 4 tokens which means i = 4, then it throws error
                                if (i != 4) // will be true for every number except 4, {1,2,3,5,6,....}
                                {
                                    msg_described_client("ERROR\n", from_connfd);
                                    break;
                                }
                                else  // it sends a message about room being assigned to the client
                                {

                                    printf("[SERVER]\n Client identified by: \"%d\" and named: \"%s\" has joined the room called: \"%s\"\n", from_id, from_client->username, from_client->roomname);
                                    char* joined_message = concat(from_client->username, " has joined");
                                    msg_every_client_same_room(joined_message, from_client);
                                    msg_described_client(joined_message, from_client->clientfd);
                                    msg_described_client("\r\n", from_client->clientfd);

                                    /*
                                    The free() function in C library allows you to release or deallocate
                                    the memory blocks which are previously allocated by calloc(), malloc()
                                    or realloc() functions. It frees up the memory blocks and returns the
                                    memory to heap. It helps freeing the memory in your program which will be available for later use
                                    */
                                    free(joined_message);
                                    from_joined = 1;    // this will make it skip the next if(!from_joined)
                                }

                              }
                              else // if the client isnt connected
                              {
                                  msg_described_client("ERROR\n", from_connfd);
                                  break;
                              }

          }
          else // from_joined = 1, Once the client has joined a chatroom they will be able to send messages
          {
              printf("[SERVER]\n In room: \"%s\", client \"%d\" said: \"%s\"\n", from_client->roomname, from_id, msg_buffer); // Print the client message on the server side

              // Append username and prompt to the message
              char* username = from_client->username;
              char* prompt = concat(username, ": ");
              char* complete_msg = concat(prompt, msg_buffer);

              // Send prompted message back to self and to all other clients in the same chat room
              msg_every_client_same_room(complete_msg, from_client);
              msg_described_client(complete_msg, from_client->clientfd);
              msg_described_client("\r\n", from_client->clientfd);

              /*
              The free() function in C library allows you to release or deallocate
              the memory blocks which are previously allocated by calloc(), malloc()
              or realloc() functions. It frees up the memory blocks and returns the
              memory to heap. It helps freeing the memory in your program which will be available for later use
              */
              free(prompt);
              free(complete_msg);
          }
        //the memset() function is used to set a one-byte value to a memory block
        //byte by byte. This function is useful for initialization of a memory block
        //byte by byte by a particular value.
        memset(msg_buffer, 0, sizeof(msg_buffer)); // Clear msg_buffer so previous messages don't leak into the next

    } // end of the big while loop   while ((valread = read(from_connfd, msg_buffer, MAX_LINE_LENGTH)) > 0)

}
