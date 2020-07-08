#ifndef _H_VIDEO
#define _H_VIDEO

#include <stdbool.h>

#include <SDL/SDL_ttf.h>

#define SCREEN_WIDTH		320
#define SCREEN_HEIGHT		240
#define FPS					60.0

#if defined _BITTBOY
#define SCREEN_BPP			16
#define VIDEO_MODE_FLAGS	SDL_SWSURFACE
#elif defined _RETROFW
#define SCREEN_BPP			16
#define VIDEO_MODE_FLAGS	(SDL_HWSURFACE | SDL_DOUBLEBUF)
#else
#define SCREEN_BPP			32
#define VIDEO_MODE_FLAGS	(SDL_HWSURFACE | SDL_DOUBLEBUF)
#endif

extern SDL_Surface *screen;
extern SDL_Surface *screen_scaled;
extern SDL_Surface *last_game_screen;
extern TTF_Font *arcade_font;
extern int screenscale;
extern int fps;

void saveLastGameScreen(void);
void flipScreenScaled(void);
void upscale2(uint32_t *to, uint32_t *from);
void upscale3(uint32_t *to, uint32_t *from);
void upscale4(uint32_t *to, uint32_t *from);
void frameCounter(void);
int frameLimiter(void);

#endif
