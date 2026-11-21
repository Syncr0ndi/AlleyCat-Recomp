// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Syncr0ndi
#pragma once
#include <stddef.h>
#include <stdint.h>

/* The one version of CAT.EXE this port is built for (55,067 bytes, 22 Aug 1984). */
#define CAT_EXE_SHA256 "4979c8867826e08881d1d4bec95c95d6bc1ac70fd21050ccdb4f733a50d7bfdd"

enum { GAME_FILE_OK = 0, GAME_FILE_MISSING = 1, GAME_FILE_WRONG = 2 };

void sha256_hex(const uint8_t *data, size_t len, char out[65]);
/* Reads `path` and checks it is the known CAT.EXE. On success *data is malloc'd. */
int game_file_load(const char *path, uint8_t **data, size_t *len);
