local bg_img, font, snd

-- Next-piece positions: two alternating columns, 35px spacing
local nx = { 266, 226, 266, 226, 266, 226 }
local ny = { 32, 67, 102, 137, 172, 207 }

-- Clear-type names (indexed by number of lines)
local clear_names = { "Single", "Double", "Triple", "Tetris", "Cheatris" }

function on_skin_load(r)
	bg_img = r.load_image("bg.png")

	-- tetromino colours (from game.txt)
	r.set_tetromino_color(fig.I, 255, 215, 64, 0)
	r.set_tetromino_color(fig.O, 255, 59, 52, 255)
	r.set_tetromino_color(fig.T, 255, 115, 121, 0)
	r.set_tetromino_color(fig.S, 255, 0, 132, 96)
	r.set_tetromino_color(fig.Z, 255, 75, 160, 255)
	r.set_tetromino_color(fig.J, 255, 255, 174, 10)
	r.set_tetromino_color(fig.L, 255, 255, 109, 247)

	r.set_brick_size(12)
	r.set_board_xy(94, 0)

	local bmp = r.load_image("bricks.png")
	r.set_bricksprite(bmp)

	r.set_ghost_alpha(64)

	font = r.load_font("gooddogp.ttf", 16)

	-- Pre-render semi-transparent boxes onto background
	r.draw_rect_to(bg_img, 100, 0, 120, 240, 255, 255, 255, 48)
	r.draw_rect_to(bg_img, 246, 22, 48, 24, 255, 255, 255, 48)
	r.draw_rect_to(bg_img, 246, 52, 48, 24, 255, 255, 255, 48)
	r.draw_rect_to(bg_img, 246, 82, 48, 24, 255, 255, 255, 48)
	r.draw_rect_to(bg_img, 246, 112, 48, 24, 255, 255, 255, 48)
	r.draw_rect_to(bg_img, 246, 142, 48, 24, 255, 255, 255, 48)
	r.draw_rect_to(bg_img, 246, 172, 48, 24, 255, 255, 255, 48)

	-- Load default sounds
	snd = sfx.loadDefaults()
end

function on_skin_unload() end

function on_background_draw()
	res.draw_image(bg_img, 0, 0)

	local mode = game.mode()

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

	res.draw_text(font, hs, 47, 25, 58, 56, 73)
	res.draw_text(font, game.score(), 47, 60, 58, 56, 73)

	-- Timer
	local t
	if mode == "ultra" then
		t = format_ms(game.ultra_time_left())
	else
		t = format_ms(game.timer())
	end
	res.draw_text(font, t, 132, 4, 58, 56, 73)

	-- Lines
	if mode == "sprint" then
		res.draw_text(font, "Lines left: " .. (40 - game.lines()), 154, 20, 58, 56, 73, 1, 0)
	else
		res.draw_text(font, "Lines: " .. game.lines(), 154, 20, 58, 56, 73, 1, 0)
	end

	-- Piece statistics (left panel)
	res.draw_text(font, game.stat(0), 36, 75, 58, 56, 73)
	res.draw_text(font, game.stat(1), 36, 99, 58, 56, 73)
	res.draw_text(font, game.stat(2), 36, 123, 58, 56, 73)
	res.draw_text(font, game.stat(3), 36, 147, 58, 56, 73)
	res.draw_text(font, game.stat(4), 36, 171, 58, 56, 73)
	res.draw_text(font, game.stat(5), 36, 195, 58, 56, 73)
	res.draw_text(font, game.stat(6), 36, 219, 58, 56, 73)

	-- Next pieces (right panel, alternating columns)
	for i = 1, 6 do
		local nxt = figure.next(i)
		if nxt then
			res.draw_piece_shape(nxt.id, nx[i], ny[i], nxt.color, 255)
		end
	end
end

function on_foreground_draw() end

-- ─── Event callbacks ───

function on_move(direction)
	sfx.play(snd.click)
end

function on_piece_lock(id)
	sfx.play(snd.hit)
end

function on_line_clear(data)
	-- Play clear sound
	if data.lines == 4 then
		sfx.play(snd.clear)
	else
		sfx.play(snd.clear)
	end

	-- Build clear name
	local lines = data.lines
	if lines > 5 then lines = 5 end
	local name = clear_names[lines]
	if data.tspin and data.tspin ~= "" then
		name = data.tspin .. name
	end

	-- Show clear name at $lcttop position
	res.show_timed_text(154, 88, name, 1500, font, 58, 56, 73, 1, 0)

	-- Show B2B at $lctmid position
	if data.b2b then
		res.show_timed_text(154, 104, "Back-2-Back", 1500, font, 58, 56, 73, 1, 0)
	end

	-- Show combo at $lctbot position
	if data.combo and data.combo > 0 then
		res.show_timed_text(154, 120, "combo " .. data.combo .. "x", 1500, font, 58, 56, 73, 1, 0)
		sfx.play(snd.combo[math.min(data.combo, 9)])
	end
end

function on_game_over(reason)
	-- no game over sound in defaults
end

function format_ms(ms)
	local cs = math.floor(ms / 10) % 100
	local ss = math.floor(ms / 1000) % 60
	local mm = math.floor(ms / 60000)
	return string.format("%02d:%02d.%02d", mm, ss, cs)
end
