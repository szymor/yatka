
#include <stdio.h>
#include <stdbool.h>

#include <SDL/SDL.h>
#include <SDL/SDL_image.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>

#include "state_mainmenu.h"
#include "main.h"
#include "joystick.h"
#include "video.h"
#include "sound.h"
#include "data_persistence.h"

#define MAX_SKIN_NUM		32
#define MAX_SKIN_NAME_LEN	16
#define MAX_SKIN_PATH_LEN	256

/* ─── menu hierarchy ─── */

enum MenuLevel
{
	ML_TOP,
	ML_GAME_SETUP,
	ML_SETTINGS,
	ML_KEYCONFIG,
};

enum TopEntry
{
	TE_GAMESTART,
	TE_SETTINGS,
	TE_EXIT,
	TE_END
};

enum GameSetupEntry
{
	GSE_LEVEL,
	GSE_INITIAL_DEBRIS,
	GSE_DEBRIS_CHANCE,
	GSE_AUTO_DEBRIS,
	GSE_END
};

enum SettingsEntry
{
	SET_SKIN,
	SET_KEYCONFIG,
	SET_END
};

enum KeyId
{
	KI_LEFT,
	KI_RIGHT,
	KI_SOFTDROP,
	KI_HARDDROP,
	KI_ROTATE_CW,
	KI_ROTATE_CCW,
	KI_HOLD,
	KI_PAUSE,
	KI_QUIT,
	KI_END
};

struct SkinEntry
{
	char name[MAX_SKIN_NAME_LEN];
	char path[MAX_SKIN_PATH_LEN];
};

int menu_skinnum = 0;
struct SkinEntry menu_skinentries[MAX_SKIN_NUM];
int menu_gamemode = GM_MARATHON;
int menu_skin = 0;
int menu_level = 0;
int menu_debris = 0;
int menu_debris_chance = 8;
int menu_auto_debris = 0;

static char custom_skin_dir[256] = "";
static char menu_error[256] = "";

/* ─── navigation state ─── */
static enum MenuLevel cur_level = ML_TOP;
static int cur_top = TE_GAMESTART;
static int cur_game_setup = GSE_LEVEL;
static int cur_settings = SET_SKIN;
static int cur_keycfg = 0;

/* key config working copy (discarded on cancel) */
static SDLKey kc_keys[KI_END];
static char kc_labels[KI_END][32] = {
	"LEFT",
	"RIGHT",
	"SOFT DROP",
	"HARD DROP",
	"ROTATE CLOCKWISE",
	"ROTATE COUNTERCLOCKWISE",
	"HOLD",
	"PAUSE",
	"QUIT"
};

/* ─── cached background ─── */
static SDL_Surface *menu_bg = NULL;

/* ─── forward declarations ─── */
static void draw_text(int x, int y, const char *string, int alignx, int aligny);
static void draw_text_col(int x, int y, const char *string,
                          int alignx, int aligny,
                          Uint8 r, Uint8 g, Uint8 b);
static void draw_top_menu(void);
static void draw_game_setup(void);
static void draw_settings(void);
static void draw_keyconfig(void);
static void load_menu_bg(const char *skin_path);
static SDLKey get_key(void);
static void up(void);
static void down(void);
static void left(void);
static void right(void);
static void action(void);
static void level_back(void);

/* ─────────────────────────────────────────────
 *  Background
 * ───────────────────────────────────────────── */

static void load_menu_bg(const char *skin_path)
{
	if (menu_bg)
	{
		SDL_FreeSurface(menu_bg);
		menu_bg = NULL;
	}

	/* skin_path is "skins/<name>/skin.lua" — derive directory */
	char dir[512];
	strncpy(dir, skin_path, sizeof dir - 1);
	dir[sizeof dir - 1] = '\0';
	char *p = dir + strlen(dir) - 1;
	while (p > dir && *p != '/') --p;
	if (*p == '/') *(p + 1) = '\0';
	else { dir[0] = '\0'; return; }

	char bg_path[640];
	snprintf(bg_path, sizeof bg_path, "%sbg.png", dir);
	menu_bg = IMG_Load(bg_path);
}

/* ─────────────────────────────────────────────
 *  Key capture
 * ───────────────────────────────────────────── */

static SDLKey get_key(void)
{
	SDL_Event event;
	while (SDL_WaitEvent(&event))
	{
		if (SDL_KEYDOWN == event.type)
			return event.key.keysym.sym;
		if (SDL_QUIT == event.type)
			exit(0);
	}
	return SDLK_ESCAPE;
}

/* ─────────────────────────────────────────────
 *  Draw helpers
 * ───────────────────────────────────────────── */

static void draw_text(int x, int y, const char *string, int alignx, int aligny)
{
	draw_text_col(x, y, string, alignx, aligny, 255, 255, 255);
}

static void draw_text_col(int x, int y, const char *string,
                          int alignx, int aligny,
                          Uint8 r, Uint8 g, Uint8 b)
{
	SDL_Color col = { .r = r, .g = g, .b = b };
	SDL_Surface *ts = TTF_RenderUTF8_Blended(arcade_font, string, col);
	if (!ts) return;
	SDL_Rect rect = { .x = x, .y = y };
	if (1 == alignx) rect.x -= ts->w / 2;
	else if (2 == alignx) rect.x -= ts->w;
	if (1 == aligny) rect.y -= ts->h / 2;
	else if (2 == aligny) rect.y -= ts->h;
	SDL_BlitSurface(ts, NULL, screen, &rect);
	SDL_FreeSurface(ts);
}

/* ──────── auto‑debris name helper ──────── */

static const char *auto_debris_name(int val)
{
	static char buff[32];
	Uint32 val_to_sec = val * AUTO_DEBRIS_TIME_UNIT / 1000;
	if (0 == val)
		strcpy(buff, "OFF");
	else if (val_to_sec < 60)
		snprintf(buff, sizeof buff, "%d seconds", val_to_sec);
	else if (val_to_sec == 60)
		strcpy(buff, "1 minute");
	else if (val_to_sec % 60 == 0)
		snprintf(buff, sizeof buff, "%d minutes", val_to_sec / 60);
	else
		snprintf(buff, sizeof buff, "%d min %d sec", val_to_sec / 60, val_to_sec % 60);
	return buff;
}

/* ─────────────────────────────────────────────
 *  Layout constants
 *
 *   0                128                       320
 *   ├── left panel ──┤├────── right panel ──────┤
 *   LX=8         LV_X=120  RX=132
 *   (name)       (value)   (description)
 * ───────────────────────────────────────────── */

#define LX     8
#define LV_X   120
#define RX     132

#define LN_H   10    /* height of a name row          */
#define VL_OFF  7    /* vertical offset value → below */
#define ENTRY_H 17   /* LN_H + VL_OFF = per-entry step */

#define SUB_Y  28    /* sub‑menu header row */

/* ─────────────────────────────────────────────
 *  Draw sub‑functions
 * ───────────────────────────────────────────── */

static void draw_top_menu(void)
{
	int const Y0 = 36;

	static const char *names[TE_END] = {
		"GAME START",
		"SETTINGS",
		"EXIT"
	};

	static const char *detail[TE_END] = {
		"Choose game mode, level,\n"
		"debris and other options\n"
		"before starting the game.",
		"Select a visual theme for\n"
		"the game and configure\n"
		"keyboard bindings.",
		"Exits the program."
	};

	for (int i = 0; i < TE_END; ++i)
	{
		int row = Y0 + i * LN_H;

		if (i == cur_top)
			draw_text_col(LX, row, names[i], 0, 0, 255, 255, 0);
		else
			draw_text(LX, row, names[i], 0, 0);

		/* description in right panel */
		if (i == cur_top)
		{
			int dy = 36;
			for (int k = 0; detail[i][k]; )
			{
				char line[32];
				int n = 0;
				while (detail[i][k] && n < 31)
				{
					if (detail[i][k] == '\n') { ++k; break; }
					line[n++] = detail[i][k++];
				}
				line[n] = '\0';
				draw_text(RX, dy, line, 0, 0);
				dy += 9;
			}
		}
	}
}

static void draw_game_setup(void)
{
	int const Y0 = 36;

	draw_text(LX, SUB_Y, "GAME SETUP", 0, 0);

	static const char *gs_names[GSE_END] = {
		"Level", "Initial Debris", "Debris Chance", "Auto Debris"
	};
	static const char *gs_descs[GSE_END] = {
		"Starting speed level.",
		"Garbage rows at start.",
		"Density of garbage lines.",
		"Periodic garbage."
	};

	for (int i = 0; i < GSE_END; ++i)
	{
		int row = Y0 + i * ENTRY_H;
		char val[16];

		switch (i)
		{
			case GSE_LEVEL:
				snprintf(val, sizeof val, "%d", menu_level);
				break;
			case GSE_INITIAL_DEBRIS:
				snprintf(val, sizeof val, "%d", menu_debris);
				break;
			case GSE_DEBRIS_CHANCE:
				snprintf(val, sizeof val, "%d", menu_debris_chance);
				break;
			case GSE_AUTO_DEBRIS:
				strcpy(val, auto_debris_name(menu_auto_debris));
				break;
		}

		if (i == cur_game_setup)
		{
			char buf[32];
			snprintf(buf, sizeof buf, "< %s >", val);
			draw_text_col(LX, row, gs_names[i], 0, 0, 255, 255, 0);
			draw_text_col(LV_X, row + VL_OFF, buf, 2, 0, 255, 255, 0);
			draw_text(RX, 36, gs_descs[i], 0, 0);
		}
		else
		{
			draw_text(LX, row, gs_names[i], 0, 0);
			draw_text(LV_X, row + VL_OFF, val, 2, 0);
		}
	}

	draw_text(RX, 100, "Press ENTER to start.", 0, 0);
	draw_text(RX, 109, "Press ESC to go back.", 0, 0);
}

static void draw_settings(void)
{
	int const Y0 = 36;

	static const char *names[SET_END] = { "Skin", "Key Config" };

	draw_text(LX, SUB_Y, "SETTINGS", 0, 0);

	for (int i = 0; i < SET_END; ++i)
	{
		int row = Y0 + i * ENTRY_H;
		if (i == cur_settings)
		{
			draw_text_col(LX, row, names[i], 0, 0, 255, 255, 0);
			if (SET_SKIN == i)
			{
				char buf[32];
				snprintf(buf, sizeof buf, "< %s >", menu_skinentries[menu_skin].name);
				draw_text_col(LV_X, row + VL_OFF, buf, 2, 0, 255, 255, 0);
			}
		}
		else
		{
			draw_text(LX, row, names[i], 0, 0);
			if (SET_SKIN == i)
				draw_text(LV_X, row + VL_OFF, menu_skinentries[menu_skin].name, 2, 0);
		}

		if (i == cur_settings)
		{
			switch (i)
			{
				case SET_SKIN:
					draw_text(RX, 36, "Visual theme, affects some", 0, 0);
					draw_text(RX, 45, "game rules and appearance.", 0, 0);
					break;
				case SET_KEYCONFIG:
					draw_text(RX, 36, "Configure keyboard bindings", 0, 0);
					draw_text(RX, 45, "for all game actions.", 0, 0);
					draw_text(RX, 58, "Press ENTER to configure.", 0, 0);
					break;
			}
		}
	}

	draw_text(RX, 76, "Press ESC to go back.", 0, 0);
}

static void draw_keyconfig(void)
{
	/*
	 * Key-config uses the full screen width so even the longest label
	 * ("ROTATE COUNTERCLOCKWISE") and the longest key name fit without
	 * overlap.  Description text lives at the very bottom of the screen.
	 */
#define KC_LX      24    /* symmetric 24 px margins                   */
#define KCENTRY_H  10
#define KC_VX      296   /* 320 - 24 = 296                            */
	int const Y0 = 38;

	draw_text(KC_LX, SUB_Y, "KEY CONFIGURATION", 0, 0);

	for (int i = 0; i < KI_END; ++i)
	{
		int row = Y0 + i * KCENTRY_H;
		if (i == cur_keycfg)
		{
			draw_text_col(KC_LX, row, kc_labels[i], 0, 0, 255, 255, 0);
			draw_text_col(KC_VX, row, SDL_GetKeyName(kc_keys[i]), 2, 0, 255, 255, 0);
		}
		else
		{
			draw_text(KC_LX, row, kc_labels[i], 0, 0);
			draw_text(KC_VX, row, SDL_GetKeyName(kc_keys[i]), 2, 0);
		}
	}

	/* description centred between the last entry and the screen bottom */
	draw_text(SCREEN_WIDTH / 2, 165, "Navigate with UP/DOWN.", 1, 0);
	draw_text(SCREEN_WIDTH / 2, 174, "Press ENTER to reassign", 1, 0);
	draw_text(SCREEN_WIDTH / 2, 183, "the selected key.", 1, 0);
	draw_text(SCREEN_WIDTH / 2, 196, "Press ESC to go back.", 1, 0);
#undef KC_LX
#undef KCENTRY_H
#undef KC_VX
}

/* ─────────────────────────────────────────────
 *  Screen render
 * ───────────────────────────────────────────── */

void mainmenu_updateScreen(void)
{
	/* 1. background */
	if (menu_bg)
		SDL_BlitSurface(menu_bg, NULL, screen, NULL);
	else
		SDL_FillRect(screen, NULL, 0);

	/* 2. semi-transparent overlay for readability */
	SDL_PixelFormat *fmt = screen->format;
	SDL_Surface *overlay = SDL_CreateRGBSurface(
		0, SCREEN_WIDTH, SCREEN_HEIGHT, fmt->BitsPerPixel,
		fmt->Rmask, fmt->Gmask, fmt->Bmask, 0);
	SDL_FillRect(overlay, NULL, SDL_MapRGB(fmt, 0, 0, 0));
	SDL_SetAlpha(overlay, SDL_SRCALPHA, 128);
	SDL_BlitSurface(overlay, NULL, screen, NULL);
	SDL_FreeSurface(overlay);

	/* 3. title */
	draw_text(SCREEN_WIDTH / 2, 12, "YATKA", 1, 0);

	/* 4. commit hash */
	draw_text(SCREEN_WIDTH - 2, SCREEN_HEIGHT - 2, xstr(COMMIT_HASH), 2, 2);

	/* 5. draw the active menu level */
	switch (cur_level)
	{
		case ML_TOP:       draw_top_menu();       break;
		case ML_GAME_SETUP: draw_game_setup();    break;
		case ML_SETTINGS:  draw_settings();       break;
		case ML_KEYCONFIG: draw_keyconfig();      break;
	}

	/* 6. error message */
	if (menu_error[0])
	{
		char buff[64];
		snprintf(buff, sizeof buff, "ERROR: %s", menu_error);
		draw_text(SCREEN_WIDTH / 2, SCREEN_HEIGHT - 16, buff, 1, 0);
	}

	flipScreenScaled();
}

/* ─────────────────────────────────────────────
 *  Input handlers
 * ───────────────────────────────────────────── */

static void up(void)
{
	playEffect(SE_CLICK);
	switch (cur_level)
	{
		case ML_TOP:        decMod(&cur_top, TE_END, false);          break;
		case ML_GAME_SETUP: decMod(&cur_game_setup, GSE_END, false);  break;
		case ML_SETTINGS:   decMod(&cur_settings, SET_END, false);     break;
		case ML_KEYCONFIG:  decMod(&cur_keycfg, KI_END, false);       break;
	}
}

static void down(void)
{
	playEffect(SE_CLICK);
	switch (cur_level)
	{
		case ML_TOP:        incMod(&cur_top, TE_END, false);          break;
		case ML_GAME_SETUP: incMod(&cur_game_setup, GSE_END, false);  break;
		case ML_SETTINGS:   incMod(&cur_settings, SET_END, false);     break;
		case ML_KEYCONFIG:  incMod(&cur_keycfg, KI_END, false);       break;
	}
}

static void left(void)
{
	playEffect(SE_CLICK);
	switch (cur_level)
	{
		case ML_GAME_SETUP:
			switch (cur_game_setup)
			{
				case GSE_LEVEL:
					if (menu_level > 0) --menu_level;
					break;
				case GSE_INITIAL_DEBRIS:
					if (menu_debris > 0) --menu_debris;
					break;
				case GSE_DEBRIS_CHANCE:
					if (menu_debris_chance > 1) --menu_debris_chance;
					break;
				case GSE_AUTO_DEBRIS:
					if (menu_auto_debris > 0) --menu_auto_debris;
					break;
			}
			break;
		case ML_SETTINGS:
			if (SET_SKIN == cur_settings)
			{
				decMod(&menu_skin, menu_skinnum, false);
				menu_error[0] = '\0';
				load_menu_bg(menu_skinentries[menu_skin].path);
			}
			break;
		default:
			break;
	}
}

static void right(void)
{
	playEffect(SE_CLICK);
	switch (cur_level)
	{
		case ML_GAME_SETUP:
			switch (cur_game_setup)
			{
				case GSE_LEVEL:
					if (menu_level < 9) ++menu_level;
					break;
				case GSE_INITIAL_DEBRIS:
					if (menu_debris < 15) ++menu_debris;
					break;
				case GSE_DEBRIS_CHANCE:
					if (menu_debris_chance < 9) ++menu_debris_chance;
					break;
				case GSE_AUTO_DEBRIS:
					if (menu_auto_debris < 18) ++menu_auto_debris;
					break;
			}
			break;
		case ML_SETTINGS:
			if (SET_SKIN == cur_settings)
			{
				incMod(&menu_skin, menu_skinnum, false);
				menu_error[0] = '\0';
				load_menu_bg(menu_skinentries[menu_skin].path);
			}
			break;
		default:
			break;
	}
}

static void level_back(void)
{
	playEffect(SE_CLICK);
	switch (cur_level)
	{
		case ML_TOP:
		{
			SDL_Event ev;
			memset(&ev, 0, sizeof ev);
			ev.type = SDL_QUIT;
			SDL_PushEvent(&ev);
		}
		break;
		case ML_GAME_SETUP:
			cur_level = ML_TOP;
			break;
		case ML_SETTINGS:
			cur_level = ML_TOP;
			break;
		case ML_KEYCONFIG:
			kleft   = kc_keys[KI_LEFT];
			kright  = kc_keys[KI_RIGHT];
			ksoftdrop = kc_keys[KI_SOFTDROP];
			kharddrop = kc_keys[KI_HARDDROP];
			krotatecw  = kc_keys[KI_ROTATE_CW];
			krotateccw = kc_keys[KI_ROTATE_CCW];
			khold   = kc_keys[KI_HOLD];
			kpause  = kc_keys[KI_PAUSE];
			kquit   = kc_keys[KI_QUIT];
			saveSettings();
			cur_level = ML_SETTINGS;
			break;
	}
}

static void action(void)
{
	playEffect(SE_CLICK);

	switch (cur_level)
	{
		case ML_TOP:
			switch (cur_top)
			{
				case TE_GAMESTART:
					cur_level = ML_GAME_SETUP;
					break;
				case TE_SETTINGS:
					cur_level = ML_SETTINGS;
					break;
				case TE_EXIT:
				{
					SDL_Event ev;
					memset(&ev, 0, sizeof ev);
					ev.type = SDL_QUIT;
					SDL_PushEvent(&ev);
				}
				break;
			}
			break;

		case ML_GAME_SETUP:
			skin_destroy(&gameskin);
			skin_init(&gameskin);
			if (!skin_loadSkin(&gameskin, menu_skinentries[menu_skin].path))
			{
				snprintf(menu_error, sizeof menu_error,
				         "No skin.lua in \"%s\"!", menu_skinentries[menu_skin].name);
				return;
			}
			menu_error[0] = '\0';
			resetGame();
			cur_level = ML_TOP;
			setGameState(GS_INGAME);
			break;

		case ML_SETTINGS:
			if (SET_KEYCONFIG == cur_settings)
			{
				SDLKey current[KI_END] = { kleft, kright, ksoftdrop, kharddrop,
				                           krotatecw, krotateccw, khold, kpause, kquit };
				memcpy(kc_keys, current, sizeof kc_keys);
				cur_keycfg = 0;
				cur_level = ML_KEYCONFIG;
			}
			break;

		case ML_KEYCONFIG:
		{
			draw_text(SCREEN_WIDTH / 2, 145, "Press a key...", 1, 0);
			flipScreenScaled();
			kc_keys[cur_keycfg] = get_key();
			break;
		}
	}
}

/* ─────────────────────────────────────────────
 *  Public API
 * ───────────────────────────────────────────── */

static void mainmenu_init_skinload_helper(DIR *dp, const char *maindir)
{
	struct dirent *ep;
	while ((ep = readdir(dp)))
	{
		if (!strcmp(ep->d_name, ".") || !strcmp(ep->d_name, ".."))
			continue;
		char skinlua_path[512];
		struct stat st;
		snprintf(skinlua_path, sizeof skinlua_path, "%s%s/skin.lua", maindir, ep->d_name);
		if (stat(skinlua_path, &st) != 0 || !S_ISREG(st.st_mode))
			continue;
		if (!strcmp(ep->d_name, "default"))
			menu_skin = menu_skinnum;
		strcpy(menu_skinentries[menu_skinnum].name, ep->d_name);
		strcpy(menu_skinentries[menu_skinnum].path, skinlua_path);
		++menu_skinnum;
	}
}

void mainmenu_init(void)
{
	DIR *dp;

	menu_skinnum = 0;
	snprintf(custom_skin_dir, sizeof custom_skin_dir, "%s/skins/", dirpath);

	dp = opendir(custom_skin_dir);
	if (dp)
	{
		mainmenu_init_skinload_helper(dp, custom_skin_dir);
		closedir(dp);
	}

	dp = opendir("skins/");
	if (dp)
	{
		mainmenu_init_skinload_helper(dp, "skins/");
		closedir(dp);
	}
	else
		perror("Couldn't open the main skin directory.");

	cur_level = ML_TOP;
	cur_top = TE_GAMESTART;
	cur_game_setup = GSE_LEVEL;
	cur_settings = SET_SKIN;
	cur_keycfg = 0;

	load_menu_bg(menu_skinentries[menu_skin].path);
}

void mainmenu_processInputEvents(void)
{
	SDL_Event event;

	if (!SDL_WaitEvent(&event))
		return;

	switch (event.type)
	{
		case SDL_JOYAXISMOTION:
			if ((event.jaxis.value < -JOY_THRESHOLD) || (event.jaxis.value > JOY_THRESHOLD))
			{
				if (event.jaxis.axis == 0)
				{
					if (event.jaxis.value < 0) left();
					else right();
				}
				if (event.jaxis.axis == 1)
				{
					if (event.jaxis.value < 0) up();
					else down();
				}
			}
			break;

		case SDL_JOYBUTTONDOWN:
			if (event.jbutton.button == JOY_PAUSE)
				action();
			if (event.jbutton.button == JOY_QUIT)
				level_back();
			break;

		case SDL_KEYDOWN:
			switch (event.key.keysym.sym)
			{
				case SDLK_UP:    up();    break;
				case SDLK_DOWN:  down();  break;
				case SDLK_LEFT:  left();  break;
				case SDLK_RIGHT: right(); break;
				default:
				{
					if (event.key.keysym.sym == kquit)
						level_back();
					else if (event.key.keysym.sym == kpause ||
					         event.key.keysym.sym == SDLK_RETURN)
						action();
				}
				break;
			}
			break;

		case SDL_QUIT:
			exit(0);
			break;
	}
}
