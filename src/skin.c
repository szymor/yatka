#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include <SDL/SDL.h>
#include <SDL/SDL_image.h>
#include <SDL/SDL_ttf.h>

#include "skin.h"
#include "skin_lua.h"
#include "main.h"
#include "video.h"
#include "state_mainmenu.h"

void skin_initSkin(struct Skin *skin)
{
	skin->path = NULL;
	skin->boardx = 0;
	skin->boardy = 0;
	for (int i = 0; i < FONT_NUM; ++i)
		skin->fonts[i] = NULL;
	skin->screen = NULL;
	for (int i = 0; i < FIGID_END; ++i)
		skin->bricksprite[i] = NULL;
	for (int i = 0; i < FIGID_GRAY; ++i)
	{
		skin->color_alphas[i] = 128;
	}
	SDL_PixelFormat *f = screen->format;
	skin->colors[0] = SDL_MapRGB(f, 0, 159, 218);
	skin->colors[1] = SDL_MapRGB(f, 254, 203, 0);
	skin->colors[2] = SDL_MapRGB(f, 149, 45, 152);
	skin->colors[3] = SDL_MapRGB(f, 105, 190, 40);
	skin->colors[4] = SDL_MapRGB(f, 237, 41, 57);
	skin->colors[5] = SDL_MapRGB(f, 0, 101, 189);
	skin->colors[6] = SDL_MapRGB(f, 255, 121, 0);
	skin->brickstyle = BS_SIMPLE;
	skin->ghost = 128;
	skin->bricksize = 12;
	skin->brickyoffset = 0;
	skin->brick_shadow = NULL;
	skin->shadowx = 0;
	skin->shadowy = 0;
	skin->holdmode = HM_EXCHANGE;
	skin->is_lua = false;
	skin->L = NULL;
}

void skin_destroySkin(struct Skin *skin)
{
	if (skin->is_lua)
	{
		skin_lua_fini(skin);
		skin->is_lua = false;
	}
	if (skin->path)
	{
		free(skin->path);
		skin->path = NULL;
	}
	skin->boardx = 0;
	skin->boardy = 0;
	for (int i = 0; i < FONT_NUM; ++i)
	{
		if (skin->fonts[i])
		{
			TTF_CloseFont(skin->fonts[i]);
			skin->fonts[i] = NULL;
		}
	}
	skin->screen = NULL;
	for (int i = 0; i < FIGID_END; ++i)
	{
		if (skin->bricksprite[i])
		{
			SDL_FreeSurface(skin->bricksprite[i]);
			skin->bricksprite[i] = NULL;
		}
	}
	skin->brickstyle = BS_SIMPLE;
	skin->ghost = 128;
	if (skin->brick_shadow)
	{
		SDL_FreeSurface(skin->brick_shadow);
		skin->brick_shadow = NULL;
	}
}

void skin_loadSkin(struct Skin *skin, const char *path)
{
	skin_destroySkin(skin);

	/* extract skin directory from the path "skins/foo/skin.lua" */
	int totallen = strlen(path) + 1;
	skin->path = (char*)malloc(totallen);
	if (NULL == skin->path) exit(ERROR_MALLOC);
	strcpy(skin->path, path);
	char *ptr = skin->path + totallen - 1;
	while (*ptr != '/') { *(ptr--) = '\0'; }
	log("Skin path: %s\n", skin->path);

	skin->is_lua = true;
	skin_lua_init(skin, skin->path);
	log("Lua skin loaded.\n");
}

void skin_updateScreen(struct Skin *skin, SDL_Surface *screen)
{
	skin->screen = screen;

	/* smooth-drop interpolation (C side, passed to Lua) */
	int interp_y = 0;
	if (smoothanim)
	{
		Uint32 ct = SDL_GetTicks();
		double fraction;
		if (next_lock_time)
			fraction = (double)(ct - last_drop_time) / (double)(next_lock_time - last_drop_time);
		else
			fraction = (double)(ct - last_drop_time) / (double)(getNextDropTime() - last_drop_time);
		int new_delta = (int)(skin->bricksize * fraction) - skin->bricksize;
		if (new_delta > draw_delta_drop)
		{
			draw_delta_drop = new_delta;
			if (draw_delta_drop > 0) draw_delta_drop = 0;
		}
		interp_y = draw_delta_drop;
	}

	skin_lua_draw_background(skin);
	skin_lua_draw_shadow(skin);
	skin_lua_draw_board(skin);
	skin_lua_draw_active_figure(skin, interp_y);
	skin_lua_draw_ghost(skin);
	skin_lua_draw_foreground(skin);
	skin_lua_draw_hud(skin);
	skin_lua_draw_timed_texts(skin);

	flipScreenScaled();
	frameCounter();
}
