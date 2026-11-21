// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Syncr0ndi
#pragma once
#include <stdint.h>
/* 8 bytes per glyph, top row first, bit 7 = leftmost pixel. Non-printable -> blank. */
const uint8_t *font8x8_glyph(uint8_t ch);
