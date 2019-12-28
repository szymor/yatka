.PHONY: all clean

PROJECT = yatka
SRC = src/main.c src/hiscore.c src/video.c src/sound.c
OBJ = $(SRC:.c=.o)
DEP = $(SRC:.c=.d)
CFLAGS = -Iinc
LFLAGS = -s
CC = gcc

all: $(PROJECT)

$(PROJECT): $(OBJ)
	$(CC) $(LFLAGS) -o $(PROJECT) $(OBJ) -lSDL -lSDL_image -lSDL_ttf -lSDL_mixer

src/%.o: src/%.c
	$(CC) $(CFLAGS) -c -o $@ $<

src/%.d: src/%.c
	@set -e; \
	rm -f $@; \
	$(CC) -MM $(CFLAGS) $< > $@.$$$$; \
	sed 's,\($*\)\.o[ :]*,src/\1.o $@ : ,g' < $@.$$$$ > $@; \
	rm -f $@.$$$$

clean:
	rm -rf $(PROJECT) $(OBJ) $(DEP)

-include $(DEP)
