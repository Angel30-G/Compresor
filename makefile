cat > Makefile << 'EOF'
CC = gcc
CFLAGS = -Wall -O2 -I.
LDFLAGS = -lssl -lcrypto

TARGET = compresor

SRCS = main.c huffman.c md5_util.c
OBJS = $(SRCS:.c=.o)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(TARGET) $(OBJS)
EOF