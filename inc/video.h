#ifndef _H_VIDEO
#define _H_VIDEO

#include <stdbool.h>

#define SCREEN_WIDTH		320
#define SCREEN_HEIGHT		240
#define ALT_SCREEN_BPP		32
#define FPS					60.0

#ifdef _BITTBOY
#define SCREEN_BPP			16
#else
#define SCREEN_BPP			32
#endif

extern int fps;

bool screenFlagUpdate(bool v);
void upscale2(uint32_t *to, uint32_t *from);
void upscale3(uint32_t *to, uint32_t *from);
void upscale4(uint32_t *to, uint32_t *from);
void frameCounter(void);
int frameLimiter(void);

#endif
