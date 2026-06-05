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

#include "zenoh-pico/link/config/parser.h"

#include <stdio.h>
#include <string.h>

#include "zenoh-pico/system/common/platform.h"

_z_config_iter_t _z_config_iter_make(const char *s, size_t n) {
    _z_config_iter_t it;
    it._cur = s;
    it._end = (s == NULL) ? NULL : (s + n);
    return it;
}

bool _z_config_iter_next(_z_config_iter_t *it, _z_config_kv_t *out) {
    while (it->_cur != NULL && it->_cur < it->_end) {
        // Find the end of the current segment (up to ';' or end of buffer).
        const char *seg_start = it->_cur;
        const char *seg_end = seg_start;
        while (seg_end < it->_end && *seg_end != _Z_CONFIG_LIST_SEPARATOR) {
            seg_end++;
        }
        // Advance cursor past the separator for the next call.
        it->_cur = (seg_end < it->_end) ? (seg_end + 1) : it->_end;

        // Skip empty segments (e.g. leading/trailing/double ';').
        if (seg_end == seg_start) {
            continue;
        }

        // Split on the first '='.
        const char *eq = seg_start;
        while (eq < seg_end && *eq != _Z_CONFIG_KEYVALUE_SEPARATOR) {
            eq++;
        }
        // A segment without '=' or with an empty key is skipped.
        if (eq == seg_end || eq == seg_start) {
            continue;
        }

        out->_key = seg_start;
        out->_key_len = (size_t)(eq - seg_start);
        out->_value = eq + 1;
        out->_value_len = (size_t)(seg_end - (eq + 1));
        return true;
    }
    return false;
}

bool _z_config_kv_key_eq(const _z_config_kv_t *kv, const char *key) {
    size_t klen = strlen(key);
    return (klen == kv->_key_len) && (memcmp(kv->_key, key, klen) == 0);
}

bool _z_config_kv_value_as_u32(const _z_config_kv_t *kv, uint32_t *out) {
    if (kv->_value_len == 0) {
        return false;
    }
    uint32_t acc = 0;
    for (size_t i = 0; i < kv->_value_len; i++) {
        char c = kv->_value[i];
        if (c < '0' || c > '9') {
            return false;
        }
        acc = (acc * 10u) + (uint32_t)(c - '0');
    }
    *out = acc;
    return true;
}

// ── Builder ──────────────────────────────────────────────────────────────────

void _z_config_builder_init(_z_config_builder_t *b) {
    b->_buf = NULL;
    b->_len = 0;
    b->_cap = 0;
    b->_error = false;
}

// Ensure at least `extra` more bytes (plus room for a NUL) are available.
static bool _z_config_builder_reserve(_z_config_builder_t *b, size_t extra) {
    if (b->_error) {
        return false;
    }
    size_t needed = b->_len + extra + 1;  // +1 for NUL
    if (needed <= b->_cap) {
        return true;
    }
    size_t new_cap = (b->_cap == 0) ? 16 : b->_cap;
    while (new_cap < needed) {
        new_cap *= 2;
    }
    char *new_buf = (char *)z_realloc(b->_buf, new_cap);
    if (new_buf == NULL) {
        b->_error = true;
        return false;
    }
    b->_buf = new_buf;
    b->_cap = new_cap;
    return true;
}

void _z_config_builder_add_substr(_z_config_builder_t *b, const char *key, const char *value, size_t value_len) {
    size_t key_len = strlen(key);
    // Optional leading ';', then key, '=', value.
    size_t sep = (b->_len == 0) ? 0 : 1;
    if (!_z_config_builder_reserve(b, sep + key_len + 1 + value_len)) {
        return;
    }
    if (sep != 0) {
        b->_buf[b->_len++] = _Z_CONFIG_LIST_SEPARATOR;
    }
    memcpy(b->_buf + b->_len, key, key_len);
    b->_len += key_len;
    b->_buf[b->_len++] = _Z_CONFIG_KEYVALUE_SEPARATOR;
    if (value_len != 0) {
        memcpy(b->_buf + b->_len, value, value_len);
        b->_len += value_len;
    }
}

void _z_config_builder_add_str(_z_config_builder_t *b, const char *key, const char *value) {
    _z_config_builder_add_substr(b, key, value, strlen(value));
}

void _z_config_builder_add_u32(_z_config_builder_t *b, const char *key, uint32_t value) {
    char tmp[11];  // up to 10 digits for uint32 + NUL
    int written = snprintf(tmp, sizeof(tmp), "%u", value);
    if (written < 0) {
        b->_error = true;
        return;
    }
    _z_config_builder_add_substr(b, key, tmp, (size_t)written);
}

char *_z_config_builder_take(_z_config_builder_t *b) {
    if (b->_error) {
        _z_config_builder_clear(b);
        return NULL;
    }
    // Guarantee a valid (possibly empty) NUL-terminated buffer.
    if (b->_buf == NULL) {
        if (!_z_config_builder_reserve(b, 0)) {
            return NULL;
        }
    }
    b->_buf[b->_len] = '\0';
    char *ret = b->_buf;
    _z_config_builder_init(b);
    return ret;
}

void _z_config_builder_clear(_z_config_builder_t *b) {
    if (b->_buf != NULL) {
        z_free(b->_buf);
    }
    _z_config_builder_init(b);
}
