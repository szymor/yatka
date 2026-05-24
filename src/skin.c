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
#include "sound.h"

/* ─────────────────────────────────────────────
 * Constants
 * ───────────────────────────────────────────── */

static const char *MT_SURFACE   = "y_surface";
static const char *MT_FONT      = "y_font";
static const char *MT_ANIMATION = "y_animation";

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

static Uint32 read_pixel(SDL_Surface *surface, int x, int y)
{
	int bpp = surface->format->BytesPerPixel;
	Uint8 *p = (Uint8 *)surface->pixels + y * surface->pitch + x * bpp;
	switch (bpp)
	{
		case 1: return *p;
		case 2: return *(Uint16 *)p;
		case 3: return p[0] | (p[1] << 8) | (p[2] << 16);
		case 4: return *(Uint32 *)p;
		default: return 0;
	}
}

static int y_load_image(lua_State *L)
{
	struct Skin *skin = (struct Skin *)lua_touserdata(L, lua_upvalueindex(1));
	const char *name = luaL_checkstring(L, 1);
	char path[512];
	char fallback[512];
	snprintf(path, sizeof path, "%s%s", skin->path, name);
	SDL_Surface *img = IMG_Load(path);
	if (!img)
	{
		snprintf(fallback, sizeof fallback, "gfx/%s", name);
		img = IMG_Load(fallback);
	}
	if (!img)
		return luaL_error(L, "IMG_Load(%s) and IMG_Load(%s) both failed", path, fallback);
	SDL_Surface *opt = SDL_DisplayFormat(img);
	SDL_FreeSurface(img);
	if (!opt)
		return luaL_error(L, "SDL_DisplayFormat failed");

	/* optional second arg: enable colour‑key from top‑left pixel */
	if (lua_toboolean(L, 2))
	{
		if (SDL_MUSTLOCK(opt)) SDL_LockSurface(opt);
		Uint32 ck = read_pixel(opt, 0, 0);
		if (SDL_MUSTLOCK(opt)) SDL_UnlockSurface(opt);
		SDL_SetColorKey(opt, SDL_SRCCOLORKEY, ck);
	}

	SDL_Surface **ud = (SDL_Surface **)lua_newuserdata(L, sizeof(SDL_Surface *));
	*ud = opt;
	luaL_setmetatable(L, MT_SURFACE);
	return 1;
}

static int y_load_font(lua_State *L)
{
	struct Skin *skin = (struct Skin *)lua_touserdata(L, lua_upvalueindex(1));
	const char *name = luaL_checkstring(L, 1);
	char path[512];
	char fallback[512];
	snprintf(path, sizeof path, "%s%s", skin->path, name);
	int size = luaL_checkinteger(L, 2);
	TTF_Font *font = TTF_OpenFont(path, size);
	if (!font)
	{
		snprintf(fallback, sizeof fallback, "gfx/%s", name);
		font = TTF_OpenFont(fallback, size);
	}
	if (!font)
		return luaL_error(L, "TTF_OpenFont(%s) and TTF_OpenFont(%s) both failed", path, fallback);

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

static struct Animation **check_animation(lua_State *L, int idx)
{
	return (struct Animation **)luaL_checkudata(L, idx, MT_ANIMATION);
}

static int y_animation_gc(lua_State *L)
{
	struct Animation **ud = check_animation(L, 1);
	if (*ud)
	{
		if ((*ud)->spritesheet) SDL_FreeSurface((*ud)->spritesheet);
		free(*ud);
		*ud = NULL;
	}
	return 0;
}

static int y_load_animation(lua_State *L)
{
	struct Skin *skin = (struct Skin *)lua_touserdata(L, lua_upvalueindex(1));
	const char *name = luaL_checkstring(L, 1);
	int fw = luaL_checkinteger(L, 2);
	int fh = luaL_checkinteger(L, 3);
	int fd = luaL_checkinteger(L, 4);

	char path[512], fallback[512];
	snprintf(path, sizeof path, "%s%s", skin->path, name);
	SDL_Surface *img = IMG_Load(path);
	if (!img)
	{
		snprintf(fallback, sizeof fallback, "gfx/%s", name);
		img = IMG_Load(fallback);
	}
	if (!img)
		return luaL_error(L, "IMG_Load(%s) and IMG_Load(%s) both failed", path, fallback);
	SDL_Surface *opt = SDL_DisplayFormat(img);
	SDL_FreeSurface(img);
	if (!opt)
		return luaL_error(L, "SDL_DisplayFormat failed");

	int fc = opt->w / fw;
	if (fc < 1) fc = 1;

	/* always enable colour‑key from top‑left pixel */
	if (SDL_MUSTLOCK(opt)) SDL_LockSurface(opt);
	Uint32 ck = read_pixel(opt, 0, 0);
	if (SDL_MUSTLOCK(opt)) SDL_UnlockSurface(opt);
	SDL_SetColorKey(opt, SDL_SRCCOLORKEY, ck);

	struct Animation *anim = (struct Animation *)malloc(sizeof(struct Animation));
	if (!anim) exit(ERROR_MALLOC);
	anim->spritesheet = opt;
	anim->frame_w = fw;
	anim->frame_h = fh;
	anim->frame_count = fc;
	anim->frame_duration = fd;

	struct Animation **ud = (struct Animation **)lua_newuserdata(L, sizeof(struct Animation *));
	*ud = anim;
	luaL_setmetatable(L, MT_ANIMATION);
	return 1;
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
 * Shared helper: draw a shape's bricks
 * ───────────────────────────────────────────── */
static void draw_shape_bricks(struct Skin *skin, SDL_Surface *screen,
                               const struct Shape *shape, int color, int alpha,
                               int base_x, int base_y, int step_x, int step_y)
{
	if (color < 0 || color >= FIGID_END) return;
	if (!skin->bricksprite[color]) return;

	int bw = skin->bricksize;
	int bh = skin->bricksize + skin->brickyoffset;
	SDL_Surface *block = skin->bricksprite[color];

	for (int i = 0; i < FIG_DIM * FIG_DIM; ++i)
	{
		if (shape->blockmap[i] == BO_EMPTY) continue;

		int cx = i % FIG_DIM;
		int cy = i / FIG_DIM;

		SDL_Rect srcrect = { .x = 0, .y = 0, .w = bw, .h = bh };
		switch (skin->brickstyle)
		{
		case BS_ORIENTATION_BASED:
			srcrect.x = (int)shape->blockmap[i] * bw - bw;
			break;
		case BS_FIGUREWISE:
			srcrect.y = (color % FIGID_GRAY) * bh;
			break;
		default: break;
		}

		SDL_Rect dst = { .x = base_x + cx * step_x, .y = base_y + cy * step_y };
		SDL_SetAlpha(block, SDL_SRCALPHA, (Uint8)alpha);
		SDL_BlitSurface(block, &srcrect, screen, &dst);
	}
}

/* res.draw_piece_shape(piece_id, base_x, base_y, color, alpha) */
static int y_draw_piece_shape(lua_State *L)
{
	struct Skin *skin = (struct Skin *)lua_touserdata(L, lua_upvalueindex(1));
	int id = luaL_checkinteger(L, 1);
	int base_x = luaL_checkinteger(L, 2);
	int base_y = luaL_checkinteger(L, 3);
	int color = luaL_checkinteger(L, 4);
	int alpha = (int)luaL_optinteger(L, 5, 255);

	if (id < 0 || id >= FIGID_GRAY) return 0;
	const struct Shape *shape = getShape((enum FigureId)id);
	if (!shape) return 0;

	int minx, maxx, miny, maxy;
	getShapeDimensions(shape, &minx, &maxx, &miny, &maxy);

	int bw = skin->bricksize;
	int sw = maxx - minx + 1;
	int sh = maxy - miny + 1;
	int ox = (4 - sw) * bw / 2 - minx * bw;
	int oy = (2 - sh) * bw / 2 - miny * bw;

	draw_shape_bricks(skin, skin->screen, shape, color, alpha,
	                  base_x + ox, base_y + oy, bw, bw);
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

/* res.play_sfx(id) — play a sound effect by enum index */
static int y_play_sfx(lua_State *L)
{
	int id = luaL_checkinteger(L, 1);
	if (id >= 0 && id < SE_END)
		playEffect((enum SfxEffect)id);
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

	/* free previously cached surface (if slot was reused) */
	if (tt->surface)
	{
		SDL_FreeSurface(tt->surface);
		tt->surface = NULL;
	}

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
	tt->fadeout_ms = (Uint32)luaL_optinteger(L, 11, 750);

	/* render and cache the surface once */
	if (tt->font)
	{
		SDL_Color col = { .r = tt->r, .g = tt->g, .b = tt->b };
		tt->surface = TTF_RenderUTF8_Blended(tt->font, tt->text, col);
	}
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
static int y_game_timer(lua_State *L) { lua_pushinteger(L, (int)game_totaltime); return 1; }
static int y_game_ultra_time_left(lua_State *L)
{
	updateTotalTime();
	Uint32 left = (game_totaltime < ULTRA_MS_LEN) ? (ULTRA_MS_LEN - game_totaltime) : 0;
	lua_pushinteger(L, (int)left);
	return 1;
}
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
 * Particle system (C update+draw, Lua spawn)
 * ───────────────────────────────────────────── */

static int y_add_particle(lua_State *L)
{
	struct Skin *skin = (struct Skin *)lua_touserdata(L, lua_upvalueindex(1));
	if (skin->particle_count >= PARTICLE_MAX) return 0;
	struct Particle *p = &skin->particles[skin->particle_count++];
	p->x = (float)luaL_checknumber(L, 1);
	p->y = (float)luaL_checknumber(L, 2);
	p->vx = (float)luaL_checknumber(L, 3);
	p->vy = (float)luaL_checknumber(L, 4);

	/* arg 5: animation userdata, surface userdata, or lightuserdata */
	struct Animation **anim_ud = (struct Animation **)luaL_testudata(L, 5, MT_ANIMATION);
	if (anim_ud && *anim_ud)
	{
		p->anim = *anim_ud;
		p->sprite = (*anim_ud)->spritesheet;
		p->anim_start_tick = SDL_GetTicks();
		/* frame rect — updated dynamically in draw */
		p->srcrect.x = 0;
		p->srcrect.y = 0;
		p->srcrect.w = (*anim_ud)->frame_w;
		p->srcrect.h = (*anim_ud)->frame_h;
	}
	else
	{
		p->anim = NULL;
		/* accept both lightuserdata (bricksprite ref) and full userdata (load_image) */
		if (lua_islightuserdata(L, 5))
			p->sprite = (SDL_Surface *)lua_touserdata(L, 5);
		else
		{
			SDL_Surface **ud = check_surface(L, 5);
			p->sprite = ud ? *ud : NULL;
		}
		p->srcrect.x = luaL_checkinteger(L, 6);
		p->srcrect.y = luaL_checkinteger(L, 7);
		p->srcrect.w = luaL_checkinteger(L, 8);
		p->srcrect.h = luaL_checkinteger(L, 9);
	}
	p->ax = (float)luaL_optnumber(L, 10, 0.0f);
	p->ay = (float)luaL_optnumber(L, 11, 0.0f);
	p->no_remove = lua_toboolean(L, 12);
	lua_pushinteger(L, skin->particle_count - 1);
	return 1;
}

static int y_move_particle(lua_State *L)
{
	struct Skin *skin = (struct Skin *)lua_touserdata(L, lua_upvalueindex(1));
	int idx = luaL_checkinteger(L, 1);
	if (idx < 0 || idx >= skin->particle_count) return 0;
	struct Particle *p = &skin->particles[idx];
	if (lua_gettop(L) >= 2) p->vx = (float)luaL_checknumber(L, 2);
	if (lua_gettop(L) >= 3) p->vy = (float)luaL_checknumber(L, 3);
	if (lua_gettop(L) >= 4) p->ax = (float)luaL_optnumber(L, 4, 0.0f);
	if (lua_gettop(L) >= 5) p->ay = (float)luaL_optnumber(L, 5, 0.0f);
	return 0;
}

static int y_remove_particle(lua_State *L)
{
	struct Skin *skin = (struct Skin *)lua_touserdata(L, lua_upvalueindex(1));
	int idx = luaL_checkinteger(L, 1);
	if (idx < 0 || idx >= skin->particle_count) return 0;
	skin->particles[idx] = skin->particles[--skin->particle_count];
	return 0;
}

static void skin_update_particles(struct Skin *skin)
{
	Uint32 now = SDL_GetTicks();
	if (!skin->last_particle_tick) { skin->last_particle_tick = now; return; }
	float dt = (now - skin->last_particle_tick) / 1000.0f;
	skin->last_particle_tick = now;
	if (dt > 0.1f) dt = 0.1f;

	for (int i = 0; i < skin->particle_count; )
	{
		struct Particle *p = &skin->particles[i];
		p->vx += p->ax * dt;
		p->vy += p->ay * dt;
		p->x += p->vx * dt;
		p->y += p->vy * dt;
		if (!p->no_remove && (p->y >= SCREEN_HEIGHT || p->x + p->srcrect.w <= 0 || p->x >= SCREEN_WIDTH))
			skin->particles[i] = skin->particles[--skin->particle_count];
		else
			++i;
	}
}

static void skin_draw_particles(struct Skin *skin)
{
	Uint32 now = SDL_GetTicks();
	for (int i = 0; i < skin->particle_count; ++i)
	{
		struct Particle *p = &skin->particles[i];
		if (!p->sprite) continue;

		SDL_Rect srcrect = p->srcrect;
		if (p->anim)
		{
			Uint32 elapsed = now - p->anim_start_tick;
			int frame = (elapsed / p->anim->frame_duration) % p->anim->frame_count;
			srcrect.x = frame * p->anim->frame_w;
			srcrect.y = 0;
			srcrect.w = p->anim->frame_w;
			srcrect.h = p->anim->frame_h;
		}

		SDL_Rect dst = { .x = (int)p->x, .y = (int)p->y };
		SDL_SetAlpha(p->sprite, SDL_SRCALPHA, 255);
		SDL_BlitSurface(p->sprite, &srcrect, skin->screen, &dst);
	}
}

/* ─────────────────────────────────────────────
 * Lua‑state initialisation
 * ───────────────────────────────────────────── */

static const luaL_Reg ylib[] = {
	{ "draw_image",  y_draw_image  },
	{ "draw_text",   y_draw_text   },
	{ "draw_rect",   y_draw_rect   },
	{ "draw_bar",    y_draw_bar    },
	{ "screen_w",    y_screen_w    },
	{ "screen_h",    y_screen_h    },
	{ NULL, NULL }
};

static void skin_init_lua(struct Skin *skin, const char *skin_path)
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

	luaL_newmetatable(L, MT_ANIMATION);
	lua_pushcfunction(L, y_animation_gc);
	lua_setfield(L, -2, "__gc");
	lua_pop(L, 1);

	/* create the `res` table with C functions + skin upvalue */
	lua_pushlightuserdata(L, skin);
	lua_pushcclosure(L, y_draw_brick, 1);
	lua_setglobal(L, "y_draw_brick");

	lua_pushlightuserdata(L, skin);
	lua_pushcclosure(L, y_brick_size, 1);
	lua_setglobal(L, "y_brick_size");

	lua_pushlightuserdata(L, skin);
	lua_pushcclosure(L, y_draw_piece_shape, 1);
	lua_setglobal(L, "y_draw_piece_shape");

	lua_newtable(L);
	luaL_setfuncs(L, ylib, 0);

	lua_getglobal(L, "y_draw_brick");
	lua_setfield(L, -2, "draw_brick");
	lua_getglobal(L, "y_brick_size");
	lua_setfield(L, -2, "brick_size");
	lua_getglobal(L, "y_draw_piece_shape");
	lua_setfield(L, -2, "draw_piece_shape");

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
	lua_pushcclosure(L, y_load_image, 1);
	lua_setfield(L, -2, "load_image");

	lua_pushlightuserdata(L, skin);
	lua_pushcclosure(L, y_load_font, 1);
	lua_setfield(L, -2, "load_font");

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

	lua_pushcfunction(L, y_play_sfx);
	lua_setfield(L, -2, "play_sfx");

	lua_pushlightuserdata(L, skin);
	lua_pushcclosure(L, y_load_animation, 1);
	lua_setfield(L, -2, "load_animation");

	lua_pushlightuserdata(L, skin);
	lua_pushcclosure(L, y_add_particle, 1);
	lua_setfield(L, -2, "add_particle");

	lua_pushlightuserdata(L, skin);
	lua_pushcclosure(L, y_move_particle, 1);
	lua_setfield(L, -2, "move_particle");

	lua_pushlightuserdata(L, skin);
	lua_pushcclosure(L, y_remove_particle, 1);
	lua_setfield(L, -2, "remove_particle");

	lua_pop(L, 1);  /* pop res */

	/* ─── sfx table (named sound effect constants) ─── */
	lua_newtable(L);
	lua_pushinteger(L, SE_NONE);     lua_setfield(L, -2, "none");
	lua_pushinteger(L, SE_CLEAR);    lua_setfield(L, -2, "clear");
	lua_pushinteger(L, SE_COMBO_1X); lua_setfield(L, -2, "combo_1");
	lua_pushinteger(L, SE_COMBO_2X); lua_setfield(L, -2, "combo_2");
	lua_pushinteger(L, SE_COMBO_3X); lua_setfield(L, -2, "combo_3");
	lua_pushinteger(L, SE_COMBO_4X); lua_setfield(L, -2, "combo_4");
	lua_pushinteger(L, SE_COMBO_5X); lua_setfield(L, -2, "combo_5");
	lua_pushinteger(L, SE_COMBO_6X); lua_setfield(L, -2, "combo_6");
	lua_pushinteger(L, SE_COMBO_7X); lua_setfield(L, -2, "combo_7");
	lua_pushinteger(L, SE_HIT);      lua_setfield(L, -2, "hit");
	lua_pushinteger(L, SE_CLICK);    lua_setfield(L, -2, "click");
	lua_setglobal(L, "sfx");

	/* ─── fig table (symbolic tetromino IDs) ─── */
	lua_newtable(L);
	lua_pushinteger(L, FIGID_I); lua_setfield(L, -2, "I");
	lua_pushinteger(L, FIGID_O); lua_setfield(L, -2, "O");
	lua_pushinteger(L, FIGID_T); lua_setfield(L, -2, "T");
	lua_pushinteger(L, FIGID_S); lua_setfield(L, -2, "S");
	lua_pushinteger(L, FIGID_Z); lua_setfield(L, -2, "Z");
	lua_pushinteger(L, FIGID_J); lua_setfield(L, -2, "J");
	lua_pushinteger(L, FIGID_L); lua_setfield(L, -2, "L");
	lua_setglobal(L, "fig");

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
	lua_pushcfunction(L, y_game_timer);              lua_setfield(L, -2, "timer");
	lua_pushcfunction(L, y_game_ultra_time_left);   lua_setfield(L, -2, "ultra_time_left");
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

static void skin_fini_lua(struct Skin *skin)
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
	skin->particle_count = 0;
	skin->last_particle_tick = 0;
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

static void skin_draw_background(struct Skin *skin) { call_lua_void(skin, "draw_background"); }
static void skin_draw_board(struct Skin *skin)
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

static void skin_draw_ghost(struct Skin *skin)
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

static void skin_draw_foreground(struct Skin *skin) { call_lua_void(skin, "draw_foreground"); }

static void skin_draw_timed_texts(struct Skin *skin)
{
	SDL_Surface *screen = skin->screen;
	Uint32 now = SDL_GetTicks();
	for (int i = 0; i < TIMED_TEXT_MAX; ++i)
	{
		struct TimedText *tt = &skin->timed_texts[i];
		if (tt->deadline <= now) continue;
		if (!tt->text[0]) continue;
		if (!tt->font) continue;
		if (!tt->surface) continue;

		Uint32 rem = tt->deadline - now;
		Uint8 alpha = 255;
		if (tt->fadeout_ms > 0 && rem < tt->fadeout_ms)
			alpha = (Uint8)(rem * 255 / tt->fadeout_ms);

		SDL_Rect dst = { .x = tt->x, .y = tt->y };
		if (tt->alignx == 1) dst.x -= tt->surface->w / 2;
		else if (tt->alignx == 2) dst.x -= tt->surface->w;
		if (tt->aligny == 1) dst.y -= tt->surface->h / 2;
		else if (tt->aligny == 2) dst.y -= tt->surface->h;

		if (alpha < 255 && tt->surface->format->Amask)
		{
			/* Create a temporary copy with faded per-pixel alpha */
			SDL_Surface *ts = SDL_CreateRGBSurface(
				tt->surface->flags,
				tt->surface->w, tt->surface->h,
				tt->surface->format->BitsPerPixel,
				tt->surface->format->Rmask,
				tt->surface->format->Gmask,
				tt->surface->format->Bmask,
				tt->surface->format->Amask);
			if (!ts) continue;

			if (SDL_MUSTLOCK(tt->surface)) SDL_LockSurface(tt->surface);
			if (SDL_MUSTLOCK(ts)) SDL_LockSurface(ts);
			memcpy(ts->pixels, tt->surface->pixels,
			       tt->surface->h * tt->surface->pitch);

			Uint32 *pix = (Uint32 *)ts->pixels;
			int n = ts->w * ts->h;
			int ashift = ts->format->Ashift;
			Uint32 amask = ts->format->Amask;
			for (int j = 0; j < n; ++j)
			{
				Uint32 a = (pix[j] & amask) >> ashift;
				a = (a * alpha) / 255;
				pix[j] = (pix[j] & ~amask) | (a << ashift);
			}
			if (SDL_MUSTLOCK(ts)) SDL_UnlockSurface(ts);
			if (SDL_MUSTLOCK(tt->surface)) SDL_UnlockSurface(tt->surface);

			SDL_BlitSurface(ts, NULL, screen, &dst);
			SDL_FreeSurface(ts);
		}
		else
		{
			SDL_BlitSurface(tt->surface, NULL, screen, &dst);
		}
	}
}

static void skin_draw_shadow(struct Skin *skin)
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

static void skin_draw_active_figure(struct Skin *skin, int interp_y)
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

	draw_shape_bricks(skin, screen, &fig->shape, color, 255,
	                  bx, by, bw, bh);
}

/* ─────────────────────────────────────────────
 * Helper: push bricksprite sheet surface + source rect
 *         into the Lua particles table (non‑owning)
 * ───────────────────────────────────────────── */
static void push_brick_sprite_table(struct Skin *skin, lua_State *L, int color, int orient)
{
	int bw = skin->bricksize;
	int bh = skin->bricksize + skin->brickyoffset;
	int sx = 0, sy = 0;

	if (color >= 0 && color < FIGID_END && skin->bricksprite[color])
	{
		switch (skin->brickstyle)
		{
			case BS_ORIENTATION_BASED:
				sx = orient * bw - bw;
				break;
			case BS_FIGUREWISE:
				sy = (color % FIGID_GRAY) * bh;
				break;
			default: break;
		}

		lua_pushlightuserdata(L, skin->bricksprite[color]);
		lua_setfield(L, -2, "sprite");
	}
	else
	{
		lua_pushnil(L);
		lua_setfield(L, -2, "sprite");
	}

	lua_pushinteger(L, sx);  lua_setfield(L, -2, "sx");
	lua_pushinteger(L, sy);  lua_setfield(L, -2, "sy");
	lua_pushinteger(L, bw);  lua_setfield(L, -2, "sw");
	lua_pushinteger(L, bh);  lua_setfield(L, -2, "sh");
}

/* ─────────────────────────────────────────────
 * Event callbacks (called from main.c)
 * ───────────────────────────────────────────── */

void skin_on_line_clear(struct Skin *skin, int lines,
                            const char *tspin_type,
                            int combo, bool b2b, int score_earned,
                            bool pc)
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
	lua_pushboolean(L, speechon);   lua_setfield(L, -2, "speech_on");
	lua_pushboolean(L, pc);         lua_setfield(L, -2, "pc");

	/* pass particles table (cleared brick positions + sprite surfaces) */
	lua_newtable(L);
	for (int i = 0; i < cleared_brick_count; ++i)
	{
		lua_newtable(L);
		lua_pushinteger(L, cleared_bricks[i].x);     lua_setfield(L, -2, "x");
		lua_pushinteger(L, cleared_bricks[i].y);     lua_setfield(L, -2, "y");
		push_brick_sprite_table(skin, L, cleared_bricks[i].color, cleared_bricks[i].orient);
		lua_rawseti(L, -2, i + 1);
	}
	lua_setfield(L, -2, "particles");

	if (lua_pcall(L, 1, 0, 0) != LUA_OK)
	{
		fprintf(stderr, "Lua on_line_clear error: %s\n", lua_tostring(L, -1));
		lua_pop(L, 1);
	}
}

void skin_on_game_over(struct Skin *skin, const char *reason)
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

void skin_on_level_up(struct Skin *skin, int level)
{
	if (!skin->L) return;
	lua_State *L = skin->L;
	lua_getglobal(L, "on_level_up");
	if (!lua_isfunction(L, -1)) { lua_pop(L, 1); return; }
	lua_pushinteger(L, level);
	if (lua_pcall(L, 1, 0, 0) != LUA_OK) lua_pop(L, 1);
}

void skin_on_piece_lock(struct Skin *skin, enum FigureId id)
{
	if (!skin->L) return;
	lua_State *L = skin->L;
	lua_getglobal(L, "on_piece_lock");
	if (!lua_isfunction(L, -1)) { lua_pop(L, 1); return; }
	lua_pushinteger(L, (int)id);
	if (lua_pcall(L, 1, 0, 0) != LUA_OK) lua_pop(L, 1);
}

void skin_on_piece_hold(struct Skin *skin, enum FigureId id)
{
	if (!skin->L) return;
	lua_State *L = skin->L;
	lua_getglobal(L, "on_piece_hold");
	if (!lua_isfunction(L, -1)) { lua_pop(L, 1); return; }
	lua_pushinteger(L, (int)id);
	if (lua_pcall(L, 1, 0, 0) != LUA_OK) lua_pop(L, 1);
}

void skin_on_hard_drop(struct Skin *skin, int rows)
{
	if (!skin->L) return;
	lua_State *L = skin->L;
	lua_getglobal(L, "on_hard_drop");
	if (!lua_isfunction(L, -1)) { lua_pop(L, 1); return; }
	lua_pushinteger(L, rows);
	if (lua_pcall(L, 1, 0, 0) != LUA_OK) lua_pop(L, 1);
}

void skin_on_combo(struct Skin *skin, int count)
{
	if (!skin->L) return;
	lua_State *L = skin->L;
	lua_getglobal(L, "on_combo");
	if (!lua_isfunction(L, -1)) { lua_pop(L, 1); return; }
	lua_pushinteger(L, count);
	if (lua_pcall(L, 1, 0, 0) != LUA_OK) lua_pop(L, 1);
}

void skin_on_move(struct Skin *skin, const char *direction)
{
	if (!skin->L) return;
	lua_State *L = skin->L;
	lua_getglobal(L, "on_move");
	if (!lua_isfunction(L, -1)) { lua_pop(L, 1); return; }
	lua_pushstring(L, direction);
	if (lua_pcall(L, 1, 0, 0) != LUA_OK) lua_pop(L, 1);
}

/* ─────────────────────────────────────────────
 * Skin lifecycle
 * ───────────────────────────────────────────── */

void skin_init(struct Skin *skin)
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

void skin_destroy(struct Skin *skin)
{
	skin_fini_lua(skin);
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
	for (int i = 0; i < TIMED_TEXT_MAX; ++i)
	{
		if (skin->timed_texts[i].surface)
		{
			SDL_FreeSurface(skin->timed_texts[i].surface);
			skin->timed_texts[i].surface = NULL;
		}
	}
}

bool skin_loadSkin(struct Skin *skin, const char *path)
{
	skin_destroy(skin);

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

	skin_init_lua(skin, skin->path);
	log("Lua skin loaded.\n");
	return true;
}

void skin_update_screen(struct Skin *skin, SDL_Surface *screen)
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

	skin_draw_background(skin);
	skin_draw_shadow(skin);
	skin_draw_board(skin);
	skin_draw_active_figure(skin, interp_y);
	skin_draw_ghost(skin);
	skin_draw_timed_texts(skin);
	skin_update_particles(skin);
	skin_draw_particles(skin);
	skin_draw_foreground(skin);

	flipScreenScaled();
	frameCounter();
}
