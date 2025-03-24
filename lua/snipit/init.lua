local ffi = require("ffi")
local bit = require("bit")
local path = require("snipit.path")

local M = {}

-- todo add path.resolve({ paths, "../../../" }) cuz this is just crazy
M.root = path.dirname(path.dirname(path.dirname(debug.getinfo(1, "S").source:sub(2)) or error("nil")) or error("nil")) or error("nil")
M.has_setup = false

M.options = {
  save_file = "out.png",
  -- font_size = 32,
  fonts = {
    regular = M.root .. "/fonts/UbuntuMono-R.ttf",
    bold = M.root .. "/fonts/UbuntuMono-B.ttf",
    italic = M.root .. "/fonts/UbuntuMono-RI.ttf",
    bold_italic = M.root .. "/fonts/UbuntuMono-BI.ttf",
  },
}

-- for future if like empty string "   " or already has that group remove it
-- todo: handle priority and dont add same hl_styles
local function resolve_group(groups, group, hl_group)
  if not groups then
    group.hl_groups = { hl_group }
    return { group }
  end

  for i = #groups, 1, -1 do
    local curr_group = groups[i]
    local col = groups[i].col
    local w = #groups[i].token

    local left = group.col - col
    local right = w - left - #group.token

    if left > 0 and right < 0 then
      group.hl_groups = { hl_group }
      table.insert(groups, i + 1, group)

      return groups
    end

    if left == 0 and right == 0 then
      table.insert(groups[i].hl_groups, hl_group)
      return groups
    end

    if left >= 0 and right >= 0 then
      assert(w > left + right)

      local ltoken = curr_group.token:sub(1, left)
      local mtoken = curr_group.token:sub(left + 1, w - right)
      local rtoken = curr_group.token:sub(w - right + 1)

      local flag = false
      if #ltoken ~= 0 then
        local lgroup = {
          col = col,
          token = ltoken,
          hl_groups = vim.deepcopy(curr_group.hl_groups)
        }
        table.insert(groups, i, lgroup)
        flag = true
      end

      if #rtoken ~= 0 then
        local rgroup = {
          col = col + left + #mtoken,
          token = rtoken,
          hl_groups = vim.deepcopy(curr_group.hl_groups)
        }
        table.insert(groups, i + 1 + (flag and 1 or 0), rgroup)
        flag = true
      end

      table.insert(curr_group.hl_groups, hl_group)
      curr_group.col = col + left
      curr_group.token = mtoken

      return groups
    end
  end

  group.hl_groups = { hl_group }
  table.insert(groups, 1, group)

  return groups
end

local function inner_get_ts_syntax(out, line1, line2)
  local buf = vim.api.nvim_get_current_buf()
  local highlighter = vim.treesitter.highlighter
  local buf_highlighter = highlighter.active[buf]

  if not buf_highlighter then
    return
  end

  buf_highlighter.tree:for_each_tree(function (tstree, tree)
    if not tstree then
      return
    end

    local root = tstree:root()
    local root_start_row, _, root_end_row, _ = root:range()

    if root_start_row > line2 or root_end_row < line1 then
      return
    end

    local query = buf_highlighter:get_query(tree:lang())

    if not query:query() then
      return
    end

    local iter = query:query():iter_captures(root, buf_highlighter.bufnr, line1, line2)

    for capture, node, metadata in iter do
      local hl = query.hl_cache[capture]
      if not hl then
        goto continue
      end

      local hl_group = query._query.captures[capture]
      if not hl_group then
        goto continue
      end

      -- todo: idk use this
      if metadata then
        -- print(vim.inspect(metadata))
      end

      local row, col, row_end, col_end = node:range()

      if row ~= row_end then -- multi line strings idk what else
        local lines = vim.split(vim.treesitter.get_node_text(node, buf), '\n')

        local line_start = math.max(1, line1 - row + 1);
        local line_end = math.min(#lines, line2 - row)

        for i = line_start, line_end, 1 do
          out.syntax[row + i] = resolve_group(out.syntax[row + i], {
            col = i == 1 and col or 0 ,
            w = #lines[i],
            token = lines[i],
          }, hl_group)

          out.cols = math.max(out.cols, col + #lines[i])
        end

        if row_end + 2 > line2 then
          return
        end
        inner_get_ts_syntax(out, row_end + 1, line2)
        return
      end

      local token = vim.treesitter.get_node_text(node, buf)

      out.syntax[row + 1] = resolve_group(out.syntax[row + 1], {
        col = col,
        w = col_end - col,
        token = token,
      }, hl_group)

      out.cols = math.max(out.cols, col + #token)
      out.rows = math.max(out.rows, row + 1) -- this is the dumbest thing im doing here

      ::continue::
    end
  end, true)
end

local function get_ts_syntax(line1, line2)
  local out = {
    syntax = {},
    rows = 0,
    cols = 0,
  }
  inner_get_ts_syntax(out, line1 - 1, line2)
  return out.syntax, out.rows, out.cols
end

local function combine_fonts(groups)
  local font_type = 0
  -- update c abi so i could do smth like SN_FONT_TYPE_BOLD | SN_FOMT_TYPE_ITALIC
  -- then this dumb if else stuff
  for i = 1, #groups do
      local hl_info = vim.api.nvim_get_hl_by_name("@" .. groups[i], true)
      if hl_info.bold then
        if font_type == 2 then
          return 3
        end
        font_type = 1
      elseif hl_info.italic then
        if font_type == 1 then
          return 3
        end
        font_type = 2
      end
  end
  return font_type
end

local function get_foreground(groups)
  for i = #groups, 1, -1 do
    if groups[i].foreground then
      return groups[i].foreground
    end
  end

  return nil
end

local libsn = nil
local sn_ctx = nil

-- this is just ship f ts
M.snip = function (opts)
  assert(libsn ~= nil and sn_ctx ~= nil)

  local elapsed = vim.loop.hrtime()
  local treverse_timer = vim.loop.hrtime()

  local err
  local syntax, rows, cols = get_ts_syntax(opts.line1, opts.line2)
  local treverse_time = vim.loop.hrtime()

  -- print("treverse:", (treverse_time - treverse_timer) / 1e6 .. "ms")

  if rows == 0 then
    return
  end

  local normal = vim.api.nvim_get_hl_by_name("Normal", true)
  assert(normal.background ~= nil)
  assert(normal.foreground ~= nil)

  libsn.sn_set_fill(
    sn_ctx,
    bit.band(bit.rshift(normal.background, 16), 0xFF),
    bit.band(bit.rshift(normal.background, 8), 0xFF),
    bit.band(normal.background, 0xFF)
  )

  err = libsn.sn_set_size(sn_ctx, rows - opts.line1 + 1, cols)
  if err ~= 0 then
    libsn.sn_done(sn_ctx)
    error("sn_set_size: " .. ffi.string(libsn.sn_error_name(err)))
  end

  local draw_timer = vim.loop.hrtime()
  for row, line in pairs(syntax) do
    for i = 1, #line do
      local val = line[i]
      local foreground = get_foreground(val.hl_groups)
      print(foreground)

      local r = bit.band(bit.rshift(foreground, 16), 0xFF)
      local g = bit.band(bit.rshift(foreground, 8), 0xFF)
      local b = bit.band(foreground, 0xFF)

      libsn.sn_set_font(sn_ctx, combine_fonts(val.hl_groups))
      libsn.sn_set_color(sn_ctx, r, g, b)

      err = libsn.sn_draw_text(sn_ctx, row - opts.line1, val.col, val.token)

      if err ~= 0 then
        libsn.sn_done(sn_ctx)
        error("sn_draw_text: " .. ffi.string(libsn.sn_error_name(err)))
      end
    end
  end

  local draw_time = vim.loop.hrtime()
  -- print("draw:", (draw_time - draw_timer) / 1e6 .. "ms")

  local output_timer = vim.loop.hrtime()

  local out = ffi.new("uint8_t*[1]")
  local out_len = ffi.new("size_t[1]")

  err = libsn.sn_output(sn_ctx, out, out_len)
  if err ~= 0 then
    libsn.sn_done(sn_ctx)
    error("sn_output: " .. ffi.string(sn_ctx.sn_error_name(err)))
  end

  local image = ffi.string(out[0], out_len[0])

  local output_time = vim.loop.hrtime()
  -- print("output:", (output_time - output_timer) / 1e6 .. "ms")

  local save_path = M.options.save_file
  if save_path then
    if not path.is_absolute(save_path) then
      save_path = vim.fn.getcwd() .. "/" .. save_path
    end

    local file = io.open(save_path, "wb")
    assert(file ~= nil)

    file:write(image)
    file:close()

    print("Saved at " .. save_path)

    libsn.sn_free_output(out)
  else
    -- vim.fn.setreg('+', image)
    print("Copied to clipboard")
  end

  print("took:", (vim.loop.hrtime() - elapsed) / 1e6 .. "ms")
end

local function resolve_lib_path(root)
  local resolve_arch = function ()
    if jit.arch == "x86" or jit.arch == "x64" then
      return "x86_64"
    end
    return jit.arch
  end

  local resolve_os = function ()
    if jit.os == "MacOS" then
      return "macos", "dylib"
    elseif jit.os == "Windows" then
      return "windows", "dll"
    else
      return string.lower(jit.os), "so"
    end
  end

  local arch = resolve_arch()
  local platform, extension = resolve_os()
  return string.format("%s/lib/%s-%s-snipit.%s", root, arch, platform, extension)
end

M.setup = function ()
  libsn = ffi.load(resolve_lib_path(M.root)) -- print that this platform is not supported by default, but they can compile it
  assert(libsn ~= nil)

  -- :// we need to fix multi line strings
  -- todo: we need to clear the bitmap->buffer after the output
  ffi.cdef[[
    typedef void* sn_ctx;

    sn_ctx sn_init();

    void sn_done(sn_ctx ctx);

    int sn_set_size(sn_ctx ctx, uint16_t rows, uint16_t cols);

    int sn_add_font(sn_ctx ctx, const char* sub_path, uint8_t font_type);

    int sn_draw_text(sn_ctx ctx, uint32_t row, uint32_t col, const char* text);

    void sn_set_font(sn_ctx ctx, uint8_t font_type);

    void sn_set_fill(sn_ctx ctx, uint8_t r, uint8_t g, uint8_t b);

    void sn_set_color(sn_ctx ctx, uint8_t r, uint8_t g, uint8_t b);

    int sn_output(sn_ctx ctx, uint8_t** dist, size_t* dist_len);

    void sn_free_output(uint8_t** src);

    const char* sn_error_name(int err);
  ]]

  local err
  local ctx = libsn.sn_init()

  if ctx == nil then
    error("sn_init: out of memory")
  end

  -- add these from root
  err = libsn.sn_add_font(ctx, M.options.fonts.regular, 0)
  if err ~= 0 then
    libsn.sn_done(ctx)
    error(string.format("sn_add_font: '%s': %s", M.options.fonts.regular, ffi.string(libsn.sn_error_name(err))))
  end

  err = libsn.sn_add_font(ctx, M.options.fonts.bold, 1)
  if err ~= 0 then
    libsn.sn_done(ctx)
    error(string.format("sn_add_font: '%s': %s", M.options.fonts.bold, ffi.string(libsn.sn_error_name(err))))
  end

  err = libsn.sn_add_font(ctx, M.options.fonts.italic, 2)
  if err ~= 0 then
    libsn.sn_done(ctx)
    error(string.format("sn_add_font: '%s': %s", M.options.fonts.italic, ffi.string(libsn.sn_error_name(err))))
  end

  err = libsn.sn_add_font(ctx, M.options.fonts.bold_italic, 3)
  if err ~= 0 then
    libsn.sn_done(ctx)
    error(string.format("sn_add_font: '%s': %s", M.options.fonts.bold_italic, ffi.string(libsn.sn_error_name(err))))
  end

  sn_ctx = ctx
  M.has_setup = true
end

return M
