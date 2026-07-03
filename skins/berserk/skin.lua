-- Yatka Lua skin: minimal example
local bg_img, font, small_font
local brick_w, brick_h, board_x, board_y
local behelit_anim, snd

function on_skin_load()
	bg_img = gfx.load_image("bg.png")

	-- set colours (using symbolic fig.* constants)
	cfg.set_tetromino_color(fig.I, 128, 215, 64, 0)
	cfg.set_tetromino_color(fig.O, 128, 59, 52, 255)
	cfg.set_tetromino_color(fig.T, 128, 115, 121, 0)
	cfg.set_tetromino_color(fig.S, 128, 0, 132, 96)
	cfg.set_tetromino_color(fig.Z, 128, 75, 160, 255)
	cfg.set_tetromino_color(fig.J, 128, 255, 174, 10)
	cfg.set_tetromino_color(fig.L, 128, 255, 109, 247)

	cfg.set_brick_size(12)
	cfg.set_board_xy(100, 0)
	board_x, board_y = 100, 0
	brick_w, brick_h = board.brick_size()

	-- load & wire bricksprite (transfers ownership to C)
	local bmp = gfx.load_image("bricks.png")
	cfg.set_bricksprite(bmp)

	-- shadow: offset (-1,-1), black at alpha 128
	cfg.set_shadow(4, 4, 48, 0, 0, 192)

	cfg.set_ghost_alpha(128)
	cfg.set_debris_dim(true)

	big_font = gfx.load_font("arcade.ttf", 8)
	font = gfx.load_font("arcade.ttf", 7)
	small_font = gfx.load_font("arcade.ttf", 6)

	behelit_anim = gfx.load_animation("behelit.png", 65, 72, 50)

	-- Pre-render static HUD labels + semi-transparent boxes onto the background once
	bg_img:draw_text(font, "Best:", 0, 0, 255, 255, 255)
	bg_img:draw_text(font, "Score:", 0, 7, 255, 255, 255)
	bg_img:draw_text(font, "Level:", 0, 14, 255, 255, 255)
	bg_img:draw_text(font, "Lines:", 0, 21, 255, 255, 255)
	bg_img:draw_text(big_font, "NEXT", 246, 11, 255, 255, 255)

	bg_img:draw_rect(100, 0, 120, 240, 255, 255, 255, 48)
	bg_img:draw_rect(246, 22, 48, 24, 255, 255, 255, 48)
	bg_img:draw_rect(246, 52, 48, 24, 255, 255, 255, 48)
	bg_img:draw_rect(246, 82, 48, 24, 255, 255, 255, 48)
	bg_img:draw_rect(246, 112, 48, 24, 255, 255, 255, 48)
	bg_img:draw_rect(246, 142, 48, 24, 255, 255, 255, 48)
	bg_img:draw_rect(246, 172, 48, 24, 255, 255, 255, 48)

	-- Pre-render semi-transparent piece icons (stat area, 30px spacing)
	for i = 0, 6 do
		bg_img:draw_shape(i, 6, 26 + i * 30, 7, 160)
	end
	snd = sfx.loadDefaults()
end

function on_skin_unload() end

function on_background_draw()
	screen.draw_image(bg_img, 0, 0)

	-- HUD (dynamic values only; static labels + boxes are baked into bg_img)
	local hiscore_type = ({
		marathon = record.MARATHON_SCORE,
		sprint   = record.SPRINT_TIME,
		ultra    = record.ULTRA_SCORE
	})[game.mode()]
	local hiscore_val = game.hiscore(hiscore_type)
	if hiscore_type == record.SPRINT_TIME then
		hiscore_val = format_ms(hiscore_val)
	end
	screen.draw_text(font, hiscore_val, 45, 0)
	screen.draw_text(font, game.score(), 45, 7)
	screen.draw_text(font, game.level(), 45, 14)

	local mode = game.mode()
	if mode == "sprint" then
		screen.draw_text(font, game.lines() .. " / 40", 45, 21)
	else
		screen.draw_text(font, game.lines(), 45, 21)
	end

	if mode == "ultra" then
		screen.draw_text(small_font, format_ms(game.ultra_time_left()), 320, 240, 255, 255, 255, 2, 2)
	else
		screen.draw_text(small_font, format_ms(game.timer()), 320, 240, 255, 255, 255, 2, 2)
	end

	screen.draw_text(small_font, game.fps() .. " fps", 320, 0, 255, 255, 255, 2, 0)

	-- stats (bars, 30px spacing to match default skin; piece icons baked into bg_img)
	for i = 0, 6 do
		screen.draw_bar(64, 38 + i * 30, 28, 7, game.stat(i), 56, 0,
			     255, 192, 192, 255,
			     255, 255, 255, 64)
	end

	-- next pieces (centered, matching DSL figure command layout)
	for i = 1, 6 do
		local nxt = figure.next(i)
		if nxt then
			screen.draw_shape(nxt.id, 246, 22 + (i - 1) * 30, nxt.color, 255)
		end
	end
end

function on_move(direction)
	sfx.play(snd.click)
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
		screen.pop_up(160, 8, "Back-2-Back", 1500, font, 255, 255, 255, 1, 0)
		local sw = screen.width
		particle.add(
			math.random() * (sw - 62),             -- random along bottom edge
			screen.height,
			(math.random() - 0.5) * 200,           -- wider horizontal spread
			-421,                                  -- shoot upward (peaks at y ≈ 40)
			behelit_anim,
			0, 0, 62, 75,
			0, 443)                                -- gravity (total flight 1.9 s = 38 frames × 50 ms)
	end
	screen.pop_up(160, 16, name, 1500, font, 255, 255, 255, 1, 0)
	if data.pc then
		screen.pop_up(160, 32, "Perfect Clear", 1500, font, 255, 255, 255, 1, 0)
	end
	if data.combo and data.combo > 0 then
		screen.pop_up(160, 24, "combo " .. data.combo .. "x", 1500, small_font, 255, 255, 255, 1, 0)
		sfx.play(snd.combo[math.min(data.combo, 9)])
	end

	-- spawn particles from cleared lines (C handles update + drawing)
	if data.particles then
		for _, p in ipairs(data.particles) do
			particle.add(
				board_x + p.x * brick_w,
				board_y + (p.y - board.invisible) * brick_w,
				(math.random() - 0.5) * 60,
				-math.random() * 120 - 40,
				p.sprite,
				p.sx, p.sy, p.sw, p.sh,
				0, 180
			)
		end
	end
end

function on_foreground_draw() end

function format_ms(ms)
	local cs = math.floor(ms / 10) % 100
	local ss = math.floor(ms / 1000) % 60
	local mm = math.floor(ms / 60000)
	return string.format("%02d:%02d.%02d", mm, ss, cs)
end

