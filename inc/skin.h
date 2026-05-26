#ifndef _H_SKIN
#define _H_SKIN

#include <stdbool.h>
#include <SDL/SDL.h>
#include <SDL/SDL_image.h>
#include <SDL/SDL_ttf.h>

#define FONT_NUM			(8)
#define ORIENTATION_NUM		(15)
#define TIMED_TEXT_MAX		(8)
#define TIMED_TEXT_LEN		(128)
#define TEXT_CACHE_SIZE		(16)
#define PARTICLE_MAX		(256)

struct TextCacheEntry
{
	TTF_Font *font;
	char text[TIMED_TEXT_LEN];
	Uint8 r, g, b;
	SDL_Surface *surface;
};

enum FigureId
{
	FIGID_I,
	FIGID_O,
	FIGID_T,
	FIGID_S,
	FIGID_Z,
	FIGID_J,
	FIGID_L,
	FIGID_GRAY,
	FIGID_END
};

enum BrickStyle
{
	BS_SIMPLE,
	BS_ORIENTATION_BASED,
	BS_FIGUREWISE,
	BS_END
};

enum HoldMode
{
	HM_OFF,
	HM_EXCHANGE,
	HM_PRESERVE
};

/* Opaque forward declaration — the actual lua_State is
 * defined in the merged skin.c and only accessed through
 * the skin_* API.  Including lua.h here would force it
 * on every translation unit that pulls in skin.h. */
struct lua_State;

struct TimedText
{
	char text[TIMED_TEXT_LEN];
	int x, y;
	Uint32 deadline;
	TTF_Font *font;
	Uint8 r, g, b;
	int alignx, aligny;
	Uint32 fadeout_ms;
	SDL_Surface *surface;        /* cached rendered text (NULL = not yet rendered) */
};

struct Animation
{
	SDL_Surface *spritesheet;
	int frame_w, frame_h;
	int frame_count;
	Uint32 frame_duration;
};

struct Particle
{
	float x, y;
	float vx, vy;
	float ax, ay;
	SDL_Surface *sprite;
	SDL_Rect srcrect;
	bool no_remove;
	struct Animation *anim;
	Uint32 anim_start_tick;
};

struct Skin
{
	SDL_Surface *screen;
	enum HoldMode holdmode;
	SDL_Surface *bricksprite[FIGID_END];
	Uint32 colors[FIGID_GRAY];
	Uint32 color_alphas[FIGID_GRAY];
	enum BrickStyle brickstyle;
	char *path;
	int boardx;
	int boardy;
	int bricksize;
	int brickyoffset;
	int ghost;
	SDL_Surface *brick_shadow;
	int shadowx;
	int shadowy;
	TTF_Font *fonts[FONT_NUM];

	struct TimedText timed_texts[TIMED_TEXT_MAX];
	struct TextCacheEntry text_cache[TEXT_CACHE_SIZE];

	struct Particle particles[PARTICLE_MAX];
	int particle_count;
	Uint32 last_particle_tick;

	struct lua_State *L;           /* per-skin lua state, NULL for failed loads */

	SDL_Surface *board_cache;      /* cached composited board surface */
};


/* ─── skin lifecycle ─── */
void skin_init(struct Skin *skin);
void skin_destroy(struct Skin *skin);
bool skin_loadSkin(struct Skin *skin, const char *path);
void skin_update_screen(struct Skin *skin, SDL_Surface *screen);

/* ─── event callbacks (called from main.c) ─── */
void skin_on_line_clear(struct Skin *skin, int lines,
                        const char *tspin_type,
                        int combo, bool b2b, int score_earned,
                        bool pc);
void skin_on_game_over(struct Skin *skin, const char *reason);
void skin_on_level_up(struct Skin *skin, int level);
void skin_on_piece_lock(struct Skin *skin, enum FigureId id);
void skin_on_piece_hold(struct Skin *skin, enum FigureId id);
void skin_on_hard_drop(struct Skin *skin, int rows);
void skin_on_combo(struct Skin *skin, int count);
void skin_on_move(struct Skin *skin, const char *direction);

#endif
