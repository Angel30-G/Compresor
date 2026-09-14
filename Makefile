CC = gcc
CFLAGS = -Wall -O2 -Iinclude -Wno-deprecated-declarations
LDFLAGS = -lssl -lcrypto

SRC_DIR = src
BIN_DIR = bin

COMMON_SRCS = $(SRC_DIR)/huffman.c $(SRC_DIR)/md5_util.c
COMMON_OBJS = $(COMMON_SRCS:$(SRC_DIR)/%.c=$(BIN_DIR)/%.o)

TARGETS = $(BIN_DIR)/compresor_serial $(BIN_DIR)/gui

all: $(TARGETS)

$(BIN_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BIN_DIR)/compresor_serial: $(COMMON_OBJS) $(BIN_DIR)/main.o
	$(CC) $^ -o $@ $(LDFLAGS)

$(BIN_DIR)/gui: $(SRC_DIR)/gui.c $(COMMON_OBJS)
	$(CC) $(CFLAGS) `pkg-config --cflags gtk+-3.0` $^ -o $@ `pkg-config --libs gtk+-3.0` $(LDFLAGS)

clean:
	rm -f $(BIN_DIR)/*.o $(TARGETS)

run: $(BIN_DIR)/gui
	./$(BIN_DIR)/gui

.PHONY: all clean run
