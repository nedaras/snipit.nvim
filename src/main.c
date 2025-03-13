#include <assert.h>
#include <errno.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <zlib.h>
#include <png.h>
#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_TRUETYPE_TABLES_H

#define SN_FONTS 3

typedef int sn_error;
struct sn_ctx_s {
  // store bitmap that will be dynamicly allocated?
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
    err = FT_Set_Pixel_Sizes(*pface, 0, 16);
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

sn_error sn_render_codepoint(sn_ctx ctx, uint32_t x, uint32_t y, uint32_t codepoint) {
  assert(ctx != NULL);
  assert(ctx->fonts_len == 1);

  FT_Face* pface = &ctx->fonts[0];
  FT_Error err; 

  uint32_t idx = FT_Get_Char_Index(*pface, codepoint); // file - 0x1F525
  assert(idx != 0); // return err

  err = FT_Load_Glyph(*pface, idx, FT_LOAD_DEFAULT | FT_LOAD_COLOR);
  if (err != FT_Err_Ok) {
    return err;
  }

  err = FT_Render_Glyph((*pface)->glyph, FT_RENDER_MODE_NORMAL);
  if (err != FT_Err_Ok) {
    return err;
  }

  if ((*pface)->glyph->bitmap.pixel_mode == FT_PIXEL_MODE_BGRA) {
    uint8_t* src = (*pface)->glyph->bitmap.buffer;

    // we would need to scale this down
    for (uint32_t y = 0; y < (*pface)->glyph->bitmap.rows; y++) {
      for (uint32_t x = 0; x < (*pface)->glyph->bitmap.width; x++) {
        uint8_t b = *src++;
        uint8_t g = *src++;
        uint8_t r = *src++;
        uint8_t a = *src++;
      }
    }
  } else {
    uint8_t* src = (*pface)->glyph->bitmap.buffer;

    for (uint32_t y = 0; y < (*pface)->glyph->bitmap.rows; y++) {
      for (uint32_t x = 0; x < (*pface)->glyph->bitmap.width; x++) {
        uint8_t h = *src++;
        printf("\e[48;2;%d;%d;%dm  \e[0m", h, h, h);
      }
      printf("\n");
    }
  }

  return 0;
}

sn_error sn_render(sn_ctx ctx, const char* text, size_t text_len) {
  for (size_t i = 0; i < text_len; i++) {
    sn_error err = sn_render_codepoint(ctx, 0, 0, text[i]);
    if (err != 0) {
      return err;
    }
  }

  return 0;
}

int main(int argc, char *argv[]) {
  if (argc < 3) return 1;

  sn_ctx ctx = NULL;
  sn_error err;

  err = sn_init(&ctx);
  if (err != 0) {
    goto err;
  }

  int32_t major, minor, patch;
  FT_Library_Version(ctx->library, &major, &minor, &patch);

  printf("snipit.nvim: using freetype v%d.%d.%d\n", major, minor, patch);
  printf("snipit.nvim: using libpng v%s\n", PNG_LIBPNG_VER_STRING);
  printf("snipit.nvim: using zlib v%s.%d\n", ZLIB_VERSION, ZLIB_VER_REVISION);

  err = sn_add_font(ctx, argv[1]);
  if (err != 0) {
    goto err;
  }

  err = sn_render(ctx, argv[2], strlen(argv[2]));
  if (err != 0) {
    goto err;
  }

  sn_done(ctx);
  return 0;

err:
  if (ctx != NULL) sn_done(ctx);

  printf("%s\n", FT_Error_String(err));

  return 1;
}
