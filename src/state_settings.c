
#include <stdio.h>
#include <stdbool.h>

#include <SDL/SDL.h>
#include <SDL/SDL_mixer.h>

#include "state_settings.h"
#include "joystick.h"
#include "main.h"
#include "video.h"
#include "sound.h"
#include "randomizer.h"
#include "skin.h"

enum SettingsLine
{
	SL_TRACK_SELECT,
	SL_MUSIC_VOL,
	SL_MUSIC_REPEAT,
	SL_SPEECH,
	SL_SMOOTHANIM,
	SL_TETROMINO_COLOR,
	SL_EASYSPIN,
	SL_LOCKDELAY,
	SL_RANDOMIZER,
	SL_END
};

bool settings_changed = false;

static bool redraw_bg = false;
static int settings_pos = 0;
static const char settings_text[][32] = {
	"  track selection           %s",
	"  music volume              %d",
	"  repeat mode               %s",
	"  speech at line clear      %s",
	"  smooth animation          %s",
	"  tetromino color           %s",
	"  easy spin                 %s",
	"  fixed lock delay          %s",
	"  tetromino randomizer      %s",
};

static char *generateSettingLine(char *buff, int pos);

static void up(void);
static void down(void);
static void left(void);
static void right(void);
static void quit(void);
static void action(void);

void settings_updateScreen(void)
{
	SDL_Surface *mask = NULL;

	SDL_BlitSurface(last_game_screen, NULL, screen, NULL);
	SDL_PixelFormat *f = screen->format;
	mask = SDL_CreateRGBSurface(0, SCREEN_WIDTH, SCREEN_HEIGHT, f->BitsPerPixel, f->Rmask, f->Gmask, f->Bmask, 0);
	SDL_FillRect(mask, NULL, SDL_MapRGB(mask->format, 0, 0, 0));
	SDL_SetAlpha(mask, SDL_SRCALPHA, 128);
	SDL_BlitSurface(mask, NULL, screen, NULL);
	SDL_FreeSurface(mask);

	SDL_Color col = {.r = 255, .g = 255, .b = 255};
	SDL_Surface *text = NULL;
	SDL_Rect rect;
	char buff[256];
	int spacing = 2;

	sprintf(buff, "SETTINGS");
	text = TTF_RenderUTF8_Blended(arcade_font, buff, col);
	rect.x = (screen->w - text->w) / 2;
	rect.y = (screen->h) / 6;
	SDL_BlitSurface(text, NULL, screen, &rect);
	SDL_FreeSurface(text);

	rect.y += text->h + spacing;
	rect.x = 10;
	for (int i = 0; i < SL_END; ++i)
	{
		rect.y += text->h + spacing;
		text = TTF_RenderUTF8_Blended(arcade_font, generateSettingLine(buff, i), col);
		SDL_BlitSurface(text, NULL, screen, &rect);
		SDL_FreeSurface(text);
	}

	flipScreenScaled();
}

static void up(void)
{
	decMod(&settings_pos, SL_END, false);
}

static void down(void)
{
	incMod(&settings_pos, SL_END, false);
}

static void left(void)
{
	switch (settings_pos)
	{
		case SL_TRACK_SELECT:
		{
			playPrevTrack();
		} break;
		case SL_MUSIC_VOL:
		{
			int vol = Mix_VolumeMusic(-1);
			vol -= 1;
			if (vol < 0)
				vol = 0;
			Mix_VolumeMusic(vol);
			settings_changed = true;
		} break;
		case SL_MUSIC_REPEAT:
		{
			repeattrack = !repeattrack;
			settings_changed = true;
		} break;
		case SL_SPEECH:
		{
			speechon = !speechon;
			settings_changed = true;
		} break;
		case SL_SMOOTHANIM:
		{
			smoothanim = !smoothanim;
			settings_changed = true;
		} break;
		case SL_TETROMINO_COLOR:
		{
			decMod((int*)&tetrominocolor, TC_END, false);
			settings_changed = true;
		} break;
		case SL_EASYSPIN:
		{
			easyspin = !easyspin;
			settings_changed = true;
		} break;
		case SL_LOCKDELAY:
		{
			lockdelay = !lockdelay;
			settings_changed = true;
		} break;
		case SL_RANDOMIZER:
		{
			decMod((int*)&randomalgo, RA_END, false);
			randomizer_reset();
			settings_changed = true;
		} break;
		default:
			break;
	}
}

static void right(void)
{
	switch (settings_pos)
	{
		case SL_TRACK_SELECT:
		{
			playNextTrack();
		} break;
		case SL_MUSIC_VOL:
		{
			int vol = Mix_VolumeMusic(-1);
			vol += 1;
			Mix_VolumeMusic(vol);
			settings_changed = true;
		} break;
		case SL_MUSIC_REPEAT:
		{
			repeattrack = !repeattrack;
			settings_changed = true;
		} break;
		case SL_SPEECH:
		{
			speechon = !speechon;
			settings_changed = true;
		} break;
		case SL_SMOOTHANIM:
		{
			smoothanim = !smoothanim;
			settings_changed = true;
		} break;
		case SL_TETROMINO_COLOR:
		{
			incMod((int*)&tetrominocolor, TC_END, false);
			settings_changed = true;
		} break;
		case SL_EASYSPIN:
		{
			easyspin = !easyspin;
			settings_changed = true;
		} break;
		case SL_LOCKDELAY:
		{
			lockdelay = !lockdelay;
			settings_changed = true;
		} break;
		case SL_RANDOMIZER:
		{
			incMod((int*)&randomalgo, RA_END, false);
			randomizer_reset();
			settings_changed = true;
		} break;
		default:
			break;
	}
}

static void quit(void)
{
	gamestate = GS_MAINMENU;
}

static void action(void)
{
	gamestate = GS_INGAME;
	if (redraw_bg)
	{
		skin_updateBackground(&gameskin);
		redraw_bg = false;
	}
}

void settings_processInputEvents(void)
{
	SDL_Event event;

	if (SDL_WaitEvent(&event))
		switch (event.type)
		{
			case SDL_JOYAXISMOTION:
				if ((event.jaxis.value < -JOY_THRESHOLD) || (event.jaxis.value > JOY_THRESHOLD))
				{
					if(event.jaxis.axis == 0)
					{
						if (event.jaxis.value < 0)
							left();
						else
							right();
					}

					if(event.jaxis.axis == 1)
					{
						if (event.jaxis.value < 0)
							up();
						else
							down();
					}
				}
				break;
			case SDL_JOYBUTTONDOWN:
				if (event.jbutton.button == JOY_PAUSE)
				{
					action();
				}
				if (event.jbutton.button == JOY_QUIT)
				{
					quit();
				}
				break;
			case SDL_KEYDOWN:
				switch (event.key.keysym.sym)
				{
					case SDLK_UP:
					{
						up();
					} break;
					case SDLK_DOWN:
					{
						down();
					} break;
					case SDLK_LEFT:
					{
						left();
					} break;
					case SDLK_RIGHT:
					{
						right();
					} break;
					default:
					{
						if (event.key.keysym.sym == kquit)
						{
							quit();
						}
						else if (event.key.keysym.sym == kpause)
						{
							action();
						}
					}
				}
				break;
#ifdef DEV
			case SDL_MOUSEBUTTONDOWN:
				if (SDL_BUTTON_LEFT == event.button.button)
				{
					setBlockAtScreenXY(event.button.x, event.button.y, BO_FULL);
				}
				else if (SDL_BUTTON_RIGHT == event.button.button)
				{
					setBlockAtScreenXY(event.button.x, event.button.y, BO_EMPTY);
				}
				skin_updateScreen(&gameskin, last_game_screen);
				break;
			case SDL_MOUSEMOTION:
				// workaround for an apparent SDL bug
				event.button.state = SDL_GetMouseState(NULL, NULL);
				if (SDL_BUTTON_LMASK & event.button.state)
				{
					setBlockAtScreenXY(event.button.x, event.button.y, BO_FULL);
				}
				else if (SDL_BUTTON_RMASK & event.button.state)
				{
					setBlockAtScreenXY(event.button.x, event.button.y, BO_EMPTY);
				}
				skin_updateScreen(&gameskin, last_game_screen);
				break;
#endif
			case SDL_QUIT:
				exit(0);
				break;
		}
}

static char *generateSettingLine(char *buff, int pos)
{
	if (pos >= SL_END)
		return NULL;
	switch (pos)
	{
		case SL_TRACK_SELECT:
		{
			sprintf(buff, settings_text[pos], music_name);
		} break;
		case SL_MUSIC_VOL:
		{
			sprintf(buff, settings_text[pos], Mix_VolumeMusic(-1));
		} break;
		case SL_MUSIC_REPEAT:
		{
			sprintf(buff, settings_text[pos], repeattrack ? "track once" : "all");
		} break;
		case SL_SPEECH:
		{
			sprintf(buff, settings_text[pos], speechon ? "on" : "off");
		} break;
		case SL_SMOOTHANIM:
		{
			sprintf(buff, settings_text[pos], smoothanim ? "on" : "off");
		} break;
		case SL_TETROMINO_COLOR:
		{
			static char *tc_strings[] = {
				"random",
				"standard",
				"gray"
			};
			sprintf(buff, settings_text[pos], tc_strings[tetrominocolor]);
		} break;
		case SL_EASYSPIN:
		{
			sprintf(buff, settings_text[pos], easyspin ? "on" : "off");
		} break;
		case SL_LOCKDELAY:
		{
			sprintf(buff, settings_text[pos], lockdelay ? "on" : "off");
		} break;
		case SL_RANDOMIZER:
		{
			sprintf(buff, settings_text[pos], getRandomizerString());
		} break;
		default:
			break;
	}

	if (pos == settings_pos)
		buff[0] = '>';

	return buff;
}
