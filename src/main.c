#include <assert.h>
#include <errno.h>
#include <stdint.h>
#include <stdbool.h>
#include <png.h>
#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_TRUETYPE_TABLES_H

#define SN_FONTSIZE 16
#define SN_FONTS    3
#define SN_WIDTH    95
#define SN_HEIGHT   32

#define max(a, b) ((a) > (b) ? (a) : (b))
#define min(a, b) ((a) < (b) ? (a) : (b))

typedef int sn_error;
struct sn_ctx_s {
  uint8_t bitmap[SN_WIDTH * SN_HEIGHT * 3];
  FT_Library library;

  FT_Face fonts[SN_FONTS];
  uint8_t fonts_len;

} typedef sn_ctx_t;

typedef sn_ctx_t* sn_ctx;

sn_error sn_init(sn_ctx* ctx) {
  *ctx = malloc(sizeof(sn_ctx_t));
  if (*ctx == NULL) {
    return FT_Err_Out_Of_Memory;
  }

  FT_Error err = FT_Init_FreeType(&(*ctx)->library);
  if (err != FT_Err_Ok) {
    free(*ctx);
    *ctx = NULL;
    return err;
  }

  (*ctx)->fonts_len = 0;
  memset((*ctx)->bitmap, 0, SN_WIDTH * SN_HEIGHT * 3);

  return 0;
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

void sn_done(sn_ctx ctx) {
  assert(ctx != NULL);

  for (uint8_t i = 0; i < ctx->fonts_len; i++) {
    assert(ctx->fonts[i] != NULL);
    assert(FT_Done_Face(ctx->fonts[i]) == FT_Err_Ok);
  }

  assert(FT_Done_FreeType(ctx->library) == FT_Err_Ok);

  free(ctx);
}

sn_error sn_add_font(sn_ctx ctx, const char* sub_path) {
  assert(ctx != NULL);
  assert(ctx->fonts_len != SN_FONTS); // ret err
  
  FT_Error err; 
  FT_Face* pface = &ctx->fonts[ctx->fonts_len];
  assert(*pface == NULL);

  err = FT_New_Face(ctx->library, sub_path, 0, pface);
  if (err != FT_Err_Ok) {
    goto err;
  }

  if (is_colored(*pface)) {
    assert((*pface)->num_fixed_sizes > 0);
    err = FT_Select_Size(*pface, 0);
    if (err != FT_Err_Ok) goto err;
  } else {
    err = FT_Set_Pixel_Sizes(*pface, 0, SN_FONTSIZE);
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

sn_error sn_render_codepoint(sn_ctx ctx, uint32_t off_x, uint32_t off_y, uint32_t codepoint, uint32_t next_codepoint, uint32_t* kerning) {
  assert(SN_WIDTH > off_x);
  assert(SN_HEIGHT > off_y);

  assert(ctx != NULL);
  assert(ctx->fonts_len == 1);

  FT_Face* pface = &ctx->fonts[0];
  FT_Error err; 

  uint32_t idx = FT_Get_Char_Index(*pface, codepoint); // file - 0x1F525
  assert(idx != 0); // return err

  err = FT_Load_Glyph(*pface, idx, FT_LOAD_RENDER);
  if (err != FT_Err_Ok) {
    return err;
  }

  FT_GlyphSlot glyph = (*pface)->glyph;
  uint32_t font_size = (*pface)->size->metrics.height;

  err = FT_Render_Glyph(glyph, FT_RENDER_MODE_NORMAL);
  if (err != FT_Err_Ok) {
    return err;
  }

  if (glyph->bitmap.pixel_mode == FT_PIXEL_MODE_BGRA) {
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
    //printf("width: %ld, height: %ld, hori_bearing_x: %ld, hori_bearing_y: %ld, vert_bearing_x: %ld, vert_bearing_y: %ld, hori_advance: %ld\n", 
      //glyph->metrics.width,
      //glyph->metrics.height,
      //glyph->metrics.horiBearingX,
      //glyph->metrics.horiBearingY,
      //glyph->metrics.vertBearingX,
      //glyph->metrics.vertBearingY,
      //glyph->metrics.horiAdvance
    //);

    uint32_t baseline_y = (glyph->metrics.horiBearingY * SN_FONTSIZE) / font_size;

    assert(baseline_y <= SN_FONTSIZE);
    off_y += SN_FONTSIZE - baseline_y;

    // todo: fix it so off_y does not become bigger then SN_HEIGHT
    assert(SN_HEIGHT > off_y);

    for (uint32_t y = 0; y < min(glyph->bitmap.rows, SN_HEIGHT - off_y); y++) {
      for (uint32_t x = 0; x < min(glyph->bitmap.width, SN_WIDTH - off_x); x++) {
        uint8_t h = glyph->bitmap.buffer[y * glyph->bitmap.width + x];

        size_t bitmap_len = SN_WIDTH * SN_HEIGHT;
        size_t bitmap_idx = (y + off_y) * SN_WIDTH + x + off_x;

        assert(bitmap_len > bitmap_idx);

        ctx->bitmap[bitmap_idx * 3 + 0] = h;
        ctx->bitmap[bitmap_idx * 3 + 1] = h;
        ctx->bitmap[bitmap_idx * 3 + 2] = h;
      }
    }
  }

  if (FT_HAS_KERNING(*pface) && next_codepoint != 0 && kerning != NULL) {
    uint32_t next_idx = FT_Get_Char_Index(*pface, next_codepoint); // file - 0x1F525
    assert(next_idx != 0); // return err

    FT_Vector vec;
    assert(FT_Get_Kerning(*pface, idx, next_idx, FT_KERNING_DEFAULT, &vec) == FT_Err_Ok); // todo: handle

    printf("x: %ld, y: %ld\n", vec.x, vec.y); // why not work?
    // https://freetype.org/freetype2/docs/tutorial/step2.html
    *kerning = (glyph->advance.x * SN_FONTSIZE) / font_size;
  } else {
    *kerning = (glyph->advance.x * SN_FONTSIZE) / font_size;
  }

  return 0;
}

sn_error sn_draw_text(sn_ctx ctx, const char* text, size_t text_len) {
  uint32_t x = 0;
  for (size_t i = 0; i < text_len; i++) {
    uint32_t kern;
    sn_error err = sn_render_codepoint(ctx, x, 0, text[i], text[i + 1], &kern); // FIX: bad things will happen if text is not nullterminated
    if (err != 0) {
      return err;
    }

    x += kern + 1; // should prob off by space if its mosocope
  }

  return 0;
}


void sn_render(sn_ctx ctx) {
    uint8_t* i = ctx->bitmap;
    for (uint32_t y = 0; y < SN_HEIGHT; y++) {
      for (uint32_t x = 0; x < SN_WIDTH; x++) {
        uint8_t r = *i++;
        uint8_t g = *i++;
        uint8_t b = *i++;
        printf("\e[48;2;%d;%d;%dm  \e[0m", r, g, b);
      }
      printf("\n");
    }
}

int main(int argc, char *argv[]) {
  if (argc < 3) return 1;

  sn_ctx ctx = NULL;
  sn_error err;

  err = sn_init(&ctx);
  if (err != 0) {
    goto err;
  }

  err = sn_add_font(ctx, argv[1]);
  if (err != 0) {
    goto err;
  }

  err = sn_draw_text(ctx, argv[2], strlen(argv[2]));
  if (err != 0) {
    goto err;
  }

  sn_render(ctx);

  sn_done(ctx);
  return 0;

err:
  if (ctx != NULL) sn_done(ctx);

  printf("%s\n", FT_Error_String(err));

  return 1;
}
