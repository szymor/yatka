local bg_img, font, snd, lh
local tetris_count = 0
local combo_streak = 0
local b2b_active = false
local last_clear_name = ""
local last_clear_score = 0
local perfect_clear_count = 0
local total_clears = 0
local total_clear_lines = 0

-- Next pieces: single column at x=152, 36px spacing
local next_x = { 152, 152, 152, 152, 152, 152 }
local next_y = { 36, 72, 108, 144, 180, 216 }

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
	cfg.set_board_xy(200, 0)
	cfg.set_holdmode("preserve")

	local bmp = gfx.load_image("bricks.png")
	cfg.set_bricksprite(bmp)

	cfg.set_ghost_alpha(69)

	font = gfx.load_font("monobit.ttf", 16)
	_, lh = font:measure("Ag")
	lh = lh // 2

	-- Pre-render semi-transparent panel background
	bg_img:draw_rect(0, 0, 200, 240, 255, 255, 255, 20)

	-- Load default sounds
	snd = sfx.loadDefaults()
end

function on_skin_unload() end

function on_background_draw()
	screen.draw_image(bg_img, 0, 0)

	local mode = game.mode()
	local y = 0

	-- Game state
	font:print("Mode: " .. mode, 5, y, 255, 255, 255); y = y + lh
	font:print("Score: " .. game.score(), 5, y, 255, 255, 255); y = y + lh

	-- All records
	local function show_rec(name, id)
		local val = game.hiscore(id)
		if id == record.SPRINT_TIME then val = format_ms(val) end
		font:print(string.format("Best %s: %s", name, val), 5, y, 180, 180, 180); y = y + lh
	end
	local labels = {
		{ record.MARATHON_SCORE, "marathon score" },
		{ record.MARATHON_LINES, "marathon lines" },
		{ record.SPRINT_TIME,    "sprint time" },
		{ record.ULTRA_SCORE,    "ultra score" },
		{ record.ULTRA_LINES,    "ultra lines" },
	}
	for _, pair in ipairs(labels) do
		local id, name = pair[1], pair[2]
		show_rec(name, id)
	end

	font:print("Level: " .. game.level(), 5, y, 255, 255, 255); y = y + lh
	font:print("Lines: " .. game.lines(), 5, y, 255, 255, 255); y = y + lh

	local ttr = game.lines() > 0 and (tetris_count * 400 / game.lines()) or 0
	font:print(string.format("Tetris: %.0f%%", ttr), 5, y, 255, 255, 255); y = y + lh

	-- Timer / mode-specific
	font:print("Timer: " .. format_ms(game.timer()), 5, y, 255, 255, 255); y = y + lh
	if mode == "sprint" then
		font:print("Target: " .. game.sprint_target .. " lines", 5, y, 255, 255, 255); y = y + lh
	elseif mode == "ultra" then
		font:print("Left: " .. format_ms(game.ultra_time_left()), 5, y, 255, 255, 255); y = y + lh
		font:print("Duration: " .. game.ultra_duration / 1000 .. "s", 5, y, 255, 255, 255); y = y + lh
	end

	-- Board info
	font:print("Board: " .. board.width .. "x" .. board.height, 5, y, 255, 255, 255); y = y + lh
	font:print("Buffer: " .. board.invisible .. " rows", 5, y, 255, 255, 255); y = y + lh

	-- Active figure
	local fig = figure.active()
	if fig then
		font:print("Fig: id=" .. fig.id .. " col=" .. fig.color, 5, y, 255, 255, 255); y = y + lh
	else
		font:print("Fig: none", 5, y, 255, 255, 255); y = y + lh
	end

	-- Piece counts
	font:print("Dropped: " .. game.dropped(), 5, y, 255, 255, 255); y = y + lh
	font:print("Pressed: " .. game.pressed(), 5, y, 255, 255, 255); y = y + lh
	font:print("FPS: " .. game.fps(), 5, y, 255, 255, 255); y = y + lh
	font:print("Debris: " .. game.debris(), 5, y, 255, 255, 255); y = y + lh
	font:print("Ticks: " .. game.ticks(), 5, y, 255, 255, 255); y = y + lh

	-- PPS / KPT
	local time_s = game.timer() / 1000
	local dropped = game.dropped()
	local pressed = game.pressed()
	local pps = (time_s > 0) and (dropped / time_s) or 0
	local kpt = (dropped > 0) and (pressed / dropped) or 0
	font:print(string.format("PPS: %.2f  KPT: %.2f", pps, kpt), 5, y, 255, 255, 255); y = y + lh

	-- Line clear stats
	font:print("Clears: " .. total_clears, 5, y, 255, 255, 255); y = y + lh
	local avg = total_clears > 0 and (total_clear_lines / total_clears) or 0
	font:print(string.format("Avg lines: %.2f", avg), 5, y, 255, 255, 255); y = y + lh
	font:print("PCs: " .. perfect_clear_count, 5, y, 255, 255, 255); y = y + lh
	font:print("Combo: " .. combo_streak, 5, y, 255, 255, 255); y = y + lh
	font:print("B2B: " .. tostring(b2b_active), 5, y, 255, 255, 255); y = y + lh
	font:print("Last: " .. last_clear_name, 5, y, 255, 255, 255); y = y + lh
	font:print("Last score: " .. last_clear_score, 5, y, 255, 255, 255); y = y + lh

	-- Piece statistics
	font:print("Stats: I" .. game.stat(0) .. " O" .. game.stat(1) .. " T" .. game.stat(2), 5, y, 255, 255, 255); y = y + lh
	font:print("   S" .. game.stat(3) .. " Z" .. game.stat(4) .. " J" .. game.stat(5) .. " L" .. game.stat(6), 5, y, 255, 255, 255); y = y + lh

	-- Hold piece
	local held = figure.held()
	if held then
		screen.draw_shape(held.id, 152, 0, held.color, 128)
	end

	-- Next pieces
	for i = 1, 6 do
		local nxt = figure.next(i)
		if nxt then
			screen.draw_shape(nxt.id, next_x[i], next_y[i], nxt.color, 128)
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
	if data.lines == 4 then
		tetris_count = tetris_count + 1
	end
	combo_streak = data.combo or 0
	b2b_active = data.b2b or false
	last_clear_score = data.score
	perfect_clear_count = perfect_clear_count + (data.pc and 1 or 0)
	total_clears = total_clears + 1
	total_clear_lines = total_clear_lines + data.lines

	local names = { "Single", "Double", "Triple", "Tetris", "Cheatris" }
	local n = data.lines
	if n > 5 then n = 5 end
	last_clear_name = names[n]
	if data.tspin and data.tspin ~= "" then
		last_clear_name = data.tspin .. last_clear_name
	end
	if data.pc then
		last_clear_name = last_clear_name .. " PC"
	end

	sfx.play(snd.clear)
	if data.combo and data.combo > 0 then
		sfx.play(snd.combo[math.min(data.combo, 9)])
	end
end

function format_ms(ms)
	local cs = math.floor(ms / 10) % 100
	local ss = math.floor(ms / 1000) % 60
	local mm = math.floor(ms / 60000)
	return string.format("%02d:%02d.%02d", mm, ss, cs)
end
