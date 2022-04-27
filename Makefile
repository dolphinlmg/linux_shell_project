CC=gcc
CFLAGS=-g -Wall

all: simple_myshell

simple_myshell: simple_myshell.c
	$(CC) $(CFLAGS) -o simple_myshell simple_myshell.c

clean: 
	rm -rf simple_myshell
