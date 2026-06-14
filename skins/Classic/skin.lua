local bg_img, font, snd
local next_x = { 255 }
local next_y = { 185 }

function on_skin_load()
	bg_img = gfx.load_image("bg.png")

	-- Classic Tetris colour palette
	cfg.set_tetromino_color(fig.I, 128, 0, 255, 255)
	cfg.set_tetromino_color(fig.O, 128, 255, 255, 0)
	cfg.set_tetromino_color(fig.T, 128, 127, 0, 255)
	cfg.set_tetromino_color(fig.S, 128, 0, 255, 0)
	cfg.set_tetromino_color(fig.Z, 128, 255, 0, 0)
	cfg.set_tetromino_color(fig.J, 128, 0, 0, 255)
	cfg.set_tetromino_color(fig.L, 128, 255, 128, 0)

	cfg.set_brick_size(12)
	cfg.set_board_xy(108, 0)

	local bmp = gfx.load_image("bricks.png")
	cfg.set_bricksprite(bmp)

	cfg.set_ghost_alpha(85)

	font = gfx.load_font("arcade.ttf", 8)

	-- Pre-render static labels onto background
	bg_img:draw_text(font, "SCORE", 25, 9, 255, 255, 255, 1, 0)
	bg_img:draw_text(font, "LEVEL", 25, 47, 255, 255, 255, 1, 0)
	bg_img:draw_text(font, game.mode() == "sprint" and "LEFT" or "LINES", 25, 85, 255, 255, 255, 1, 0)

	-- Load default sounds
	snd = sfx.loadDefaults()
end

function on_skin_unload() end

function on_background_draw()
	screen.draw_image(bg_img, 0, 0)

	local mode = game.mode()

	-- Score
	font:print(game.score(), 8, 29, 255, 255, 255)

	-- Level
	font:print(game.level(), 8, 66, 255, 255, 255)

	-- Lines
	font:print(mode == "sprint" and 40 - game.lines() or game.lines(), 8, 100, 255, 255, 255)

	-- Timer
	local t
	if mode == "ultra" then
		t = format_ms(game.ultra_time_left())
	else
		t = format_ms(game.timer())
	end
	font:print(t, 52, 183, 255, 0, 0, 1, 0)

	-- First next piece
	local nxt = figure.next(1)
	if nxt then
		screen.draw_shape(nxt.id, 255, 185, nxt.color, 255)
	end
end

function on_foreground_draw() end

-- ─── Event callbacks ───

function on_move(direction)
	snd.click:play()
end

function on_piece_lock(id)
	snd.hit:play()
end

function on_line_clear(data)
	snd.clear:play()
	if data.combo and data.combo > 0 then
		snd.combo[math.min(data.combo, 9)]:play()
	end
end

function format_ms(ms)
	local cs = math.floor(ms / 10) % 100
	local ss = math.floor(ms / 1000) % 60
	local mm = math.floor(ms / 60000)
	return string.format("%02d:%02d.%02d", mm, ss, cs)
end
