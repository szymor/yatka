local bg_img, big_font, small_font, snd

-- Next pieces: single column at x=246, 30px spacing
local nx = { 246, 246, 246, 246 }
local ny = { 67, 97, 127, 157 }

-- Clear-type names (indexed by number of lines)
local clear_names = { "Single", "Double", "Triple", "Tetris", "Cheatris" }

function on_skin_load()
	local mode = game.mode()
	bg_img = gfx.create_image(320, 240, 0, 0, 0)

	-- tetromino colours (classic retro palette)
	cfg.set_tetromino_color(fig.I, 128, 0, 255, 255)
	cfg.set_tetromino_color(fig.O, 128, 255, 255, 0)
	cfg.set_tetromino_color(fig.T, 128, 127, 0, 255)
	cfg.set_tetromino_color(fig.S, 128, 0, 255, 0)
	cfg.set_tetromino_color(fig.Z, 128, 255, 0, 0)
	cfg.set_tetromino_color(fig.J, 128, 0, 0, 255)
	cfg.set_tetromino_color(fig.L, 128, 255, 128, 0)

	cfg.set_brick_size(12)
	cfg.set_board_xy(100, 0)
	cfg.set_holdmode("preserve")

	-- bricks.png missing from skin, falls back to gfx/bricks.png
	local bmp = gfx.load_image("bricks.png")
	cfg.set_bricksprite(bmp)

	cfg.set_ghost_alpha(69)

	big_font = gfx.load_font("monobit.ttf", 32)
	small_font = gfx.load_font("monobit.ttf", 28)

	-- Pre-render semi-transparent boxes onto background
	bg_img:draw_rect(0, 0, 100, 240, 255, 255, 255, 20)
	bg_img:draw_rect(220, 0, 120, 240, 255, 255, 255, 20)
	bg_img:draw_rect(36, 55, 55, 48, 255, 255, 255, 60)
	bg_img:draw_rect(242, 55, 55, 140, 255, 255, 255, 60)

	-- Pre-render static labels onto background
	if mode == "sprint" then
		bg_img:draw_text(big_font, "TIME", 5, -5, 255, 255, 255)
		bg_img:draw_text(big_font, "BEST", 228, -5, 255, 255, 255)
	else
		bg_img:draw_text(big_font, "SCORE", 5, -5, 255, 255, 255)
		bg_img:draw_text(big_font, "HI-SCORE", 228, -5, 255, 255, 255)
	end
	bg_img:draw_text(big_font, "Hold", 45, 30, 255, 255, 255)
	bg_img:draw_text(big_font, "Next", 248, 30, 255, 255, 255)

	-- Load default sounds
	snd = sfx.loadDefaults()
end

function on_skin_unload() end

function on_background_draw()
	screen.draw_image(bg_img, 0, 0)

	local mode = game.mode()

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

	screen.draw_text(big_font, hs, 228, 10, 255, 255, 255)

	-- Level and lines (small font, bottom left)
	screen.draw_text(small_font, "Level: " .. game.level(), 10, 160, 255, 255, 255)
	if mode == "sprint" then
		screen.draw_text(big_font, format_ms(game.timer()), 5, 10, 255, 255, 255)
		screen.draw_text(small_font, "Left: " .. (40 - game.lines()), 10, 175, 255, 255, 255)
	else
		screen.draw_text(big_font, game.score(), 5, 10, 255, 255, 255)
		screen.draw_text(small_font, "Lines: " .. game.lines(), 10, 175, 255, 255, 255)
	end

	if mode == "ultra" then
		screen.draw_text(big_font, "Time left:", 5, 100, 255, 255, 255)
		screen.draw_text(big_font, format_ms(game.ultra_time_left()), 10, 115, 255, 255, 255)
	end

	-- Hold piece
	local held = figure.held()
	if held then
		screen.draw_shape(held.id, 40, 65, held.color, 255)
	end

	-- Next pieces
	for i = 1, 4 do
		local nxt = figure.next(i)
		if nxt then
			screen.draw_shape(nxt.id, nx[i], ny[i], nxt.color, 255)
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
	sfx.play(snd.clear)

	-- Build clear name
	local lines = data.lines
	if lines > 5 then lines = 5 end
	local name = clear_names[lines]
	if data.tspin and data.tspin ~= "" then
		name = data.tspin .. name
	end

	-- Show clear info centered above board ($lcttop/mid/bot positions)
	screen.pop_up(160, 8, name, 1500, small_font, 255, 255, 255, 1, 0)

	if data.b2b then
		screen.pop_up(160, 20, "Back-2-Back", 1500, small_font, 255, 255, 255, 1, 0)
	end

	if data.combo and data.combo > 0 then
		screen.pop_up(160, 32, "combo " .. data.combo .. "x", 1500, small_font, 255, 255, 255, 1, 0)
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
