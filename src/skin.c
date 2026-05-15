#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <SDL/SDL.h>
#include <SDL/SDL_image.h>
#include <SDL/SDL_ttf.h>

#include <lua5.4/lua.h>
#include <lua5.4/lauxlib.h>
#include <lua5.4/lualib.h>

#include "skin.h"
#include "main.h"
#include "state_mainmenu.h"
#include "video.h"

/* ─────────────────────────────────────────────
 * Constants
 * ───────────────────────────────────────────── */

static const char *MT_SURFACE = "y_surface";
static const char *MT_FONT    = "y_font";

/* ─────────────────────────────────────────────
 * Lua helper: push file content as string
 * ───────────────────────────────────────────── */

static int push_file_as_string(lua_State *L, const char *path)
{
	FILE *f = fopen(path, "rb");
	if (!f)
		return luaL_error(L, "cannot open %s", path);

	fseek(f, 0, SEEK_END);
	long len = ftell(f);
	rewind(f);

	char *buf = malloc((size_t)len + 1);
	if (!buf)
	{
		fclose(f);
		return luaL_error(L, "out of memory");
	}
	size_t rlen = fread(buf, 1, (size_t)len, f);
	buf[rlen] = '\0';
	fclose(f);

	lua_pushlstring(L, buf, rlen);
	free(buf);
	return 1;
}

/* ─────────────────────────────────────────────
 * Light‑userdata helpers for SDL objects
 * ───────────────────────────────────────────── */

static SDL_Surface **check_surface(lua_State *L, int idx)
{
	return (SDL_Surface **)luaL_checkudata(L, idx, MT_SURFACE);
}

static TTF_Font **check_font(lua_State *L, int idx)
{
	return (TTF_Font **)luaL_checkudata(L, idx, MT_FONT);
}

/* ─────────────────────────────────────────────
 * C functions exposed to Lua via `res` table
 * ───────────────────────────────────────────── */

static int y_load_image(lua_State *L)
{
	const char *path = luaL_checkstring(L, 1);
	SDL_Surface *img = IMG_Load(path);
	if (!img)
		return luaL_error(L, "IMG_Load(%s) failed", path);
	SDL_Surface *opt = SDL_DisplayFormat(img);
	SDL_FreeSurface(img);
	if (!opt)
		return luaL_error(L, "SDL_DisplayFormat failed");

	SDL_Surface **ud = (SDL_Surface **)lua_newuserdata(L, sizeof(SDL_Surface *));
	*ud = opt;
	luaL_setmetatable(L, MT_SURFACE);
	return 1;
}

static int y_load_font(lua_State *L)
{
	const char *path = luaL_checkstring(L, 1);
	int size = luaL_checkinteger(L, 2);
	TTF_Font *font = TTF_OpenFont(path, size);
	if (!font)
		return luaL_error(L, "TTF_OpenFont(%s) failed", path);

	TTF_Font **ud = (TTF_Font **)lua_newuserdata(L, sizeof(TTF_Font *));
	*ud = font;
	luaL_setmetatable(L, MT_FONT);
	return 1;
}

static int y_surface_gc(lua_State *L)
{
	SDL_Surface **ud = check_surface(L, 1);
	if (*ud) { SDL_FreeSurface(*ud); *ud = NULL; }
	return 0;
}

static int y_font_gc(lua_State *L)
{
	TTF_Font **ud = check_font(L, 1);
	if (*ud) { TTF_CloseFont(*ud); *ud = NULL; }
	return 0;
}

static int y_draw_image(lua_State *L)
{
	SDL_Surface **ud = check_surface(L, 1);
	int x = luaL_checkinteger(L, 2);
	int y = luaL_checkinteger(L, 3);
	if (!*ud) return 0;
	SDL_Rect dst = { .x = x, .y = y };
	SDL_BlitSurface(*ud, NULL, screen, &dst);
	return 0;
}

static int y_draw_text(lua_State *L)
{
	TTF_Font **fud = check_font(L, 1);
	const char *str = luaL_checkstring(L, 2);
	int x = luaL_checkinteger(L, 3);
	int y = luaL_checkinteger(L, 4);
	int r = (int)luaL_optinteger(L, 5, 255);
	int g = (int)luaL_optinteger(L, 6, 255);
	int b = (int)luaL_optinteger(L, 7, 255);
	int alignx = (int)luaL_optinteger(L, 8, 0);
	int aligny = (int)luaL_optinteger(L, 9, 0);

	if (!*fud) return 0;
	if (!str || !*str) return 0;

	SDL_Color col = { .r = r, .g = g, .b = b };
	SDL_Surface *ts = TTF_RenderUTF8_Blended(*fud, str, col);
	if (!ts) return 0;

	SDL_Rect dst = { .x = x, .y = y };
	if (alignx == 1) dst.x -= ts->w / 2;
	else if (alignx == 2) dst.x -= ts->w;
	if (aligny == 1) dst.y -= ts->h / 2;
	else if (aligny == 2) dst.y -= ts->h;

	SDL_BlitSurface(ts, NULL, screen, &dst);
	SDL_FreeSurface(ts);
	return 0;
}

static int y_draw_rect(lua_State *L)
{
	int x = luaL_checkinteger(L, 1);
	int y = luaL_checkinteger(L, 2);
	int w = luaL_checkinteger(L, 3);
	int h = luaL_checkinteger(L, 4);
	int r = (int)luaL_checkinteger(L, 5);
	int g = (int)luaL_checkinteger(L, 6);
	int b = (int)luaL_checkinteger(L, 7);
	int a = (int)luaL_checkinteger(L, 8);

	SDL_Surface *mask = SDL_CreateRGBSurface(0, w, h, 32,
						 0x000000ff, 0x0000ff00, 0x00ff0000, 0xff000000);
	if (!mask) return 0;
	SDL_FillRect(mask, NULL, SDL_MapRGBA(mask->format, r, g, b, a));
	SDL_Rect dst = { .x = x, .y = y };
	SDL_BlitSurface(mask, NULL, screen, &dst);
	SDL_FreeSurface(mask);
	return 0;
}

static int y_draw_bar(lua_State *L)
{
	int x = luaL_checkinteger(L, 1);
	int y = luaL_checkinteger(L, 2);
	int w = luaL_checkinteger(L, 3);
	int h = luaL_checkinteger(L, 4);
	int val = luaL_checkinteger(L, 5);
	int maxv = luaL_checkinteger(L, 6);
	int dir = luaL_checkinteger(L, 7);
	int rl = (int)luaL_checkinteger(L, 8), gl = (int)luaL_checkinteger(L, 9);
	int bl = (int)luaL_checkinteger(L, 10), al = (int)luaL_checkinteger(L, 11);
	int rr = (int)luaL_checkinteger(L, 12), gr = (int)luaL_checkinteger(L, 13);
	int br = (int)luaL_checkinteger(L, 14), ar = (int)luaL_checkinteger(L, 15);

	if (val > maxv) val = maxv;
	if (maxv < 1) maxv = 1;

	SDL_Surface *bar = SDL_CreateRGBSurface(SDL_SRCALPHA, w, h, 32,
						0x000000ff, 0x0000ff00, 0x00ff0000, 0xff000000);
	if (!bar) return 0;

	SDL_Rect fill = { 0, 0, 0, 0 };
	SDL_Rect rest = { 0, 0, 0, 0 };
	int filled_w = val * w / maxv;

	switch (dir)
	{
		case 0: fill = (SDL_Rect){ 0, 0, filled_w, h };
		        rest = (SDL_Rect){ filled_w, 0, w - filled_w, h }; break;
		case 1: fill = (SDL_Rect){ w - filled_w, 0, filled_w, h };
		        rest = (SDL_Rect){ 0, 0, w - filled_w, h }; break;
		case 2: fill = (SDL_Rect){ 0, 0, w, val * h / maxv };
		        rest = (SDL_Rect){ 0, val * h / maxv, w, h - val * h / maxv }; break;
		case 3: fill = (SDL_Rect){ 0, h - val * h / maxv, w, val * h / maxv };
		        rest = (SDL_Rect){ 0, 0, w, h - val * h / maxv }; break;
	}
	SDL_FillRect(bar, &fill, SDL_MapRGBA(bar->format, rl, gl, bl, al));
	SDL_FillRect(bar, &rest, SDL_MapRGBA(bar->format, rr, gr, br, ar));

	SDL_Rect dst = { .x = x, .y = y, .w = w, .h = h };
	SDL_BlitSurface(bar, NULL, screen, &dst);
	SDL_FreeSurface(bar);
	return 0;
}

static int y_screen_w(lua_State *L)
{
	lua_pushinteger(L, SCREEN_WIDTH);
	return 1;
}

static int y_screen_h(lua_State *L)
{
	lua_pushinteger(L, SCREEN_HEIGHT);
	return 1;
}

/* res.brick_size() → w, h */
static int y_brick_size(lua_State *L)
{
	struct Skin *skin = (struct Skin *)lua_touserdata(L, lua_upvalueindex(1));
	lua_pushinteger(L, skin->bricksize);
	lua_pushinteger(L, skin->bricksize + skin->brickyoffset);
	return 2;
}

/* res.draw_brick(x, y, color, orient, alpha) */
static int y_draw_brick(lua_State *L)
{
	struct Skin *skin = (struct Skin *)lua_touserdata(L, lua_upvalueindex(1));
	int x = luaL_checkinteger(L, 1);
	int y = luaL_checkinteger(L, 2);
	int color = luaL_checkinteger(L, 3);
	int orient = luaL_checkinteger(L, 4);
	int alpha = (int)luaL_optinteger(L, 5, 255);

	if (color < 0 || color >= FIGID_END)
		return 0;
	if (!skin->bricksprite[color])
		return 0;

	SDL_Rect srcrect = { .x = 0, .y = 0,
		.w = skin->bricksize,
		.h = skin->bricksize + skin->brickyoffset };

	SDL_Surface *block = skin->bricksprite[color];
	switch (skin->brickstyle)
	{
		case BS_ORIENTATION_BASED:
			srcrect.x = orient * srcrect.w - srcrect.w;
			break;
		case BS_FIGUREWISE:
			srcrect.y = (color % FIGID_GRAY) * srcrect.h;
			break;
		default: break;
	}

	SDL_Rect dst = { .x = x, .y = y };
	SDL_SetAlpha(block, SDL_SRCALPHA, (Uint8)alpha);
	SDL_BlitSurface(block, &srcrect, screen, &dst);
	return 0;
}

/* ─────────────────────────────────────────────
 * Setter functions (registered in res table)
 * ───────────────────────────────────────────── */

static int y_set_brick_size(lua_State *L)
{
	struct Skin *skin = (struct Skin *)lua_touserdata(L, lua_upvalueindex(1));
	int w = luaL_checkinteger(L, 1);
	int h_off = (int)luaL_optinteger(L, 2, 0);
	skin->bricksize = w;
	skin->brickyoffset = h_off;
	brick_size = w;
	draw_delta_drop = -w;
	return 0;
}

static int y_set_board_xy(lua_State *L)
{
	struct Skin *skin = (struct Skin *)lua_touserdata(L, lua_upvalueindex(1));
	skin->boardx = luaL_checkinteger(L, 1);
	skin->boardy = luaL_checkinteger(L, 2);
	return 0;
}

static int y_set_holdmode(lua_State *L)
{
	struct Skin *skin = (struct Skin *)lua_touserdata(L, lua_upvalueindex(1));
	const char *mode = luaL_checkstring(L, 1);
	if      (!strcmp(mode, "off"))       skin->holdmode = HM_OFF;
	else if (!strcmp(mode, "preserve"))  skin->holdmode = HM_PRESERVE;
	else                                 skin->holdmode = HM_EXCHANGE;
	return 0;
}

static int y_set_tc(lua_State *L)
{
	struct Skin *skin = (struct Skin *)lua_touserdata(L, lua_upvalueindex(1));
	int id = luaL_checkinteger(L, 1);
	int alpha = luaL_checkinteger(L, 2);
	int r = luaL_checkinteger(L, 3);
	int g = luaL_checkinteger(L, 4);
	int b = luaL_checkinteger(L, 5);
	if (id < 0 || id >= FIGID_GRAY) return 0;
	skin->color_alphas[id] = alpha;
	skin->colors[id] = SDL_MapRGB(screen->format, r, g, b);
	return 0;
}

static int y_set_bricksprite(lua_State *L)
{
	struct Skin *skin = (struct Skin *)lua_touserdata(L, lua_upvalueindex(1));
	SDL_Surface **ud = check_surface(L, 1);
	if (!*ud) return luaL_error(L, "set_bricksprite: null image");

	/* free any existing brick sprites */
	for (int i = 0; i < FIGID_END; ++i)
	{
		if (skin->bricksprite[i])
		{
			SDL_FreeSurface(skin->bricksprite[i]);
			skin->bricksprite[i] = NULL;
		}
	}

	int s = skin->bricksize;
	int oy = skin->brickyoffset;

	/* determine brick style from dimensions */
	skin->brickstyle = BS_SIMPLE;
	if (((s + oy) == (*ud)->h) && (ORIENTATION_NUM * s == (*ud)->w))
		skin->brickstyle = BS_ORIENTATION_BASED;
	else if ((FIGID_GRAY * (s + oy) == (*ud)->h) && (s == (*ud)->w))
		skin->brickstyle = BS_FIGUREWISE;

	/* store the gray sprite and tint into 7 colored variants */
	skin->bricksprite[FIGID_GRAY] = *ud;  /* transfer ownership */
	*ud = NULL;  /* prevent Lua gc from freeing it */

	SDL_PixelFormat *fmt = screen->format;
	int bw = skin->bricksprite[FIGID_GRAY]->w;
	int bh = skin->bricksprite[FIGID_GRAY]->h;

	for (int i = 0; i < FIGID_GRAY; ++i)
	{
		SDL_SetAlpha(skin->bricksprite[FIGID_GRAY], SDL_SRCALPHA,
			     (Uint8)skin->color_alphas[i]);
		skin->bricksprite[i] = SDL_CreateRGBSurface(0, bw, bh,
			fmt->BitsPerPixel, fmt->Rmask, fmt->Gmask, fmt->Bmask, 0);
		SDL_FillRect(skin->bricksprite[i], NULL, skin->colors[i]);
		SDL_BlitSurface(skin->bricksprite[FIGID_GRAY], NULL,
				skin->bricksprite[i], NULL);
	}
	SDL_SetAlpha(skin->bricksprite[FIGID_GRAY], SDL_SRCALPHA, 128);
	return 0;
}

static int y_set_shadow(lua_State *L)
{
	struct Skin *skin = (struct Skin *)lua_touserdata(L, lua_upvalueindex(1));
	int offx = luaL_checkinteger(L, 1);
	int offy = luaL_checkinteger(L, 2);
	int r = (int)luaL_checkinteger(L, 3);
	int g = (int)luaL_checkinteger(L, 4);
	int b = (int)luaL_checkinteger(L, 5);
	int a = (int)luaL_checkinteger(L, 6);

	if (skin->brick_shadow)
	{
		SDL_FreeSurface(skin->brick_shadow);
		skin->brick_shadow = NULL;
	}

	skin->shadowx = offx;
	skin->shadowy = offy;

	int bw = skin->bricksize;
	int bh = skin->bricksize + skin->brickyoffset;
	skin->brick_shadow = SDL_CreateRGBSurface(0, bw, bh, 32,
		0x000000ff, 0x0000ff00, 0x00ff0000, 0xff000000);
	if (!skin->brick_shadow) return 0;
	SDL_FillRect(skin->brick_shadow, NULL,
		     SDL_MapRGBA(skin->brick_shadow->format, r, g, b, a));
	SDL_SetAlpha(skin->brick_shadow, SDL_SRCALPHA, a);
	return 0;
}

static int y_set_ghost_alpha(lua_State *L)
{
	struct Skin *skin = (struct Skin *)lua_touserdata(L, lua_upvalueindex(1));
	skin->ghost = (int)luaL_checkinteger(L, 1);
	return 0;
}

/* res.show_timed_text(x, y, text, timeout_ms [, font, r, g, b, ax, ay]) */
static int y_show_timed_text(lua_State *L)
{
	struct Skin *skin = (struct Skin *)lua_touserdata(L, lua_upvalueindex(1));
	int x = luaL_checkinteger(L, 1);
	int y = luaL_checkinteger(L, 2);
	const char *text = luaL_checkstring(L, 3);
	int timeout = luaL_checkinteger(L, 4);

	if (timeout <= 0 || !text || !*text) return 0;

	/* find a free slot */
	int slot = -1;
	Uint32 now = SDL_GetTicks();
	for (int i = 0; i < TIMED_TEXT_MAX; ++i)
	{
		if (skin->timed_texts[i].deadline <= now)
		{
			slot = i;
			break;
		}
	}
	if (slot < 0) return 0;  /* all slots busy */

	struct TimedText *tt = &skin->timed_texts[slot];
	strncpy(tt->text, text, TIMED_TEXT_LEN - 1);
	tt->text[TIMED_TEXT_LEN - 1] = '\0';
	tt->x = x;
	tt->y = y;
	tt->deadline = now + timeout;

	/* optional font: check if arg 5 is a font userdata */
	tt->font = NULL;
	if (lua_gettop(L) >= 5)
	{
		TTF_Font **fud = (TTF_Font **)luaL_testudata(L, 5, MT_FONT);
		if (fud) tt->font = *fud;
	}

	tt->r = (int)luaL_optinteger(L, 6, 255);
	tt->g = (int)luaL_optinteger(L, 7, 255);
	tt->b = (int)luaL_optinteger(L, 8, 255);
	tt->alignx = (int)luaL_optinteger(L, 9, 1);
	tt->aligny = (int)luaL_optinteger(L, 10, 0);
	return 0;
}

/* ─────────────────────────────────────────────
 * Board / game / figure data access for Lua
 * ───────────────────────────────────────────── */

static void push_figure_cells(lua_State *L, const struct Shape *shape)
{
	lua_newtable(L);
	int n = 1;
	for (int i = 0; i < FIG_DIM * FIG_DIM; ++i)
	{
		if (shape->blockmap[i] != BO_EMPTY)
		{
			lua_newtable(L);
			lua_pushinteger(L, i % FIG_DIM);        lua_setfield(L, -2, "x");
			lua_pushinteger(L, i / FIG_DIM);        lua_setfield(L, -2, "y");
			lua_pushinteger(L, shape->blockmap[i]); lua_setfield(L, -2, "orient");
			lua_rawseti(L, -2, n++);
		}
	}
}

static void push_figure(lua_State *L, const struct Figure *fig, bool with_pos)
{
	if (!fig || fig->id >= FIGID_GRAY)
	{
		lua_pushnil(L);
		return;
	}
	lua_newtable(L);
	lua_pushinteger(L, (int)fig->id);    lua_setfield(L, -2, "id");
	lua_pushinteger(L, (int)fig->color); lua_setfield(L, -2, "color");
	if (with_pos)
	{
		lua_pushinteger(L, fig->x);      lua_setfield(L, -2, "x");
		lua_pushinteger(L, fig->y);      lua_setfield(L, -2, "y");
		lua_pushinteger(L, fig->shape.phase); lua_setfield(L, -2, "phase");
	}
	push_figure_cells(L, &fig->shape);
	lua_setfield(L, -2, "cells");
}

/* board.get(x,y) → {color,orient} or nil */
static int y_board_get(lua_State *L)
{
	int x = luaL_checkinteger(L, 1);
	int y = luaL_checkinteger(L, 2);
	if (x < 0 || x >= BOARD_WIDTH || y < 0 || y >= BOARD_HEIGHT)
	{ lua_pushnil(L); return 1; }
	int idx = y * BOARD_WIDTH + x;
	if (board[idx].orientation == BO_EMPTY)
	{ lua_pushnil(L); return 1; }
	lua_newtable(L);
	lua_pushinteger(L, board[idx].color);       lua_setfield(L, -2, "color");
	lua_pushinteger(L, board[idx].orientation); lua_setfield(L, -2, "orient");
	return 1;
}

static int y_game_score(lua_State *L)  { lua_pushinteger(L, score); return 1; }
static int y_game_level(lua_State *L)  { lua_pushinteger(L, level); return 1; }
static int y_game_lines(lua_State *L)  { lua_pushinteger(L, lines); return 1; }
static int y_game_dropped(lua_State *L) { lua_pushinteger(L, dropped_pieces_num); return 1; }
static int y_game_pressed(lua_State *L) { lua_pushinteger(L, pressed_keys_num); return 1; }
static int y_game_stat(lua_State *L)
{
	int id = luaL_checkinteger(L, 1);
	if (id < 0 || id >= FIGID_GRAY) id = 0;
	lua_pushinteger(L, statistics[id]);
	return 1;
}
static int y_game_mode(lua_State *L)
{
	static const char *names[] = { "marathon", "sprint", "ultra" };
	lua_pushstring(L, names[menu_gamemode]);
	return 1;
}
static int y_game_timer_str(lua_State *L) { lua_pushstring(L, gametimer); return 1; }
static int y_game_fps(lua_State *L) { lua_pushinteger(L, fps); return 1; }
static int y_game_ticks(lua_State *L) { lua_pushinteger(L, SDL_GetTicks()); return 1; }
static int y_game_ghost_y(lua_State *L)
{
	if (!figures[0]) { lua_pushnil(L); return 1; }
	int tfy = figures[0]->y;
	int steps = 0;
	while (!isFigureColliding()) {
		++figures[0]->y;
		++steps;
	}
	if (steps > 0)
		--figures[0]->y;
	int ghost_y = figures[0]->y;
	figures[0]->y = tfy;
	lua_pushinteger(L, ghost_y);
	return 1;
}
static int y_game_shape_cells(lua_State *L)
{
	int id = luaL_checkinteger(L, 1);
	if (id < 0 || id >= FIGID_GRAY) { lua_pushnil(L); return 1; }
	const struct Shape *shape = getShape((enum FigureId)id);
	if (!shape) { lua_pushnil(L); return 1; }
	push_figure_cells(L, shape);
	return 1;
}

static int y_figure_active(lua_State *L)  { push_figure(L, figures[0], true);  return 1; }
static int y_figure_next(lua_State *L)
{
	int n = luaL_checkinteger(L, 1);
	if (n < 1 || n >= FIG_NUM) { lua_pushnil(L); return 1; }
	push_figure(L, figures[n], false);
	return 1;
}
static int y_figure_held(lua_State *L) { push_figure(L, &preserved, false); return 1; }

/* ─────────────────────────────────────────────
 * Lua‑state initialisation
 * ───────────────────────────────────────────── */

static const luaL_Reg ylib[] = {
	{ "load_image",  y_load_image  },
	{ "load_font",   y_load_font   },
	{ "draw_image",  y_draw_image  },
	{ "draw_text",   y_draw_text   },
	{ "draw_rect",   y_draw_rect   },
	{ "draw_bar",    y_draw_bar    },
	{ "screen_w",    y_screen_w    },
	{ "screen_h",    y_screen_h    },
	{ NULL, NULL }
};

static void skin_lua_init(struct Skin *skin, const char *skin_path)
{
	lua_State *L = luaL_newstate();
	if (!L)
	{
		fprintf(stderr, "luaL_newstate failed\n");
		return;
	}
	luaL_openlibs(L);
	skin->L = L;

	/* create metatables for gc */
	luaL_newmetatable(L, MT_SURFACE);
	lua_pushcfunction(L, y_surface_gc);
	lua_setfield(L, -2, "__gc");
	lua_pop(L, 1);

	luaL_newmetatable(L, MT_FONT);
	lua_pushcfunction(L, y_font_gc);
	lua_setfield(L, -2, "__gc");
	lua_pop(L, 1);

	/* create the `res` table with C functions + skin upvalue */
	lua_pushlightuserdata(L, skin);
	lua_pushcclosure(L, y_draw_brick, 1);
	lua_setglobal(L, "y_draw_brick");

	lua_pushlightuserdata(L, skin);
	lua_pushcclosure(L, y_brick_size, 1);
	lua_setglobal(L, "y_brick_size");

	lua_newtable(L);
	luaL_setfuncs(L, ylib, 0);

	lua_getglobal(L, "y_draw_brick");
	lua_setfield(L, -2, "draw_brick");
	lua_getglobal(L, "y_brick_size");
	lua_setfield(L, -2, "brick_size");

	lua_setglobal(L, "res");

	/* add setter functions with skin upvalue into res table */
	lua_getglobal(L, "res");

	lua_pushlightuserdata(L, skin);
	lua_pushcclosure(L, y_set_brick_size, 1);
	lua_setfield(L, -2, "set_brick_size");

	lua_pushlightuserdata(L, skin);
	lua_pushcclosure(L, y_set_board_xy, 1);
	lua_setfield(L, -2, "set_board_xy");

	lua_pushlightuserdata(L, skin);
	lua_pushcclosure(L, y_set_holdmode, 1);
	lua_setfield(L, -2, "set_holdmode");

	lua_pushlightuserdata(L, skin);
	lua_pushcclosure(L, y_set_tc, 1);
	lua_setfield(L, -2, "set_tetromino_color");

	lua_pushlightuserdata(L, skin);
	lua_pushcclosure(L, y_set_bricksprite, 1);
	lua_setfield(L, -2, "set_bricksprite");

	lua_pushlightuserdata(L, skin);
	lua_pushcclosure(L, y_set_shadow, 1);
	lua_setfield(L, -2, "set_shadow");

	lua_pushlightuserdata(L, skin);
	lua_pushcclosure(L, y_set_ghost_alpha, 1);
	lua_setfield(L, -2, "set_ghost_alpha");

	lua_pushlightuserdata(L, skin);
	lua_pushcclosure(L, y_show_timed_text, 1);
	lua_setfield(L, -2, "show_timed_text");

	/* add skin path to res table */
	lua_pushstring(L, skin_path);
	lua_setfield(L, -2, "skin_path");

	lua_pop(L, 1);  /* pop res */

	/* ─── board table ─── */
	lua_newtable(L);
	lua_pushinteger(L, BOARD_WIDTH);  lua_setfield(L, -2, "width");
	lua_pushinteger(L, BOARD_HEIGHT); lua_setfield(L, -2, "height");
	lua_pushcfunction(L, y_board_get); lua_setfield(L, -2, "get");
	lua_setglobal(L, "board");

	/* ─── game table ─── */
	lua_newtable(L);
	lua_pushcfunction(L, y_game_score);    lua_setfield(L, -2, "score");
	lua_pushcfunction(L, y_game_level);    lua_setfield(L, -2, "level");
	lua_pushcfunction(L, y_game_lines);    lua_setfield(L, -2, "lines");
	lua_pushcfunction(L, y_game_dropped);  lua_setfield(L, -2, "dropped");
	lua_pushcfunction(L, y_game_pressed);  lua_setfield(L, -2, "pressed");
	lua_pushcfunction(L, y_game_stat);     lua_setfield(L, -2, "stat");
	lua_pushcfunction(L, y_game_mode);     lua_setfield(L, -2, "mode");
	lua_pushcfunction(L, y_game_timer_str); lua_setfield(L, -2, "timer_str");
	lua_pushcfunction(L, y_game_fps);        lua_setfield(L, -2, "fps");
	lua_pushcfunction(L, y_game_ticks);      lua_setfield(L, -2, "ticks");
	lua_pushcfunction(L, y_game_shape_cells); lua_setfield(L, -2, "shape_cells");
	lua_pushcfunction(L, y_game_ghost_y);    lua_setfield(L, -2, "ghost_y");
	lua_setglobal(L, "game");

	/* ─── figure table ─── */
	lua_newtable(L);
	lua_pushcfunction(L, y_figure_active); lua_setfield(L, -2, "active");
	lua_pushcfunction(L, y_figure_next);   lua_setfield(L, -2, "next");
	lua_pushcfunction(L, y_figure_held);   lua_setfield(L, -2, "held");
	lua_setglobal(L, "figure");

	/* load & run skin.lua */
	char script_path[512];
	snprintf(script_path, sizeof script_path, "%sskin.lua", skin_path);

	if (luaL_dofile(L, script_path) != LUA_OK)
	{
		fprintf(stderr, "Lua error: %s\n", lua_tostring(L, -1));
		lua_pop(L, 1);
	}

	/* call skin.load(res) */
	lua_getglobal(L, "skin_load");
	if (lua_isfunction(L, -1))
	{
		lua_getglobal(L, "res");
		if (lua_pcall(L, 1, 0, 0) != LUA_OK)
		{
			fprintf(stderr, "Lua skin_load error: %s\n", lua_tostring(L, -1));
			lua_pop(L, 1);
		}
	}
	else lua_pop(L, 1);
}

static void skin_lua_fini(struct Skin *skin)
{
	if (!skin->L) return;
	lua_State *L = skin->L;

	/* call skin.unload() */
	lua_getglobal(L, "skin_unload");
	if (lua_isfunction(L, -1))
	{
		if (lua_pcall(L, 0, 0, 0) != LUA_OK)
			lua_pop(L, 1);
	}
	else lua_pop(L, 1);

	lua_close(L);
	skin->L = NULL;
}

/* ─────────────────────────────────────────────
 * Helper: call a Lua function with no args (pcall guarded)
 * ───────────────────────────────────────────── */

static void call_lua_void(struct Skin *skin, const char *func)
{
	if (!skin->L) return;
	lua_State *L = skin->L;
	lua_getglobal(L, func);
	if (lua_isfunction(L, -1))
	{
		if (lua_pcall(L, 0, 0, 0) != LUA_OK)
		{
			fprintf(stderr, "Lua %s error: %s\n", func, lua_tostring(L, -1));
			lua_pop(L, 1);
		}
	}
	else lua_pop(L, 1);
}

/* ─────────────────────────────────────────────
 * Per‑frame render callbacks (internal)
 * ───────────────────────────────────────────── */

static void skin_lua_draw_background(struct Skin *skin) { call_lua_void(skin, "draw_background"); }
static void skin_lua_draw_board(struct Skin *skin)
{
	int bw = skin->bricksize;
	int bh = skin->bricksize + skin->brickyoffset;

	for (int i = BOARD_WIDTH * BOARD_HEIGHT - 1; i >= BOARD_WIDTH * INVISIBLE_ROW_COUNT; --i)
	{
		if (board[i].orientation == BO_EMPTY) continue;

		int color = board[i].color;
		if (color < 0 || color >= FIGID_END) continue;
		if (!skin->bricksprite[color]) continue;

		int x = (i % BOARD_WIDTH) * bw + skin->boardx;
		int y = (i / BOARD_WIDTH - INVISIBLE_ROW_COUNT) * bw
			+ skin->boardy - skin->brickyoffset;

		int above = 0;
		if (skin->brickyoffset > 0)
		{
			int above_idx = i - BOARD_WIDTH;
			if (above_idx >= 0 && board[above_idx].orientation != BO_EMPTY)
				above = skin->brickyoffset;
		}

		SDL_Rect srcrect = { .x = 0, .y = 0, .w = bw, .h = bh - above };
		SDL_Rect dst = { .x = x, .y = y + above, .w = 0, .h = 0 };

		switch (skin->brickstyle)
		{
		case BS_ORIENTATION_BASED:
			srcrect.x = (int)board[i].orientation * bw - bw;
			break;
		case BS_FIGUREWISE:
			srcrect.y = (color % FIGID_GRAY) * bh;
			break;
		default: break;
		}

		SDL_Surface *block = skin->bricksprite[color];
		SDL_SetAlpha(block, SDL_SRCALPHA, 255);
		SDL_BlitSurface(block, &srcrect, screen, &dst);
	}
}

static void skin_lua_draw_ghost(struct Skin *skin)
{
	if (!figures[0]) return;
	if (skin->ghost <= 0) return;

	/* drop figure until collision to find ghost position */
	int tfy = figures[0]->y;
	while (!isFigureColliding())
		++figures[0]->y;
	if (tfy != figures[0]->y)
		--figures[0]->y;

	int ghost_y = figures[0]->y;
	figures[0]->y = tfy;

	/* only draw if ghost is far enough (at least FIG_DIM rows below) */
	if ((ghost_y - tfy) < FIG_DIM)
		return;

	struct Figure *fig = figures[0];
	int bx = skin->boardx + skin->bricksize * fig->x;
	int by = skin->boardy + skin->bricksize * (ghost_y - INVISIBLE_ROW_COUNT)
		- skin->brickyoffset;
	int bw = skin->bricksize;
	int bh = skin->bricksize + skin->brickyoffset;

	for (int i = 0; i < FIG_DIM * FIG_DIM; ++i)
	{
		if (fig->shape.blockmap[i] == BO_EMPTY) continue;

		int color = fig->color;
		if (color < 0 || color >= FIGID_END) continue;
		if (!skin->bricksprite[color]) continue;

		int cx = i % FIG_DIM;
		int cy = i / FIG_DIM;

		SDL_Rect srcrect = { .x = 0, .y = 0, .w = bw, .h = bh };
		SDL_Rect dst = {
			.x = bx + cx * bw,
			.y = by + cy * bh
		};

		switch (skin->brickstyle)
		{
		case BS_ORIENTATION_BASED:
			srcrect.x = (int)fig->shape.blockmap[i] * bw - bw;
			break;
		case BS_FIGUREWISE:
			srcrect.y = (color % FIGID_GRAY) * bh;
			break;
		default: break;
		}

		SDL_Surface *block = skin->bricksprite[color];
		SDL_SetAlpha(block, SDL_SRCALPHA, (Uint8)skin->ghost);
		SDL_BlitSurface(block, &srcrect, screen, &dst);
	}
}

static void skin_lua_draw_foreground(struct Skin *skin) { call_lua_void(skin, "draw_foreground"); }
static void skin_lua_draw_hud(struct Skin *skin)        { call_lua_void(skin, "draw_hud"); }

static void skin_lua_draw_timed_texts(struct Skin *skin)
{
	Uint32 now = SDL_GetTicks();
	for (int i = 0; i < TIMED_TEXT_MAX; ++i)
	{
		struct TimedText *tt = &skin->timed_texts[i];
		if (tt->deadline <= now) continue;
		if (!tt->text[0]) continue;
		if (!tt->font) continue;

		Uint32 rem = tt->deadline - now;
		Uint8 alpha = (Uint8)((rem < 750) ? (rem * 255 / 750) : 255);

		SDL_Color col = { .r = tt->r, .g = tt->g, .b = tt->b };
		SDL_Surface *ts = TTF_RenderUTF8_Blended(tt->font, tt->text, col);
		if (!ts) continue;

		SDL_Rect dst = { .x = tt->x, .y = tt->y };
		if (tt->alignx == 1) dst.x -= ts->w / 2;
		else if (tt->alignx == 2) dst.x -= ts->w;
		if (tt->aligny == 1) dst.y -= ts->h / 2;
		else if (tt->aligny == 2) dst.y -= ts->h;

		SDL_SetAlpha(ts, SDL_SRCALPHA, alpha);
		SDL_BlitSurface(ts, NULL, screen, &dst);
		SDL_FreeSurface(ts);
	}
}

static void skin_lua_draw_shadow(struct Skin *skin)
{
	if (!skin->brick_shadow) return;
	SDL_Rect srcrect = { .x = 0, .y = 0,
		.w = skin->bricksize,
		.h = skin->bricksize + skin->brickyoffset };

	/* board shadow */
	for (int i = BOARD_WIDTH * BOARD_HEIGHT - 1; i >= BOARD_WIDTH * INVISIBLE_ROW_COUNT; --i)
	{
		if (board[i].orientation == BO_EMPTY) continue;
		SDL_Rect dst = {
			.x = (i % BOARD_WIDTH) * skin->bricksize + skin->boardx + skin->shadowx,
			.y = (i / BOARD_WIDTH - INVISIBLE_ROW_COUNT) * skin->bricksize + skin->boardy
			     - skin->brickyoffset + skin->shadowy
		};
		int above = ((i - BOARD_WIDTH) >= 0 && board[i - BOARD_WIDTH].orientation != BO_EMPTY)
			? skin->brickyoffset : 0;
		dst.y += above;
		srcrect.h = skin->bricksize + skin->brickyoffset - above;
		SDL_BlitSurface(skin->brick_shadow, &srcrect, screen, &dst);
	}

	/* active figure shadow */
	if (!figures[0]) return;
	int fx = skin->boardx + skin->bricksize * figures[0]->x + skin->shadowx;
	int fy = skin->boardy + skin->bricksize * (figures[0]->y - INVISIBLE_ROW_COUNT)
		- skin->brickyoffset + skin->shadowy;
	if (smoothanim)
		fy += draw_delta_drop;

	for (int i = FIG_DIM * FIG_DIM - 1; i >= 0; --i)
	{
		if (!figures[0]->shape.blockmap[i]) continue;
		int above = ((i - FIG_DIM) >= 0 && figures[0]->shape.blockmap[i - FIG_DIM])
			? skin->brickyoffset : 0;
		SDL_Rect dst = {
			.x = (i % FIG_DIM) * skin->bricksize + fx,
			.y = (i / FIG_DIM) * skin->bricksize + fy + above
		};
		srcrect.h = skin->bricksize + skin->brickyoffset - above;
		SDL_BlitSurface(skin->brick_shadow, &srcrect, screen, &dst);
	}
}

static void skin_lua_draw_active_figure(struct Skin *skin, int interp_y)
{
	if (!figures[0]) return;

	struct Figure *fig = figures[0];
	int color = fig->color;
	if (color < 0 || color >= FIGID_END) return;
	if (!skin->bricksprite[color]) return;

	int bw = skin->bricksize;
	int bh = skin->bricksize + skin->brickyoffset;
	int bx = skin->boardx + skin->bricksize * fig->x;
	int by = skin->boardy + skin->bricksize * (fig->y - INVISIBLE_ROW_COUNT)
		- skin->brickyoffset + interp_y;

	for (int i = 0; i < FIG_DIM * FIG_DIM; ++i)
	{
		if (fig->shape.blockmap[i] == BO_EMPTY) continue;

		int cx = i % FIG_DIM;
		int cy = i / FIG_DIM;

		SDL_Rect srcrect = { .x = 0, .y = 0, .w = bw, .h = bh };
		SDL_Rect dst = {
			.x = bx + cx * bw,
			.y = by + cy * bh
		};

		switch (skin->brickstyle)
		{
		case BS_ORIENTATION_BASED:
			srcrect.x = (int)fig->shape.blockmap[i] * bw - bw;
			break;
		case BS_FIGUREWISE:
			srcrect.y = (color % FIGID_GRAY) * bh;
			break;
		default: break;
		}

		SDL_Surface *block = skin->bricksprite[color];
		SDL_SetAlpha(block, SDL_SRCALPHA, 255);
		SDL_BlitSurface(block, &srcrect, screen, &dst);
	}
}

/* ─────────────────────────────────────────────
 * Event callbacks (called from main.c)
 * ───────────────────────────────────────────── */

void skin_lua_on_line_clear(struct Skin *skin, int lines,
                            const char *tspin_type,
                            int combo, bool b2b, int score_earned)
{
	if (!skin->L) return;
	lua_State *L = skin->L;
	lua_getglobal(L, "on_line_clear");
	if (!lua_isfunction(L, -1)) { lua_pop(L, 1); return; }

	lua_newtable(L);
	lua_pushinteger(L, lines);      lua_setfield(L, -2, "lines");
	lua_pushstring(L, tspin_type);  lua_setfield(L, -2, "tspin");
	lua_pushinteger(L, combo);      lua_setfield(L, -2, "combo");
	lua_pushboolean(L, b2b);        lua_setfield(L, -2, "b2b");
	lua_pushinteger(L, score_earned); lua_setfield(L, -2, "score");

	if (lua_pcall(L, 1, 0, 0) != LUA_OK)
	{
		fprintf(stderr, "Lua on_line_clear error: %s\n", lua_tostring(L, -1));
		lua_pop(L, 1);
	}
}

void skin_lua_on_game_over(struct Skin *skin, const char *reason)
{
	if (!skin->L) return;
	lua_State *L = skin->L;
	lua_getglobal(L, "on_game_over");
	if (!lua_isfunction(L, -1)) { lua_pop(L, 1); return; }
	lua_pushstring(L, reason);
	if (lua_pcall(L, 1, 0, 0) != LUA_OK)
	{
		fprintf(stderr, "Lua on_game_over error: %s\n", lua_tostring(L, -1));
		lua_pop(L, 1);
	}
}

void skin_lua_on_level_up(struct Skin *skin, int level)
{
	if (!skin->L) return;
	lua_State *L = skin->L;
	lua_getglobal(L, "on_level_up");
	if (!lua_isfunction(L, -1)) { lua_pop(L, 1); return; }
	lua_pushinteger(L, level);
	if (lua_pcall(L, 1, 0, 0) != LUA_OK) lua_pop(L, 1);
}

void skin_lua_on_piece_lock(struct Skin *skin, enum FigureId id)
{
	if (!skin->L) return;
	lua_State *L = skin->L;
	lua_getglobal(L, "on_piece_lock");
	if (!lua_isfunction(L, -1)) { lua_pop(L, 1); return; }
	lua_pushinteger(L, (int)id);
	if (lua_pcall(L, 1, 0, 0) != LUA_OK) lua_pop(L, 1);
}

void skin_lua_on_piece_hold(struct Skin *skin, enum FigureId id)
{
	if (!skin->L) return;
	lua_State *L = skin->L;
	lua_getglobal(L, "on_piece_hold");
	if (!lua_isfunction(L, -1)) { lua_pop(L, 1); return; }
	lua_pushinteger(L, (int)id);
	if (lua_pcall(L, 1, 0, 0) != LUA_OK) lua_pop(L, 1);
}

void skin_lua_on_hard_drop(struct Skin *skin, int rows)
{
	if (!skin->L) return;
	lua_State *L = skin->L;
	lua_getglobal(L, "on_hard_drop");
	if (!lua_isfunction(L, -1)) { lua_pop(L, 1); return; }
	lua_pushinteger(L, rows);
	if (lua_pcall(L, 1, 0, 0) != LUA_OK) lua_pop(L, 1);
}

void skin_lua_on_combo(struct Skin *skin, int count)
{
	if (!skin->L) return;
	lua_State *L = skin->L;
	lua_getglobal(L, "on_combo");
	if (!lua_isfunction(L, -1)) { lua_pop(L, 1); return; }
	lua_pushinteger(L, count);
	if (lua_pcall(L, 1, 0, 0) != LUA_OK) lua_pop(L, 1);
}

/* ─────────────────────────────────────────────
 * Skin lifecycle
 * ───────────────────────────────────────────── */

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
	skin->L = NULL;
}

void skin_destroySkin(struct Skin *skin)
{
	skin_lua_fini(skin);
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

bool skin_loadSkin(struct Skin *skin, const char *path)
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

	/* check if skin.lua exists */
	char script_path[512];
	snprintf(script_path, sizeof script_path, "%sskin.lua", skin->path);
	FILE *f = fopen(script_path, "r");
	if (!f)
	{
		log("No skin.lua found in %s — skin not loaded.\n", skin->path);
		return false;
	}
	fclose(f);

	skin_lua_init(skin, skin->path);
	log("Lua skin loaded.\n");
	return true;
}

void skin_updateScreen(struct Skin *skin, SDL_Surface *screen)
{
	skin->screen = screen;

	/* smooth-drop interpolation (C side, passed to draw_active_figure) */
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
