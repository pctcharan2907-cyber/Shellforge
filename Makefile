CC=gcc
CFLAGS=-Wall -Wextra -std=c11 -Iinclude
SRC=src/lexer.c src/parser.c src/expand.c src/builtin.c src/main.c src/token.c
TARGET=shellforge

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) $(SRC) -lreadline -o $(TARGET)

clean:
	rm -f $(TARGET)
