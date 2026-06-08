#pragma once

#include "arch.h"
#include "config.h"
#include "fs.h"
#include "net.h"
#include "str.h"

struct mg_http_header {
  struct ct_str name;   // Header name
  struct ct_str value;  // Header value
};

struct mg_http_message {
  struct ct_str method, uri, query, proto;             // Request/response line
  struct mg_http_header headers[MG_MAX_HTTP_HEADERS];  // Headers
  struct ct_str body;                                  // Body
  struct ct_str head;                                  // Request + headers
  struct ct_str message;  // Request + headers + body
};

// Parameter for mg_http_serve_dir()
struct mg_http_serve_opts {
  const char *root_dir;       // Web root directory, must be non-NULL
  const char *ssi_pattern;    // SSI file name pattern, e.g. #.shtml
  const char *extra_headers;  // Extra HTTP headers to add in responses
  const char *mime_types;     // Extra mime types, ext1=type1,ext2=type2,..
  const char *page404;        // Path to the 404 page, or NULL by default
  struct ct_fs *fs;           // Filesystem implementation. Use NULL for POSIX
};

// Parameter for mg_http_next_multipart
struct mg_http_part {
  struct ct_str name;      // Form field name
  struct ct_str filename;  // Filename for file uploads
  struct ct_str body;      // Part contents
};

int mg_http_parse(const char *s, size_t len, struct mg_http_message *);
int mg_http_get_request_len(const unsigned char *buf, size_t buf_len);
void mg_http_printf_chunk(struct ct_connection *cnn, const char *fmt, ...);
void mg_http_write_chunk(struct ct_connection *c, const char *buf, size_t len);
void mg_http_delete_chunk(struct ct_connection *c, struct mg_http_message *hm);
struct ct_connection *mg_http_listen(struct ct_mgr *, const char *url,
                                    ct_event_handler_t fn, void *fn_data);
struct ct_connection *mg_http_connect(struct ct_mgr *, const char *url,
                                    ct_event_handler_t fn, void *fn_data);
void mg_http_serve_dir(struct ct_connection *, struct mg_http_message *hm,
                       const struct mg_http_serve_opts *);
void mg_http_serve_file(struct ct_connection *, struct mg_http_message *hm,
                        const char *path, const struct mg_http_serve_opts *);
void mg_http_reply(struct ct_connection *, int status_code, const char *headers,
                   const char *body_fmt, ...);
struct ct_str *mg_http_get_header(struct mg_http_message *, const char *name);
struct ct_str mg_http_var(struct ct_str buf, struct ct_str name);
int mg_http_get_var(const struct ct_str *, const char *name, char *, size_t);
int ct_url_decode(const char *s, size_t n, char *to, size_t to_len, int form);
size_t ct_url_encode(const char *s, size_t n, char *buf, size_t len);
void mg_http_creds(struct mg_http_message *, char *, size_t, char *, size_t);
long mg_http_upload(struct ct_connection *c, struct mg_http_message *hm,
                    struct ct_fs *fs, const char *dir, size_t max_size);
void mg_http_bauth(struct ct_connection *, const char *user, const char *pass);
struct ct_str mg_http_get_header_var(struct ct_str s, struct ct_str v);
size_t mg_http_next_multipart(struct ct_str, size_t, struct mg_http_part *);
int mg_http_status(const struct mg_http_message *hm);