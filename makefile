all: newSpice

newSpice: code/main.c
	gcc code/main.c -o newSpice -lncurses

