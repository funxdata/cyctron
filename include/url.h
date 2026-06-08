#pragma once
#include "str.h"

unsigned short ct_url_port(const char *url);
int ct_url_is_ssl(const char *url);
struct ct_str ct_url_host(const char *url);
struct ct_str ct_url_user(const char *url);
struct ct_str ct_url_pass(const char *url);
const char *ct_url_uri(const char *url);