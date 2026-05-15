-- Yatka Lua skin: minimal example
local bg_img, font, small_font
local brick_w, brick_h



function skin_load(r)
	bg_img = r.load_image(r.skin_path .. "bg.png")

	-- set colours (row-major: I,O,T,S,Z,J,L)
	r.set_tetromino_color(0, 128, 215, 64, 0)     -- I
	r.set_tetromino_color(1, 128, 59, 52, 255)     -- O
	r.set_tetromino_color(2, 128, 115, 121, 0)     -- T
	r.set_tetromino_color(3, 128, 0, 132, 96)      -- S
	r.set_tetromino_color(4, 128, 75, 160, 255)    -- Z
	r.set_tetromino_color(5, 128, 255, 174, 10)    -- J
	r.set_tetromino_color(6, 128, 255, 109, 247)   -- L

	r.set_brick_size(12)
	r.set_board_xy(100, 0)
	brick_w, brick_h = r.brick_size()

	-- load & wire bricksprite (transfers ownership to C)
	local bmp = r.load_image(r.skin_path .. "bricks.png")
	r.set_bricksprite(bmp)

	-- shadow: offset (-1,-1), black at alpha 128
	r.set_shadow(4, 4, 48, 0, 0, 192)

	r.set_ghost_alpha(128)

	font = r.load_font(r.skin_path .. "arcade.ttf", 7)
	small_font = r.load_font(r.skin_path .. "arcade.ttf", 6)
end

function skin_unload() end

function draw_background()
	res.draw_image(bg_img, 0, 0)
	-- semi-transparent boxes (board area + next-piece backgrounds)
	res.draw_rect(100, 0, 120, 240, 255, 255, 255, 48)
	res.draw_rect(246, 22, 48, 24, 255, 255, 255, 48)
	res.draw_rect(246, 52, 48, 24, 255, 255, 255, 48)
	res.draw_rect(246, 82, 48, 24, 255, 255, 255, 48)
	res.draw_rect(246, 112, 48, 24, 255, 255, 255, 48)
	res.draw_rect(246, 142, 48, 24, 255, 255, 255, 48)
	res.draw_rect(246, 172, 48, 24, 255, 255, 255, 48)
end

function draw_board()
	local bx, by = 100, 0
	-- skip invisible row (y=0), BOARD_HEIGHT=21, visible rows 1..20
	for y = 1, board.height - 1 do
		for x = 0, board.width - 1 do
			local blk = board.get(x, y)
			if blk then
				res.draw_brick(
					bx + x * brick_w,
					by + (y - 1) * brick_w,
					blk.color, blk.orient)
			end
		end
	end
end

function draw_active_figure(interp_y)
	local fig = figure.active()
	if not fig then return end
	local bx, by = 100, 0
	for _, cell in ipairs(fig.cells) do
		res.draw_brick(
			bx + (fig.x + cell.x) * brick_w,
			by + (fig.y + cell.y - 1) * brick_w + interp_y,
			fig.color, cell.orient)
	end
end


function on_line_clear(data)
	local clear_names = { "Single", "Double", "Triple", "Tetris", "Cheatris" }
	local lines = data.lines
	if lines > 5 then lines = 5 end
	local name = clear_names[lines]
	if data.tspin and data.tspin ~= "" then
		name = data.tspin .. name
	end
	if data.b2b then
		res.show_timed_text(160, 8, "Back-2-Back", 1500, font, 255, 255, 255, 1, 0)
	end
	res.show_timed_text(160, 16, name, 1500, font, 255, 255, 255, 1, 0)
	if data.combo and data.combo > 0 then
		res.show_timed_text(160, 24, "combo " .. data.combo .. "x", 1500, small_font, 255, 255, 255, 1, 0)
	end
end

function draw_foreground() end

function draw_hud()
	res.draw_text(font, "Score: " .. game.score(), 0, 0)
	res.draw_text(font, "Level: " .. game.level(), 0, 7)
	res.draw_text(font, "Lines: " .. game.lines(), 0, 14)
	res.draw_text(small_font, game.dropped() .. " pcs", 0, 21)
	res.draw_text(small_font, game.timer_str(), 320, 240, 255, 255, 255, 2, 2)
	res.draw_text(small_font, game.fps() .. " fps", 320, 0, 255, 255, 255, 2, 0)

	-- stats (bars, 30px spacing to match default skin)
	for i = 0, 6 do
		-- shape icon next to the bar (centered like DSL shape command)
		local cells = game.shape_cells(i)
		if cells then
			local minx, maxx, miny, maxy = 4, -1, 4, -1
			for _, c in ipairs(cells) do
				if c.x < minx then minx = c.x end
				if c.x > maxx then maxx = c.x end
				if c.y < miny then miny = c.y end
				if c.y > maxy then maxy = c.y end
			end
			local sw, sh = maxx - minx + 1, maxy - miny + 1
			local ox = (4 - sw) * brick_w / 2 - minx * brick_w
			local oy = (2 - sh) * brick_w / 2 - miny * brick_w
			local sx, sy = 6 + ox, 26 + i * 30 + oy
			for _, cell in ipairs(cells) do
				res.draw_brick(sx + cell.x * brick_w,
					       sy + cell.y * brick_w,
					       7, cell.orient, 160)
			end
		end
		res.draw_bar(64, 38 + i * 30, 28, 7, game.stat(i), 56, 0,
			     255, 192, 192, 255,
			     255, 255, 255, 64)
	end

	-- next pieces (centered, matching DSL figure command layout)
	for i = 1, 6 do
		local nxt = figure.next(i)
		if nxt then
			local cells = game.shape_cells(nxt.id)
			if cells then
				local minx, maxx, miny, maxy = 4, -1, 4, -1
				for _, c in ipairs(cells) do
					if c.x < minx then minx = c.x end
					if c.x > maxx then maxx = c.x end
					if c.y < miny then miny = c.y end
					if c.y > maxy then maxy = c.y end
				end
				local sw, sh = maxx - minx + 1, maxy - miny + 1
				local ox = (4 - sw) * brick_w / 2 - minx * brick_w
				local oy = (2 - sh) * brick_w / 2 - miny * brick_w
				local nx, ny = 246 + ox, 22 + (i - 1) * 30 + oy
				for _, cell in ipairs(cells) do
					res.draw_brick(
						nx + cell.x * brick_w,
						ny + cell.y * brick_w,
						nxt.color, cell.orient, 128)
				end
			end
		end
	end


end
