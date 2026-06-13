-- Yatka Lua skin: minimal example
local bg_img, font, small_font
local brick_w, brick_h, board_x, board_y
local behelit_anim

function on_skin_load(r)
	bg_img = r.load_image("bg.png")

	-- set colours (using symbolic fig.* constants)
	r.set_tetromino_color(fig.I, 128, 215, 64, 0)
	r.set_tetromino_color(fig.O, 128, 59, 52, 255)
	r.set_tetromino_color(fig.T, 128, 115, 121, 0)
	r.set_tetromino_color(fig.S, 128, 0, 132, 96)
	r.set_tetromino_color(fig.Z, 128, 75, 160, 255)
	r.set_tetromino_color(fig.J, 128, 255, 174, 10)
	r.set_tetromino_color(fig.L, 128, 255, 109, 247)

	r.set_brick_size(12)
	r.set_board_xy(100, 0)
	board_x, board_y = 100, 0
	brick_w, brick_h = r.brick_size()

	-- load & wire bricksprite (transfers ownership to C)
	local bmp = r.load_image("bricks.png")
	r.set_bricksprite(bmp)

	-- shadow: offset (-1,-1), black at alpha 128
	r.set_shadow(4, 4, 48, 0, 0, 192)

	r.set_ghost_alpha(128)

	big_font = r.load_font("arcade.ttf", 8)
	font = r.load_font("arcade.ttf", 7)
	small_font = r.load_font("arcade.ttf", 6)

	behelit_anim = r.load_animation("behelit.png", 65, 72, 50)

	-- Pre-render static HUD labels + semi-transparent boxes onto the background once
	r.draw_text_to(bg_img, font, "Best:", 0, 0, 255, 255, 255)
	r.draw_text_to(bg_img, font, "Score:", 0, 7, 255, 255, 255)
	r.draw_text_to(bg_img, font, "Level:", 0, 14, 255, 255, 255)
	r.draw_text_to(bg_img, font, "Lines:", 0, 21, 255, 255, 255)
	r.draw_text_to(bg_img, big_font, "NEXT", 246, 11, 255, 255, 255)

	r.draw_rect_to(bg_img, 100, 0, 120, 240, 255, 255, 255, 48)
	r.draw_rect_to(bg_img, 246, 22, 48, 24, 255, 255, 255, 48)
	r.draw_rect_to(bg_img, 246, 52, 48, 24, 255, 255, 255, 48)
	r.draw_rect_to(bg_img, 246, 82, 48, 24, 255, 255, 255, 48)
	r.draw_rect_to(bg_img, 246, 112, 48, 24, 255, 255, 255, 48)
	r.draw_rect_to(bg_img, 246, 142, 48, 24, 255, 255, 255, 48)
	r.draw_rect_to(bg_img, 246, 172, 48, 24, 255, 255, 255, 48)

	-- Pre-render semi-transparent piece icons (stat area, 30px spacing)
	for i = 0, 6 do
		r.draw_piece_shape_to(bg_img, i, 6, 26 + i * 30, 7, 160)
	end
end

function on_skin_unload() end

function on_background_draw()
	res.draw_image(bg_img, 0, 0)

	-- HUD (dynamic values only; static labels + boxes are baked into bg_img)
	local hiscore_type = ({
		marathon = game.MARATHON_SCORE,
		sprint   = game.SPRINT_TIME,
		ultra    = game.ULTRA_SCORE
	})[game.mode()]
	local hiscore_val = game.hiscore(hiscore_type)
	if hiscore_type == game.SPRINT_TIME then
		hiscore_val = format_ms(hiscore_val)
	end
	res.draw_text(font, hiscore_val, 45, 0)
	res.draw_text(font, game.score(), 45, 7)
	res.draw_text(font, game.level(), 45, 14)

	local mode = game.mode()
	if mode == "sprint" then
		res.draw_text(font, game.lines() .. " / 40", 45, 21)
	else
		res.draw_text(font, game.lines(), 45, 21)
	end

	if mode == "ultra" then
		res.draw_text(small_font, format_ms(game.ultra_time_left()), 320, 240, 255, 255, 255, 2, 2)
	else
		res.draw_text(small_font, format_ms(game.timer()), 320, 240, 255, 255, 255, 2, 2)
	end

	res.draw_text(small_font, game.fps() .. " fps", 320, 0, 255, 255, 255, 2, 0)

	-- stats (bars, 30px spacing to match default skin; piece icons baked into bg_img)
	for i = 0, 6 do
		res.draw_bar(64, 38 + i * 30, 28, 7, game.stat(i), 56, 0,
			     255, 192, 192, 255,
			     255, 255, 255, 64)
	end

	-- next pieces (centered, matching DSL figure command layout)
	for i = 1, 6 do
		local nxt = figure.next(i)
		if nxt then
			res.draw_piece_shape(nxt.id, 246, 22 + (i - 1) * 30, nxt.color, 255)
		end
	end
end

function on_move(direction)
	res.play_sfx(sfx.click)
end

function on_piece_lock(data)
	res.play_sfx(sfx.hit)
end

function on_line_clear(data)
	if not data.speech_on then
		res.play_sfx(sfx.clear)
	end
	local clear_names = { "Single", "Double", "Triple", "Tetris", "Cheatris" }
	local lines = data.lines
	if lines > 5 then lines = 5 end
	local name = clear_names[lines]
	if data.tspin and data.tspin ~= "" then
		name = data.tspin .. name
	end
	if data.b2b then
		res.show_timed_text(160, 8, "Back-2-Back", 1500, font, 255, 255, 255, 1, 0)
		local sw = res.screen_w()
		res.add_particle(
			math.random() * (sw - 62),             -- random along bottom edge
			res.screen_h(),
			(math.random() - 0.5) * 200,           -- wider horizontal spread
			-421,                                  -- shoot upward (peaks at y ≈ 40)
			behelit_anim,
			0, 0, 62, 75,
			0, 443)                                -- gravity (total flight 1.9 s = 38 frames × 50 ms)
	end
	res.show_timed_text(160, 16, name, 1500, font, 255, 255, 255, 1, 0)
	if data.pc then
		res.show_timed_text(160, 32, "Perfect Clear", 1500, font, 255, 255, 255, 1, 0)
	end
	if data.combo and data.combo > 0 then
		res.show_timed_text(160, 24, "combo " .. data.combo .. "x", 1500, small_font, 255, 255, 255, 1, 0)
		res.play_sfx(math.min(sfx.combo_1 + data.combo - 1, sfx.combo_9))
	end

	-- spawn particles from cleared lines (C handles update + drawing)
	if data.particles then
		for _, p in ipairs(data.particles) do
			res.add_particle(
				board_x + p.x * brick_w,
				board_y + (p.y - 1) * brick_w,
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

