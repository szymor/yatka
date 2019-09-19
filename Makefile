.PHONY: all clean

PROJECT = yatka

$(PROJECT): main.c
	gcc -g -o $(PROJECT) main.c -lSDL -lSDL_image -lSDL_ttf -lSDL_mixer

all: $(PROJECT)

clean:
	rm -rf $(PROJECT)
