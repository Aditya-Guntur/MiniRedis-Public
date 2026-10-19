CC = gcc
CFLAGS = -Wall -Wextra -Werror -O2 -std=c2x
LDFLAGS =
TARGET = tree
SOURCES = tree.c command_handler.c
OBJECTS = $(SOURCES:.c=.o)

all: clean $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJECTS) $(TARGET)
