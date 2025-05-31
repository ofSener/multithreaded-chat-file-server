#include "../common.h"

void print_colored_message(int type, const char* username, const char* content) {
    switch (type) {
        case MSG_SUCCESS:
            printf("%s[Server]:%s %s\n", COLOR_GREEN, COLOR_RESET, content);
            break;
        case MSG_ERROR:
            printf("%s[Error]:%s %s\n", COLOR_RED, COLOR_RESET, content);
            break;
        case MSG_BROADCAST:
            printf("%s[%s]:%s %s\n", COLOR_BLUE, username, COLOR_RESET, content);
            break;
        case MSG_WHISPER:
            printf("%s[Whisper from %s]:%s %s\n", COLOR_MAGENTA, username, COLOR_RESET, content);
            break;
        case MSG_SERVER_SHUTDOWN:
            printf("%s[Server Shutdown]:%s %s\n", COLOR_YELLOW, COLOR_RESET, content);
            break;
        default:
            printf("[%s]: %s\n", username, content);
            break;
    }
}

void print_help(void) {
    printf("\n%sAvailable Commands:%s\n", COLOR_CYAN, COLOR_RESET);
    printf("  /join <room_name>           - Join or create a room\n");
    printf("  /leave                      - Leave the current room\n");
    printf("  /broadcast <message>        - Send message to everyone in the room\n");
    printf("  /whisper <username> <msg>   - Send private message to user\n");
    printf("  /sendfile <filename> <user> - Send file to user\n");
    printf("  /help                       - Show this help message\n");
    printf("  /exit                       - Disconnect from server\n");
    printf("\n%sFile restrictions:%s .txt, .pdf, .jpg, .png (max 3MB)\n", COLOR_YELLOW, COLOR_RESET);
    printf("%sUsername/Room rules:%s Alphanumeric only, max 16/32 chars\n\n", COLOR_YELLOW, COLOR_RESET);
}

int read_file_data(const char* filename, char** file_data, size_t* file_size) {
    FILE* file = fopen(filename, "rb");
    if (!file) {
        return -1;
    }
    
    // Get file size
    fseek(file, 0, SEEK_END);
    *file_size = ftell(file);
    fseek(file, 0, SEEK_SET);
    
    if (*file_size > MAX_FILE_SIZE) {
        fclose(file);
        return -2; // File too large
    }
    
    // Allocate memory and read file
    *file_data = malloc(*file_size);
    if (!*file_data) {
        fclose(file);
        return -3; // Memory allocation failed
    }
    
    size_t bytes_read = fread(*file_data, 1, *file_size, file);
    fclose(file);
    
    if (bytes_read != *file_size) {
        free(*file_data);
        *file_data = NULL;
        return -4; // Read error
    }
    
    return 0;
} 