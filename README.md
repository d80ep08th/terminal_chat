# terminal_chat
use terminal windows to chat in a particular room on a particular port

# Compile
>make chat

# Run server
>./server 

# Run client
>./client PORT

## Then enter 
> JOIN ROOM USERNAME

The script validates for "JOIN"

'''
// Client struct
>typedef struct
>{
>    int clientfd;   // File descriptor for the client's connection
>    int identifier; // Client identifier
>    int joined;     // Boolean that says whether a client has joined a room 1 if yes 0 if no
>    char roomname[MAX_ROOMNAME_LENGTH]; // The chat room a client is part of
>    char username[MAX_NAME_LENGTH]; // String name of the user
>} client_struct;

// Buffer for pre-threading
>typedef struct
>{
>    client_struct **buf;
>	int n;			/* Maximum number of slots */
>	int front;		/* buf[(front+1)%n] is the first item */
>	int rear;		/* buf[rear%n] is the last item */
>	int slots;		/* Counts avalible slots */
>	pthread_mutex_t mutex;	/* Protects access to buf */
>	pthread_cond_t not_empty;
>	pthread_cond_t not_full;
>} sbuf_t;
'''
