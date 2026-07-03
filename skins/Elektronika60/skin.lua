-- Elektronika 60 monochrome terminal skin

local bg_img, font, beep, beep2
local clear_names = { "Single", "Double", "Triple", "Tetris", "Cheatris" }

function on_skin_load()
	bg_img = gfx.load_image("bg.png")

	-- Monochrome green - all pieces identical color
	cfg.set_active_figure_dim(true)

	cfg.set_brick_size(10)
	cfg.set_board_xy(100, 20)

	local bmp = gfx.load_image("bricks.png")
	cfg.set_bricksprite(bmp)

	cfg.set_ghost_alpha(0)

	-- debris_dim false: debris renders with normal block colours (not dimmed)
	cfg.set_debris_dim(true)

	font = gfx.load_font("arcade.ttf", 8)

	-- Pre-render static labels onto background
	local r, g, b = 163, 206, 39
	bg_img:draw_text(font, (game.mode() == "sprint") and "TIME" or "SCORE", 230, 20, r, g, b)
	bg_img:draw_text(font, "LEVEL", 230, 50, r, g, b)
	bg_img:draw_text(font, (game.mode() == "sprint") and "LINES LEFT" or "LINES", 230, 80, r, g, b)

	-- Load single beep sound for all effects
	beep = sfx.load("sfx/beep.wav")
	beep2 = sfx.load("sfx/beep2.wav")
end

function on_skin_unload() end

function on_background_draw()
	screen.draw_image(bg_img, 0, 0)

	local mode = game.mode()
	local r, g, b = 163, 206, 39

	-- Score or timer (sprint mode)
	if mode == "sprint" then
		screen.draw_text(font, format_ms(game.timer()), 230, 30, r, g, b)
	else
		screen.draw_text(font, game.score(), 230, 30, r, g, b)
	end

	-- Level
	screen.draw_text(font, game.level(), 230, 60, r, g, b)

	-- Lines (sprint shows remaining)
	if mode == "sprint" then
		screen.draw_text(font, 40 - game.lines(), 230, 90, r, g, b)
	else
		screen.draw_text(font, game.lines(), 230, 90, r, g, b)
	end

	-- Timer (bottom-aligned)
	local t
	if mode == "ultra" then
		t = format_ms(game.ultra_time_left())
		screen.draw_text(font, t, 230, 236, r, g, b, 0, 2)
	elseif mode == "marathon" then
		t = format_ms(game.timer())
		screen.draw_text(font, t, 230, 236, r, g, b, 0, 2)
	end

	-- Next pieces (3 previews, 30px spacing)
	for i = 1, 3 do
		local nxt = figure.next(i)
		if nxt then
			screen.draw_shape(nxt.id, 230, 140 + (i - 1) * 30, fig.GRAY, 255)
		end
	end
end

function on_foreground_draw() end

function on_move(direction)
	beep2:play()
end

function on_piece_lock(data)
	beep:play()
end

function on_rotate(direction)
	beep:play()
end

function on_line_clear(data)
	beep:play()

	local lines = data.lines
	if lines > 5 then lines = 5 end
	local name = clear_names[lines]
	if data.tspin and data.tspin ~= "" then
		name = data.tspin .. name
	end

	screen.pop_up(160, 8, name, 1500, font, 163, 206, 39, 1, 0)

	if data.b2b then
		screen.pop_up(160, 18, "Back-2-Back", 1500, font, 163, 206, 39, 1, 0)
	end

	if data.combo and data.combo > 0 then
		screen.pop_up(160, 28, "combo " .. data.combo .. "x", 1500, font, 163, 206, 39, 1, 0)
	end
end

function on_game_over(reason) end

function format_ms(ms)
	local cs = math.floor(ms / 10) % 100
	local ss = math.floor(ms / 1000) % 60
	local mm = math.floor(ms / 60000)
	return string.format("%02d:%02d.%02d", mm, ss, cs)
end
