local bg_img, font, label_x

function on_skin_load(r)
	bg_img = r.load_image("bg.png")

	-- tetromino colours (from game.txt)
	r.set_tetromino_color(fig.I, 128, 0, 159, 218)
	r.set_tetromino_color(fig.O, 128, 254, 203, 0)
	r.set_tetromino_color(fig.T, 128, 149, 45, 152)
	r.set_tetromino_color(fig.S, 128, 105, 190, 40)
	r.set_tetromino_color(fig.Z, 128, 237, 41, 57)
	r.set_tetromino_color(fig.J, 128, 0, 101, 189)
	r.set_tetromino_color(fig.L, 128, 255, 121, 0)

	r.set_brick_size(12)
	r.set_board_xy(100, 0)

	-- bricksprite
	local bmp = r.load_image("bricks.png")
	r.set_bricksprite(bmp)

	-- no shadow under bricks
	r.set_ghost_alpha(128)

	-- font
	font = r.load_font("VeraMono-Bold.ttf", 10)

	-- Pre-render background boxes
	r.draw_rect_to(bg_img, 100, 0, 120, 240, 255, 255, 255, 36)
	r.draw_rect_to(bg_img, 0, 0, 100, 240, 0, 0, 0, 80)
	r.draw_rect_to(bg_img, 220, 0, 100, 240, 0, 0, 0, 80)

	-- Pre-render static piece icons (left stat area, 29px spacing)
	for i = 0, 6 do
		r.draw_piece_shape_to(bg_img, i, 6, 39 + i * 29, i, 180)
	end

	-- Label offset for dynamic alignment
	label_x = 4
	res.draw_text_to(bg_img, font, "Top:", label_x, 0, 255, 255, 48)
	res.draw_text_to(bg_img, font, "Score:", label_x, 9, 255, 255, 48)
	res.draw_text_to(bg_img, font, "Level:", label_x, 18, 255, 255, 48)
	res.draw_text_to(bg_img, font, "Lines:", label_x, 27, 255, 255, 48)
end

function on_skin_unload() end

function on_background_draw()
	res.draw_image(bg_img, 0, 0)

	-- HUD labels + values (aligned top-left)
	local val_x = 45

	res.draw_text(font, game.hiscore(), val_x + 1, 1, 0, 0, 0)
	res.draw_text(font, game.hiscore(), val_x, 0, 255, 255, 48)

	res.draw_text(font, game.score(), val_x + 1, 10, 0, 0, 0)
	res.draw_text(font, game.score(), val_x, 9, 255, 255, 48)

	res.draw_text(font, game.level(), val_x + 1, 19, 0, 0, 0)
	res.draw_text(font, game.level(), val_x, 18, 255, 255, 48)

	res.draw_text(font, game.lines(), val_x + 1, 28, 0, 0, 0)
	res.draw_text(font, game.lines(), val_x, 27, 255, 255, 48)

	-- Timer (centred at top)
	local mode = game.mode()
	local t
	if mode == "ultra" then
		t = format_ms(game.ultra_time_left())
	else
		t = format_ms(game.timer())
	end
	res.draw_text(font, t, 161, 6, 0, 0, 0, 1, 0)
	res.draw_text(font, t, 160, 4, 255, 255, 48, 1, 0)

	-- Stats (bars)
	for i = 0, 6 do
		res.draw_bar(64, 50 + i * 27, 28, 7, game.stat(i), 56, 0,
			     255, 192, 192, 255,
			     255, 255, 255, 64)
	end

	-- "Next:" label
	res.draw_text(font, "Next:", 227, 0, 255, 255, 48)

	-- Next pieces (alternating columns, 35px spacing)
	local nx = { 266, 226, 266, 226, 266, 226 }
	local ny = { 22, 57, 92, 127, 162, 197 }
	for i = 1, 6 do
		local nxt = figure.next(i)
		if nxt then
			res.draw_piece_shape(nxt.id, nx[i], ny[i], nxt.color, 255)
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
		res.show_timed_text(161, 21, "Back-2-Back", 1500, font, 0, 0, 0, 1, 0)
		res.show_timed_text(160, 20, "Back-2-Back", 1500, font, 255, 255, 48, 1, 0)
	end
	res.show_timed_text(161, 31, name, 1500, font, 0, 0, 0, 1, 0)
	res.show_timed_text(160, 30, name, 1500, font, 255, 255, 48, 1, 0)
	if data.combo and data.combo > 0 then
		res.show_timed_text(161, 41, "combo " .. data.combo .. "x", 1500, font, 0, 0, 0, 1, 0)
		res.show_timed_text(160, 40, "combo " .. data.combo .. "x", 1500, font, 255, 255, 48, 1, 0)
		res.play_sfx(math.min(sfx.combo_1 + data.combo - 1, sfx.combo_9))
	end
	if data.pc then
		res.show_timed_text(161, 51, "Perfect Clear", 1500, font, 0, 0, 0, 1, 0)
		res.show_timed_text(160, 50, "Perfect Clear", 1500, font, 255, 255, 48, 1, 0)
	end
end

function on_foreground_draw() end

function format_ms(ms)
	local cs = math.floor(ms / 10) % 100
	local ss = math.floor(ms / 1000) % 60
	local mm = math.floor(ms / 60000)
	return string.format("%02d:%02d.%02d", mm, ss, cs)
end
