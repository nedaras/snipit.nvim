#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STB_TRUETYPE_IMPLEMENTATION 

#include <stdio.h>
#include <stdlib.h>
#include "stb_image_write.h"
#include "stb_truetype.h"

int main() {

  /* load font file */
  long size;
  unsigned char* fontBuffer;

  FILE* fontFile = fopen("/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf", "rb");
  fseek(fontFile, 0, SEEK_END);
  size = ftell(fontFile); /* how long is the file ? */
  fseek(fontFile, 0, SEEK_SET); /* reset */

  fontBuffer = malloc(size);

  fread(fontBuffer, size, 1, fontFile);
  fclose(fontFile);

  /* prepare font */
  stbtt_fontinfo info;
  if (!stbtt_InitFont(&info, fontBuffer, 0))
  {
    printf("failed\n");
  }

  int b_w = 512; /* bitmap width */
  int b_h = 128; /* bitmap height */
  int l_h = 64; /* line height */

  /* create a bitmap for the phrase */
  unsigned char* bitmap = malloc(b_w * b_h * 3);

  for (int i = 0; i < b_w * b_h; i++) {
    bitmap[i * 3 + 0] = 0;
    bitmap[i * 3 + 1] = 0;
    bitmap[i * 3 + 2] = 0;
  }

  /* calculate font scaling */
  float scale = stbtt_ScaleForPixelHeight(&info, l_h);

  char* word = "s";

  int x = 0;

  int ascent, descent, lineGap;
  stbtt_GetFontVMetrics(&info, &ascent, &descent, &lineGap);

  ascent = roundf(ascent * scale);
  descent = roundf(descent * scale);

  for (int i = 0; i < strlen(word); ++i)
  {
    int w;
    int h;

    int ox;
    int oy;

    //stbtt_MakeCodepointBitmap(&info, bitmap + byteOffset, c_x2 - c_x1, c_y2 - c_y1, b_w, scale, scale, word[i]);
    unsigned char* char_bitmap = stbtt_GetCodepointBitmap(&info, 1.5 * scale, scale, word[i], &w, &h, &ox, &oy);

    for (int y = 0; y < h; y++) {
      for (int x = 0; x < w; x++) {
        int h = char_bitmap[y * w + x];
        int i = y * b_w + x;

        int r = 255;
        int g = 255;
        int b = 255;

        bitmap[i * 3 + 0] = (bitmap[i * 3 + 0] * (255 - h) + r * h) / 255;
        bitmap[i * 3 + 1] = (bitmap[i * 3 + 1] * (255 - h) + g * h) / 255;
        bitmap[i * 3 + 2] = (bitmap[i * 3 + 2] * (255 - h) + b * h) / 255;

      }
    }
    //printf("\n");
    stbtt_FreeBitmap(char_bitmap, 0);

    //printf("(%d; %d) (%d; %d)\n", w, h, ox, oy);
    //int kern;
    //int kern = stbtt_GetCodepointKernAdvance(&info, word[i], word[i + 1]);
    //printf("kern: %d\n", kern);
    //x += roundf(kern * scale);
  }

  stbi_write_png("out.png", b_w, b_h, 3, bitmap, b_w * 3);

  free(fontBuffer);
  free(bitmap);

  return 0;
}
