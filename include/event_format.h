// include/event_format.h
// Pure formatting helpers for event payloads (no network, no globals).
#ifndef EVENT_FORMAT_H
#define EVENT_FORMAT_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Escape a string for embedding inside a JSON string literal. */
void event_escape_json(const char *in, char *out, size_t out_size);

/** Allocate an escaped copy of `in` (caller frees). */
char *event_escape_alloc(const char *in);

/**
 * Build {"type":TYPE,"data":DATA,"timestamp":N}.
 * DATA must already be valid JSON. Returns 0 and sets *out (caller frees).
 */
int event_format_with_type(const char *type, const char *data, char **out);

/** Build {"type":"message","message":"...","timestamp":N}. */
int event_format_message(const char *msg, char **out);

/** Build {"type":TYPE,"data":"...","timestamp":N} (data is a string). */
int event_format_json_string(const char *type, const char *str, char **out);

#ifdef __cplusplus
}
#endif

#endif /* EVENT_FORMAT_H */
