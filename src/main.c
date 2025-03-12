#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_TRUETYPE_TABLES_H

bool is_colored(FT_Face face) {
  static const uint32_t tag = FT_MAKE_TAG('C', 'B', 'D', 'T');

  size_t len = 0;
  FT_Load_Sfnt_Table(face, tag, 0, NULL, &len);

  return len != 0;
}

int main(int argc, char *argv[]) {
  if (argc == 1) return 1;


  FT_Library library = NULL;
  FT_Face face = NULL;

  FT_Error err;

  err = FT_Init_FreeType(&library);
  if (err != FT_Err_Ok) {
    printf("snipit.nvim: %s\n", FT_Error_String(err));
    goto err;
  }

  int32_t major, minor, patch;
  FT_Library_Version(library, &major, &minor, &patch);

  printf("snipit.nvim: using freetype version: %d.%d.%d\n", major, minor, patch);

  err = FT_New_Face(library, argv[1], 0, &face);
  if (err != FT_Err_Ok) {
    printf("snipit.nvim: %s\n", FT_Error_String(err));
    goto err;
  }

  if (is_colored(face)) {
    printf("Using colored font?\n");
    assert(face->num_fixed_sizes > 0);

    err = FT_Select_Size(face, 0);
    if (err != FT_Err_Ok) {
      printf("snipit.nvim: select size: %s\n", FT_Error_String(err));
      goto err;
    }
  } else {
    err = FT_Set_Pixel_Sizes(face, 0, 22);
    if (err != FT_Err_Ok) {
      printf("snipit.nvim: set pixel sizes: %s\n", FT_Error_String(err));
      goto err;
    }
  }

  uint32_t idx = FT_Get_Char_Index(face, 0x1f480);
  assert(idx != 0);

  printf("idx: %d\n", idx);

  err = FT_Load_Glyph(face, idx, FT_LOAD_COLOR | FT_LOAD_DEFAULT);
  if (err != FT_Err_Ok) {
    printf("snipit.nvim: load glyph: %s #%d\n", FT_Error_String(err), err);
    goto err;
  }

  err = FT_Render_Glyph(face->glyph, FT_RENDER_MODE_NORMAL);
  if (err != FT_Err_Ok) {
    printf("snipit.nvim: %s\n", FT_Error_String(err));
    goto err;
  }

  if (face->glyph->bitmap.pixel_mode == FT_PIXEL_MODE_BGRA) {
    printf("this char has some color\n");
  } else {
    uint8_t* h = face->glyph->bitmap.buffer;
    for (uint32_t y = 0; y < face->glyph->bitmap.rows; y++) {
      for (uint32_t x = 0; x < face->glyph->bitmap.width; x++) {
        printf("%s ", *h > 100 ? "#" : " ");
        h++;
      }
      printf("\n");
    }
  }

  printf("...\n");

  if (face) FT_Done_Face(face);
  if (library) FT_Done_FreeType(library);
  return 0;

err:
  if (face) FT_Done_Face(face);
  if (library) FT_Done_FreeType(library);
  return 1;
}

