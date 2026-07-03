local bg_img, font, snd

-- Next pieces (right panel, single column, 30px spacing)
local nx = { 251, 251, 251 }
local ny = { 35, 65, 95 }

-- Clear-type names (indexed by number of lines)
local clear_names = { "Single", "Double", "Triple", "Tetris", "Cheatris" }

function on_skin_load()
	bg_img = gfx.load_image("bg.png")

	-- tetromino colours (default palette)
	cfg.set_tetromino_color(fig.I, 128, 0, 159, 218)
	cfg.set_tetromino_color(fig.O, 128, 254, 203, 0)
	cfg.set_tetromino_color(fig.T, 128, 149, 45, 152)
	cfg.set_tetromino_color(fig.S, 128, 105, 190, 40)
	cfg.set_tetromino_color(fig.Z, 128, 237, 41, 57)
	cfg.set_tetromino_color(fig.J, 128, 0, 101, 189)
	cfg.set_tetromino_color(fig.L, 128, 255, 121, 0)

	cfg.set_brick_size(12)
	cfg.set_board_xy(100, 0)

	local bmp = gfx.load_image("bricks.png")
	cfg.set_bricksprite(bmp)

	cfg.set_ghost_alpha(128)

	font = gfx.load_font("arcade.ttf", 8)

	-- Pre-render static labels onto background with shadow effect (center-aligned)
	-- "SCORE" at center (45, 15)
	bg_img:draw_text(font, "SCORE", 44, 14, 153, 153, 153, align.CENTER, align.TOP)
	bg_img:draw_text(font, "SCORE", 44, 16, 153, 153, 153, align.CENTER, align.TOP)
	bg_img:draw_text(font, "SCORE", 46, 14, 153, 153, 153, align.CENTER, align.TOP)
	bg_img:draw_text(font, "SCORE", 46, 16, 153, 153, 153, align.CENTER, align.TOP)
	bg_img:draw_text(font, "SCORE", 46, 15, 153, 153, 153, align.CENTER, align.TOP)
	bg_img:draw_text(font, "SCORE", 45, 15, 100, 100, 100, align.CENTER, align.TOP)

	-- "LEVEL" at center (45, 53)
	bg_img:draw_text(font, "LEVEL", 44, 52, 153, 153, 153, align.CENTER, align.TOP)
	bg_img:draw_text(font, "LEVEL", 44, 54, 153, 153, 153, align.CENTER, align.TOP)
	bg_img:draw_text(font, "LEVEL", 46, 52, 153, 153, 153, align.CENTER, align.TOP)
	bg_img:draw_text(font, "LEVEL", 46, 54, 153, 153, 153, align.CENTER, align.TOP)
	bg_img:draw_text(font, "LEVEL", 46, 53, 153, 153, 153, align.CENTER, align.TOP)
	bg_img:draw_text(font, "LEVEL", 45, 53, 100, 100, 100, align.CENTER, align.TOP)

	-- "LINES" at center (45, 91)
	bg_img:draw_text(font, "LINES", 44, 90, 153, 153, 153, align.CENTER, align.TOP)
	bg_img:draw_text(font, "LINES", 44, 92, 153, 153, 153, align.CENTER, align.TOP)
	bg_img:draw_text(font, "LINES", 46, 90, 153, 153, 153, align.CENTER, align.TOP)
	bg_img:draw_text(font, "LINES", 46, 92, 153, 153, 153, align.CENTER, align.TOP)
	bg_img:draw_text(font, "LINES", 45, 91, 100, 100, 100, align.CENTER, align.TOP)

	-- "NEXT" at center (276, 15)
	bg_img:draw_text(font, "NEXT", 275, 14, 153, 153, 153, align.CENTER, align.TOP)
	bg_img:draw_text(font, "NEXT", 275, 16, 153, 153, 153, align.CENTER, align.TOP)
	bg_img:draw_text(font, "NEXT", 277, 14, 153, 153, 153, align.CENTER, align.TOP)
	bg_img:draw_text(font, "NEXT", 277, 16, 153, 153, 153, align.CENTER, align.TOP)
	bg_img:draw_text(font, "NEXT", 277, 15, 153, 153, 153, align.CENTER, align.TOP)
	bg_img:draw_text(font, "NEXT", 276, 15, 100, 100, 100, align.CENTER, align.TOP)

	-- Load custom sounds
	snd = {}
	snd.hit = sfx.load("sfx/lock.wav")
	snd.clear = sfx.load("sfx/clear.wav")
	snd.move = sfx.load("sfx/move.wav")
	snd.rotate = sfx.load("sfx/rotate.wav")
	snd.hold = sfx.load("sfx/hold.wav")
	snd.hold_not = sfx.load("sfx/hold_not.wav")
	snd.tetris = sfx.load("sfx/tetris.wav")
	snd.game_over = sfx.load("sfx/game_over.wav")
end

function on_skin_unload() end

function on_background_draw()
	screen.draw_image(bg_img, 0, 0)

	-- Score value
	screen.draw_text(font, game.score(), 20, 35, 200, 200, 200, align.LEFT, align.TOP)

	-- Level value
	screen.draw_text(font, game.level(), 20, 72, 200, 200, 200, align.LEFT, align.TOP)

	-- Lines value
	local mode = game.mode()
	if mode == "sprint" then
		screen.draw_text(font, 40 - game.lines(), 20, 111, 200, 200, 200, align.LEFT, align.TOP)
	else
		screen.draw_text(font, game.lines(), 20, 111, 200, 200, 200, align.LEFT, align.TOP)
	end

	-- Timer (centered above board)
	if mode == "ultra" then
		screen.draw_text(font, format_ms(game.ultra_time_left()), 160, 4, 100, 100, 100, align.CENTER, align.TOP)
	else
		screen.draw_text(font, format_ms(game.timer()), 160, 4, 100, 100, 100, align.CENTER, align.TOP)
	end

	-- Next pieces
	for i = 1, 3 do
		local nxt = figure.next(i)
		if nxt then
			screen.draw_shape(nxt.id, nx[i], ny[i], nxt.color, 255)
		end
	end
end

function on_foreground_draw() end

-- ─── Event callbacks ───

function on_move(direction)
	sfx.play(snd.move)
end

function on_rotate(direction)
	sfx.play(snd.rotate)
end

function on_piece_lock(id)
	sfx.play(snd.hit)
end

function on_piece_hold(id)
	sfx.play(snd.hold)
end

function on_hold_fail()
	sfx.play(snd.hold_not)
end

function on_line_clear(data)
	if data.lines == 4 then
		sfx.play(snd.tetris)
	else
		sfx.play(snd.clear)
	end

	-- Find the highest occupied cell on the board
	local top_y = nil
	for y = board.invisible, board.height - 1 do
		for x = 0, board.width - 1 do
			if board.get(x, y) then
				top_y = y
				break
			end
		end
		if top_y then break end
	end

	-- Convert board row to screen pixel position (board starts at (100, 0), brick_size=12)
	-- Board y is raw board index (0-23); rows 0..board.invisible-1 are above the visible area
	local board_x, board_y = 100, 0
	local brick_size = 12
	local base_y
	if top_y then
		base_y = board_y + (top_y - board.invisible - 5) * brick_size  -- slightly above the highest piece
	else
		base_y = 48  -- fallback
	end

	local lines = data.lines
	if lines > 5 then lines = 5 end
	local name = clear_names[lines]
	if data.tspin and data.tspin ~= "" then
		name = data.tspin .. " " .. name
	end

	-- Show clear info, stacked tightly line by line
	local ly = base_y
	screen.pop_up(160, ly, name, 1500, font, 255, 255, 255, align.CENTER, align.TOP)
	ly = ly + 8
	screen.pop_up(160, ly, "+" .. data.score .. " points", 1500, font, 255, 255, 255, align.CENTER, align.TOP)
	ly = ly + 8

	if data.b2b then
		screen.pop_up(160, ly, "Back-2-Back", 1500, font, 255, 255, 255, align.CENTER, align.TOP)
		ly = ly + 8
	end

	if data.combo and data.combo > 0 then
		screen.pop_up(160, ly, "Combo " .. data.combo .. "x", 1500, font, 255, 255, 255, align.CENTER, align.TOP)
		ly = ly + 8
	end

	if data.pc then
		screen.pop_up(160, ly, "Perfect Clear", 1500, font, 255, 255, 255, align.CENTER, align.TOP)
	end
end

function on_game_over(reason)
	sfx.play(snd.game_over)
end

function on_hard_drop(rows, start_y)
end

function on_pause()
end

function format_ms(ms)
	local cs = math.floor(ms / 10) % 100
	local ss = math.floor(ms / 1000) % 60
	local mm = math.floor(ms / 60000)
	return string.format("%02d:%02d.%02d", mm, ss, cs)
end
