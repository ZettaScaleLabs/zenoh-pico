//
// Copyright (c) 2026 ZettaScale Technology
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Apache License, Version 2.0
// which is available at https://www.apache.org/licenses/LICENSE-2.0.
//
// SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
//
// Contributors:
//   ZettaScale Zenoh Team, <zenoh@zettascale.tech>
//

// Minimal, intmap-free codec for endpoint config strings.
//
// Endpoint configuration is serialized as a `;`-separated list of `key=value`
// pairs, e.g. `tout=5000;iface=eth0`. This header provides:
//   * a zero-copy parsing iterator (`_z_config_iter_t`) yielding key/value
//     substring views, and
//   * a growable string builder (`_z_config_builder_t`) for emitting that form.
//
// Both are deliberately decoupled from `_z_str_intmap_t` so that the typed
// per-link config (de)serializers can avoid pulling in the generic map.

#ifndef ZENOH_PICO_LINK_CONFIG_PARSER_H
#define ZENOH_PICO_LINK_CONFIG_PARSER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "zenoh-pico/utils/result.h"

#ifdef __cplusplus
extern "C" {
#endif

#define _Z_CONFIG_KEYVALUE_SEPARATOR '='
#define _Z_CONFIG_LIST_SEPARATOR ';'

// A non-owning view of a single `key=value` pair.
typedef struct {
    const char *_key;
    size_t _key_len;
    const char *_value;
    size_t _value_len;
} _z_config_kv_t;

// Iterator over a config string. Does not own or modify the underlying buffer.
typedef struct {
    const char *_cur;
    const char *_end;
} _z_config_iter_t;

// Create an iterator over the first `n` bytes of `s` (may be NULL if n == 0).
_z_config_iter_t _z_config_iter_make(const char *s, size_t n);

// Advance the iterator. On success fills `*out` with the next pair and returns
// true. Returns false when iteration is complete. Empty segments are skipped.
bool _z_config_iter_next(_z_config_iter_t *it, _z_config_kv_t *out);

// True if the pair's key equals the NUL-terminated `key`.
bool _z_config_kv_key_eq(const _z_config_kv_t *kv, const char *key);

// Parse the pair's value as an unsigned 32-bit integer (base 10). Returns false
// if the value is empty or not a valid number.
bool _z_config_kv_value_as_u32(const _z_config_kv_t *kv, uint32_t *out);

// A growable builder that appends `key=value` pairs separated by `;`.
typedef struct {
    char *_buf;
    size_t _len;
    size_t _cap;
    bool _error;  // sticky: set on allocation failure
} _z_config_builder_t;

// Initialize an empty builder.
void _z_config_builder_init(_z_config_builder_t *b);

// Append `key=value`, where value is given as a substring of length `value_len`.
// A leading `;` is inserted automatically between entries.
void _z_config_builder_add_substr(_z_config_builder_t *b, const char *key, const char *value, size_t value_len);

// Append `key=value` for a NUL-terminated value.
void _z_config_builder_add_str(_z_config_builder_t *b, const char *key, const char *value);

// Append `key=<decimal>` for an unsigned 32-bit value.
void _z_config_builder_add_u32(_z_config_builder_t *b, const char *key, uint32_t value);

// Finalize: returns a heap-allocated NUL-terminated string (possibly empty) and
// resets the builder. Returns NULL if a previous append failed to allocate.
// Ownership of the returned buffer transfers to the caller.
char *_z_config_builder_take(_z_config_builder_t *b);

// Release any buffer held by the builder without producing a string.
void _z_config_builder_clear(_z_config_builder_t *b);

#ifdef __cplusplus
}
#endif

#endif /* ZENOH_PICO_LINK_CONFIG_PARSER_H */
