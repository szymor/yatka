
#include <stdio.h>
#include <stdbool.h>

#include <SDL/SDL.h>
#include <SDL/SDL_mixer.h>

#include "state_settings.h"
#include "main.h"
#include "video.h"
#include "sound.h"
#include "randomizer.h"

enum SettingsLine
{
	SL_TRACK_SELECT,
	SL_MUSIC_VOL,
	SL_MUSIC_REPEAT,
	SL_SMOOTHANIM,
	SL_TETROMINO_COLOR,
	SL_DEBRIS_COLOR,
	SL_EASYSPIN,
	SL_LOCKDELAY,
	SL_GHOST,
	SL_STATISTICS,
	SL_NEXT_NUMBER,
	SL_RANDOMIZER,
	SL_DEBUG,
	SL_END
};

bool settings_changed = false;

static bool redraw_bg = false;
static int settings_pos = 0;
static const char settings_text[][32] = {
	"  track selection           %d",
	"  music volume              %d",
	"  repeat mode               %s",
	"  smooth animation          %s",
	"  tetromino color           %s",
	"  debris color              %s",
	"  easy spin                 %s",
	"  fixed lock delay          %s",
	"  ghost opacity             %d",
	"  statistics mode           %s",
	"  no of next tetrominoes    %d",
	"  tetromino randomizer      %s",
	"  debug mode                %s"
};

static char *generateSettingLine(char *buff, int pos);

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
	rect.x = (screen->w) / 10;
	for (int i = 0; i < SL_END; ++i)
	{
		rect.y += text->h + spacing;
		text = TTF_RenderUTF8_Blended(arcade_font, generateSettingLine(buff, i), col);
		SDL_BlitSurface(text, NULL, screen, &rect);
		SDL_FreeSurface(text);
	}

	flipScreenScaled();
}

void settings_processInputEvents(void)
{
	SDL_Event event;

	if (SDL_WaitEvent(&event))
		switch (event.type)
		{
			case SDL_KEYDOWN:
				switch (event.key.keysym.sym)
				{
					case SDLK_UP:
					{
						decMod(&settings_pos, SL_END, false);
					} break;
					case SDLK_DOWN:
					{
						incMod(&settings_pos, SL_END, false);
					} break;
					case SDLK_LEFT:
					{
						switch (settings_pos)
						{
							case SL_TRACK_SELECT:
							{
								decMod(&current_track, MUSIC_TRACK_NUM, false);
								Mix_PlayMusic(music[current_track], 1);
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
							case SL_SMOOTHANIM:
							{
								smoothanim = !smoothanim;
								settings_changed = true;
							} break;
							case SL_TETROMINO_COLOR:
							{
								randomcolors = !randomcolors;
								settings_changed = true;
							} break;
							case SL_DEBRIS_COLOR:
							{
								grayblocks = !grayblocks;
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
							case SL_GHOST:
							{
								decMod(&ghostalpha, 256, true);
								settings_changed = true;
							} break;
							case SL_STATISTICS:
							{
								numericbars = !numericbars;
								settings_changed = true;
							} break;
							case SL_NEXT_NUMBER:
							{
								decMod(&nextblocks, MAX_NEXTBLOCKS + 1, true);
								redraw_bg = true;
								settings_changed = true;
							} break;
							case SL_RANDOMIZER:
							{
								decMod((int*)&randomalgo, RA_END, false);
								settings_changed = true;
							} break;
							case SL_DEBUG:
							{
								debug = !debug;
								settings_changed = true;
							} break;
							default:
								break;
						}
					} break;
					case SDLK_RIGHT:
					{
						switch (settings_pos)
						{
							case SL_TRACK_SELECT:
							{
								incMod(&current_track, MUSIC_TRACK_NUM, false);
								Mix_PlayMusic(music[current_track], 1);
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
							case SL_SMOOTHANIM:
							{
								smoothanim = !smoothanim;
								settings_changed = true;
							} break;
							case SL_TETROMINO_COLOR:
							{
								randomcolors = !randomcolors;
								settings_changed = true;
							} break;
							case SL_DEBRIS_COLOR:
							{
								grayblocks = !grayblocks;
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
							case SL_GHOST:
							{
								incMod(&ghostalpha, 256, true);
								settings_changed = true;
							} break;
							case SL_STATISTICS:
							{
								numericbars = !numericbars;
								settings_changed = true;
							} break;
							case SL_NEXT_NUMBER:
							{
								incMod(&nextblocks, MAX_NEXTBLOCKS + 1, true);
								redraw_bg = true;
								settings_changed = true;
							} break;
							case SL_RANDOMIZER:
							{
								incMod((int*)&randomalgo, RA_END, false);
								settings_changed = true;
							} break;
							case SL_DEBUG:
							{
								debug = !debug;
								settings_changed = true;
							} break;
							default:
								break;
						}
					} break;
					case KEY_QUIT:
					{
						SDL_Event ev;
						ev.type = SDL_QUIT;
						SDL_PushEvent(&ev);
					} break;
					case KEY_PAUSE:
						gamestate = GS_INGAME;
						if (redraw_bg)
						{
							initBackground();
							redraw_bg = false;
						}
						break;
				}
				break;
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
			sprintf(buff, settings_text[pos], current_track + 1);
		} break;
		case SL_MUSIC_VOL:
		{
			sprintf(buff, settings_text[pos], Mix_VolumeMusic(-1));
		} break;
		case SL_MUSIC_REPEAT:
		{
			sprintf(buff, settings_text[pos], repeattrack ? "track once" : "all");
		} break;
		case SL_SMOOTHANIM:
		{
			sprintf(buff, settings_text[pos], smoothanim ? "on" : "off");
		} break;
		case SL_TETROMINO_COLOR:
		{
			sprintf(buff, settings_text[pos], randomcolors ? "random" : "piecewise");
		} break;
		case SL_DEBRIS_COLOR:
		{
			sprintf(buff, settings_text[pos], grayblocks ? "gray" : "original");
		} break;
		case SL_EASYSPIN:
		{
			sprintf(buff, settings_text[pos], easyspin ? "on" : "off");
		} break;
		case SL_LOCKDELAY:
		{
			sprintf(buff, settings_text[pos], lockdelay ? "on" : "off");
		} break;
		case SL_GHOST:
		{
			sprintf(buff, settings_text[pos], ghostalpha);
		} break;
		case SL_STATISTICS:
		{
			sprintf(buff, settings_text[pos], numericbars ? "numbers" : "bars");
		} break;
		case SL_NEXT_NUMBER:
		{
			sprintf(buff, settings_text[pos], nextblocks);
		} break;
		case SL_RANDOMIZER:
		{
			sprintf(buff, settings_text[pos], getRandomizerString());
		} break;
		case SL_DEBUG:
		{
			sprintf(buff, settings_text[pos], debug ? "on" : "off");
		} break;
		default:
			break;
	}

	if (pos == settings_pos)
		buff[0] = '>';

	return buff;
}
