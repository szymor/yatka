.PHONY: all clean

PROJECT = yatka
SRC = src/main.c src/data_persistence.c src/video.c src/sound.c \
	src/state_gameover.c src/state_settings.c src/randomizer.c \
	src/state_mainmenu.c src/skin.c
OBJ = $(SRC:.c=.o)
DEP = $(SRC:.c=.d)
CFLAGS = -g -Iinc -DDEV
LDFLAGS = $(shell pkg-config --libs sdl SDL_image SDL_ttf SDL_mixer)
CC = gcc

all: $(PROJECT)

$(PROJECT): $(OBJ)
	$(CC) -o $(PROJECT) $(OBJ) $(LDFLAGS)

src/%.o: src/%.c
	$(CC) $(CFLAGS) -c -o $@ $<

src/%.d: src/%.c
	@set -e; \
	rm -f $@; \
	$(CC) -MM $(CFLAGS) $< > $@.$$$$; \
	sed 's,\($*\)\.o[ :]*,src/\1.o $@ : ,g' < $@.$$$$ > $@; \
	rm -f $@.$$$$

clean:
	rm -rf $(PROJECT) $(OBJ) $(DEP) src/*.d.*

-include $(DEP)
