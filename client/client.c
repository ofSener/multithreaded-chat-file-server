#include "../common.h"

// Global client state
static int client_socket = -1;
static int client_running = 1;
static pthread_t receive_thread;

// Function prototypes
void* receive_messages(void* arg);
void signal_handler(int sig);
int connect_to_server(const char* server_ip, int port);
int authenticate(const char* username);
void process_command(const char* input);
void cleanup_client(void);

int main(int argc, char* argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <server_ip> <port>\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    
    const char* server_ip = argv[1];
    int port = atoi(argv[2]);
    
    if (port <= 0 || port > 65535) {
        fprintf(stderr, "Invalid port number\n");
        exit(EXIT_FAILURE);
    }
    
    // Set up signal handler
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    // Connect to server
    if (connect_to_server(server_ip, port) < 0) {
        fprintf(stderr, "Failed to connect to server\n");
        exit(EXIT_FAILURE);
    }
    
    printf("%sConnected to server %s:%d%s\n", COLOR_GREEN, server_ip, port, COLOR_RESET);
    
    // Get username
    char username[MAX_USERNAME_LEN + 1];
    printf("Enter your username: ");
    fflush(stdout);
    
    if (!fgets(username, sizeof(username), stdin)) {
        cleanup_client();
        exit(EXIT_FAILURE);
    }
    
    // Remove newline
    username[strcspn(username, "\n")] = '\0';
    
    // Authenticate
    if (authenticate(username) < 0) {
        cleanup_client();
        exit(EXIT_FAILURE);
    }
    
    // Start receive thread
    if (pthread_create(&receive_thread, NULL, receive_messages, NULL) != 0) {
        perror("Failed to create receive thread");
        cleanup_client();
        exit(EXIT_FAILURE);
    }
    
    // Show help
    print_help();
    
    // Main command loop
    char input[BUFFER_SIZE];
    printf("> ");
    fflush(stdout);
    
    while (client_running && fgets(input, sizeof(input), stdin)) {
        // Remove newline
        input[strcspn(input, "\n")] = '\0';
        
        if (strlen(input) > 0) {
            process_command(input);
        }
        
        if (client_running) {
            printf("> ");
            fflush(stdout);
        }
    }
    
    cleanup_client();
    return 0;
}

void* receive_messages(void* arg) {
    (void)arg; // Suppress unused parameter warning
    
    Message msg;
    while (client_running) {
        if (receive_message(client_socket, &msg) < 0) {
            if (client_running) {
                printf("\n%sConnection lost to server%s\n", COLOR_RED, COLOR_RESET);
                client_running = 0;
            }
            break;
        }
        
        // Clear current line and print message
        printf("\r\033[K");
        print_colored_message(msg.type, msg.username, msg.content);
        
        if (msg.type == MSG_SERVER_SHUTDOWN) {
            client_running = 0;
            break;
        }
        
        if (client_running) {
            printf("> ");
            fflush(stdout);
        }
    }
    
    return NULL;
}

void signal_handler(int sig) {
    if (sig == SIGINT || sig == SIGTERM) {
        printf("\n%sDisconnecting...%s\n", COLOR_YELLOW, COLOR_RESET);
        client_running = 0;
        
        if (client_socket >= 0) {
            Message exit_msg = {0};
            exit_msg.type = MSG_EXIT;
            send_message(client_socket, &exit_msg);
            
            // Give server time to process exit message
            usleep(100000); // 100ms
        }
        
        // Force exit after cleanup attempt
        cleanup_client();
        exit(0);
    }
}

int connect_to_server(const char* server_ip, int port) {
    client_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (client_socket < 0) {
        perror("Socket creation failed");
        return -1;
    }
    
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    
    if (inet_pton(AF_INET, server_ip, &server_addr.sin_addr) <= 0) {
        perror("Invalid address");
        close(client_socket);
        client_socket = -1;
        return -1;
    }
    
    if (connect(client_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Connection failed");
        close(client_socket);
        client_socket = -1;
        return -1;
    }
    
    return 0;
}

int authenticate(const char* username) {
    Message auth_msg = {0};
    strcpy(auth_msg.username, username);
    send_message(client_socket, &auth_msg);
    
    Message response;
    if (receive_message(client_socket, &response) < 0) {
        printf("%sAuthentication failed: Connection lost%s\n", COLOR_RED, COLOR_RESET);
        return -1;
    }
    
    if (response.type == MSG_ERROR) {
        printf("%sAuthentication failed: %s%s\n", COLOR_RED, response.content, COLOR_RESET);
        return -1;
    }
    
    printf("%s%s%s\n", COLOR_GREEN, response.content, COLOR_RESET);
    return 0;
}

void process_command(const char* input) {
    if (input[0] != '/') {
        printf("%sCommands must start with '/'. Type /help for available commands.%s\n", 
               COLOR_YELLOW, COLOR_RESET);
        return;
    }
    
    char command[64];
    char arg1[256];
    char arg2[256];
    char* rest;
    
    // Parse command
    if (sscanf(input, "/%s %s %s", command, arg1, arg2) < 1) {
        printf("%sInvalid command format%s\n", COLOR_RED, COLOR_RESET);
        return;
    }
    
    Message msg = {0};
    
    if (strcmp(command, "help") == 0) {
        print_help();
        return;
    }
    else if (strcmp(command, "exit") == 0) {
        msg.type = MSG_EXIT;
        send_message(client_socket, &msg);
        client_running = 0;
        return;
    }
    else if (strcmp(command, "join") == 0) {
        if (strlen(arg1) == 0) {
            printf("%sUsage: /join <room_name>%s\n", COLOR_YELLOW, COLOR_RESET);
            return;
        }
        msg.type = MSG_JOIN_ROOM;
        strcpy(msg.room_name, arg1);
    }
    else if (strcmp(command, "leave") == 0) {
        msg.type = MSG_LEAVE_ROOM;
    }
    else if (strcmp(command, "broadcast") == 0) {
        // Get the rest of the message after "/broadcast "
        rest = strstr(input, " ");
        if (!rest || strlen(rest + 1) == 0) {
            printf("%sUsage: /broadcast <message>%s\n", COLOR_YELLOW, COLOR_RESET);
            return;
        }
        msg.type = MSG_BROADCAST;
        strcpy(msg.content, rest + 1);
    }
    else if (strcmp(command, "whisper") == 0) {
        if (strlen(arg1) == 0 || strlen(arg2) == 0) {
            printf("%sUsage: /whisper <username> <message>%s\n", COLOR_YELLOW, COLOR_RESET);
            return;
        }
        
        // Get the rest of the message after username
        char search_str[300];
        snprintf(search_str, sizeof(search_str), "/whisper %s ", arg1);
        rest = strstr(input, search_str);
        if (!rest) {
            printf("%sUsage: /whisper <username> <message>%s\n", COLOR_YELLOW, COLOR_RESET);
            return;
        }
        rest += strlen(search_str);
        
        msg.type = MSG_WHISPER;
        strcpy(msg.target, arg1);
        strcpy(msg.content, rest);
    }
    else if (strcmp(command, "sendfile") == 0) {
        if (strlen(arg1) == 0 || strlen(arg2) == 0) {
            printf("%sUsage: /sendfile <filename> <username>%s\n", COLOR_YELLOW, COLOR_RESET);
            return;
        }
        
        // Validate file extension
        if (!is_valid_file_extension(arg1)) {
            printf("%sInvalid file type. Allowed: .txt, .pdf, .jpg, .png%s\n", 
                   COLOR_RED, COLOR_RESET);
            return;
        }
        
        // Read file data
        char* file_data;
        size_t file_size;
        int result = read_file_data(arg1, &file_data, &file_size);
        
        if (result == -1) {
            printf("%sFile not found: %s%s\n", COLOR_RED, arg1, COLOR_RESET);
            return;
        } else if (result == -2) {
            printf("%sFile too large. Maximum size is 3MB.%s\n", COLOR_RED, COLOR_RESET);
            return;
        } else if (result < 0) {
            printf("%sError reading file: %s%s\n", COLOR_RED, arg1, COLOR_RESET);
            return;
        }
        
        msg.type = MSG_SEND_FILE;
        strcpy(msg.filename, arg1);
        strcpy(msg.target, arg2);
        msg.file_size = file_size;
        
        // Copy file data to message content (limited by MAX_MESSAGE_LEN)
        size_t copy_size = (file_size < MAX_MESSAGE_LEN) ? file_size : MAX_MESSAGE_LEN;
        memcpy(msg.content, file_data, copy_size);
        
        if (file_size > MAX_MESSAGE_LEN) {
            printf("%sWarning: File truncated due to message size limit%s\n", 
                   COLOR_YELLOW, COLOR_RESET);
            msg.file_size = copy_size;
        }
        
        free(file_data);
    }
    else {
        printf("%sUnknown command: %s. Type /help for available commands.%s\n", 
               COLOR_RED, command, COLOR_RESET);
        return;
    }
    
    send_message(client_socket, &msg);
}

void cleanup_client(void) {
    client_running = 0;
    
    if (client_socket >= 0) {
        // Shutdown socket to unblock receive thread
        shutdown(client_socket, SHUT_RDWR);
        close(client_socket);
        client_socket = -1;
    }
    
    // Wait for receive thread to finish (it should exit quickly now)
    pthread_join(receive_thread, NULL);
    
    printf("%sDisconnected. Goodbye!%s\n", COLOR_GREEN, COLOR_RESET);
} 