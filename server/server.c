#include "../common.h"

// Global server state
static Client clients[MAX_CLIENTS];
static Room rooms[MAX_ROOMS];
static int server_socket;
static int server_running = 1;
static pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t rooms_mutex = PTHREAD_MUTEX_INITIALIZER;

// Function prototypes
void* handle_client(void* arg);
void signal_handler(int sig);
int find_client_by_username(const char* username);
int find_room_by_name(const char* room_name);
int create_room(const char* room_name);
void join_room(Client* client, const char* room_name);
void leave_room(Client* client);
void broadcast_to_room(Client* sender, const char* message);
void send_whisper(Client* sender, const char* target_username, const char* message);
void handle_file_transfer(Client* sender, const char* filename, const char* target_username, const char* file_data, size_t file_size);
void cleanup_client(Client* client);
void shutdown_server(void);

int main(int argc, char* argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <port>\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    
    int port = atoi(argv[1]);
    if (port <= 0 || port > 65535) {
        fprintf(stderr, "Invalid port number\n");
        exit(EXIT_FAILURE);
    }
    
    // Initialize server state
    memset(clients, 0, sizeof(clients));
    memset(rooms, 0, sizeof(rooms));
    
    // Initialize room mutexes
    for (int i = 0; i < MAX_ROOMS; i++) {
        pthread_mutex_init(&rooms[i].mutex, NULL);
    }
    
    // Set up signal handler for graceful shutdown
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    // Create socket
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }
    
    // Set socket options
    int opt = 1;
    if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("Setsockopt failed");
        exit(EXIT_FAILURE);
    }
    
    // Bind socket
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);
    
    if (bind(server_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind failed");
        exit(EXIT_FAILURE);
    }
    
    // Listen for connections
    if (listen(server_socket, MAX_CLIENTS) < 0) {
        perror("Listen failed");
        exit(EXIT_FAILURE);
    }
    
    log_message("[INFO] Server listening on port %d...", port);
    
    // Initialize file handler
    if (init_file_handler() != 0) {
        log_message("[ERROR] Failed to initialize file handler");
        exit(EXIT_FAILURE);
    }
    
    // Accept connections
    while (server_running) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        
        int client_socket = accept(server_socket, (struct sockaddr*)&client_addr, &client_len);
        if (client_socket < 0) {
            if (server_running) {
                perror("Accept failed");
            }
            continue;
        }
        
        // Find available client slot
        pthread_mutex_lock(&clients_mutex);
        int client_index = -1;
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (!clients[i].active) {
                client_index = i;
                break;
            }
        }
        
        if (client_index == -1) {
            pthread_mutex_unlock(&clients_mutex);
            log_message("[REJECTED] Server full, rejecting connection from %s", 
                       inet_ntoa(client_addr.sin_addr));
            
            Message reject_msg = {0};
            reject_msg.type = MSG_ERROR;
            strcpy(reject_msg.content, "Server is full. Try again later.");
            send_message(client_socket, &reject_msg);
            close(client_socket);
            continue;
        }
        
        // Initialize client
        clients[client_index].socket_fd = client_socket;
        clients[client_index].address = client_addr;
        clients[client_index].active = 1;
        strcpy(clients[client_index].current_room, "");
        strcpy(clients[client_index].last_room, "");
        
        pthread_mutex_unlock(&clients_mutex);
        
        // Create thread for client
        if (pthread_create(&clients[client_index].thread_id, NULL, handle_client, 
                          &clients[client_index]) != 0) {
            perror("Thread creation failed");
            cleanup_client(&clients[client_index]);
            continue;
        }
        
        log_message("[CONNECT] New client connected from %s", inet_ntoa(client_addr.sin_addr));
    }
    
    shutdown_server();
    return 0;
}

void* handle_client(void* arg) {
    Client* client = (Client*)arg;
    Message msg;
    
    // Get username
    if (receive_message(client->socket_fd, &msg) < 0) {
        cleanup_client(client);
        return NULL;
    }
    
    // Validate username
    if (!is_valid_username(msg.username)) {
        Message error_msg = {0};
        error_msg.type = MSG_ERROR;
        strcpy(error_msg.content, "Invalid username. Use alphanumeric characters only (max 16 chars).");
        send_message(client->socket_fd, &error_msg);
        cleanup_client(client);
        return NULL;
    }
    
    // Check for duplicate username
    pthread_mutex_lock(&clients_mutex);
    if (find_client_by_username(msg.username) != -1) {
        pthread_mutex_unlock(&clients_mutex);
        
        Message error_msg = {0};
        error_msg.type = MSG_ERROR;
        strcpy(error_msg.content, "Username already taken. Choose another.");
        send_message(client->socket_fd, &error_msg);
        
        log_message("[REJECTED] Duplicate username attempted: %s", msg.username);
        cleanup_client(client);
        return NULL;
    }
    
    strcpy(client->username, msg.username);
    pthread_mutex_unlock(&clients_mutex);
    
    // Send success message
    Message success_msg = {0};
    success_msg.type = MSG_SUCCESS;
    strcpy(success_msg.content, "Connected successfully!");
    send_message(client->socket_fd, &success_msg);
    
    log_message("[LOGIN] user '%s' connected from %s", client->username, 
               inet_ntoa(client->address.sin_addr));
    
    // Handle client messages
    while (server_running && client->active) {
        if (receive_message(client->socket_fd, &msg) < 0) {
            break;
        }
        
        switch (msg.type) {
            case MSG_JOIN_ROOM: {
                Message error_msg = {0};
                if (!is_valid_room_name(msg.room_name)) {
                    error_msg.type = MSG_ERROR;
                    strcpy(error_msg.content, "Invalid room name. Use alphanumeric characters only (max 32 chars).");
                    send_message(client->socket_fd, &error_msg);
                } else {
                    join_room(client, msg.room_name);
                }
                break;
            }
                
            case MSG_LEAVE_ROOM:
                leave_room(client);
                break;
                
            case MSG_BROADCAST: {
                Message error_msg = {0};
                if (strlen(client->current_room) == 0) {
                    error_msg.type = MSG_ERROR;
                    strcpy(error_msg.content, "You must join a room first.");
                    send_message(client->socket_fd, &error_msg);
                } else {
                    broadcast_to_room(client, msg.content);
                }
                break;
            }
                
            case MSG_WHISPER:
                send_whisper(client, msg.target, msg.content);
                break;
                
            case MSG_SEND_FILE:
                handle_file_transfer(client, msg.filename, msg.target, msg.content, msg.file_size);
                break;
                
            case MSG_EXIT:
                client->active = 0;
                break;
                
            default: {
                Message error_msg = {0};
                error_msg.type = MSG_ERROR;
                strcpy(error_msg.content, "Unknown command.");
                send_message(client->socket_fd, &error_msg);
                break;
            }
        }
    }
    
    cleanup_client(client);
    return NULL;
}

void signal_handler(int sig) {
    if (sig == SIGINT || sig == SIGTERM) {
        log_message("[SHUTDOWN] SIGINT received. Shutting down server...");
        server_running = 0;
        
        // Notify all clients and close their sockets
        Message shutdown_msg = {0};
        shutdown_msg.type = MSG_SERVER_SHUTDOWN;
        strcpy(shutdown_msg.content, "Server is shutting down. Goodbye!");
        
        pthread_mutex_lock(&clients_mutex);
        int active_clients = 0;
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (clients[i].active) {
                send_message(clients[i].socket_fd, &shutdown_msg);
                // Close client socket to unblock recv() calls
                shutdown(clients[i].socket_fd, SHUT_RDWR);
                close(clients[i].socket_fd);
                clients[i].active = 0;
                active_clients++;
            }
        }
        pthread_mutex_unlock(&clients_mutex);
        
        log_message("[SHUTDOWN] Disconnecting %d clients, saving logs.", active_clients);
        
        // Close server socket to stop accepting new connections
        if (server_socket >= 0) {
            close(server_socket);
        }
        
        // Cleanup file handler
        cleanup_file_handler();
        
        // Force exit after brief cleanup
        usleep(200000); // 200ms for cleanup
        log_message("[SHUTDOWN] Server shutdown complete");
        exit(0);
    }
}

int find_client_by_username(const char* username) {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].active && strcmp(clients[i].username, username) == 0) {
            return i;
        }
    }
    return -1;
}

int find_room_by_name(const char* room_name) {
    for (int i = 0; i < MAX_ROOMS; i++) {
        if (rooms[i].active && strcmp(rooms[i].name, room_name) == 0) {
            return i;
        }
    }
    return -1;
}

int create_room(const char* room_name) {
    for (int i = 0; i < MAX_ROOMS; i++) {
        if (!rooms[i].active) {
            strcpy(rooms[i].name, room_name);
            rooms[i].client_count = 0;
            rooms[i].active = 1;
            memset(rooms[i].clients, 0, sizeof(rooms[i].clients));
            log_message("[ROOM-CREATE] Room '%s' created", room_name);
            return i;
        }
    }
    return -1;
}

void join_room(Client* client, const char* room_name) {
    char old_room[MAX_ROOM_NAME_LEN + 1] = "";
    int is_rejoining = 0;
    
    // Debug logging
    log_message("[DEBUG] join_room: user='%s', room_name='%s', current_room='%s', last_room='%s'", 
                client->username, room_name, client->current_room, client->last_room);
    
    // Store old room name for logging
    if (strlen(client->current_room) > 0) {
        strcpy(old_room, client->current_room);
        // Check if rejoining the same room
        if (strcmp(old_room, room_name) == 0) {
            is_rejoining = 1;
            log_message("[DEBUG] Detected rejoining (same as current room)");
        }
    } else if (strlen(client->last_room) > 0 && strcmp(client->last_room, room_name) == 0) {
        // User is rejoining their last room after leaving
        is_rejoining = 1;
        log_message("[DEBUG] Detected rejoining (same as last room)");
    }
    
    log_message("[DEBUG] is_rejoining = %d", is_rejoining);
    
    pthread_mutex_lock(&rooms_mutex);
    
    // Leave current room if in one (without acquiring rooms_mutex again)
    if (strlen(old_room) > 0) {
        int old_room_index = find_room_by_name(old_room);
        if (old_room_index != -1) {
            Room* old_room_ptr = &rooms[old_room_index];
            pthread_mutex_lock(&old_room_ptr->mutex);
            
            // Remove client from old room
            for (int i = 0; i < old_room_ptr->client_count; i++) {
                if (old_room_ptr->clients[i] == client) {
                    for (int j = i; j < old_room_ptr->client_count - 1; j++) {
                        old_room_ptr->clients[j] = old_room_ptr->clients[j + 1];
                    }
                    old_room_ptr->client_count--;
                    break;
                }
            }
            
            // Deactivate room if empty
            if (old_room_ptr->client_count == 0) {
                old_room_ptr->active = 0;
                log_message("[ROOM-DESTROY] Room '%s' destroyed (empty)", old_room_ptr->name);
            }
            
            pthread_mutex_unlock(&old_room_ptr->mutex);
        }
        strcpy(client->current_room, "");
    }
    
    // Find or create new room
    int room_index = find_room_by_name(room_name);
    if (room_index == -1) {
        room_index = create_room(room_name);
        if (room_index == -1) {
            pthread_mutex_unlock(&rooms_mutex);
            Message error_msg = {0};
            error_msg.type = MSG_ERROR;
            strcpy(error_msg.content, "Cannot create room. Server limit reached.");
            send_message(client->socket_fd, &error_msg);
            return;
        }
    }
    
    Room* room = &rooms[room_index];
    pthread_mutex_lock(&room->mutex);
    
    // Check room capacity
    if (room->client_count >= MAX_CLIENTS) {
        pthread_mutex_unlock(&room->mutex);
        pthread_mutex_unlock(&rooms_mutex);
        Message error_msg = {0};
        error_msg.type = MSG_ERROR;
        strcpy(error_msg.content, "Room is full.");
        send_message(client->socket_fd, &error_msg);
        return;
    }
    
    // Add client to new room
    room->clients[room->client_count] = client;
    room->client_count++;
    strcpy(client->current_room, room_name);
    
    // Clear last_room if rejoining or switching
    if (is_rejoining || strlen(old_room) > 0) {
        strcpy(client->last_room, "");
    }
    
    pthread_mutex_unlock(&room->mutex);
    pthread_mutex_unlock(&rooms_mutex);
    
    // Send success message
    Message success_msg = {0};
    success_msg.type = MSG_SUCCESS;
    snprintf(success_msg.content, sizeof(success_msg.content), 
             "You joined the room '%s'", room_name);
    send_message(client->socket_fd, &success_msg);
    
    // Log room switching, rejoining, or joining
    if (is_rejoining) {
        log_message("[ROOM] user '%s' rejoined '%s'", client->username, room_name);
    } else if (strlen(old_room) > 0) {
        log_message("[ROOM] user '%s' left room '%s', joined '%s'", client->username, old_room, room_name);
    } else {
        log_message("[JOIN] user '%s' joined room '%s'", client->username, room_name);
    }
}

void leave_room(Client* client) {
    if (strlen(client->current_room) == 0) {
        Message error_msg = {0};
        error_msg.type = MSG_ERROR;
        strcpy(error_msg.content, "You are not in any room.");
        send_message(client->socket_fd, &error_msg);
        return;
    }
    
    // Store current room as last room before leaving
    strcpy(client->last_room, client->current_room);
    
    pthread_mutex_lock(&rooms_mutex);
    int room_index = find_room_by_name(client->current_room);
    if (room_index != -1) {
        Room* room = &rooms[room_index];
        pthread_mutex_lock(&room->mutex);
        
        // Remove client from room
        for (int i = 0; i < room->client_count; i++) {
            if (room->clients[i] == client) {
                for (int j = i; j < room->client_count - 1; j++) {
                    room->clients[j] = room->clients[j + 1];
                }
                room->client_count--;
                break;
            }
        }
        
        // Deactivate room if empty
        if (room->client_count == 0) {
            room->active = 0;
            log_message("[ROOM-DESTROY] Room '%s' destroyed (empty)", room->name);
        }
        
        pthread_mutex_unlock(&room->mutex);
    }
    pthread_mutex_unlock(&rooms_mutex);
    
    log_message("[LEAVE] user '%s' left room '%s'", client->username, client->current_room);
    strcpy(client->current_room, "");
    
    Message success_msg = {0};
    success_msg.type = MSG_SUCCESS;
    strcpy(success_msg.content, "You left the room.");
    send_message(client->socket_fd, &success_msg);
}

void broadcast_to_room(Client* sender, const char* message) {
    pthread_mutex_lock(&rooms_mutex);
    int room_index = find_room_by_name(sender->current_room);
    if (room_index == -1) {
        pthread_mutex_unlock(&rooms_mutex);
        return;
    }
    
    Room* room = &rooms[room_index];
    pthread_mutex_lock(&room->mutex);
    
    Message broadcast_msg = {0};
    broadcast_msg.type = MSG_BROADCAST;
    strcpy(broadcast_msg.username, sender->username);
    strcpy(broadcast_msg.content, message);
    
    for (int i = 0; i < room->client_count; i++) {
        if (room->clients[i] != sender) {
            send_message(room->clients[i]->socket_fd, &broadcast_msg);
        }
    }
    
    pthread_mutex_unlock(&room->mutex);
    pthread_mutex_unlock(&rooms_mutex);
    
    // Send confirmation to sender
    Message success_msg = {0};
    success_msg.type = MSG_SUCCESS;
    strcpy(success_msg.content, "Message sent to room");
    send_message(sender->socket_fd, &success_msg);
    
    log_message("[BROADCAST] user '%s': %s", sender->username, message);
}

void send_whisper(Client* sender, const char* target_username, const char* message) {
    pthread_mutex_lock(&clients_mutex);
    int target_index = find_client_by_username(target_username);
    if (target_index == -1) {
        pthread_mutex_unlock(&clients_mutex);
        Message error_msg = {0};
        error_msg.type = MSG_ERROR;
        strcpy(error_msg.content, "User not found or offline.");
        send_message(sender->socket_fd, &error_msg);
        return;
    }
    
    Client* target = &clients[target_index];
    
    Message whisper_msg = {0};
    whisper_msg.type = MSG_WHISPER;
    strcpy(whisper_msg.username, sender->username);
    strcpy(whisper_msg.content, message);
    send_message(target->socket_fd, &whisper_msg);
    
    pthread_mutex_unlock(&clients_mutex);
    
    // Send confirmation to sender
    Message success_msg = {0};
    success_msg.type = MSG_SUCCESS;
    snprintf(success_msg.content, sizeof(success_msg.content), 
             "Whisper sent to %s", target_username);
    send_message(sender->socket_fd, &success_msg);
    
    log_message("[WHISPER] %s -> %s: %s", sender->username, target_username, message);
}

void handle_file_transfer(Client* sender, const char* filename, const char* target_username, 
                         const char* file_data, size_t file_size) {
    // Validate file
    if (!is_valid_file_extension(filename)) {
        Message error_msg = {0};
        error_msg.type = MSG_ERROR;
        strcpy(error_msg.content, "Invalid file type. Allowed: .txt, .pdf, .jpg, .png");
        send_message(sender->socket_fd, &error_msg);
        return;
    }
    
    if (file_size > MAX_FILE_SIZE) {
        Message error_msg = {0};
        error_msg.type = MSG_ERROR;
        strcpy(error_msg.content, "File too large. Maximum size is 3MB.");
        send_message(sender->socket_fd, &error_msg);
        log_message("[ERROR] File '%s' from user '%s' exceeds size limit.", filename, sender->username);
        return;
    }
    
    // Check if target user exists
    pthread_mutex_lock(&clients_mutex);
    int target_index = find_client_by_username(target_username);
    if (target_index == -1) {
        pthread_mutex_unlock(&clients_mutex);
        Message error_msg = {0};
        error_msg.type = MSG_ERROR;
        strcpy(error_msg.content, "Target user not found or offline.");
        send_message(sender->socket_fd, &error_msg);
        return;
    }
    pthread_mutex_unlock(&clients_mutex);
    
    // Enqueue file transfer
    if (enqueue_file_transfer(filename, sender->username, target_username, file_data, file_size) == 0) {
        Message success_msg = {0};
        success_msg.type = MSG_SUCCESS;
        int queue_size = get_queue_size();
        if (queue_size > 1) {
            snprintf(success_msg.content, sizeof(success_msg.content), 
                     "File added to upload queue. Queue size: %d", queue_size);
        } else {
            strcpy(success_msg.content, "File upload started.");
        }
        send_message(sender->socket_fd, &success_msg);
        
        log_message("[COMMAND] %s initiated file transfer to %s", sender->username, target_username);
    } else {
        Message error_msg = {0};
        error_msg.type = MSG_ERROR;
        strcpy(error_msg.content, "Upload queue is full. Try again later.");
        send_message(sender->socket_fd, &error_msg);
    }
}

void cleanup_client(Client* client) {
    if (!client->active) return;
    
    log_message("[DISCONNECT] user '%s' lost connection. Cleaned up resources.", client->username);
    
    // Leave room if in one
    if (strlen(client->current_room) > 0) {
        leave_room(client);
    }
    
    // Close socket
    if (client->socket_fd >= 0) {
        close(client->socket_fd);
    }
    
    // Clear client data
    memset(client, 0, sizeof(Client));
}

void shutdown_server(void) {
    log_message("[SHUTDOWN] Server shutdown initiated");
    
    // Mark all clients as inactive and close their connections
    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].active) {
            clients[i].active = 0;
            if (clients[i].socket_fd >= 0) {
                shutdown(clients[i].socket_fd, SHUT_RDWR);
                close(clients[i].socket_fd);
            }
            memset(&clients[i], 0, sizeof(Client));
        }
    }
    pthread_mutex_unlock(&clients_mutex);
    
    // Cleanup file handler
    cleanup_file_handler();
    
    // Destroy mutexes
    pthread_mutex_destroy(&clients_mutex);
    pthread_mutex_destroy(&rooms_mutex);
    for (int i = 0; i < MAX_ROOMS; i++) {
        pthread_mutex_destroy(&rooms[i].mutex);
    }
    
    // Close server socket
    if (server_socket >= 0) {
        close(server_socket);
    }
    
    log_message("[SHUTDOWN] Server shutdown complete");
} 