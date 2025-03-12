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

  err = FT_New_Face(library, argv[1], 0, &face);
  if (err != FT_Err_Ok) {
    printf("snipit.nvim: %s\n", FT_Error_String(err));
    goto err;
  }

  if (is_colored(face)) {
    printf("Using colored font?\n");
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
