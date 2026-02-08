CC = gcc
CFLAGS = -std=c11 -Wall -Wextra -Wpedantic -g -I./src

SRCS = src/main.c src/utf8.c src/token.c src/lexer.c src/ast.c src/parser.c \
       src/value.c src/env.c src/gc.c src/eval.c src/builtins.c src/error.c \
       src/hashtable.c

OBJS = $(SRCS:.c=.o)
TARGET = ojisan

ifeq ($(OS),Windows_NT)
LDFLAGS = -lwinhttp
else
LDFLAGS =
endif

.PHONY: all clean test

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET) $(TARGET).exe

test: $(TARGET)
	@echo "Running basic tests..."
	./$(TARGET) examples/hello.oji
