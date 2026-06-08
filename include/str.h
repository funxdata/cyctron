#pragma once

#include "arch.h"

// Describes an arbitrary chunk of memory
struct mg_str {
  char *buf;   // String data
  size_t len;  // String length
};

// Using macro to avoid shadowing C++ struct constructor, see #1298
#define ct_str(s) ct_str_s(s)

struct ct_str ct_str(const char *s);
struct ct_str ct_str_n(const char *s, size_t n);
int ct_casecmp(const char *s1, const char *s2);
int ct_strcmp(const struct ct_str str1, const struct ct_str str2);
int ct_strcasecmp(const struct ct_str str1, const struct ct_str str2);
struct ct_str mg_strdup(const struct ct_str s);
bool ct_match(struct ct_str str, struct ct_str pattern, struct ct_str *caps);
bool ct_span(struct ct_str s, struct ct_str *a, struct ct_str *b, char delim);

bool ct_str_to_num(struct ct_str, int base, void *val, size_t val_len);