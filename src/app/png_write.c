// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Syncr0ndi
/* Minimal PNG writer (RGB, stored/uncompressed deflate) for debug screenshots. */
#include "png_write.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint32_t crc_table[256];
static void crc_init(void) {
    for (uint32_t n = 0; n < 256; n++) {
        uint32_t c = n;
        for (int k = 0; k < 8; k++) c = (c & 1) ? 0xEDB88320u ^ (c >> 1) : c >> 1;
        crc_table[n] = c;
    }
}
static uint32_t crc(uint32_t c, const uint8_t *b, size_t n) {
    c ^= 0xFFFFFFFFu;
    while (n--) c = crc_table[(c ^ *b++) & 0xFF] ^ (c >> 8);
    return c ^ 0xFFFFFFFFu;
}
static void be32(uint8_t *p, uint32_t v) { p[0] = (uint8_t)(v >> 24); p[1] = (uint8_t)(v >> 16); p[2] = (uint8_t)(v >> 8); p[3] = (uint8_t)v; }
static void chunk(FILE *f, const char *type, const uint8_t *data, uint32_t len) {
    uint8_t h[8]; be32(h, len); memcpy(h + 4, type, 4);
    fwrite(h, 1, 8, f); if (len) fwrite(data, 1, len, f);
    uint32_t c = crc(0, h + 4, 4); c = crc(c, data, len);
    uint8_t t[4]; be32(t, c); fwrite(t, 1, 4, f);
}

int png_write_argb(const char *path, const uint32_t *px, int w, int h, int scale) {
    static int inited; if (!inited) { crc_init(); inited = 1; }
    int W = w * scale, H = h * scale;
    size_t row = (size_t)W * 3 + 1, raw_len = row * H;
    uint8_t *raw = malloc(raw_len);
    for (int y = 0; y < H; y++) {
        uint8_t *r = raw + y * row; *r++ = 0;
        for (int x = 0; x < W; x++) {
            uint32_t c = px[(y / scale) * w + x / scale];
            *r++ = (uint8_t)(c >> 16); *r++ = (uint8_t)(c >> 8); *r++ = (uint8_t)c;
        }
    }
    size_t nblk = (raw_len + 65534) / 65535;
    size_t zlen = 2 + raw_len + nblk * 5 + 4;
    uint8_t *z = malloc(zlen), *q = z;
    *q++ = 0x78; *q++ = 0x01;
    uint32_t a = 1, b = 0;
    for (size_t i = 0; i < raw_len; i++) { a = (a + raw[i]) % 65521; b = (b + a) % 65521; }
    for (size_t off = 0; off < raw_len; off += 65535) {
        size_t n = raw_len - off > 65535 ? 65535 : raw_len - off;
        *q++ = off + n >= raw_len ? 1 : 0;
        *q++ = (uint8_t)n; *q++ = (uint8_t)(n >> 8); *q++ = (uint8_t)~n; *q++ = (uint8_t)(~n >> 8);
        memcpy(q, raw + off, n); q += n;
    }
    be32(q, (b << 16) | a); q += 4;
    FILE *f = fopen(path, "wb");
    if (!f) { free(raw); free(z); return 0; }
    static const uint8_t sig[8] = { 0x89, 'P', 'N', 'G', 13, 10, 26, 10 };
    fwrite(sig, 1, 8, f);
    uint8_t ihdr[13]; be32(ihdr, (uint32_t)W); be32(ihdr + 4, (uint32_t)H);
    ihdr[8] = 8; ihdr[9] = 2; ihdr[10] = 0; ihdr[11] = 0; ihdr[12] = 0;
    chunk(f, "IHDR", ihdr, 13);
    chunk(f, "IDAT", z, (uint32_t)(q - z));
    chunk(f, "IEND", NULL, 0);
    fclose(f); free(raw); free(z);
    return 1;
}
