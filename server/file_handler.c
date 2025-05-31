#include "../common.h"

// File transfer queue
static FileTransfer upload_queue[MAX_UPLOAD_QUEUE];
static int queue_front = 0;
static int queue_rear = 0;
static int queue_size = 0;
static pthread_mutex_t queue_mutex = PTHREAD_MUTEX_INITIALIZER;
static sem_t* queue_semaphore = NULL;
static pthread_t file_worker_threads[MAX_UPLOAD_QUEUE];
static int file_handler_initialized = 0;

// Forward declarations
void* file_worker(void* arg);
int process_file_transfer(FileTransfer* transfer);

int init_file_handler(void) {
    if (file_handler_initialized) return 0;
    
    // Use named semaphore for better compatibility with Debian
    sem_unlink("/chatserver_queue_sem"); // Clean up any existing semaphore
    queue_semaphore = sem_open("/chatserver_queue_sem", O_CREAT | O_EXCL, 0644, MAX_UPLOAD_QUEUE);
    if (queue_semaphore == SEM_FAILED) {
        perror("Failed to create semaphore");
        return -1;
    }
    
    // Initialize worker threads
    for (int i = 0; i < MAX_UPLOAD_QUEUE; i++) {
        if (pthread_create(&file_worker_threads[i], NULL, file_worker, NULL) != 0) {
            perror("Failed to create file worker thread");
            return -1;
        }
    }
    
    file_handler_initialized = 1;
    log_message("[FILE-INIT] File transfer system initialized with %d workers", MAX_UPLOAD_QUEUE);
    return 0;
}

void cleanup_file_handler(void) {
    if (!file_handler_initialized) return;
    
    // Cancel all worker threads
    for (int i = 0; i < MAX_UPLOAD_QUEUE; i++) {
        pthread_cancel(file_worker_threads[i]);
        pthread_join(file_worker_threads[i], NULL);
    }
    
    // Clean up queue
    pthread_mutex_lock(&queue_mutex);
    for (int i = 0; i < queue_size; i++) {
        int index = (queue_front + i) % MAX_UPLOAD_QUEUE;
        if (upload_queue[index].file_data) {
            free(upload_queue[index].file_data);
        }
    }
    queue_size = 0;
    pthread_mutex_unlock(&queue_mutex);
    
    // Clean up semaphore
    if (queue_semaphore != NULL) {
        sem_close(queue_semaphore);
        sem_unlink("/chatserver_queue_sem");
        queue_semaphore = NULL;
    }
    
    file_handler_initialized = 0;
    log_message("[FILE-CLEANUP] File transfer system cleaned up");
}

int enqueue_file_transfer(const char* filename, const char* sender, const char* receiver, 
                         const char* file_data, size_t file_size) {
    if (!file_handler_initialized) {
        if (init_file_handler() != 0) {
            return -1;
        }
    }
    
    pthread_mutex_lock(&queue_mutex);
    
    if (queue_size >= MAX_UPLOAD_QUEUE) {
        pthread_mutex_unlock(&queue_mutex);
        log_message("[FILE-QUEUE] Upload queue full, rejecting file '%s' from %s", filename, sender);
        return -1; // Queue full
    }
    
    // Add to queue
    FileTransfer* transfer = &upload_queue[queue_rear];
    strncpy(transfer->filename, filename, MAX_FILENAME_LEN - 1);
    transfer->filename[MAX_FILENAME_LEN - 1] = '\0';
    strncpy(transfer->sender, sender, MAX_USERNAME_LEN);
    transfer->sender[MAX_USERNAME_LEN] = '\0';
    strncpy(transfer->receiver, receiver, MAX_USERNAME_LEN);
    transfer->receiver[MAX_USERNAME_LEN] = '\0';
    transfer->file_size = file_size;
    transfer->timestamp = time(NULL);
    
    // Allocate and copy file data
    transfer->file_data = malloc(file_size);
    if (!transfer->file_data) {
        pthread_mutex_unlock(&queue_mutex);
        log_message("[FILE-ERROR] Failed to allocate memory for file '%s'", filename);
        return -1;
    }
    memcpy(transfer->file_data, file_data, file_size);
    
    queue_rear = (queue_rear + 1) % MAX_UPLOAD_QUEUE;
    queue_size++;
    
    pthread_mutex_unlock(&queue_mutex);
    
    log_message("[FILE-QUEUE] Upload '%s' from %s to %s added to queue. Queue size: %d", 
                filename, sender, receiver, queue_size);
    
    // Signal that work is available
    if (queue_semaphore != NULL) {
        sem_post(queue_semaphore);
    }
    
    return 0;
}

FileTransfer* dequeue_file_transfer(void) {
    pthread_mutex_lock(&queue_mutex);
    
    if (queue_size == 0) {
        pthread_mutex_unlock(&queue_mutex);
        return NULL;
    }
    
    FileTransfer* transfer = malloc(sizeof(FileTransfer));
    if (!transfer) {
        pthread_mutex_unlock(&queue_mutex);
        return NULL;
    }
    
    // Copy transfer data
    *transfer = upload_queue[queue_front];
    
    // Clear the queue slot
    memset(&upload_queue[queue_front], 0, sizeof(FileTransfer));
    
    queue_front = (queue_front + 1) % MAX_UPLOAD_QUEUE;
    queue_size--;
    
    pthread_mutex_unlock(&queue_mutex);
    
    return transfer;
}

void* file_worker(void* arg) {
    (void)arg; // Suppress unused parameter warning
    
    while (1) {
        // Wait for work
        if (queue_semaphore != NULL && sem_wait(queue_semaphore) == 0) {
            FileTransfer* transfer = dequeue_file_transfer();
            if (transfer) {
                time_t wait_time = time(NULL) - transfer->timestamp;
                if (wait_time > 0) {
                    log_message("[FILE] '%s' from user '%s' started upload after %ld seconds in queue", 
                               transfer->filename, transfer->sender, wait_time);
                }
                
                int result = process_file_transfer(transfer);
                if (result == 0) {
                    log_message("[SEND FILE] '%s' sent from %s to %s (success)", 
                               transfer->filename, transfer->sender, transfer->receiver);
                } else {
                    log_message("[SEND FILE] '%s' from %s to %s (failed)", 
                               transfer->filename, transfer->sender, transfer->receiver);
                }
                
                if (transfer->file_data) {
                    free(transfer->file_data);
                }
                free(transfer);
            }
        } else {
            // If semaphore operation fails, break the loop
            break;
        }
    }
    
    return NULL;
}

int process_file_transfer(FileTransfer* transfer) {
    // Create uploads directory if it doesn't exist
    struct stat st = {0};
    if (stat("uploads", &st) == -1) {
        if (mkdir("uploads", 0755) != 0) {
            log_message("[FILE-ERROR] Failed to create uploads directory");
            return -1;
        }
    }
    
    // Create filename with path - increased buffer size for safety
    char filepath[1024];
    char original_filepath[1024];
    int ret = snprintf(filepath, sizeof(filepath), "uploads/%s_%s", transfer->receiver, transfer->filename);
    if (ret >= (int)sizeof(filepath)) {
        log_message("[FILE-ERROR] Filepath too long for file '%s'", transfer->filename);
        return -1;
    }
    
    // Check for filename conflicts and rename if necessary
    int counter = 1;
    strcpy(original_filepath, filepath);
    
    while (access(filepath, F_OK) == 0) {
        char* dot = strrchr(original_filepath, '.');
        if (dot) {
            *dot = '\0';
            ret = snprintf(filepath, sizeof(filepath), "%s_%d%s", original_filepath, counter, dot);
            *dot = '.'; // Restore original
        } else {
            ret = snprintf(filepath, sizeof(filepath), "%s_%d", original_filepath, counter);
        }
        
        if (ret >= (int)sizeof(filepath)) {
            log_message("[FILE-ERROR] Generated filepath too long for file '%s'", transfer->filename);
            return -1;
        }
        counter++;
    }
    
    if (strcmp(filepath, original_filepath) != 0) {
        log_message("[FILE] Conflict: '%s' renamed to avoid collision", transfer->filename);
    }
    
    // Write file
    FILE* file = fopen(filepath, "wb");
    if (!file) {
        log_message("[FILE-ERROR] Failed to create file '%s'", filepath);
        return -1;
    }
    
    size_t written = fwrite(transfer->file_data, 1, transfer->file_size, file);
    fclose(file);
    
    if (written != transfer->file_size) {
        log_message("[FILE-ERROR] Incomplete write for file '%s'", filepath);
        unlink(filepath); // Remove incomplete file
        return -1;
    }
    
    return 0;
}

int get_queue_size(void) {
    pthread_mutex_lock(&queue_mutex);
    int size = queue_size;
    pthread_mutex_unlock(&queue_mutex);
    return size;
} 