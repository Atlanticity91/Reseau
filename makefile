CC = gcc
CFLAGS = -Wall -Wextra -g -pthread -std=c11 -Iinclude
LFLAGS = -pthread
SRC = net_utils.c net_socket.c server_udp.c client_udp.c server_tcp.c client_tcp.c
EXE = server_udp client_udp server_tcp client_tcp

SRC_DIR = src
OBJ_DIR = obj
BIN_DIR = bin

OBJ_LIST = $(SRC:%.c=$(OBJ_DIR)/%.o)
COM_LIST = $(OBJ_DIR)/net_utils.o $(OBJ_DIR)/net_socket.o

$(shell mkdir -p $(OBJ_DIR) $(BIN_DIR))

all: $(addprefix $(BIN_DIR)/, $(EXE))

$(BIN_DIR)/%: $(OBJ_DIR)/%.o $(COM_LIST)
	$(CC) -o $@ $^ $(LFLAGS)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) -c $(CFLAGS) $< -o $@

.PHONY: clean
clean:
	rm -rf $(OBJ_DIR) $(BIN_DIR)
