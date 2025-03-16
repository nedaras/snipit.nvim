#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <png.h>
#include <ft2build.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include FT_FREETYPE_H
#include FT_TRUETYPE_TABLES_H

#include "utf8.h"

#define SN_API extern

enum sn_font_type_enum : uint8_t {
  SN_FONT_TYPE_REGULAR,
  SN_FONT_TYPE_BOLD,
  SN_FONT_TYPE_ITALIC,
  SN_FONT_TYPE_BOLDITALIC,
  SN_FONT_TYPE_EMOJI,

  SN_FONT_TYPES,
} typedef sn_font_type;

#define SN_LINE_HEIGHT  36
#define SN_FONT_SIZE    32

#define max(a, b) ((a) > (b) ? (a) : (b))
#define min(a, b) ((a) < (b) ? (a) : (b))

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

  FT_Face fonts[SN_FONT_TYPES];

  int8_t font_type;
  sn_color_t pencil_color;
  sn_color_t fill_color;
} typedef sn_ctx_t;

typedef sn_ctx_t* sn_ctx;

// todo: enable dymanic size after we implement own arr_list thingy
SN_API sn_ctx sn_init() {
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

  out->bitmap.width = 0;
  out->bitmap.height = 0;
  out->bitmap.buffer = NULL;

  for (int i = 0; i < SN_FONT_TYPES; i++) {
    out->fonts[i] = NULL;
  }

  out->font_type = -1;
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

  if (ctx->bitmap.buffer != NULL) {
    free(ctx->bitmap.buffer);
  }

  for (uint8_t i = 0; i < SN_FONT_TYPES; i++) {
    if (ctx->fonts[i] == NULL) continue;
    assert(FT_Done_Face(ctx->fonts[i]) == FT_Err_Ok);
  }

  assert(FT_Done_FreeType(ctx->library) == FT_Err_Ok);

  free(ctx);
}

SN_API sn_error sn_set_size(sn_ctx ctx, uint16_t rows, uint16_t cols) {
  uint32_t width = cols * (SN_FONT_SIZE >> 1);
  uint32_t height = rows * SN_LINE_HEIGHT;

  // todo: add like realloc or reserve the buffer size
  if (ctx->bitmap.buffer != NULL) {
    free(ctx->bitmap.buffer);
  }

  ctx->bitmap.buffer = malloc(width * height * 3);
  if (ctx->bitmap.buffer == NULL) {
    return FT_Err_Out_Of_Memory;
  }

  ctx->bitmap.width = width;
  ctx->bitmap.height = height;

  uint8_t* src = ctx->bitmap.buffer;
  for (uint32_t i = 0; i <  ctx->bitmap.width * ctx->bitmap.height; i++) {
    *src++ = ctx->fill_color.r;
    *src++ = ctx->fill_color.g;
    *src++ = ctx->fill_color.b;
  }

  return 0;
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

SN_API sn_error sn_add_font(sn_ctx ctx, const char* sub_path, sn_font_type font_type) {
  assert(ctx != NULL);
  assert(SN_FONT_TYPES > font_type);
  assert(ctx->fonts[font_type] == NULL);
  
  FT_Error err; 
  FT_Face* pface = &ctx->fonts[font_type];
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
    err = FT_Set_Pixel_Sizes(*pface, 0, SN_FONT_SIZE);
    if (err != FT_Err_Ok) goto err;
  }

  if (ctx->font_type == -1) {
    ctx->font_type = font_type;
  }

  return 0;

err:
  if (*pface != NULL) {
    assert(FT_Done_Face(*pface) == FT_Err_Ok);
    *pface = NULL;
  }
  return err;
}

sn_error sn_render_codepoint(sn_ctx ctx, int32_t off_x, int32_t off_y, uint32_t codepoint, uint32_t* advance) {
  assert(ctx->bitmap.width > off_x);
  assert(ctx->bitmap.height > off_y);

  assert(ctx != NULL);
  assert(ctx->font_type != -1);
  assert(ctx->fonts[ctx->font_type] != NULL);

  FT_Face* pface = &ctx->fonts[ctx->font_type];
  FT_Error err; 

  uint32_t idx = FT_Get_Char_Index(*pface, codepoint); // fire - 0x1F525

  err = FT_Load_Glyph(*pface, idx, FT_LOAD_RENDER);
  if (err != FT_Err_Ok) {
    return err;
  }

  FT_GlyphSlot glyph = (*pface)->glyph;

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
    // todo fix that these values can be signed
    int32_t bearing_x = glyph->metrics.horiBearingX >> 6;
    int32_t bearing_y = glyph->metrics.horiBearingY >> 6;

    assert(bearing_y <= SN_FONT_SIZE); // it it true?

    // we need to fix these negative bearings

    off_y += SN_FONT_SIZE - bearing_y;
    off_x += bearing_x;

    for (uint32_t y = 0; y < min(glyph->bitmap.rows, min(ctx->bitmap.height, ctx->bitmap.height - off_y)); y++) {
      for (uint32_t x = 0; x < min(glyph->bitmap.width, min(ctx->bitmap.width, ctx->bitmap.width - off_x)); x++) {
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
    *advance = glyph->advance.x >> 6;
  }

  return 0;
}

SN_API sn_error sn_draw_text(sn_ctx ctx, uint32_t row, uint32_t col, const char* text) {
  utf8_iter iter;
  utf8_init(&iter, text);

  uint32_t x = 0;
  while (utf8_next(&iter)) {
    uint32_t advance; 
    sn_error err = sn_render_codepoint(ctx, col * (SN_FONT_SIZE >> 1) + x, row * SN_LINE_HEIGHT, iter.codepoint, &advance);
    if (err != 0) {
      return err;
    }
    x += advance;
    text++;
  }

  return 0;
}

SN_API void sn_set_font(sn_ctx ctx, sn_font_type font_type) {
  assert(SN_FONT_TYPES > font_type);
  ctx->font_type = font_type;
}

SN_API void sn_set_fill(sn_ctx ctx, uint8_t r, uint8_t g, uint8_t b) {
  assert(ctx != NULL);
  ctx->fill_color = (sn_color_t){r, g, b};
}

SN_API void sn_set_color(sn_ctx ctx, uint8_t r, uint8_t g, uint8_t b) {
  ctx->pencil_color = (sn_color_t){r, g, b};
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
