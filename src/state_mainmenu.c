
#include <stdio.h>
#include <string.h>
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
#include "randomizer.h"

/* MAX_SKIN_NUM, MAX_SKIN_NAME_LEN, MAX_SKIN_PATH_LEN and
 * struct SkinEntry are defined in state_mainmenu.h */

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
	GSE_GAMEMODE,
	GSE_LEVEL,
	GSE_INITIAL_DEBRIS,
	GSE_DEBRIS_CHANCE,
	GSE_AUTO_DEBRIS,
	GSE_END
};

enum SettingsEntry
{
	SET_SKIN,
	SET_SPEECH,
	SET_SMOOTHANIM,
	SET_TETROMINO_COLOR,
	SET_EASYSPIN,
	SET_LOCKDELAY,
	SET_DROPTYPE,
	SET_RANDOMIZER,
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

/* ─── skin readme cache with word‑wrap + auto‑scroll ─── */
#define README_MAX      2048
#define README_LINES_MAX 120
#define README_VISIBLE   13
#define README_SCROLL_DELAY_MS  6000
#define README_SCROLL_INTERVAL  800

static char readme_data[README_MAX] = "";
static char readme_lines_buf[README_MAX];
static const char *readme_lines[README_LINES_MAX];
static int readme_line_count = 0;
static int readme_scroll_px = 0;
static Uint32 readme_scroll_tick = 0;
static int readme_loaded_skin = -1;

static void rebuild_readme_lines(void)
{
	int max_w = SCREEN_WIDTH - 132 - 4;
	char word[64];
	int wl = 0;
	int bp = 0; /* next free byte in readme_lines_buf */
	readme_line_count = 0;
	readme_scroll_px = 0;
	readme_scroll_tick = SDL_GetTicks();

	int line_start = 0; /* index where current line starts in buffer */
	int lb = 0;         /* byte length of current line (excluding \0) */

	for (int ri = 0; ; ++ri)
	{
		char c = readme_data[ri];
		if (c == ' ' || c == '\n' || c == '\0')
		{
			word[wl] = '\0';
			if (wl > 0)
			{
				int word_len = strlen(word);
				/* build test string: current line + " " + word */
				char test[320];
				if (lb > 0)
					snprintf(test, sizeof test, "%.*s %s", lb, readme_lines_buf + line_start, word);
				else
					snprintf(test, sizeof test, "%s", word);

				int tw;
				TTF_SizeUTF8(arcade_font, test, &tw, NULL);

				if (tw > max_w && lb > 0)
				{
					/* flush current line (already null-terminated at line_start+lb) */
					if (readme_line_count < README_LINES_MAX)
						readme_lines[readme_line_count++] = readme_lines_buf + line_start;
					/* start fresh line with this word */
					memcpy(readme_lines_buf + bp, word, word_len + 1);
					line_start = bp;
					lb = word_len;
					bp += word_len + 1;
				}
				else
				{
					if (lb > 0)
					{
						/* append space + word to current line */
						readme_lines_buf[line_start + lb] = ' ';
						++lb;
						memcpy(readme_lines_buf + line_start + lb, word, word_len + 1);
						lb += word_len;
						bp = line_start + lb + 1;
					}
					else
					{
						/* first word on the line */
						memcpy(readme_lines_buf + bp, word, word_len + 1);
						line_start = bp;
						lb = word_len;
						bp += word_len + 1;
					}
				}
			}
			wl = 0;

			if (c == '\n')
			{
				if (lb > 0)
				{
					if (readme_line_count < README_LINES_MAX)
						readme_lines[readme_line_count++] = readme_lines_buf + line_start;
				}
				else
				{
					/* preserve empty line */
					if (readme_line_count < README_LINES_MAX)
						readme_lines[readme_line_count++] = readme_lines_buf + bp;
					readme_lines_buf[bp++] = '\0';
				}
				lb = 0;
			}
			if (c == '\0') break;
		}
		else
		{
			if (wl < (int)sizeof word - 1)
				word[wl++] = c;
		}
	}

	/* flush last line */
	if (lb > 0 && readme_line_count < README_LINES_MAX)
		readme_lines[readme_line_count++] = readme_lines_buf + line_start;
}

static void load_skin_readme(const char *skin_path)
{
	char dir[512];
	strncpy(dir, skin_path, sizeof dir - 1);
	dir[sizeof dir - 1] = '\0';
	char *p = dir + strlen(dir) - 1;
	while (p > dir && *p != '/') --p;
	if (*p == '/') *(p + 1) = '\0';
	else { readme_data[0] = '\0'; rebuild_readme_lines(); return; }

	char path[640];
	snprintf(path, sizeof path, "%sreadme.txt", dir);
	FILE *f = fopen(path, "r");
	if (!f) { readme_data[0] = '\0'; rebuild_readme_lines(); return; }

	size_t len = fread(readme_data, 1, README_MAX - 1, f);
	fclose(f);
	readme_data[len] = '\0';
	rebuild_readme_lines();
}

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
	int const Y0 = 46;

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
			int dy = 46;
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
	int const Y0 = 46;

	draw_text(LX, SUB_Y, "GAME SETUP", 0, 0);

	static const char *gs_names[GSE_END] = {
		"Game Mode", "Level", "Initial Debris", "Debris Chance", "Auto Debris"
	};
	static const char *gs_descs[GSE_END] = {
		"Marathon / Sprint / Ultra.",
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
			case GSE_GAMEMODE:
			{
				static const char *mode_names[] = { "Marathon", "Sprint", "Ultra" };
				strcpy(val, mode_names[menu_gamemode]);
			}
				break;
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
			draw_text(RX, 46, gs_descs[i], 0, 0);
		}
		else
		{
			draw_text(LX, row, gs_names[i], 0, 0);
			draw_text(LV_X, row + VL_OFF, val, 2, 0);
		}
	}

	draw_text(RX, 110, "Press ENTER to start.", 0, 0);
	draw_text(RX, 119, "Press ESC to go back.", 0, 0);
}

static void draw_settings(void)
{
	int const Y0 = 46;

	static const char *names[SET_END] = {
		"Skin",
		"Speech", "Smooth Animation",
		"Tetromino Color", "Easy Spin", "Fixed Lock Delay",
		"Drop Type", "Randomizer",
		"Key Config"
	};
	static const char *descs[SET_END] = {
		"Visual theme, affects some\ngame rules and appearance.",
		"Announce cleared lines\nwith speech synthesis.",
		"Smooth piece movement.",
		"Color scheme for pieces.",
		"Easier T-spin detection.",
		"Fixed delay before lock.",
		"Sonic drop or hard drop.",
		"Algorithm that determines\nthe next tetromino.",
		"Configure keyboard bindings\nfor all game actions.\nPress ENTER to configure."
	};

	draw_text(LX, SUB_Y, "SETTINGS", 0, 0);

	/* reset readme scroll when user navigates back to skin entry */
	static int prev_setting = -1;
	int just_entered_skin = (prev_setting != SET_SKIN && cur_settings == SET_SKIN);
	prev_setting = cur_settings;

	for (int i = 0; i < SET_END; ++i)
	{
		int row = Y0 + i * ENTRY_H;
		char val[16];

		switch (i)
		{
			case SET_SPEECH:
				strcpy(val, speechon ? "on" : "off");
				break;
			case SET_SMOOTHANIM:
				strcpy(val, smoothanim ? "on" : "off");
				break;
			case SET_TETROMINO_COLOR:
			{
				static const char *tc_names[] = { "random", "standard", "gray" };
				strcpy(val, tc_names[tetrominocolor]);
			}
				break;
			case SET_EASYSPIN:
				strcpy(val, easyspin ? "on" : "off");
				break;
			case SET_LOCKDELAY:
				strcpy(val, lockdelay ? "on" : "off");
				break;
			case SET_DROPTYPE:
				strcpy(val, sonicdrop ? "sonic" : "hard");
				break;
			case SET_RANDOMIZER:
				strcpy(val, getRandomizerString());
				break;
		}

		if (i == cur_settings)
		{
			draw_text_col(LX, row, names[i], 0, 0, 255, 255, 0);
			switch (i)
			{
				case SET_SKIN:
				{
					char buf[32];
					snprintf(buf, sizeof buf, "< %s >", menu_skinentries[menu_skin].name);
					draw_text_col(LV_X, row + VL_OFF, buf, 2, 0, 255, 255, 0);
				}
					break;
				case SET_KEYCONFIG:
					break;
				default:
				{
					char buf[32];
					snprintf(buf, sizeof buf, "< %s >", val);
					draw_text_col(LV_X, row + VL_OFF, buf, 2, 0, 255, 255, 0);
				}
					break;
			}
			/* description in right panel */
			int dy = 46;
			if (SET_SKIN == i)
			{
				/* word‑wrapped skin readme with auto‑scroll */
				if (readme_loaded_skin != menu_skin)
				{
					load_skin_readme(menu_skinentries[menu_skin].path);
					readme_loaded_skin = menu_skin;
				}
				if (readme_line_count > 0)
				{
					if (just_entered_skin)
					{
						readme_scroll_px = 0;
						readme_scroll_tick = SDL_GetTicks();
					}

					/* smooth pixel scroll, rests at end */
					int max_px = readme_line_count * 9 - README_VISIBLE * 9;
					if (max_px < 0) max_px = 0;
					if (max_px > 0)
					{
						Uint32 now = SDL_GetTicks();
						Uint32 elapsed = now - readme_scroll_tick;
						int target_px = 0;
						if (elapsed > README_SCROLL_DELAY_MS)
						{
							target_px = (int)((elapsed - README_SCROLL_DELAY_MS)
							          * 9 / README_SCROLL_INTERVAL);
						}
						if (target_px > max_px) target_px = max_px;
						readme_scroll_px = target_px;
					}
					/* draw the visible lines with pixel offset */
					int first = readme_scroll_px / 9;
					int y_off = readme_scroll_px % 9;
					int dy2 = dy - y_off;
					for (int li = first; li < readme_line_count; ++li)
					{
						if (dy2 > 161) break;
						if (dy2 + 9 >= dy)
							draw_text(RX, dy2, readme_lines[li], 0, 0);
						dy2 += 9;
					}
				}
				else
				{
					draw_text(RX, dy, "Visual theme, affects some", 0, 0);
					draw_text(RX, dy + 9, "game rules and appearance.", 0, 0);
				}
			}
			else
			{
				for (int k = 0; descs[i][k]; )
				{
					char line[32];
					int n = 0;
					while (descs[i][k] && n < 31)
					{
						if (descs[i][k] == '\n') { ++k; break; }
						line[n++] = descs[i][k++];
					}
					line[n] = '\0';
					draw_text(RX, dy, line, 0, 0);
					dy += 9;
				}
			}
		}
		else
		{
			draw_text(LX, row, names[i], 0, 0);
			switch (i)
			{
				case SET_SKIN:
					draw_text(LV_X, row + VL_OFF, menu_skinentries[menu_skin].name, 2, 0);
					break;
				case SET_KEYCONFIG:
					break;
				default:
					draw_text(LV_X, row + VL_OFF, val, 2, 0);
					break;
			}
		}
	}

	draw_text(RX, 179, "Press ESC to go back.", 0, 0);
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
	int const Y0 = 48;

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
	draw_text(SCREEN_WIDTH / 2, 175, "Navigate with UP/DOWN.", 1, 0);
	draw_text(SCREEN_WIDTH / 2, 184, "Press ENTER to reassign", 1, 0);
	draw_text(SCREEN_WIDTH / 2, 193, "the selected key.", 1, 0);
	draw_text(SCREEN_WIDTH / 2, 206, "Press ESC to go back.", 1, 0);
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
				case GSE_GAMEMODE:
					decMod(&menu_gamemode, GM_END, false);
					break;
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
			switch (cur_settings)
			{
				case SET_SKIN:
					decMod(&menu_skin, menu_skinnum, false);
					menu_error[0] = '\0';
					load_menu_bg(menu_skinentries[menu_skin].path);
					break;
				case SET_SPEECH:
					speechon = !speechon;
					break;
				case SET_SMOOTHANIM:
					smoothanim = !smoothanim;
					break;
				case SET_TETROMINO_COLOR:
					decMod((int*)&tetrominocolor, TC_END, false);
					break;
				case SET_EASYSPIN:
					easyspin = !easyspin;
					break;
				case SET_LOCKDELAY:
					lockdelay = !lockdelay;
					break;
				case SET_DROPTYPE:
					sonicdrop = !sonicdrop;
					break;
				case SET_RANDOMIZER:
					decMod((int*)&randomalgo, RA_END, false);
					randomizer_reset();
					break;
				default:
					break;
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
				case GSE_GAMEMODE:
					incMod(&menu_gamemode, GM_END, false);
					break;
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
			switch (cur_settings)
			{
				case SET_SKIN:
					incMod(&menu_skin, menu_skinnum, false);
					menu_error[0] = '\0';
					load_menu_bg(menu_skinentries[menu_skin].path);
					break;
				case SET_SPEECH:
					speechon = !speechon;
					break;
				case SET_SMOOTHANIM:
					smoothanim = !smoothanim;
					break;
				case SET_TETROMINO_COLOR:
					incMod((int*)&tetrominocolor, TC_END, false);
					break;
				case SET_EASYSPIN:
					easyspin = !easyspin;
					break;
				case SET_LOCKDELAY:
					lockdelay = !lockdelay;
					break;
				case SET_DROPTYPE:
					sonicdrop = !sonicdrop;
					break;
				case SET_RANDOMIZER:
					incMod((int*)&randomalgo, RA_END, false);
					randomizer_reset();
					break;
				default:
					break;
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
			saveSettings();
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

	/* apply saved skin from config */
	const char *saved = getLoadedSkinName();
	if (saved[0])
	{
		for (int i = 0; i < menu_skinnum; ++i)
		{
			if (!strcmp(menu_skinentries[i].name, saved))
			{
				menu_skin = i;
				break;
			}
		}
	}

	cur_level = ML_TOP;
	cur_top = TE_GAMESTART;
	cur_game_setup = GSE_GAMEMODE;
	cur_settings = SET_SKIN;
	cur_keycfg = 0;

	readme_loaded_skin = -1;
	readme_scroll_px = 0;
	load_menu_bg(menu_skinentries[menu_skin].path);
}

void mainmenu_processInputEvents(void)
{
	SDL_Event event;

	if (!SDL_PollEvent(&event))
	{
		SDL_Delay(50);
		return;
	}

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
