CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -pthread -D_GNU_SOURCE
SERVER_DIR = server
CLIENT_DIR = client

# Target executables (must be exactly these names per requirements)
SERVER_TARGET = chatserver
CLIENT_TARGET = chatclient

# Source files
SERVER_SOURCES = $(SERVER_DIR)/server.c $(SERVER_DIR)/utils.c $(SERVER_DIR)/file_handler.c common_utils.c
CLIENT_SOURCES = $(CLIENT_DIR)/client.c $(CLIENT_DIR)/utils.c common_utils.c

# Object files
SERVER_OBJECTS = $(SERVER_SOURCES:.c=.o)
CLIENT_OBJECTS = $(CLIENT_SOURCES:.c=.o)

.PHONY: all clean server client

all: $(SERVER_TARGET) $(CLIENT_TARGET)

$(SERVER_TARGET): $(SERVER_OBJECTS)
	$(CC) $(CFLAGS) -o $@ $^

$(CLIENT_TARGET): $(CLIENT_OBJECTS)
	$(CC) $(CFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

server: $(SERVER_TARGET)

client: $(CLIENT_TARGET)

clean:
	rm -f $(SERVER_OBJECTS) $(CLIENT_OBJECTS) $(SERVER_TARGET) $(CLIENT_TARGET)
	rm -f server_log.txt common_utils.o
	rm -rf uploads/

install:
	mkdir -p $(SERVER_DIR) $(CLIENT_DIR)

.SUFFIXES: .c .o 