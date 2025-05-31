#ifndef COMMON_H
#define COMMON_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <semaphore.h>
#include <signal.h>
#include <time.h>
#include <errno.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <ctype.h>

// Constants
#define MAX_CLIENTS 15
#define MAX_ROOMS 50
#define MAX_USERNAME_LEN 16
#define MAX_ROOM_NAME_LEN 32
#define MAX_MESSAGE_LEN 1024
#define MAX_FILENAME_LEN 256
#define MAX_FILE_SIZE (3 * 1024 * 1024) // 3MB
#define MAX_UPLOAD_QUEUE 5
#define BUFFER_SIZE 4096

// Message types
#define MSG_JOIN_ROOM 1
#define MSG_LEAVE_ROOM 2
#define MSG_BROADCAST 3
#define MSG_WHISPER 4
#define MSG_SEND_FILE 5
#define MSG_EXIT 6
#define MSG_ERROR 7
#define MSG_SUCCESS 8
#define MSG_FILE_DATA 9
#define MSG_SERVER_SHUTDOWN 10

// Color codes for client output
#define COLOR_RESET "\033[0m"
#define COLOR_RED "\033[31m"
#define COLOR_GREEN "\033[32m"
#define COLOR_YELLOW "\033[33m"
#define COLOR_BLUE "\033[34m"
#define COLOR_MAGENTA "\033[35m"
#define COLOR_CYAN "\033[36m"

// File types allowed
extern const char* ALLOWED_EXTENSIONS[];
extern const int NUM_ALLOWED_EXTENSIONS;

// Message structure
typedef struct {
    int type;
    char username[MAX_USERNAME_LEN + 1];
    char target[MAX_USERNAME_LEN + 1];
    char room_name[MAX_ROOM_NAME_LEN + 1];
    char content[MAX_MESSAGE_LEN];
    char filename[MAX_FILENAME_LEN];
    size_t file_size;
    int success;
} Message;

// Client structure
typedef struct {
    int socket_fd;
    char username[MAX_USERNAME_LEN + 1];
    char current_room[MAX_ROOM_NAME_LEN + 1];
    char last_room[MAX_ROOM_NAME_LEN + 1];
    struct sockaddr_in address;
    pthread_t thread_id;
    int active;
} Client;

// Room structure
typedef struct {
    char name[MAX_ROOM_NAME_LEN + 1];
    Client* clients[MAX_CLIENTS];
    int client_count;
    pthread_mutex_t mutex;
    int active;
} Room;

// File transfer structure
typedef struct {
    char filename[MAX_FILENAME_LEN];
    char sender[MAX_USERNAME_LEN + 1];
    char receiver[MAX_USERNAME_LEN + 1];
    size_t file_size;
    char* file_data;
    time_t timestamp;
} FileTransfer;

// Common function prototypes
int is_valid_username(const char* username);
int is_valid_room_name(const char* room_name);
int is_valid_file_extension(const char* filename);
void log_message(const char* format, ...);
char* get_timestamp(void);
void send_message(int socket_fd, Message* msg);
int receive_message(int socket_fd, Message* msg);

// File handler functions (server only)
int init_file_handler(void);
void cleanup_file_handler(void);
int enqueue_file_transfer(const char* filename, const char* sender, const char* receiver, 
                         const char* file_data, size_t file_size);
int get_queue_size(void);

// Client utility functions
void print_colored_message(int type, const char* username, const char* content);
void print_help(void);
int read_file_data(const char* filename, char** file_data, size_t* file_size);

#endif // COMMON_H 