local bg_img, font, label_x
local board_x, board_y, brick_w, star_anim, snd

function on_skin_load()
	bg_img = gfx.load_image("bg.png")

	-- tetromino colours (from game.txt)
	cfg.set_tetromino_color(fig.I, 128, 0, 159, 218)
	cfg.set_tetromino_color(fig.O, 128, 254, 203, 0)
	cfg.set_tetromino_color(fig.T, 128, 149, 45, 152)
	cfg.set_tetromino_color(fig.S, 128, 105, 190, 40)
	cfg.set_tetromino_color(fig.Z, 128, 237, 41, 57)
	cfg.set_tetromino_color(fig.J, 128, 0, 101, 189)
	cfg.set_tetromino_color(fig.L, 128, 255, 121, 0)

	brick_w = 12
	cfg.set_brick_size(brick_w)
	board_x = 100
	board_y = 0
	cfg.set_board_xy(board_x, board_y)

	-- load the animated star spritesheet
	star_anim = gfx.load_animation("star.png", 16, 16, 17)

	-- bricksprite
	local bmp = gfx.load_image("bricks.png")
	cfg.set_bricksprite(bmp)

	-- no shadow under bricks
	cfg.set_ghost_alpha(128)

	-- font
	font = gfx.load_font("VeraMono-Bold.ttf", 10)

	-- Pre-render background boxes
	bg_img:draw_rect(100, 0, 120, 240, 255, 255, 255, 36)
	bg_img:draw_rect(0, 0, 100, 240, 0, 0, 0, 80)
	bg_img:draw_rect(220, 0, 100, 240, 0, 0, 0, 80)

	-- Pre-render static piece icons (left stat area, 29px spacing)
	for i = 0, 6 do
		bg_img:draw_shape(i, 6, 39 + i * 29, i, 180)
	end

	-- Label offset for dynamic alignment
	label_x = 4
	bg_img:draw_text(font, "Top:", label_x, 0, 255, 255, 48)
	bg_img:draw_text(font, "Score:", label_x, 9, 255, 255, 48)
	bg_img:draw_text(font, "Level:", label_x, 18, 255, 255, 48)
	bg_img:draw_text(font, "Lines:", label_x, 27, 255, 255, 48)
	snd = sfx.loadDefaults()
end

function on_skin_unload() end

function on_background_draw()
	screen.draw_image(bg_img, 0, 0)

	local mode = game.mode()
	-- HUD labels + values (aligned top-left)
	local val_x = 45
	local hiscore_type = ({
		marathon = record.MARATHON_SCORE,
		sprint   = record.SPRINT_TIME,
		ultra    = record.ULTRA_SCORE
	})[mode]
	local hiscore_val = game.hiscore(hiscore_type)
	if hiscore_type == record.SPRINT_TIME then
		hiscore_val = format_ms(hiscore_val)
	end

	screen.draw_text(font, hiscore_val, val_x + 1, 1, 0, 0, 0)
	screen.draw_text(font, hiscore_val, val_x, 0, 255, 255, 48)

	screen.draw_text(font, game.score(), val_x + 1, 10, 0, 0, 0)
	screen.draw_text(font, game.score(), val_x, 9, 255, 255, 48)

	screen.draw_text(font, game.level(), val_x + 1, 19, 0, 0, 0)
	screen.draw_text(font, game.level(), val_x, 18, 255, 255, 48)

	local lines = game.lines()
	if mode == "sprint" then
		lines = lines .. "/40"
	end
	screen.draw_text(font, lines, val_x + 1, 28, 0, 0, 0)
	screen.draw_text(font, lines, val_x, 27, 255, 255, 48)

	-- Timer (centred at top)
	local t
	if mode == "ultra" then
		t = format_ms(game.ultra_time_left())
	else
		t = format_ms(game.timer())
	end
	screen.draw_text(font, t, 161, 6, 0, 0, 0, 1, 0)
	screen.draw_text(font, t, 160, 4, 255, 255, 48, 1, 0)

	-- Stats (bars)
	for i = 0, 6 do
		screen.draw_bar(64, 50 + i * 29, 28, 7, game.stat(i), 56, 0,
			     255, 192, 192, 255,
			     255, 255, 255, 64)
	end

	-- "Next:" label
	screen.draw_text(font, "Next:", 227, 0, 255, 255, 48)

	-- Next pieces (alternating columns, 35px spacing)
	local nx = { 266, 226, 266, 226, 266, 226 }
	local ny = { 22, 57, 92, 127, 162, 197 }
	for i = 1, 6 do
		local nxt = figure.next(i)
		if nxt then
			screen.draw_shape(nxt.id, nx[i], ny[i], nxt.color, 255)
		end
	end
end

function on_move(direction)
	sfx.play(snd.click)
end

function on_hard_drop(rows, start_y)
	local fig = figure.active()
	if not fig then return end
	local num_stars = math.min(rows, 5)
	for i = 1, num_stars do
		local t = i / (num_stars + 1)
		local py = board_y + (start_y + t * rows - 1) * brick_w + math.random() * brick_w
		local px = board_x + fig.x * brick_w + math.random() * brick_w
		local vx = (math.random() - 0.5) * 30
		local vy = -math.random() * 40 - 20
		particle.add(px, py, vx, vy, star_anim, 0, 0, 16, 16, 0, 30, true, 1)
	end
end

function on_piece_lock(data)
	sfx.play(snd.hit)
end

function on_line_clear(data)
	if not data.speech_on then
		sfx.play(snd.clear)
	end

	local clear_names = { "Single", "Double", "Triple", "Tetris", "Cheatris" }
	local lines = data.lines
	if lines > 5 then lines = 5 end
	local name = clear_names[lines]
	if data.tspin and data.tspin ~= "" then
		name = data.tspin .. name
	end
	if data.b2b then
		screen.pop_up(161, 21, "Back-2-Back", 1500, font, 0, 0, 0, 1, 0)
		screen.pop_up(160, 20, "Back-2-Back", 1500, font, 255, 255, 48, 1, 0)
	end
	screen.pop_up(161, 31, name, 1500, font, 0, 0, 0, 1, 0)
	screen.pop_up(160, 30, name, 1500, font, 255, 255, 48, 1, 0)
	if data.combo and data.combo > 0 then
		screen.pop_up(161, 41, "combo " .. data.combo .. "x", 1500, font, 0, 0, 0, 1, 0)
		screen.pop_up(160, 40, "combo " .. data.combo .. "x", 1500, font, 255, 255, 48, 1, 0)
		sfx.play(snd.combo[math.min(data.combo, 9)])
	end
	if data.pc then
		screen.pop_up(161, 51, "Perfect Clear", 1500, font, 0, 0, 0, 1, 0)
		screen.pop_up(160, 50, "Perfect Clear", 1500, font, 255, 255, 48, 1, 0)
	end
end

function on_foreground_draw() end

function format_ms(ms)
	local cs = math.floor(ms / 10) % 100
	local ss = math.floor(ms / 1000) % 60
	local mm = math.floor(ms / 60000)
	return string.format("%02d:%02d.%02d", mm, ss, cs)
end
