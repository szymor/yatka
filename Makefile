.PHONY: all clean

PROJECT = yatka

all: main.c
	gcc -o $(PROJECT) main.c -lSDL

clean:
	rm -rf $(PROJECT)
