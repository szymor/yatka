local bg_img, font, small_font, fg_overlay
local stat_order = { 2, 5, 4, 1, 3, 6, 0 }  -- T, J, Z, O, S, L, I
local snd

function on_skin_load()
	bg_img = gfx.load_image("bg.png")
	fg_overlay = gfx.load_image("fg.png")

	-- tetromino colours (from game.txt)
	cfg.set_tetromino_color(fig.I, 255, 215, 64, 0)
	cfg.set_tetromino_color(fig.O, 255, 59, 52, 255)
	cfg.set_tetromino_color(fig.T, 255, 115, 121, 0)
	cfg.set_tetromino_color(fig.S, 255, 0, 132, 96)
	cfg.set_tetromino_color(fig.Z, 255, 75, 160, 255)
	cfg.set_tetromino_color(fig.J, 255, 255, 174, 10)
	cfg.set_tetromino_color(fig.L, 255, 255, 109, 247)

	cfg.set_brick_size(8)
	cfg.set_board_xy(128, 48)

	local bmp = gfx.load_image("bricks.png")
	cfg.set_bricksprite(bmp)

	cfg.set_ghost_alpha(0)

	font = gfx.load_font("arcade.ttf", 8)
	small_font = gfx.load_font("arcade.ttf", 6)

	snd = {}
	snd.hit = sfx.load("sfx/drop.wav")
	snd.clear = sfx.load("sfx/line_clear.wav")
	snd.click = sfx.load("sfx/side_move.wav")
	snd.game_over = sfx.load("sfx/game_over.wav")
	snd.level_up = sfx.load("sfx/level_up.wav")
	snd.pause = sfx.load("sfx/pause.wav")
	snd.rotate = sfx.load("sfx/rotate.wav")
	snd.tetris = sfx.load("sfx/tetris.wav")
end

function on_skin_unload() end

function on_background_draw()
	screen.draw_image(bg_img, 0, 0)

	local mode = game.mode()

	-- Header labels (static, drawn each frame for centering support)
	screen.draw_text(font, "Yatka", 80, 32, 255, 255, 255, 1, 0)
	screen.draw_text(small_font, "Statistics", 80, 70, 255, 255, 255, 1, 0)

	-- Hiscore (dynamic type per mode)
	local hiscore_type = ({
		marathon = record.MARATHON_SCORE,
		sprint   = record.SPRINT_TIME,
		ultra    = record.ULTRA_SCORE
	})[mode]
	local hs = game.hiscore(hiscore_type)
	if hiscore_type == record.SPRINT_TIME then
		hs = format_ms(hs)
	end

	screen.draw_text(font, "Top", 224, 30, 255, 255, 255)
	screen.draw_text(font, hs, 224, 39, 255, 255, 255)
	if mode == "sprint" then
		screen.draw_text(font, "Time", 224, 56, 255, 255, 255)
		screen.draw_text(font, format_ms(game.timer()), 224, 65, 255, 255, 255)
	else
		screen.draw_text(font, "Score", 224, 56, 255, 255, 255)
		screen.draw_text(font, game.score(), 224, 65, 255, 255, 255)
	end

	screen.draw_text(font, "Next", 240, 104, 255, 255, 255, 1, 0)
	screen.draw_text(font, "Level", 244, 158, 255, 255, 255, 1, 0)
	screen.draw_text(font, game.level(), 244, 167, 255, 255, 255, 1, 0)
	screen.draw_text(font, "Debris", 248, 190, 255, 255, 255, 1, 0)
	screen.draw_text(font, game.debris(), 248, 199, 255, 255, 255, 1, 0)


	-- Piece statistics (left panel, next to piece sprites on bg.png)
	for i = 1, 7 do
		local sid = stat_order[i]
		screen.draw_text(font, game.stat(sid), 105, 94 + (i - 1) * 16, 215, 64, 0, 2, 0)
	end

	-- First next piece (below "Next" label)
	local nxt = figure.next(1)
	if nxt then
		screen.draw_shape(nxt.id, 224, 120, nxt.color, 255)
	end
end

function on_foreground_draw()
	screen.draw_image(fg_overlay, 119, 0)

	local mode = game.mode()
	if mode == "sprint" then
		screen.draw_text(font, "Lines " .. game.lines() .. "/40", 168, 24, 255, 255, 255, 1, 0)
	elseif mode == "ultra" then
		screen.draw_text(font, format_ms(game.ultra_time_left()), 168, 24, 255, 255, 255, 1, 0)
	else
		screen.draw_text(font, "Lines " .. game.lines(), 168, 24, 255, 255, 255, 1, 0)
	end
end

-- ─── Event callbacks ───

function on_move(direction)
	sfx.play(snd.click)
end

function on_piece_lock(id)
	sfx.play(snd.hit)
end

function on_line_clear(data)
	if data.levelup then
		sfx.play(snd.level_up)
	elseif data.lines == 4 then
		sfx.play(snd.tetris)
	else
		sfx.play(snd.clear)
	end
end

function on_game_over(reason)
	sfx.play(snd.game_over)
end

function on_pause()
	sfx.play(snd.pause)
end

function on_rotate(direction)
	sfx.play(snd.rotate)
end

function format_ms(ms)
	local ss = math.floor(ms / 1000) % 60
	local mm = math.floor(ms / 60000)
	return string.format("%02d:%02d", mm, ss)
end
