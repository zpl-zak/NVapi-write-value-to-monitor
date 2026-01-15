# Makefile for writeValueToDisplay (Linux)

CC = gcc
CFLAGS = -Wall -Wextra -O2
TARGET = writeValueToDisplay
SRC = writeValueToDisplay.c

.PHONY: all clean install

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $@ $^

install: $(TARGET)
	install -m 755 $(TARGET) /usr/local/bin/

clean:
	rm -f $(TARGET)
