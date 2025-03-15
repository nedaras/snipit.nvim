#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <png.h>
#include <ft2build.h>
#include <limits.h>
#include FT_FREETYPE_H
#include FT_TRUETYPE_TABLES_H

#define SN_API

#define SN_LINE_HEIGHT 16
#define SN_FONTS    4 // need 4 fonts (regular, italic, bold, emoji)

#define max(a, b) ((a) > (b) ? (a) : (b))
#define min(a, b) ((a) < (b) ? (a) : (b))

#define div_ceil(a, b) (((a) + (b) - 1) / (b))

typedef int sn_error;

struct sn_color_s {
  uint8_t r;
  uint8_t g;
  uint8_t b;
} typedef sn_color_t;

struct sn_bitmap_s {
  uint8_t* buffer;
  uint32_t width;
  uint32_t height;
} typedef sn_bitmap_t;

struct sn_ctx_s {
  sn_bitmap_t bitmap;

  FT_Library library;

  FT_Face fonts[SN_FONTS];
  uint8_t fonts_len;

  sn_color_t pencil_color;
  sn_color_t fill_color;
} typedef sn_ctx_t;

typedef sn_ctx_t* sn_ctx;

// todo: enable dymanic size after we implement own arr_list thingy
SN_API sn_ctx sn_init(uint32_t width, uint32_t height) {
  FT_Error err;
  sn_ctx out = malloc(sizeof(sn_ctx_t));

  if (out == NULL) {
    err = FT_Err_Out_Of_Memory;
    goto err;
  }

  err = FT_Init_FreeType(&out->library);
  if (err != FT_Err_Ok) {
    goto err;
  }

  out->bitmap.width = width;
  out->bitmap.height = height;
  out->bitmap.buffer = malloc(width * height * 3);

  if (out->bitmap.buffer == NULL) {
    err = FT_Err_Out_Of_Memory;
    goto err;
  }

  memset(out->bitmap.buffer, 0, width);

  for (int i = 0; i < SN_FONTS; i++) {
    out->fonts[i] = NULL;
  }

  out->fonts_len = 0;
  out->pencil_color = (sn_color_t){ 255, 255, 255 };
  out->fill_color = (sn_color_t){ 0, 0, 0 };

  return out;

err:
  if (out == NULL) return NULL;

  if (out->bitmap.buffer != NULL) {
    free(out->bitmap.buffer);
  }
   
  free(out);
  return NULL;
}

SN_API void sn_done(sn_ctx ctx) {
  assert(ctx != NULL);

  assert(ctx->bitmap.buffer != NULL);
  free(ctx->bitmap.buffer);

  for (uint8_t i = 0; i < ctx->fonts_len; i++) {
    assert(ctx->fonts[i] != NULL);
    assert(FT_Done_Face(ctx->fonts[i]) == FT_Err_Ok);
  }

  assert(FT_Done_FreeType(ctx->library) == FT_Err_Ok);

  free(ctx);
}

SN_API const char* sn_error_name(sn_error err) {
  return FT_Error_String(err);
}

bool is_colored(FT_Face face) {
  static const uint32_t tag = FT_MAKE_TAG('C', 'B', 'D', 'T');

  size_t len = 0;
  FT_Error err = FT_Load_Sfnt_Table(face, tag, 0, NULL, &len);
  if (err == FT_Err_Table_Missing) {
    return false;
  }

  assert(err == FT_Err_Ok); // todo: handle

  return len != 0;
}

// todo: rename to set_font and have SN_FONT_TYPE_
// and add `sub_path_len` param
SN_API sn_error sn_add_font(sn_ctx ctx, const char* sub_path) {
  assert(ctx != NULL);
  assert(ctx->fonts_len != SN_FONTS); // ret err
  
  FT_Error err; 
  FT_Face* pface = &ctx->fonts[ctx->fonts_len];
  assert(*pface == NULL);

  err = FT_New_Face(ctx->library, sub_path, 0, pface);
  if (err != FT_Err_Ok) {
    goto err;
  }

  if (is_colored(*pface)) { // check if FT_HAS_COLOR works
    assert((*pface)->num_fixed_sizes > 0);
    err = FT_Select_Size(*pface, 0);
    if (err != FT_Err_Ok) goto err;
  } else {
    err = FT_Set_Pixel_Sizes(*pface, 0, SN_LINE_HEIGHT);
    if (err != FT_Err_Ok) goto err;
  }

  ctx->fonts_len++;
  return 0;

err:
  if (*pface != NULL) {
    assert(FT_Done_Face(*pface) == FT_Err_Ok);
    *pface = NULL;
  }
  return err;
}

sn_error sn_render_codepoint(sn_ctx ctx, uint32_t off_x, uint32_t off_y, uint32_t codepoint, uint32_t* advance) {
  assert(ctx->bitmap.width > off_x);
  assert(ctx->bitmap.height > off_y);

  assert(ctx != NULL);
  assert(ctx->fonts_len == 1);

  FT_Face* pface = &ctx->fonts[0];
  FT_Error err; 

  uint32_t idx = FT_Get_Char_Index(*pface, codepoint); // fire - 0x1F525
  assert(idx != 0); // draw that square x for like invalid char

  err = FT_Load_Glyph(*pface, idx, FT_LOAD_RENDER);
  if (err != FT_Err_Ok) {
    return err;
  }

  FT_GlyphSlot glyph = (*pface)->glyph;
  uint32_t line_height = (*pface)->size->metrics.height;

  err = FT_Render_Glyph(glyph, FT_RENDER_MODE_NORMAL);
  if (err != FT_Err_Ok) {
    return err;
  }

  if (glyph->bitmap.pixel_mode == FT_PIXEL_MODE_BGRA) {
    // before rendering these colored emojis we will need to scale them down
    assert(false);

    uint8_t* src = glyph->bitmap.buffer;

    // we would need to scale this down
    for (uint32_t y = 0; y < glyph->bitmap.rows; y++) {
      for (uint32_t x = 0; x < glyph->bitmap.width; x++) {
        uint8_t b = *src++;
        uint8_t g = *src++;
        uint8_t r = *src++;
        uint8_t a = *src++;
      }
    }
  } else {
    uint32_t bearing_x = div_ceil(glyph->metrics.horiBearingX * SN_LINE_HEIGHT, line_height);
    uint32_t bearing_y = div_ceil(glyph->metrics.horiBearingY * SN_LINE_HEIGHT, line_height);

    assert(bearing_y <= SN_LINE_HEIGHT); // it it true?

    off_y += SN_LINE_HEIGHT - bearing_y;
    off_x += bearing_x;

    // todo: fix it so off_y does not become bigger then SN_HEIGHT
    assert(ctx->bitmap.width > off_x);
    assert(ctx->bitmap.height > off_y);

    for (uint32_t y = 0; y < min(glyph->bitmap.rows, ctx->bitmap.height - off_y); y++) {
      for (uint32_t x = 0; x < min(glyph->bitmap.width, ctx->bitmap.width - off_x); x++) {
        uint8_t h = glyph->bitmap.buffer[y * glyph->bitmap.width + x];
        uint8_t inv_h = 255 - h;

        size_t bitmap_len = ctx->bitmap.width * ctx->bitmap.height;
        size_t bitmap_idx = (y + off_y) * ctx->bitmap.width + x + off_x;

        assert(bitmap_len > bitmap_idx);

        uint8_t* bg_r = &ctx->bitmap.buffer[bitmap_idx * 3 + 0];
        uint8_t* bg_g = &ctx->bitmap.buffer[bitmap_idx * 3 + 1];
        uint8_t* bg_b = &ctx->bitmap.buffer[bitmap_idx * 3 + 2];

        *bg_r = (inv_h * (*bg_r) + h * ctx->pencil_color.r) / 255;
        *bg_g = (inv_h * (*bg_g) + h * ctx->pencil_color.g) / 255;
        *bg_b = (inv_h * (*bg_b) + h * ctx->pencil_color.b) / 255;
      }
    }
  }

  // todo: add kerning if i rly want to
  if (advance != NULL) {
    *advance = div_ceil(glyph->advance.x * SN_LINE_HEIGHT, line_height);
  }

  return 0;
}

SN_API sn_error sn_draw_text(sn_ctx ctx, uint32_t row, uint32_t col, const char* text) {
  uint32_t x = 0;
  while (*text != '\0') {
    uint32_t advance; 
    // todo: get space width for spacing SN_LINE_HEIGHT >> 1 is not correct
    sn_error err = sn_render_codepoint(ctx, col * (SN_LINE_HEIGHT >> 1) + x, row * SN_LINE_HEIGHT, *text, &advance);
    if (err != 0) {
      return err;
    }
    x += advance;
    text++;
  }

  return 0;
}

SN_API void sn_set_color(sn_ctx ctx, uint8_t r, uint8_t g, uint8_t b) {
  ctx->pencil_color = (sn_color_t){r, g, b};
}

SN_API void sn_fill(sn_ctx ctx, uint8_t r, uint8_t g, uint8_t b) {
  uint8_t* src = ctx->bitmap.buffer;
  for (uint32_t i = 0; i <  ctx->bitmap.width * ctx->bitmap.height; i++) {
    *src++ = r;
    *src++ = g;
    *src++ = b;
  }

  ctx->fill_color = (sn_color_t){r, g, b};
}

struct sn_writer_state_s {
  uint8_t* out;
  size_t out_len;

  sn_error err;
} typedef sn_writer_state_t;

void sn_output_writer_write(png_structp ptr, uint8_t* buf, size_t buf_len) {
  if (buf_len == 0) return;
  sn_writer_state_t* state = png_get_io_ptr(ptr);

  assert(buf != NULL);
  assert(state != NULL);

  if (state->err != 0) {
    return;
  }

  // do some logic here like vector store capacity and do some growing
  if (true) {
    uint8_t* old = state->out;
    uint8_t* new = malloc(state->out_len + buf_len);

    if (new == NULL) {
      free(state->out);

      state->out = NULL;
      state->out_len = 0;
      state->err = FT_Err_Out_Of_Memory;

      return;
    }

    if (old != NULL) {
      memcpy(new, old, state->out_len);
      free(old);
    }

    state->out = new;
  }

  assert(state->out != NULL);
  memcpy(state->out + state->out_len, buf, buf_len);
  state->out_len += buf_len;
}

SN_API sn_error sn_output(sn_ctx ctx, uint8_t** dist, size_t* dist_len) {
  assert(dist != NULL);
  assert(dist_len != NULL);

  assert(ctx->bitmap.buffer != NULL);
  assert(ctx->bitmap.width > 0);
  assert(ctx->bitmap.height > 0);

  sn_error err;

  png_structp writer = NULL;
  png_infop info = NULL;

  writer = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
  if (writer == NULL) {
    err = FT_Err_Out_Of_Memory;
    goto err;
  }

  info = png_create_info_struct(writer);
  if (info == NULL) {
    goto err;
  }

  if (setjmp(png_jmpbuf(writer))) {
    err = FT_Err_Out_Of_Memory; // is this true?
    goto err;
  }

  sn_writer_state_t write_state = (sn_writer_state_t){ NULL, 0, 0 };

  png_set_write_fn(writer, &write_state, &sn_output_writer_write, NULL);
  png_set_IHDR(writer, info, ctx->bitmap.width, ctx->bitmap.height, 8, PNG_COLOR_TYPE_RGB, PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);

  png_write_info(writer, info);

  for (uint32_t i = 0; i < ctx->bitmap.height; i++) {
    png_const_bytep row = ctx->bitmap.buffer + (i * ctx->bitmap.width * 3);
    png_write_row(writer, row); // todo: try to oneshot it with that write_image mb result will be smaller
    err = write_state.err;
    if (err != 0) {
      goto err;
    }
  }

  png_write_end(writer, NULL);
  err = write_state.err;
  if (err != 0) {
    goto err;
  }

  png_destroy_write_struct(&writer, &info);

  *dist = write_state.out;
  *dist_len = write_state.out_len;

  return 0;

err:
  assert(write_state.out == NULL);
  assert(write_state.out_len == 0);

  if (writer != NULL) {
    png_destroy_write_struct(&writer, info != NULL ? &info : NULL);
  } else {
    assert(info == NULL);
  }

  return err;
}

SN_API void sn_free_output(uint8_t** src) {
  assert(src != NULL);
  assert(*src != NULL);

  free(*src);
  *src = NULL;
}
