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

    -- print(root_start_row, root_end_row)

    local query = buf_highlighter:get_query(tree:lang())

    if not query:query() then
      return
    end

    local iter = query:query():iter_captures(root, buf_highlighter.bufnr, line1 - 1, line2)

    for capture, node in iter do
      local hl = query.hl_cache[capture]
      if not hl then
        goto continue
      end

      local group = query._query.captures[capture]
      if not group then
        goto continue
      end

      local row, col, row_end, _ = node:range()

      -- seems that if we get multi line token we cannot extarct futher tokens
      -- and we need to handle these tokens, but it is hard cuz if we thse multi line stokens can have other tokens inside
      if row ~= row_end then
        if row_end + 2 > line2 then
          return
        end
        inner_get_ts_syntax(out, row_end + 2, line2)
        return
      end

      assert(row <= 65535)
      assert(col <= 65535)

      local token = vim.treesitter.get_node_text(node, buf)
      local key = bit.bor(bit.lshift(row, 16), col)

      if not out.syntax[key] then
        out.syntax[key] = {
          token = token,
          groups = {},
        }
      end

      out.cols = math.max(out.cols, col + #token)
      table.insert(out.syntax[key].groups, group)
        ::continue::
    end
  end, true)
end

local function get_ts_syntax(line1, line2)
  local out = {
    syntax = {},
    cols = 0,
  }
  inner_get_ts_syntax(out, line1, line2)
  return out.syntax, out.cols
end

local function combine_fonts(groups)
  local font_type = 0
  -- update c abi so i could do smth like SN_FONT_TYPE_BOLD | SN_FOMT_TYPE_ITALIC
  -- then this dumb if else stuff
  for _, val in ipairs(groups) do
      local hl_info = vim.api.nvim_get_hl_by_name("@" .. val, true)
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

local libsn = nil
local sn_ctx = nil

M.snip = function (opts)
  if libsn == nil or sn_ctx == nil then
    return
  end
  local elapsed = vim.loop.hrtime()

  local treverse_timer = vim.loop.hrtime()
  local err
  local syntax, cols = get_ts_syntax(opts.line1, opts.line2)
  local treverse_time = vim.loop.hrtime()

  -- print("treverse:", (treverse_time - treverse_timer) / 1e6 .. "ms")

  if next(syntax) == nil then
    return
  end

  -- fill with normal bg color
  libsn.sn_set_fill(sn_ctx, 25, 23, 36)

  err = libsn.sn_set_size(sn_ctx, opts.line2 - opts.line1 + 1, cols)
  if err ~= 0 then
    libsn.sn_done(sn_ctx)
    error("sn_set_size: " .. ffi.string(libsn.sn_error_name(err)))
  end

  -- this is unordered
  local draw_timer = vim.loop.hrtime()
  for key, val in pairs(syntax) do
    local row = bit.rshift(key, 16)
    local col = bit.band(key, 0xFFFF)
    local hl_info = vim.api.nvim_get_hl_by_name("@" .. val.groups[#val.groups], true)

    -- print(row, col, val.token)

    libsn.sn_set_font(sn_ctx, combine_fonts(val.groups))

    if hl_info.foreground then
      local r = bit.band(bit.rshift(hl_info.foreground, 16), 0xFF)
      local g = bit.band(bit.rshift(hl_info.foreground, 8), 0xFF)
      local b = bit.band(hl_info.foreground, 0xFF)

      libsn.sn_set_color(sn_ctx, r, g, b)
    else
      libsn.sn_set_color(sn_ctx, 0, 255, 255) -- use Normal hi
    end

    err = libsn.sn_draw_text(sn_ctx, row - opts.line1 + 1, col, val.token)
    if err ~= 0 then
      libsn.sn_done(sn_ctx)
      error("sn_draw_text: " .. ffi.string(libsn.sn_error_name(err)))
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
