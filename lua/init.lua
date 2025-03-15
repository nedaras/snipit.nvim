local ffi = require("ffi")
local bit = require("bit")

local utf8 = require("snipit.utf8")

local M = {}

local sn = ffi.load("/home/nedas/source/snipit/zig-out/lib/libsnipit.so")

-- :// we need to fix multi line strings
ffi.cdef[[
  typedef void* sn_ctx;

  sn_ctx sn_init(uint32_t width, uint32_t height);

  void sn_done(sn_ctx ctx);

  int sn_add_font(sn_ctx ctx, const char* sub_path);

  int sn_draw_text(sn_ctx ctx, uint32_t row, uint32_t col, const char* text);

  void sn_set_color(sn_ctx ctx, uint8_t r, uint8_t g, uint8_t b);

  void sn_fill(sn_ctx ctx, uint8_t r, uint8_t g, uint8_t b);

  int sn_output(sn_ctx ctx, uint8_t** dist, size_t* dist_len);

  void sn_free_output(uint8_t** src);

  const char* sn_error_name(int err);
]]

M.options = {
  root = debug.getinfo(1, "S").source:sub(2) --:match("(.*[/\\])")
}

M.test = function ()

  local err
  local ctx = sn.sn_init(1024, 1024)

  assert(ctx ~= nil, "init")

  err = sn.sn_add_font(ctx, "/usr/share/fonts/truetype/ubuntu/UbuntuMono-B.ttf")
  assert(err == 0, "add_font: " .. ffi.string(sn.sn_error_name(err))) -- if we will just assert freeing is still needed

  sn.sn_fill(ctx, 25, 23, 36)
  sn.sn_set_color(ctx, 0, 255, 255)

  local text = "snipit.nvim";
  assert(sn.sn_draw_text(ctx, text, #text) == 0, "draw_text")

  local out = ffi.new("uint8_t*[1]")
  local out_len = ffi.new("size_t[1]")

  assert(sn.sn_output(ctx, out, out_len), "output")

  local file = io.open("out.png", "wb")
  assert(file ~= nil)

  file:write(ffi.string(out[0], out_len[0]))
  file:close()

  print("len", out_len[0])

  sn.sn_free_output(out)
  sn.sn_done(ctx)

  -- sn.sn_get_cwd(buf, PATH_MAX)

  -- print(ffi.string(buf));
end

local function get_ts_syntax(line1, line2)
  local syntax = {}

  local buf = vim.api.nvim_get_current_buf()
  local highlighter = vim.treesitter.highlighter

  if not highlighter.active[buf] then
    return syntax
  end

  local buf_highlighter = highlighter.active[buf]

  buf_highlighter.tree:for_each_tree(function (tstree, tree)
    if not tstree then
      return
    end

    local root = tstree:root()
    local query = buf_highlighter:get_query(tree:lang())

    if not query:query() then
      return
    end

    local iter = query:query():iter_captures(root, buf_highlighter.bufnr, line1 - 1, line2)

    for capture, node in iter do
      local group = query._query.captures[capture]
      local row, col, row_end, col_end = node:range()

      if row ~= row_end then -- fix this stuff handle new lines \n
        return
      end

      -- todo: need to rethink this stuff or mb no u shouldnt be snapping 65k chars
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
    end
  end, true)

  return syntax
end

M.setup = function ()
  vim.api.nvim_create_user_command("Snipit", function (opts)
    local syntax = get_ts_syntax(opts.line1, opts.line2)

    if next(syntax) == nil then
      return
    end

    local err
    local ctx = sn.sn_init(1024, 1024)

    assert(ctx ~= nil, "init")

    err = sn.sn_add_font(ctx, "/usr/share/fonts/truetype/ubuntu/UbuntuMono-B.ttf")
    assert(err == 0, "add_font: " .. ffi.string(sn.sn_error_name(err))) -- if we will just assert freeing is still needed

    sn.sn_fill(ctx, 25, 23, 36)

    -- this is unordered
    for key, val in pairs(syntax) do
      local row = bit.rshift(key, 16)
      local col = bit.band(key, 0xFFFF)
      local hl_info = vim.api.nvim_get_hl_by_name("@" .. val.groups[#val.groups], true)

      if hl_info.foreground then
        local r = bit.band(bit.rshift(hl_info.foreground, 16), 0xFF)
        local g = bit.band(bit.rshift(hl_info.foreground, 8), 0xFF)
        local b = bit.band(hl_info.foreground, 0xFF)

        sn.sn_set_color(ctx, r, g, b)
      else
        sn.sn_set_color(ctx, 0, 255, 255) -- use default hi
      end

      sn.sn_draw_text(ctx, row - opts.line1 + 1, col, val.token)
    end

    local out = ffi.new("uint8_t*[1]")
    local out_len = ffi.new("size_t[1]")

    assert(sn.sn_output(ctx, out, out_len), "output")

    local file = io.open("out.png", "wb")
    assert(file ~= nil)

    file:write(ffi.string(out[0], out_len[0]))
    file:close()

    print("len", out_len[0])

    sn.sn_free_output(out)
    sn.sn_done(ctx)

  end, { range = "%", nargs = "?" })
end

return M
