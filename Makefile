.PHONY: all clean

PROJECT = yatka

$(PROJECT): main.c
	gcc -o $(PROJECT) main.c -lSDL -lSDL_image

all: $(PROJECT)

clean:
	rm -rf $(PROJECT)