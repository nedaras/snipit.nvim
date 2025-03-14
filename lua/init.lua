local ffi = require("ffi")

local M = {}

local sn = ffi.load("/home/nedas/source/snipit/zig-out/lib/libsnipit.so")

ffi.cdef[[
  typedef void* sn_ctx;

  sn_ctx sn_init(uint32_t width, uint32_t height);

  void sn_done(sn_ctx ctx);

  int sn_add_font(sn_ctx ctx, const char* sub_path);

  int sn_draw_text(sn_ctx ctx, const char* text, size_t text_len);

  void sn_set_color(sn_ctx ctx, uint8_t r, uint8_t g, uint8_t b);

  void sn_fill(sn_ctx ctx, uint8_t r, uint8_t g, uint8_t b);

  int sn_output(sn_ctx ctx, uint8_t** dist, size_t* dist_len);

  void sn_free_output(uint8_t** src);

  const char* sn_error_name(int err);
]]

M.test = function ()
  local err
  local ctx = sn.sn_init(256, 256)

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
end

return M
