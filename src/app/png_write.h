#pragma once
#include <stdint.h>
/* Write an ARGB8888 image as PNG, optionally scaled up by an integer factor. Returns 1 on success. */
int png_write_argb(const char *path, const uint32_t *px, int w, int h, int scale);
