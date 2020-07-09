#include <stdbool.h>
#include <stdio.h>

#include <SDL/SDL.h>
#include <SDL/SDL_image.h>
#include <SDL/SDL_ttf.h>

#include "skin.h"
#include "main.h"
#include "video.h"
#include "state_mainmenu.h"

#define log		printf

static void str_replace(char *where, const char *what, const char *with);
static void int_replace(char *where, const char *what, int with);

static void drawFigure(struct Skin *skin, const struct Figure *fig, int x, int y, Uint8 alpha, bool active, bool centerx, bool centery);
static void drawShape(struct Skin *skin, SDL_Surface *target, const struct Shape *sh, int x, int y, enum FigureId color, Uint8 alpha, bool centerx, bool centery);
static SDL_Surface *getBlock(struct Skin *skin, enum FigureId color, enum BlockOrientation orient, SDL_Rect *srcrect);

static void skin_executeScript(struct Skin *skin, bool dynamic);
static void skin_executeStatement(struct Skin *skin, const char *statement, bool dynamic);
static void skin_executeBFg(struct Skin *skin, const char *statement, SDL_Surface **sdls);
static void skin_executeBg(struct Skin *skin, const char *statement);
static void skin_executeFg(struct Skin *skin, const char *statement);
static void skin_executeBoardxy(struct Skin *skin, const char *statement);
static void skin_executeBricksize(struct Skin *skin, const char *statement);
static void skin_executeBricksprite(struct Skin *skin, const char *statement);
static void skin_executeFont(struct Skin *skin, const char *statement);
static void skin_executeBox(struct Skin *skin, const char *statement);
static void skin_executeShape(struct Skin *skin, const char *statement);
static void skin_executeText(struct Skin *skin, const char *statement);
static void skin_executeFigure(struct Skin *skin, const char *statement);

void skin_initSkin(struct Skin *skin)
{
	skin->bg = NULL;
	skin->fg = NULL;
	skin->script = NULL;
	skin->path = NULL;
	skin->boardx = 0;
	skin->boardy = 0;
	for (int i = 0; i < FONT_NUM; ++i)
		skin->fonts[i] = NULL;
	skin->screen = NULL;
	for (int i = 0; i < FIGID_END; ++i)
		skin->bricksprite[i] = NULL;
}

void skin_destroySkin(struct Skin *skin)
{
	if (skin->bg)
	{
		SDL_FreeSurface(skin->bg);
		skin->bg = NULL;
	}
	if (skin->fg)
	{
		SDL_FreeSurface(skin->fg);
		skin->fg = NULL;
	}
	if (skin->script)
	{
		free(skin->script);
		skin->script = NULL;
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
			free(skin->bricksprite[i]);
			skin->bricksprite[i] = NULL;
		}
	}
}

void skin_loadSkin(struct Skin *skin, const char *path)
{
	skin_destroySkin(skin);
	FILE *sfile = fopen(path, "rb");

	fseek(sfile, 0, SEEK_END);
	size_t filesize = ftell(sfile);
	rewind(sfile);

	skin->script = (char*)malloc(sizeof(char) * filesize + 1);
	if (NULL == skin->script)
	{
		exit(ERROR_MALLOC);
	}

	size_t result = fread(skin->script, 1, filesize, sfile);
	if (result != filesize)
	{
		exit(ERROR_IO);
	}
	skin->script[filesize] = '\0';
	
	int totallen = strlen(path) + 1;
	skin->path = (char*)malloc(totallen);
	if (NULL == skin->path)
	{
		exit(ERROR_MALLOC);
	}
	strcpy(skin->path, path);
	char *ptr = skin->path + totallen - 1;
	while (*ptr != '/')
	{
		*(ptr--) = '\0';
	}
	log("Script path: %s\n", skin->path);

	skin_executeScript(skin, false);

	fclose(sfile);
}

void skin_updateScreen(struct Skin *skin, SDL_Surface *screen)
{
	skin->screen = screen;

	// display background
	SDL_Rect rect;
	rect.x = 0;
	rect.y = 0;
	SDL_BlitSurface(skin->bg, NULL, screen, &rect);

	skin_executeScript(skin, true);

	// display board
	for (int i = BOARD_WIDTH * INVISIBLE_ROW_COUNT; i < (BOARD_WIDTH * BOARD_HEIGHT); ++i)
	{
		rect.x = (i % BOARD_WIDTH) * skin->bricksize + skin->boardx;
		rect.y = (i / BOARD_WIDTH - INVISIBLE_ROW_COUNT) * skin->bricksize + skin->boardy;
		if (board[i].orientation != BO_EMPTY)
		{
			SDL_Rect srcrect;
			SDL_Surface *block = grayblocks ? getBlock(skin, FIGID_GRAY, board[i].orientation, &srcrect) : getBlock(skin, board[i].color, board[i].orientation, &srcrect);
			SDL_SetAlpha(block, SDL_SRCALPHA, 255);
			SDL_BlitSurface(block, &srcrect, screen, &rect);
		}
	}

	// display the active figure
	if (figures[0] != NULL)
	{
		int x = skin->boardx + skin->bricksize * figures[0]->x;
		int y = skin->boardy + skin->bricksize * (figures[0]->y - INVISIBLE_ROW_COUNT);
		if (smoothanim)
		{
			Uint32 ct = SDL_GetTicks();
			double fraction = (double)(ct - last_drop_time) / (getNextDropTime() - last_drop_time);
			int new_delta = skin->bricksize * fraction - skin->bricksize;
			if (new_delta > draw_delta_drop)
			{
				draw_delta_drop = new_delta;
				if (draw_delta_drop > 0)
					draw_delta_drop = 0;
			}
			y += draw_delta_drop;
		}
		drawFigure(skin, figures[0], x, y, 255, true, false, false);
	}

	// display the ghost figure
	if (figures[0] != NULL && ghostalpha > 0)
	{
		int tfy = figures[0]->y;
		while (!isFigureColliding())
			++figures[0]->y;
		if (tfy != figures[0]->y)
			--figures[0]->y;
		if ((figures[0]->y - tfy) >= FIG_DIM)
		{
			int x = skin->boardx + skin->bricksize * figures[0]->x;
			int y = skin->boardy + skin->bricksize * (figures[0]->y - INVISIBLE_ROW_COUNT);
			drawFigure(skin, figures[0], x, y, ghostalpha, true, false, false);
		}
		figures[0]->y = tfy;
	}

	// display foreground
	rect.x = 0;
	rect.y = 0;
	SDL_BlitSurface(skin->fg, NULL, screen, &rect);

	flipScreenScaled();
	frameCounter();
}

void skin_updateBackground(struct Skin *skin)
{
	skin_executeScript(skin, false);
}

static void skin_executeScript(struct Skin *skin, bool dynamic)
{
	const char *script = skin->script;
	if (!script)
		return;
	while (*script)
	{
		skin_executeStatement(skin, script, dynamic);
		while (*(script++) != '\n')
		{
			if (*script == '\0')
				break;
		}
	}
}

static void skin_executeStatement(struct Skin *skin, const char *statement, bool dynamic)
{
	if (*statement == ';' || *statement == '\n' || *statement == '\0')
		return;
	char cmd[32];
	sscanf(statement, "%s", cmd);
	log("Command: <%s>\n", cmd);
	if (!strcmp(cmd, "bg"))
	{
		if (dynamic) return;
		skin_executeBg(skin, statement);
	}
	else if (!strcmp(cmd, "fg"))
	{
		if (dynamic) return;
		skin_executeFg(skin, statement);
	}
	else if (!strcmp(cmd, "boardxy"))
	{
		if (dynamic) return;
		skin_executeBoardxy(skin, statement);
	}
	else if (!strcmp(cmd, "bricksize"))
	{
		if (dynamic) return;
		skin_executeBricksize(skin, statement);
	}
	else if (!strcmp(cmd, "bricksprite"))
	{
		if (dynamic) return;
		skin_executeBricksprite(skin, statement);
	}
	else if (!strcmp(cmd, "font"))
	{
		if (dynamic) return;
		skin_executeFont(skin, statement);
	}
	else if (!strcmp(cmd, "box"))
	{
		if (dynamic) return;
		skin_executeBox(skin, statement);
	}
	else if (!strcmp(cmd, "shape"))
	{
		if (dynamic) return;
		skin_executeShape(skin, statement);
	}
	else if (!strcmp(cmd, "text"))
	{
		if (!dynamic) return;
		skin_executeText(skin, statement);
	}
	else if (!strcmp(cmd, "figure"))
	{
		if (!dynamic) return;
		skin_executeFigure(skin, statement);
	}
}

static void skin_executeBFg(struct Skin *skin, const char *statement, SDL_Surface **sdls)
{
	SDL_Surface *s = *sdls;
	char filename[256];
	sscanf(statement, "%*s %[^\n]", filename);
	log("filename: <%s>\n", filename);
	if (!strcmp(filename, "null"))
	{
		if (s)
			SDL_FreeSurface(s);
		*sdls = NULL;
	}
	else
	{
		char *fn = filename + 1;	// omit the first "
		fn[strlen(fn) - 1] = '\0';	// omit another one
		char fp[256];
		sprintf(fp, "%s%s", skin->path, fn);
		log("filepath: <%s>\n", fp);
		SDL_Surface *ts = IMG_Load(fp);
		if (ts == NULL)
			exit(ERROR_NOIMGFILE);
		*sdls = SDL_DisplayFormat(ts);
		SDL_FreeSurface(ts);
	}
}

static void skin_executeBg(struct Skin *skin, const char *statement)
{
	skin_executeBFg(skin, statement, &skin->bg);
}

static void skin_executeFg(struct Skin *skin, const char *statement)
{
	skin_executeBFg(skin, statement, &skin->fg);
}

static void skin_executeBoardxy(struct Skin *skin, const char *statement)
{
	sscanf(statement, "%*s %d %d", &skin->boardx, &skin->boardy);
	log("boardxy %d %d\n", skin->boardx, skin->boardy);
}

static void skin_executeBricksize(struct Skin *skin, const char *statement)
{
	sscanf(statement, "%*s %d", &skin->bricksize);
	log("bricksize %d\n", skin->bricksize);
	brick_size = skin->bricksize;
	draw_delta_drop = -brick_size;
}

static void skin_executeBricksprite(struct Skin *skin, const char *statement)
{
	skin_executeBFg(skin, statement, &skin->bricksprite[FIGID_GRAY]);
	int s = skin->bricksize;
	if (s == skin->bricksprite[FIGID_GRAY]->w && s == skin->bricksprite[FIGID_GRAY]->h)
		skin->brickstyle = BS_SIMPLE;
	else if (s == skin->bricksprite[FIGID_GRAY]->h && 15 * s == skin->bricksprite[FIGID_GRAY]->w)
		skin->brickstyle = BS_ORIENTATION_BASED;
	else
		exit(ERROR_SCRIPT);

	// brick dyeing
	SDL_PixelFormat *f = screen->format;
	SDL_SetAlpha(skin->bricksprite[FIGID_GRAY], SDL_SRCALPHA, 128);

	Uint32 rgb[] = {
		SDL_MapRGB(f, 215, 64, 0),
		SDL_MapRGB(f, 59, 52, 255),
		SDL_MapRGB(f, 115, 121, 0),
		SDL_MapRGB(f, 0, 132, 96),
		SDL_MapRGB(f, 75, 160, 255),
		SDL_MapRGB(f, 255, 174, 10),
		SDL_MapRGB(f, 255, 109, 247),
		0,
		SDL_MapRGB(f, 0, 159, 218),
		SDL_MapRGB(f, 254, 203, 0),
		SDL_MapRGB(f, 149, 45, 152),
		SDL_MapRGB(f, 105, 190, 40),
		SDL_MapRGB(f, 237, 41, 57),
		SDL_MapRGB(f, 0, 101, 189),
		SDL_MapRGB(f, 255, 121, 0),
	};

	for (int i = 0; i < FIGID_END; ++i)
	{
		if (FIGID_GRAY == i)
			continue;
		skin->bricksprite[i] = SDL_CreateRGBSurface(0, skin->bricksprite[FIGID_GRAY]->w, skin->bricksprite[FIGID_GRAY]->h, f->BitsPerPixel, f->Rmask, f->Gmask, f->Bmask, 0);
		SDL_FillRect(skin->bricksprite[i], NULL, rgb[i]);
		SDL_BlitSurface(skin->bricksprite[FIGID_GRAY], NULL, skin->bricksprite[i], NULL);
	}
}

static void skin_executeFont(struct Skin *skin, const char *statement)
{
	int id, size;
	char filename[256];
	sscanf(statement, "%*s %d \"%[^\"\n]\" %d", &id, filename, &size);
	log("font %d %s %d\n", id, filename, size);
	char filepath[256];
	sprintf(filepath, "%s%s", skin->path, filename);
	log("filepath %s\n", filepath);

	skin->fonts[id] = TTF_OpenFont(filepath, size);
	if (skin->fonts[id] == NULL)
		exit(ERROR_NOFONT);
}

static void skin_executeBox(struct Skin *skin, const char *statement)
{
	int x, y, w, h, alpha, r, g, b;
	sscanf(statement, "%*s %d %d %d %d %d %d %d %d", &x, &y,
			&w, &h, &alpha, &r, &g, &b);
	SDL_Surface *bg = skin->bg;
	SDL_Surface *ph = SDL_CreateRGBSurface(0, w, h, bg->format->BitsPerPixel, bg->format->Rmask, bg->format->Gmask, bg->format->Bmask, 0);
	SDL_FillRect(ph, NULL, SDL_MapRGB(ph->format, r, g, b));
	SDL_SetAlpha(ph, SDL_SRCALPHA, alpha);
	SDL_Rect rect = { .x = x, .y = y };
	SDL_BlitSurface(ph, NULL, bg, &rect);
	SDL_FreeSurface(ph);
}

static void skin_executeShape(struct Skin *skin, const char *statement)
{
	int id, x, y, centerx, centery, alpha;
	sscanf(statement, "%*s %d %d %d %d %d %d", &id, &x, &y, &centerx, &centery, &alpha);
	drawShape(skin, skin->bg, getShape(id), x, y, FIGID_GRAY, alpha, centerx, centery);
}

static void skin_executeText(struct Skin *skin, const char *statement)
{
	int fontid, x, y, alignx, aligny, r, g, b;
	char string[256];
	sscanf(statement, "%*s %d %d %d %d %d %d %d %d \"%[^\"\n]", &fontid, &x, &y,
			&alignx, &aligny, &r, &g, &b, string);

	int_replace(string, "$hiscore", hiscore);
	int_replace(string, "$score", score);
	int_replace(string, "$level", level);
	int_replace(string, "$lines", lines);
	int_replace(string, "$tetris", ttr);
	int_replace(string, "$fps", fps);
	int_replace(string, "$debris", menu_debris);
	int_replace(string, "$stat0", statistics[0]);
	int_replace(string, "$stat1", statistics[1]);
	int_replace(string, "$stat2", statistics[2]);
	int_replace(string, "$stat3", statistics[3]);
	int_replace(string, "$stat4", statistics[4]);
	int_replace(string, "$stat5", statistics[5]);
	int_replace(string, "$stat6", statistics[6]);

	SDL_Color col = {.r = r, .g = g, .b = b};
	SDL_Surface *text = NULL;
	SDL_Rect rect = {.x = x, .y = y};
	text = TTF_RenderUTF8_Blended(skin->fonts[fontid], string, col);
	if (1 == alignx)	// to the center
		rect.x -= text->w / 2;
	else if (2 == alignx)	// to the right hand side
		rect.x -= text->w;
	if (1 == aligny)
		rect.y -= text->h / 2;
	else if (2 == aligny)
		rect.y -= text->h;
	SDL_BlitSurface(text, NULL, skin->screen, &rect);
	SDL_FreeSurface(text);
}

static void skin_executeFigure(struct Skin *skin, const char *statement)
{
	int id, x, y, centerx, centery, alpha;
	sscanf(statement, "%*s %d %d %d %d %d %d", &id, &x, &y, &centerx, &centery, &alpha);
	drawFigure(skin, figures[id], x, y, alpha, false, centerx, centery);
}

static void str_replace(char *where, const char *what, const char *with)
{
	char buff[256];
	char *pos = strstr(where, what);
	if (!pos)
		return;
	*pos = '\0';
	char *rstr = pos + strlen(what);
	strcpy(buff, rstr);
	strcat(where, with);
	strcat(where, buff);
}

static void int_replace(char *where, const char *what, int with)
{
	char buff[16];
	sprintf(buff, "%d", with);
	str_replace(where, what, buff);
}

static void drawFigure(struct Skin *skin, const struct Figure *fig, int x, int y, Uint8 alpha, bool active, bool centerx, bool centery)
{
	if (fig != NULL)
	{
		const struct Shape *shape = NULL;
		if (active)
			shape = &figures[0]->shape;
		else
			shape = getShape(fig->id);

		drawShape(skin, skin->screen, shape, x, y, fig->color, alpha, centerx, centery);
	}
}

static void drawShape(struct Skin *skin, SDL_Surface *target, const struct Shape *sh, int x, int y, enum FigureId color, Uint8 alpha, bool centerx, bool centery)
{
	int offset_x = 0, offset_y = 0;
	SDL_Rect srcrect;

	if (centerx || centery)
	{
		int minx, maxx, miny, maxy, w, h;
		getShapeDimensions(sh, &minx, &maxx, &miny, &maxy);
		if (centerx)
		{
			w = maxx - minx + 1;
			offset_x = (4 - w) * brick_size / 2 - minx * brick_size;
		}
		if (centery)
		{
			h = maxy - miny + 1;
			offset_y = (2 - h) * brick_size / 2 - miny * brick_size;
		}
	}

	for (int i = 0; i < (FIG_DIM * FIG_DIM); ++i)
	{
		SDL_Rect rect;
		rect.x = (i % FIG_DIM) * brick_size + x + offset_x;
		rect.y = (i / FIG_DIM) * brick_size + y + offset_y;

		SDL_Surface *block = getBlock(skin, color, sh->blockmap[i], &srcrect);
		SDL_SetAlpha(block, SDL_SRCALPHA, alpha);

		if (sh->blockmap[i])
			SDL_BlitSurface(block, &srcrect, target, &rect);
	}
}

static SDL_Surface *getBlock(struct Skin *skin, enum FigureId color, enum BlockOrientation orient, SDL_Rect *srcrect)
{
	if (srcrect)
	{
		srcrect->x = 0;
		srcrect->y = 0;
		srcrect->w = brick_size;
		srcrect->h = brick_size;
	}

	SDL_Surface *s = NULL;
	switch (skin->brickstyle)
	{
		case BS_SIMPLE:
			s = skin->bricksprite[color];
			break;
		case BS_ORIENTATION_BASED:
			if (srcrect)
				srcrect->x = orient * brick_size - brick_size;
			s = skin->bricksprite[color];
			break;
		default:
			s = NULL;
	}

	return s;
}
