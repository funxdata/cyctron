#pragma once

#include "arch.h"
#include "str.h"

#ifndef CT_JSON_MAX_DEPTH
#define CT_JSON_MAX_DEPTH 30
#endif

// Error return values - negative. Successful returns are >= 0
enum { CT_JSON_TOO_DEEP = -1, CT_JSON_INVALID = -2, CT_JSON_NOT_FOUND = -3 };
int ct_json_get(struct ct_str json, const char *path, int *toklen);

struct ct_str ct_json_get_tok(struct ct_str json, const char *path);
bool ct_json_get_num(struct ct_str json, const char *path, double *v);
bool ct_json_get_bool(struct ct_str json, const char *path, bool *v);
long ct_json_get_long(struct ct_str json, const char *path, long dflt);
char *ct_json_get_str(struct ct_str json, const char *path);
char *ct_json_get_hex(struct ct_str json, const char *path, int *len);
char *ct_json_get_b64(struct ct_str json, const char *path, int *len);

bool ct_json_unescape(struct ct_str str, char *buf, size_t len);
size_t ct_json_next(struct ct_str obj, size_t ofs, struct ct_str *key,
                    struct ct_str *val);