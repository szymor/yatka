.PHONY: all clean purge

PROJECT = yatka
SRC = src/main.c src/data_persistence.c src/video.c src/sound.c \
	src/state_gameover.c src/state_settings.c src/randomizer.c \
	src/state_mainmenu.c src/skin.c
OBJ = $(SRC:.c=.o)
DEP = $(SRC:.c=.d)
PKGS = sdl SDL_image SDL_ttf SDL_mixer

COMMIT_HASH != git tag --points-at HEAD
ifeq ($(COMMIT_HASH), )
COMMIT_HASH != git rev-parse --short=7 HEAD
endif

$(shell git diff-index --quiet HEAD)
ifneq ($(.SHELLSTATUS),0)
COMMIT_HASH := $(COMMIT_HASH)_dirty
endif

CFLAGS = -Werror --pedantic $(shell pkg-config --cflags $(PKGS)) -g -Iinc -DDEV -DCOMMIT_HASH=$(COMMIT_HASH)
LDFLAGS = $(shell pkg-config --libs $(PKGS))
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
	rm -rf $(PROJECT) $(OBJ) src/*.d.*

purge: clean
	rm -rf $(DEP)

-include $(DEP)
