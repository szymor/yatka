.PHONY: all clean

PROJECT = yatka
SRC = src/main.c src/hiscore.c src/video.c src/sound.c \
	src/state_gameover.c src/state_settings.c src/randomizer.c
OBJ = ${SRC:.c=.o}
DEP = ${SRC:.c=.d}
CFLAGS = -Iinc $(shell pkg-config --cflags SDL SDL_image SDL_ttf SDL_mixer)
LFLAGS = -s $(shell pkg-config --libs SDL SDL_image SDL_ttf SDL_mixer)
CC = gcc

all: ${PROJECT}

${PROJECT}: ${OBJ}
	${CC} ${LFLAGS} -o ${PROJECT} ${OBJ}

src/%.o: src/%.c
	${CC} ${CFLAGS} -c -o $@ $<

src/%.d: src/%.c
	@set -e; \
	rm -f $@; \
	${CC} -MM ${CFLAGS} $< > $@.$$$$; \
	sed 's,\($*\}\.o[ :]*,src/\1.o $@ : ,g' < $@.$$$$ > $@; \
	rm -f $@.$$$$

clean:
	rm -rf ${PROJECT} ${OBJ} ${DEP}

-include ${DEP}
