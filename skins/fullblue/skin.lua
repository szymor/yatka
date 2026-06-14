local bg_strip, font, small_font, snd
local bg_frame_w = 320
local bg_frames = 3
local bg_cycle_ms = 300000

-- Piece shape icons (left panel, 30px spacing)
local sx = { 6, 6, 6, 6, 6, 6, 6 }
local sy = { 30, 60, 90, 120, 150, 180, 210 }

-- Next pieces (right panel, single column, 30px spacing)
local next_x = { 246, 246, 246, 246, 246, 246 }
local next_y = { 55, 85, 115, 145, 175, 205 }

-- Clear-type names
local clear_names = { "Single", "Double", "Triple", "Tetris", "Cheatris" }

function on_skin_load()
	bg_strip = gfx.load_image("bg.png")  -- 960x240, three frames

	-- Blue-tinted monochrome palette
	cfg.set_tetromino_color(fig.I, 240, 0, 255, 255)
	cfg.set_tetromino_color(fig.O, 224, 0, 0, 255)
	cfg.set_tetromino_color(fig.T, 208, 0, 0, 255)
	cfg.set_tetromino_color(fig.S, 240, 0, 0, 255)
	cfg.set_tetromino_color(fig.Z, 255, 0, 0, 0)
	cfg.set_tetromino_color(fig.J, 192, 0, 255, 255)
	cfg.set_tetromino_color(fig.L, 208, 0, 255, 255)

	cfg.set_brick_size(12)
	cfg.set_board_xy(100, 0)
	cfg.set_holdmode("preserve")

	local bmp = gfx.load_image("bricks.png")
	cfg.set_bricksprite(bmp)

	cfg.set_ghost_alpha(64)
	cfg.set_shadow(2, 2, 128, 24, 24, 48)

	font = gfx.load_font("LiberationSans-Bold.ttf", 9)
	small_font = gfx.load_font("LiberationSans-Bold.ttf", 7)



	-- Pre-render HUD elements onto all three background frames
	local function hud_at(dx)
		bg_strip:draw_rect(100 + dx, 0, 120, 240, 255, 255, 255, 32)
		bg_strip:draw_rect(240 + dx, 9, 60, 36, 255, 255, 255, 16)
		bg_strip:draw_rect(240 + dx, 49, 60, 186, 255, 255, 255, 16)
		bg_strip:draw_text(font, "Hold", 246 + dx, 3, 255, 255, 255)
		bg_strip:draw_text(font, "Next", 246 + dx, 43, 255, 255, 255)
		for i = 0, 6 do
			bg_strip:draw_shape(i, sx[i + 1] + dx, sy[i + 1], i, 128)
		end
	end
	hud_at(0)
	hud_at(320)
	hud_at(640)

	-- Load default sounds
	snd = sfx.loadDefaults()
end

function on_skin_unload() end

function on_background_draw()
	local bg_frame = math.floor(game.ticks() / bg_cycle_ms) % bg_frames
	bg_strip:draw(0, 0, bg_frame * bg_frame_w, 0, bg_frame_w, 240)

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

	-- Score (and hiscore, except sprint where it's shown in the timer line)
	if mode == "sprint" then
		screen.draw_text(font, game.score(), 4, 4, 255, 255, 255)
	else
		screen.draw_text(font, game.score() .. " / " .. hs, 4, 4, 255, 255, 255)
	end

	-- Level
	screen.draw_text(font, "Level " .. game.level(), 4, 16, 255, 255, 255)

	-- Timer / record (centered above board)
	if mode == "sprint" then
		local rec = game.hiscore(record.SPRINT_TIME)
		screen.draw_text(font, format_ms(game.timer()) .. " / " .. format_ms(rec), 160, 4, 255, 255, 255, 1, 0)
	elseif mode == "ultra" then
		screen.draw_text(font, format_ms(game.ultra_time_left()), 160, 4, 255, 255, 255, 1, 0)
	else
		screen.draw_text(font, format_ms(game.timer()), 160, 4, 255, 255, 255, 1, 0)
	end

	-- Lines (centered above board)
	screen.draw_text(font, game.lines() .. " line(s)", 160, 16, 255, 255, 255, 1, 0)

	-- FPS (top-right, small font)
	screen.draw_text(small_font, game.fps(), 320, 0, 255, 255, 255, 2, 0)

	-- Stat bars (left panel)
	for i = 0, 6 do
		screen.draw_bar(64, 38 + i * 30, 28, 7, game.stat(i), 56, 0,
			     192, 192, 255, 255,
			     255, 255, 255, 96)
	end

	-- Hold piece
	local held = figure.held()
	if held then
		screen.draw_shape(held.id, 246, 15, held.color, 255)
	end

	-- Next pieces
	for i = 1, 6 do
		local nxt = figure.next(i)
		if nxt then
			screen.draw_shape(nxt.id, next_x[i], next_y[i], nxt.color, 255)
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

	local lines = data.lines
	if lines > 5 then lines = 5 end
	local name = clear_names[lines]
	if data.tspin and data.tspin ~= "" then
		name = data.tspin .. name
	end

	-- Clear info centered above board ($lcttop/mid/bot positions)
	screen.pop_up(160, 32, name, 1500, font, 255, 255, 255, 1, 0)

	if data.b2b then
		screen.pop_up(160, 44, "Back-2-Back", 1500, font, 255, 255, 255, 1, 0)
	end

	if data.combo and data.combo > 0 then
		screen.pop_up(160, 56, "combo " .. data.combo .. "x", 1500, font, 255, 255, 255, 1, 0)
		sfx.play(snd.combo[math.min(data.combo, 9)])
	end
end

function on_game_over(reason)
end

function format_ms(ms)
	local cs = math.floor(ms / 10) % 100
	local ss = math.floor(ms / 1000) % 60
	local mm = math.floor(ms / 60000)
	return string.format("%02d:%02d.%02d", mm, ss, cs)
end
