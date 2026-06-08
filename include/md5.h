#pragma once

#include "arch.h"

typedef struct {
  uint32_t buf[4];
  uint32_t bits[2];
  unsigned char in[64];
} ct_md5_ctx;

void ct_md5_init(ct_md5_ctx *c);
void ct_md5_update(ct_md5_ctx *c, const unsigned char *data, size_t len);
void ct_md5_final(ct_md5_ctx *c, unsigned char[16]);