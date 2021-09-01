#include <stdbool.h>
#include <stdio.h>

#include <SDL/SDL.h>
#include <SDL/SDL_image.h>
#include <SDL/SDL_ttf.h>

#include "skin.h"
#include "main.h"
#include "video.h"
#include "state_mainmenu.h"

#define log		/*printf*/

static void str_replace(char *where, const char *what, const char *with);
static void int_replace(char *where, const char *what, int with);
static void replace_all_vars(char *where);

static void drawFigure(struct Skin *skin, const struct Figure *fig, int x, int y, Uint8 alpha, bool active, bool centerx, bool centery);
static void drawShape(struct Skin *skin, SDL_Surface *target, const struct Shape *sh, int x, int y, enum FigureId color, Uint8 alpha, bool centerx, bool centery);
static SDL_Surface *getBlock(struct Skin *skin, enum FigureId color, enum BlockOrientation orient, SDL_Rect *srcrect);

static void skin_executeScript(struct Skin *skin, bool dynamic);
static void skin_executeStatement(struct Skin *skin, const char *statement, bool dynamic);
static void skin_executeBFg(struct Skin *skin, const char *statement, SDL_Surface **sdls);
static void skin_executeBg(struct Skin *skin, const char *statement);
static void skin_executeFg(struct Skin *skin, const char *statement);
static void skin_executeBganim(struct Skin *skin, const char *statement, bool dynamic);
static void skin_executeBoardxy(struct Skin *skin, const char *statement);
static void skin_executeTC(struct Skin *skin, const char *statement);
static void skin_executeBricksize(struct Skin *skin, const char *statement);
static void skin_executeBricksprite(struct Skin *skin, const char *statement);
static void skin_executeDebriscolor(struct Skin *skin, const char *statement);
static void skin_executeGhost(struct Skin *skin, const char *statement);
static void skin_executeShadow(struct Skin *skin, const char *statement);
static void skin_executeFont(struct Skin *skin, const char *statement);
static void skin_executeBox(struct Skin *skin, const char *statement);
static void skin_executeShape(struct Skin *skin, const char *statement);
static void skin_executeText(struct Skin *skin, const char *statement);
static void skin_executeBar(struct Skin *skin, const char *statement);
static void skin_executeFigure(struct Skin *skin, const char *statement);

void skin_initSkin(struct Skin *skin)
{
	skin->bgsheet = NULL;
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
	skin->debriscolor = FIGID_END;
	skin->ghost = 128;
	skin->bricksize = 12;
	skin->brickyoffset = 0;
	skin->bgrect.x = 0;
	skin->bgrect.y = 0;
	skin->bgrect.w = SCREEN_WIDTH;
	skin->bgrect.h = SCREEN_HEIGHT;
	skin->bgmode = BAM_REPLACE;
	skin->brick_shadow = NULL;
	skin->shadowx = 0;
	skin->shadowy = 0;
}

void skin_destroySkin(struct Skin *skin)
{
	if (skin->bgsheet)
	{
		SDL_FreeSurface(skin->bgsheet);
		skin->bgsheet = NULL;
	}
	if (skin->bg)
	{
		if (BAM_REPLACE != skin->bgmode)
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
	skin->brickstyle = BS_SIMPLE;
	skin->debriscolor = FIGID_END;
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
	SDL_Rect rect;
	skin->screen = screen;

	// draw delta drop update for smooth animation
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
	}

	// display background
	SDL_BlitSurface(skin->bg, &skin->bgrect, screen, NULL);

	skin_executeScript(skin, true);

	if (skin->brick_shadow)
	{
		// display shadow of board
		for (int i = BOARD_WIDTH * BOARD_HEIGHT - 1; i >= BOARD_WIDTH * INVISIBLE_ROW_COUNT; --i)
		{
			rect.x = (i % BOARD_WIDTH) * skin->bricksize + skin->boardx + skin->shadowx;
			rect.y = (i / BOARD_WIDTH - INVISIBLE_ROW_COUNT) * skin->bricksize + skin->boardy - skin->brickyoffset + skin->shadowy;
			if (board[i].orientation != BO_EMPTY)
			{
				int sthAboveOffset = ((i - BOARD_WIDTH) >= 0 && (board[i - BOARD_WIDTH].orientation != BO_EMPTY)) ?
										skin->brickyoffset : 0;
				rect.y += sthAboveOffset;
				SDL_Rect srcrect = {.x = 0, .y = 0};
				srcrect.w = skin->bricksize;
				srcrect.h = skin->bricksize + skin->brickyoffset - sthAboveOffset;
				SDL_BlitSurface(skin->brick_shadow, &srcrect, screen, &rect);
			}
		}

		// display shadow of active figure
		if (figures[0] != NULL)
		{
			int x = skin->boardx + skin->bricksize * figures[0]->x + skin->shadowx;
			int y = skin->boardy + skin->bricksize * (figures[0]->y - INVISIBLE_ROW_COUNT) - skin->brickyoffset + skin->shadowy;
			if (smoothanim)
				y += draw_delta_drop;
			for (int i = FIG_DIM * FIG_DIM - 1; i >= 0; --i)
			{
				if (figures[0]->shape.blockmap[i])
				{
					int sthAboveOffset = ((i - FIG_DIM) >= 0 && figures[0]->shape.blockmap[i - FIG_DIM]) ?
											skin->brickyoffset : 0;
					rect.x = (i % FIG_DIM) * skin->bricksize + x;
					rect.y = (i / FIG_DIM) * skin->bricksize + y + sthAboveOffset;
					SDL_Rect srcrect = {.x = 0, .y = 0};
					srcrect.w = skin->bricksize;
					srcrect.h = skin->bricksize + skin->brickyoffset - sthAboveOffset;
					SDL_BlitSurface(skin->brick_shadow, &srcrect, screen, &rect);
				}
			}
		}
	}

	// display board
	for (int i = BOARD_WIDTH * BOARD_HEIGHT - 1; i >= BOARD_WIDTH * INVISIBLE_ROW_COUNT; --i)
	{
		rect.x = (i % BOARD_WIDTH) * skin->bricksize + skin->boardx;
		rect.y = (i / BOARD_WIDTH - INVISIBLE_ROW_COUNT) * skin->bricksize + skin->boardy - skin->brickyoffset;
		if (board[i].orientation != BO_EMPTY)
		{
			SDL_Rect srcrect;
			SDL_Surface *block;
			if (skin->debriscolor >= FIGID_END)
				block = getBlock(skin, board[i].color, board[i].orientation, &srcrect);
			else
				block = getBlock(skin, skin->debriscolor, board[i].orientation, &srcrect);
			SDL_SetAlpha(block, SDL_SRCALPHA, 255);
			SDL_BlitSurface(block, &srcrect, screen, &rect);
		}
	}

	// display the active figure
	if (figures[0] != NULL)
	{
		int x = skin->boardx + skin->bricksize * figures[0]->x;
		int y = skin->boardy + skin->bricksize * (figures[0]->y - INVISIBLE_ROW_COUNT) - skin->brickyoffset;
		if (smoothanim)
			y += draw_delta_drop;
		drawFigure(skin, figures[0], x, y, 255, true, false, false);
	}

	// fix brick display directly above the active figure
	if (skin->brickyoffset > 0)
	{
		if (figures[0] != NULL)
		{
			for (int x = 0; x < FIG_DIM; ++x)
			{
				int top = FIG_DIM;
				for (int y = 0; y < FIG_DIM; ++y)
				{
					if (BO_EMPTY != figures[0]->shape.blockmap[y * FIG_DIM + x])
					{
						top = y - 1;
						break;
					}
				}
				if (FIG_DIM != top)
				{
					// redraw the whole column
					for (int bricky = top + figures[0]->y; bricky >= INVISIBLE_ROW_COUNT; --bricky)
					{
						int brickx = x + figures[0]->x;
						int brick_index = bricky * BOARD_WIDTH + brickx;
						if (brick_index >= 0 && board[brick_index].orientation != BO_EMPTY)
						{
							SDL_Rect srcrect;
							SDL_Surface *block;
							if (skin->debriscolor >= FIGID_END)
								block = getBlock(skin, board[brick_index].color, board[brick_index].orientation, &srcrect);
							else
								block = getBlock(skin, skin->debriscolor, board[brick_index].orientation, &srcrect);
							SDL_SetAlpha(block, SDL_SRCALPHA, 255);
							rect.x = skin->boardx + skin->bricksize * brickx;
							rect.y = skin->boardy + skin->bricksize * (bricky - INVISIBLE_ROW_COUNT) - skin->brickyoffset;
							SDL_BlitSurface(block, &srcrect, screen, &rect);
						}
					}
				}
			}
		}
	}

	// display the ghost figure
	if (figures[0] != NULL && skin->ghost > 0)
	{
		int tfy = figures[0]->y;
		while (!isFigureColliding())
			++figures[0]->y;
		if (tfy != figures[0]->y)
			--figures[0]->y;
		if ((figures[0]->y - tfy) >= FIG_DIM)
		{
			int x = skin->boardx + skin->bricksize * figures[0]->x;
			int y = skin->boardy + skin->bricksize * (figures[0]->y - INVISIBLE_ROW_COUNT) - skin->brickyoffset;
			drawFigure(skin, figures[0], x, y, skin->ghost, true, false, false);
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
	else if (!strcmp(cmd, "bganim"))
	{
		skin_executeBganim(skin, statement, dynamic);
	}
	else if (!strcmp(cmd, "boardxy"))
	{
		if (dynamic) return;
		skin_executeBoardxy(skin, statement);
	}
	else if (!strcmp(cmd, "tc"))
	{
		if (dynamic) return;
		skin_executeTC(skin, statement);
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
	else if (!strcmp(cmd, "debriscolor"))
	{
		if (dynamic) return;
		skin_executeDebriscolor(skin, statement);
	}
	else if (!strcmp(cmd, "ghost"))
	{
		if (dynamic) return;
		skin_executeGhost(skin, statement);
	}
	else if (!strcmp(cmd, "shadow"))
	{
		if (dynamic) return;
		skin_executeShadow(skin, statement);
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
	else if (!strcmp(cmd, "bar"))
	{
		if (!dynamic) return;
		skin_executeBar(skin, statement);
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
	skin_executeBFg(skin, statement, &skin->bgsheet);
	skin->bg = skin->bgsheet;
}

static void skin_executeFg(struct Skin *skin, const char *statement)
{
	skin_executeBFg(skin, statement, &skin->fg);
}

static void skin_executeBganim(struct Skin *skin, const char *statement, bool dynamic)
{
	static int frame_duration = 1000;
	static Uint32 last_redraw = 0;
	static int index = 0;
	static int bgsheet_w = 1;
	static int bgsheet_h = 1;
	char buff[256];
	if (dynamic)
	{
		// executed each frame redraw
		Uint32 ct = SDL_GetTicks();
		Uint32 diff = ct - last_redraw;
		if (diff > frame_duration)
		{
			Uint32 total_fd = (diff / frame_duration) * frame_duration;
			last_redraw += total_fd;
			diff -= total_fd;
			index = (index + 1) % (bgsheet_w * bgsheet_h);
			if (BAM_REPLACE == skin->bgmode)
			{
				skin->bgrect.x = (index % bgsheet_w) * SCREEN_WIDTH;
				skin->bgrect.y = (index / bgsheet_w) * SCREEN_HEIGHT;
			}
		}
		if (BAM_BLEND == skin->bgmode)
		{
			SDL_Rect r1, r2;
			r1.w = r2.w = SCREEN_WIDTH;
			r1.h = r2.h = SCREEN_HEIGHT;
			r1.x = (index % bgsheet_w) * SCREEN_WIDTH;
			r1.y = (index / bgsheet_w) * SCREEN_HEIGHT;
			int index_next = (index + 1) % (bgsheet_w * bgsheet_h);
			r2.x = (index_next % bgsheet_w) * SCREEN_WIDTH;
			r2.y = (index_next / bgsheet_w) * SCREEN_HEIGHT;
			SDL_SetAlpha(skin->bgsheet, 0, 255);
			SDL_BlitSurface(skin->bgsheet, &r1, skin->bg, NULL);
			SDL_SetAlpha(skin->bgsheet, SDL_SRCALPHA, (Uint8)(255 * diff / frame_duration));
			SDL_BlitSurface(skin->bgsheet, &r2, skin->bg, NULL);
		}
	}
	else
	{
		// initialization
		frame_duration = 1000;
		sscanf(statement, "%*s %s %d", buff, &frame_duration);
		log("bganim %s %d\n", buff, frame_duration);
		if (!strcmp(buff, "blend"))
		{
			skin->bgmode = BAM_BLEND;
			Uint32 bpp = skin->bgsheet->format->BitsPerPixel;
			Uint32 rm = skin->bgsheet->format->Rmask;
			Uint32 bm = skin->bgsheet->format->Bmask;
			Uint32 gm = skin->bgsheet->format->Gmask;
			Uint32 am = skin->bgsheet->format->Amask;
			skin->bg = SDL_CreateRGBSurface(0, SCREEN_WIDTH, SCREEN_HEIGHT, bpp, rm, gm, bm, am);
		}
		else
		{
			skin->bgmode = BAM_REPLACE;
		}
		last_redraw = SDL_GetTicks();
		bgsheet_w = skin->bgsheet->w / SCREEN_WIDTH;
		bgsheet_h = skin->bgsheet->h / SCREEN_HEIGHT;
	}
}

static void skin_executeBoardxy(struct Skin *skin, const char *statement)
{
	sscanf(statement, "%*s %d %d", &skin->boardx, &skin->boardy);
	log("boardxy %d %d\n", skin->boardx, skin->boardy);
}

static void skin_executeTC(struct Skin *skin, const char *statement)
{
	int colorid, alpha, r, g, b;
	sscanf(statement, "%*s %d %d %d %d %d", &colorid, &alpha, &r, &g, &b);
	log("tc %d %d %d %d %d\n", colorid, alpha, r, g, b);
	skin->color_alphas[colorid] = alpha;
	SDL_PixelFormat *f = screen->format;
	skin->colors[colorid] = SDL_MapRGB(f, r, g, b);
}

static void skin_executeBricksize(struct Skin *skin, const char *statement)
{
	sscanf(statement, "%*s %d %d", &skin->bricksize, &skin->brickyoffset);
	log("bricksize %d %d\n", skin->bricksize, skin->brickyoffset);
	brick_size = skin->bricksize;
	draw_delta_drop = -brick_size;
}

static void skin_executeBricksprite(struct Skin *skin, const char *statement)
{
	skin_executeBFg(skin, statement, &skin->bricksprite[FIGID_GRAY]);
	int s = skin->bricksize;
	int oy = skin->brickyoffset;
	if ((s == skin->bricksprite[FIGID_GRAY]->w) &&
		((s + oy) == skin->bricksprite[FIGID_GRAY]->h))
	{
		skin->brickstyle = BS_SIMPLE;
	}
	else if (((s + oy) == skin->bricksprite[FIGID_GRAY]->h) &&
			(ORIENTATION_NUM * s == skin->bricksprite[FIGID_GRAY]->w))
	{
		skin->brickstyle = BS_ORIENTATION_BASED;
	}
	else if ((FIGID_GRAY * (s + oy) == skin->bricksprite[FIGID_GRAY]->h) &&
			(s == skin->bricksprite[FIGID_GRAY]->w))
	{
		skin->brickstyle = BS_FIGUREWISE;
	}
	else
	{
		exit(ERROR_SCRIPT);
	}

	// brick dyeing
	SDL_PixelFormat *f = screen->format;
	for (int i = 0; i < FIGID_GRAY; ++i)
	{
		SDL_SetAlpha(skin->bricksprite[FIGID_GRAY], SDL_SRCALPHA, skin->color_alphas[i]);
		skin->bricksprite[i] = SDL_CreateRGBSurface(0, skin->bricksprite[FIGID_GRAY]->w, skin->bricksprite[FIGID_GRAY]->h, f->BitsPerPixel, f->Rmask, f->Gmask, f->Bmask, 0);
		SDL_FillRect(skin->bricksprite[i], NULL, skin->colors[i]);
		SDL_BlitSurface(skin->bricksprite[FIGID_GRAY], NULL, skin->bricksprite[i], NULL);
	}
	SDL_SetAlpha(skin->bricksprite[FIGID_GRAY], SDL_SRCALPHA, 128);
}

static void skin_executeDebriscolor(struct Skin *skin, const char *statement)
{
	sscanf(statement, "%*s %d", &skin->debriscolor);
	log("debriscolor %d\n", skin->debriscolor);
}

static void skin_executeGhost(struct Skin *skin, const char *statement)
{
	sscanf(statement, "%*s %d", &skin->ghost);
	log("ghost %d\n", skin->ghost);
}

static void skin_executeShadow(struct Skin *skin, const char *statement)
{
	int r, g, b, a;
	sscanf(statement, "%*s %d %d %d %d %d %d", &skin->shadowx, &skin->shadowy, &r, &g, &b, &a);
	log("shadow %d %d %d %d %d %d\n", skin->shadowx, skin->shadowy, r, g, b, a);

	int bw = skin->bricksize;
	int bh = skin->bricksize + skin->brickyoffset;
	int bpp = skin->bg->format->BitsPerPixel;
	Uint32 mr = skin->bg->format->Rmask;
	Uint32 mg = skin->bg->format->Gmask;
	Uint32 mb = skin->bg->format->Bmask;
	Uint32 ma = skin->bg->format->Amask;
	skin->brick_shadow = SDL_CreateRGBSurface(0, bw, bh, bpp, mr, mg, mb, ma);
	Uint32 shadow_color = SDL_MapRGB(skin->brick_shadow->format, r, g, b);
	SDL_FillRect(skin->brick_shadow, NULL, shadow_color);
	SDL_SetAlpha(skin->brick_shadow, SDL_SRCALPHA, a);
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
	SDL_Surface *bg = skin->bgsheet;
	SDL_Surface *ph = SDL_CreateRGBSurface(0, w, h, bg->format->BitsPerPixel, bg->format->Rmask, bg->format->Gmask, bg->format->Bmask, 0);
	SDL_FillRect(ph, NULL, SDL_MapRGB(ph->format, r, g, b));
	SDL_SetAlpha(ph, SDL_SRCALPHA, alpha);
	int bgw = bg->w / SCREEN_WIDTH;
	int bgh = bg->h / SCREEN_HEIGHT;
	for (int i = 0; i < bgw * bgh; ++i)
	{
		SDL_Rect rect;
		rect.x = (i % bgw) * SCREEN_WIDTH + x;
		rect.y = (i / bgw) * SCREEN_HEIGHT + y;
		SDL_BlitSurface(ph, NULL, bg, &rect);
	}
	SDL_FreeSurface(ph);
}

static void skin_executeShape(struct Skin *skin, const char *statement)
{
	int id, x, y, centerx, centery, alpha;
	sscanf(statement, "%*s %d %d %d %d %d %d", &id, &x, &y, &centerx, &centery, &alpha);

	int bgw = skin->bgsheet->w / SCREEN_WIDTH;
	int bgh = skin->bgsheet->h / SCREEN_HEIGHT;
	for (int i = 0; i < bgw * bgh; ++i)
	{
		int xx = (i % bgw) * SCREEN_WIDTH + x;
		int yy = (i / bgw) * SCREEN_HEIGHT + y;
		drawShape(skin, skin->bgsheet, getShape(id), xx, yy, FIGID_GRAY, alpha, centerx, centery);
	}
}

static void skin_executeText(struct Skin *skin, const char *statement)
{
	int fontid, x, y, alignx, aligny, r, g, b;
	char string[256];
	sscanf(statement, "%*s %d %d %d %d %d %d %d %d \"%[^\"\n]", &fontid, &x, &y,
			&alignx, &aligny, &r, &g, &b, string);

	replace_all_vars(string);

	SDL_Color col = {.r = r, .g = g, .b = b};
	SDL_Surface *text = NULL;
	SDL_Rect rect = {.x = x, .y = y};
	text = TTF_RenderUTF8_Blended(skin->fonts[fontid], string, col);
	if (text)
	{
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
}

static void skin_executeBar(struct Skin *skin, const char *statement)
{
	int x, y, w, h, limit, dir, rl, gl, bl, al, rr, gr, br, ar;
	char var[256];
	sscanf(statement, "%*s %d %d %d %d %s %d %d %d %d %d %d %d %d %d %d", &x, &y,
			&w, &h, var, &limit, &dir, &rl, &gl, &bl, &al, &rr, &gr, &br, &ar);

	replace_all_vars(var);
	int value = 0;
	sscanf(var, "%d", &value);
	if (value > limit)
		value = limit;

	SDL_Surface *bar = SDL_CreateRGBSurface(SDL_SRCALPHA,
		w, h, 32, 0x000000ff, 0x0000ff00, 0x00ff0000, 0xff000000);

	SDL_Rect rect = { .x = 0, .y = 0, .w = 0, .h = 0 };
	Uint32 col = 0;
	if (0 == dir)	// left to right
	{
		rect.x = 0;
		rect.y = 0;
		rect.w = value * w / limit;
		rect.h = h;
	}
	else if (1 == dir)	// right to left
	{
		rect.w = value * w / limit;
		rect.h = h;
		rect.x = w - rect.w;
		rect.y = 0;
	}
	else if (2 == dir)	// up to down
	{
		rect.x = 0;
		rect.y = 0;
		rect.w = w;
		rect.h = value * h / limit;
	}
	else if (3 == dir)	// down to up
	{
		rect.w = w;
		rect.h = value * h / limit;
		rect.x = 0;
		rect.y = h - rect.h;
	}
	col = SDL_MapRGBA(bar->format, rl, gl, bl, al);
	SDL_FillRect(bar, &rect, col);

	if (0 == dir)	// left to right
	{
		rect.x = rect.w;
		rect.y = 0;
		rect.w = w - rect.w;
		rect.h = h;
	}
	else if (1 == dir)	// right to left
	{
		rect.x = 0;
		rect.y = 0;
		rect.w = w - rect.w;
		rect.h = h;
	}
	else if (2 == dir)	// up to down
	{
		rect.x = 0;
		rect.y = rect.h;
		rect.w = w;
		rect.h = h - rect.h;
	}
	else if (3 == dir)	// down to up
	{
		rect.w = w;
		rect.h = h - rect.h;
		rect.x = 0;
		rect.y = 0;
	}
	col = SDL_MapRGBA(bar->format, rr, gr, br, ar);
	SDL_FillRect(bar, &rect, col);

	rect.x = x;
	rect.y = y;
	rect.w = w;
	rect.h = h;
	SDL_BlitSurface(bar, NULL, skin->screen, &rect);
	SDL_FreeSurface(bar);
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

static void replace_all_vars(char *where)
{
	int_replace(where, "$hiscore", hiscore);
	int_replace(where, "$score", score);
	int_replace(where, "$level", level);
	int_replace(where, "$lines", lines);
	int_replace(where, "$tetris", ttr);
	int_replace(where, "$fps", fps);
	int_replace(where, "$debris", menu_debris);
	int_replace(where, "$stat0", statistics[0]);
	int_replace(where, "$stat1", statistics[1]);
	int_replace(where, "$stat2", statistics[2]);
	int_replace(where, "$stat3", statistics[3]);
	int_replace(where, "$stat4", statistics[4]);
	int_replace(where, "$stat5", statistics[5]);
	int_replace(where, "$stat6", statistics[6]);
	str_replace(where, "$lcttop", lctext_top);
	str_replace(where, "$lctmid", lctext_mid);
	str_replace(where, "$lctbot", lctext_bot);
	str_replace(where, "$timer", gametimer);
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
			offset_x = (4 - w) * skin->bricksize / 2 - minx * skin->bricksize;
		}
		if (centery)
		{
			h = maxy - miny + 1;
			offset_y = (2 - h) * skin->bricksize / 2 - miny * skin->bricksize;
		}
	}

	if (255 == alpha || 0 == skin->brickyoffset)
	{
		for (int i = FIG_DIM * FIG_DIM - 1; i >= 0; --i)
		{
			SDL_Rect rect;
			rect.x = (i % FIG_DIM) * skin->bricksize + x + offset_x;
			rect.y = (i / FIG_DIM) * skin->bricksize + y + offset_y;

			SDL_Surface *block = getBlock(skin, color, sh->blockmap[i], &srcrect);
			SDL_SetAlpha(block, SDL_SRCALPHA, alpha);
			if (sh->blockmap[i])
				SDL_BlitSurface(block, &srcrect, target, &rect);
		}
	}
	else
	{
		int w = skin->bricksize * FIG_DIM;
		int h = skin->bricksize * FIG_DIM + skin->brickyoffset;
		SDL_Surface *pixmap = SDL_CreateRGBSurface(0, w, h, 32, 0x000000ff, 0x0000ff00, 0x00ff0000, 0xff000000);
		Uint32 full_transparent = SDL_MapRGBA(pixmap->format, 0, 0, 0, 0);
		SDL_FillRect(pixmap, NULL, full_transparent);
		Uint32 alpha_transparent = SDL_MapRGBA(pixmap->format, 0, 0, 0, alpha);
		for (int i = FIG_DIM * FIG_DIM - 1; i >= 0; --i)
		{
			SDL_Rect rect;
			rect.x = (i % FIG_DIM) * skin->bricksize;
			rect.y = (i / FIG_DIM) * skin->bricksize;
			rect.w = skin->bricksize;
			rect.h = skin->bricksize + skin->brickyoffset;
			SDL_Surface *block = getBlock(skin, color, sh->blockmap[i], &srcrect);
			if (sh->blockmap[i])
			{
				SDL_Surface *block_rgba = SDL_CreateRGBSurface(0, skin->bricksize, skin->bricksize + skin->brickyoffset, 32, 0x000000ff, 0x0000ff00, 0x00ff0000, 0xff000000);
				SDL_SetAlpha(block, 0, 0);
				SDL_BlitSurface(block, &srcrect, block_rgba, NULL);
				SDL_FillRect(pixmap, &rect, alpha_transparent);
				SDL_SetAlpha(block_rgba, SDL_SRCALPHA, 0);
				SDL_BlitSurface(block_rgba, &srcrect, pixmap, &rect);
				SDL_FreeSurface(block_rgba);
			}
		}
		SDL_Rect rect;
		rect.x = x + offset_x;
		rect.y = y + offset_y;
		SDL_BlitSurface(pixmap, NULL, target, &rect);
		SDL_FreeSurface(pixmap);
	}
}

static SDL_Surface *getBlock(struct Skin *skin, enum FigureId color, enum BlockOrientation orient, SDL_Rect *srcrect)
{
	if (srcrect)
	{
		srcrect->x = 0;
		srcrect->y = 0;
		srcrect->w = skin->bricksize;
		srcrect->h = skin->bricksize + skin->brickyoffset;
	}

	SDL_Surface *s = NULL;
	switch (skin->brickstyle)
	{
		case BS_SIMPLE:
			s = skin->bricksprite[color];
			break;
		case BS_ORIENTATION_BASED:
			if (srcrect)
				srcrect->x = orient * srcrect->w - srcrect->w;
			s = skin->bricksprite[color];
			break;
		case BS_FIGUREWISE:
			if (srcrect)
				srcrect->y = (color % FIGID_GRAY) * srcrect->h;
			s = skin->bricksprite[color];
			break;
		default:
			s = NULL;
	}

	return s;
}
