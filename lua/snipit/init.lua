local ffi = require("ffi")
local bit = require("bit")

local M = {}

print("snipit.nvim")

local sn = ffi.load("/home/nedas/source/snipit/zig-out/lib/libsnipit.so")

-- :// we need to fix multi line strings
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

M.options = {
  root = debug.getinfo(1, "S").source:sub(2), --:match("(.*[/\\])")
  save_file = "out.png",
  -- font_size
  -- fonts:
    -- regular
    -- italic
    -- bold
}

local function get_ts_syntax(syntax, line1, line2)
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
        get_ts_syntax(syntax, row_end + 2, line2)
        return
      end

      assert(row <= 65535)
      assert(col <= 65535)

      local token = vim.treesitter.get_node_text(node, buf)
      local key = bit.bor(bit.lshift(row, 16), col)

      if not syntax[key] then
        syntax[key] = {
          token = token,
          groups = {},
        }
      end

      table.insert(syntax[key].groups, group)
        ::continue::
    end
  end, true)
end

local function is_alpha(c)
  return (c >= 'a' and c <= 'z') or (c > 'A' and c <= 'Z')
end

local function is_absolute(path)
  if ffi.os == "Windows" then
    return path:sub(2, 2) == ':' and (is_alpha(path:sub(1, 1)))
  end

  return path:sub(1, 1) == '/'
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

M.setup = function ()
  local err
  local ctx = sn.sn_init()

  if ctx == nil then
    error("sn_init: out of memory")
  end

  err = sn.sn_add_font(ctx, "/home/nedas/source/snipit/fonts/UbuntuMono-Regular.ttf", 0)
  if err ~= 0 then
    sn.sn_done(ctx)
    error("sn_add_font: " .. ffi.string(sn.sn_error_name(err)))
  end

  err = sn.sn_add_font(ctx, "/home/nedas/source/snipit/fonts/UbuntuMono-Bold.ttf", 1)
  if err ~= 0 then
    sn.sn_done(ctx)
    error("sn_add_font: " .. ffi.string(sn.sn_error_name(err)))
  end

  err = sn.sn_add_font(ctx, "/home/nedas/source/snipit/fonts/UbuntuMono-Italic.ttf", 2)
  if err ~= 0 then
    sn.sn_done(ctx)
    error("sn_add_font: " .. ffi.string(sn.sn_error_name(err)))
  end

  err = sn.sn_add_font(ctx, "/home/nedas/source/snipit/fonts/UbuntuMono-BoldItalic.ttf", 3)
  if err ~= 0 then
    sn.sn_done(ctx)
    error("sn_add_font: " .. ffi.string(sn.sn_error_name(err)))
  end

  vim.api.nvim_create_user_command("Snipit", function (opts)
    local syntax = {}
    get_ts_syntax(syntax, opts.line1, opts.line2)

    if next(syntax) == nil then
      return
    end

    local cols = 0
    for key, val in pairs(syntax) do
      local col = bit.band(key, 0xFFFF)
      cols = math.max(cols, col + #val.token)
    end

    -- fill with normal bg color
    sn.sn_set_fill(ctx, 25, 23, 36)

    err = sn.sn_set_size(ctx, opts.line2 - opts.line1 + 1, cols)
    if err ~= 0 then
      sn.sn_done(ctx)
      error("sn_set_size: " .. ffi.string(sn.sn_error_name(err)))
    end


    -- this is unordered
    for key, val in pairs(syntax) do
      local row = bit.rshift(key, 16)
      local col = bit.band(key, 0xFFFF)
      local hl_info = vim.api.nvim_get_hl_by_name("@" .. val.groups[#val.groups], true)

      -- print(row, col, val.token)

      sn.sn_set_font(ctx, combine_fonts(val.groups))

      if hl_info.foreground then
        local r = bit.band(bit.rshift(hl_info.foreground, 16), 0xFF)
        local g = bit.band(bit.rshift(hl_info.foreground, 8), 0xFF)
        local b = bit.band(hl_info.foreground, 0xFF)

        sn.sn_set_color(ctx, r, g, b)
      else
        sn.sn_set_color(ctx, 0, 255, 255) -- use Normal hi
      end

      err = sn.sn_draw_text(ctx, row - opts.line1 + 1, col, val.token)
      if err ~= 0 then
        sn.sn_done(ctx)
        error("sn_draw_text: " .. ffi.string(sn.sn_error_name(err)))
      end
    end

    local out = ffi.new("uint8_t*[1]")
    local out_len = ffi.new("size_t[1]")

    err = sn.sn_output(ctx, out, out_len)
    if err ~= 0 then
      sn.sn_done(ctx)
        error("sn_output: " .. ffi.string(sn.sn_error_name(err)))
    end

    local image = ffi.string(out[0], out_len[0])

    local save_path = M.options.save_file
    if save_path then
      if not is_absolute(save_path) then
        save_path = vim.fn.getcwd() .. "/" .. save_path
      end

      local file = io.open(save_path, "wb")
      assert(file ~= nil)

      file:write(image)
      file:close()

      print("Saved at " .. save_path)

      sn.sn_free_output(out)
    else
      -- vim.fn.setreg('+', image)
      print("Copied to clipboard")
    end


  end, { range = "%", nargs = "?" })
end

return M
