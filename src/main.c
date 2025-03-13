#include <assert.h>
#include <errno.h>
#include <stdint.h>
#include <stdbool.h>
#include <png.h>
#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_TRUETYPE_TABLES_H

bool is_colored(FT_Face face) {
  static const uint32_t tag = FT_MAKE_TAG('C', 'B', 'D', 'T');

  size_t len = 0;
  FT_Error err = FT_Load_Sfnt_Table(face, tag, 0, NULL, &len);
  assert(err == FT_Err_Ok);

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

  uint32_t idx = FT_Get_Char_Index(face, 0x1F480);
  assert(idx != 0);

  printf("idx: %d\n", idx);

  err = FT_Load_Glyph(face, idx, FT_LOAD_COLOR | FT_LOAD_DEFAULT);
  if (err != FT_Err_Ok) {
    printf("snipit.nvim: load glyph: %s\n", FT_Error_String(err));
    goto err;
  }

  err = FT_Render_Glyph(face->glyph, FT_RENDER_MODE_NORMAL);
  if (err != FT_Err_Ok) {
    printf("snipit.nvim: %s\n", FT_Error_String(err));
    goto err;
  }

  uint8_t* bitmap = NULL;

  uint32_t w;
  uint32_t h;

  if (face->glyph->bitmap.pixel_mode == FT_PIXEL_MODE_BGRA) {
    w = face->glyph->bitmap.width;
    h = face->glyph->bitmap.rows;

    bitmap = malloc(w * h * 3); // RGB
    if (bitmap == NULL) {
      printf("snipit.lua: out of memory");
      goto err;
    }

    memset(bitmap, 0, w * h * 3);

    uint8_t* src = face->glyph->bitmap.buffer;
    uint8_t* dist = bitmap;

    for (uint32_t y = 0; y < h; y++) {
      for (uint32_t x = 0; x < w; x++) {
        // need some alpha blending here
        uint8_t b = *src++;
        uint8_t g = *src++;
        uint8_t r = *src++;
        uint8_t a = *src++;

        *dist++ = r;
        *dist++ = g;
        *dist++ = b;
      }
    }
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

  if (bitmap == NULL) {
    goto cleanup;
  }

  printf("save\n");

  uint8_t* i = bitmap;
  for (uint32_t y = 0; y < h; y++) {
    for (uint32_t x = 0; x < w; x++) {
      uint8_t r = *i++;
      uint8_t g = *i++;
      uint8_t b = *i++;

      printf("\e[48;2;%d;%d;%dm  \e[0m", r, g, b);
    }
    printf("\n");
  }

  FILE* file = fopen("out.png", "wb");
  if (file == NULL) {
    printf("snipit.nvim: out.png: cannot open: %s\n", strerror(errno));
    goto err;
  }

  // below err handling is not a thing no more

  png_structp png_writer = NULL;
  png_infop png_info = NULL;

  png_writer = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
  if (png_writer == NULL) {
    goto err;
  }

  png_info = png_create_info_struct(png_writer);
  if (png_info == NULL) {
    goto err;
  }

  if (setjmp(png_jmpbuf(png_writer))) {
    goto err;
  }

  png_init_io(png_writer, file);
  png_set_IHDR(png_writer, png_info, w, h, 8, PNG_COLOR_TYPE_RGB, PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);

  png_write_info(png_writer, png_info);

  png_bytep *image = (png_bytep *)malloc(sizeof(png_bytep) * h);
  assert(image != NULL);

  for (uint32_t i = 0; i < h; i++) {
    image[i] = bitmap + (i * w * 3);
  }
  
  png_write_image(png_writer, image);
  png_write_end(png_writer, png_info);

  free(image);

cleanup:
  // we should put all these png_writer and some other stuff higher so it would be defined or idk add defer in c somehow
  if (png_writer != NULL || png_info != NULL) png_destroy_write_struct(&png_writer, &png_info);
  if (file != NULL) fclose(file);
  if (bitmap != NULL) free(bitmap);
  if (face != NULL) FT_Done_Face(face);
  if (library != NULL) FT_Done_FreeType(library);
  return 0;

err:
  if (png_writer != NULL || png_info != NULL) png_destroy_write_struct(&png_writer, &png_info);
  if (file != NULL) fclose(file);
  if (bitmap != NULL) free(bitmap);
  if (face != NULL) FT_Done_Face(face);
  if (library != NULL) FT_Done_FreeType(library);
  return 1;
}
