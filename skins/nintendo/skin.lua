local bg_img, font, small_font, fg_overlay
local stat_order = { 2, 5, 4, 1, 3, 6, 0 }  -- T, J, Z, O, S, L, I
local game_over_sfx, level_up_sfx, pause_sfx
local rotate_sfx, side_move_sfx, tetris_sfx

function on_skin_load(r)
	bg_img = r.load_image("bg.png")
	fg_overlay = r.load_image("fg.png")

	-- tetromino colours (from game.txt)
	r.set_tetromino_color(fig.I, 255, 215, 64, 0)
	r.set_tetromino_color(fig.O, 255, 59, 52, 255)
	r.set_tetromino_color(fig.T, 255, 115, 121, 0)
	r.set_tetromino_color(fig.S, 255, 0, 132, 96)
	r.set_tetromino_color(fig.Z, 255, 75, 160, 255)
	r.set_tetromino_color(fig.J, 255, 255, 174, 10)
	r.set_tetromino_color(fig.L, 255, 255, 109, 247)

	r.set_brick_size(8)
	r.set_board_xy(128, 48)

	local bmp = r.load_image("bricks.png")
	r.set_bricksprite(bmp)

	r.set_ghost_alpha(0)

	font = r.load_font("arcade.ttf", 8)
	small_font = r.load_font("arcade.ttf", 6)

	sfx.hit = sfx.load("sfx/drop.wav")
	sfx.clear = sfx.load("sfx/line_clear.wav")
	sfx.click = sfx.load("sfx/side_move.wav")
	game_over_sfx = sfx.load("sfx/game_over.wav")
	level_up_sfx = sfx.load("sfx/level_up.wav")
	pause_sfx = sfx.load("sfx/pause.wav")
	rotate_sfx = sfx.load("sfx/rotate.wav")
	tetris_sfx = sfx.load("sfx/tetris.wav")
end

function on_skin_unload() end

function on_background_draw()
	res.draw_image(bg_img, 0, 0)

	local mode = game.mode()

	-- Header labels (static, drawn each frame for centering support)
	res.draw_text(font, "Yatka", 80, 32, 255, 255, 255, 1, 0)
	res.draw_text(small_font, "Statistics", 80, 70, 255, 255, 255, 1, 0)

	-- Hiscore (dynamic type per mode)
	local hiscore_type = ({
		marathon = game.MARATHON_SCORE,
		sprint   = game.SPRINT_TIME,
		ultra    = game.ULTRA_SCORE
	})[mode]
	local hs = game.hiscore(hiscore_type)
	if hiscore_type == game.SPRINT_TIME then
		hs = format_ms(hs)
	end

	res.draw_text(font, "Top", 224, 30, 255, 255, 255)
	res.draw_text(font, hs, 224, 39, 255, 255, 255)
	if mode == "sprint" then
		res.draw_text(font, "Time", 224, 56, 255, 255, 255)
		res.draw_text(font, format_ms(game.timer()), 224, 65, 255, 255, 255)
	else
		res.draw_text(font, "Score", 224, 56, 255, 255, 255)
		res.draw_text(font, game.score(), 224, 65, 255, 255, 255)
	end

	res.draw_text(font, "Next", 240, 104, 255, 255, 255, 1, 0)
	res.draw_text(font, "Level", 244, 158, 255, 255, 255, 1, 0)
	res.draw_text(font, game.level(), 244, 167, 255, 255, 255, 1, 0)
	res.draw_text(font, "Debris", 248, 190, 255, 255, 255, 1, 0)
	res.draw_text(font, game.debris(), 248, 199, 255, 255, 255, 1, 0)


	-- Piece statistics (left panel, next to piece sprites on bg.png)
	for i = 1, 7 do
		local sid = stat_order[i]
		res.draw_text(font, game.stat(sid), 105, 94 + (i - 1) * 16, 215, 64, 0, 2, 0)
	end

	-- First next piece (below "Next" label)
	local nxt = figure.next(1)
	if nxt then
		res.draw_piece_shape(nxt.id, 224, 120, nxt.color, 255)
	end
end

function on_foreground_draw()
	res.draw_image(fg_overlay, 119, 0)

	local mode = game.mode()
	if mode == "sprint" then
		res.draw_text(font, "Lines " .. game.lines() .. "/40", 168, 24, 255, 255, 255, 1, 0)
	elseif mode == "ultra" then
		res.draw_text(font, format_ms(game.ultra_time_left()), 168, 24, 255, 255, 255, 1, 0)
	else
		res.draw_text(font, "Lines " .. game.lines(), 168, 24, 255, 255, 255, 1, 0)
	end
end

-- ─── Event callbacks ───

function on_move(direction)
	sfx.play(sfx.click)
end

function on_piece_lock(id)
	sfx.play(sfx.hit)
end

function on_line_clear(data)
	if data.levelup then
		sfx.play(level_up_sfx)
	elseif data.lines == 4 then
		sfx.play(tetris_sfx)
	else
		sfx.play(sfx.clear)
	end
end

function on_game_over(reason)
	sfx.play(game_over_sfx)
end

function on_pause()
	sfx.play(pause_sfx)
end

function on_rotate(direction)
	sfx.play(rotate_sfx)
end

function format_ms(ms)
	local ss = math.floor(ms / 1000) % 60
	local mm = math.floor(ms / 60000)
	return string.format("%02d:%02d", mm, ss)
end
