#include "common.h"

// Allowed file extensions
const char* ALLOWED_EXTENSIONS[] = {".txt", ".pdf", ".jpg", ".png"};
const int NUM_ALLOWED_EXTENSIONS = 4;

int is_valid_username(const char* username) {
    if (!username || strlen(username) == 0 || strlen(username) > MAX_USERNAME_LEN) {
        return 0;
    }
    
    for (int i = 0; username[i]; i++) {
        if (!isalnum(username[i])) {
            return 0;
        }
    }
    return 1;
}

int is_valid_room_name(const char* room_name) {
    if (!room_name || strlen(room_name) == 0 || strlen(room_name) > MAX_ROOM_NAME_LEN) {
        return 0;
    }
    
    for (int i = 0; room_name[i]; i++) {
        if (!isalnum(room_name[i])) {
            return 0;
        }
    }
    return 1;
}

int is_valid_file_extension(const char* filename) {
    if (!filename) return 0;
    
    char* dot = strrchr(filename, '.');
    if (!dot) return 0;
    
    for (int i = 0; i < NUM_ALLOWED_EXTENSIONS; i++) {
        if (strcasecmp(dot, ALLOWED_EXTENSIONS[i]) == 0) {
            return 1;
        }
    }
    return 0;
}

void send_message(int socket_fd, Message* msg) {
    if (send(socket_fd, msg, sizeof(Message), 0) < 0) {
        perror("Failed to send message");
    }
}

int receive_message(int socket_fd, Message* msg) {
    ssize_t bytes_received = recv(socket_fd, msg, sizeof(Message), 0);
    if (bytes_received <= 0) {
        return -1; // Connection closed or error
    }
    return 0;
} 