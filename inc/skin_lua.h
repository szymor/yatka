#ifndef _H_SKIN_LUA
#define _H_SKIN_LUA

#include <lua5.4/lua.h>
#include <lua5.4/lauxlib.h>
#include <lua5.4/lualib.h>

#include "skin.h"

/* Returns true if the skin directory contains a Lua script */
bool skin_lua_available(const char *skin_path);

/* Initialise Lua state for a skin (calls skin.load) */
void skin_lua_init(struct Skin *skin, const char *skin_path);

/* Tear down Lua state for a skin (calls skin.unload) */
void skin_lua_fini(struct Skin *skin);

/* ─── per-frame render callbacks ─── */

void skin_lua_draw_background(struct Skin *skin);
void skin_lua_draw_shadow(struct Skin *skin);
void skin_lua_draw_board(struct Skin *skin);
void skin_lua_draw_active_figure(struct Skin *skin, int interp_y);
void skin_lua_draw_ghost(struct Skin *skin);
void skin_lua_draw_foreground(struct Skin *skin);
void skin_lua_draw_hud(struct Skin *skin);

/* ─── event callbacks ─── */

void skin_lua_on_line_clear(struct Skin *skin, int lines,
                            const char *tspin_type,
                            int combo, bool b2b, int score_earned);
void skin_lua_on_game_over(struct Skin *skin, const char *reason);
void skin_lua_on_level_up(struct Skin *skin, int level);
void skin_lua_on_piece_lock(struct Skin *skin, enum FigureId id);
void skin_lua_on_piece_hold(struct Skin *skin, enum FigureId id);
void skin_lua_on_hard_drop(struct Skin *skin, int rows);
void skin_lua_on_combo(struct Skin *skin, int count);

#endif
