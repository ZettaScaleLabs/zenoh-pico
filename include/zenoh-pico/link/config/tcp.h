//
// Copyright (c) 2022 ZettaScale Technology
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

#ifndef ZENOH_PICO_LINK_CONFIG_TCP_H
#define ZENOH_PICO_LINK_CONFIG_TCP_H

#include <stdint.h>

#include "zenoh-pico/collections/string.h"
#include "zenoh-pico/config.h"
#include "zenoh-pico/utils/result.h"

#ifdef __cplusplus
extern "C" {
#endif

// ── Typed config struct ──────────────────────────────────────────────────────
//
// This alternative of the `_z_endpoint_config_t` variant is included into the
// variant (in `endpoint_config.h`) only when `Z_FEATURE_LINK_TCP` is enabled.

// tcp#tout=<ms>
typedef struct {
    uint32_t _tout;
} _z_tcp_config_t;

// tcp is POD: clear is a no-op.
static inline void _z_tcp_config_clear(_z_tcp_config_t *c) { (void)c; }

#if Z_FEATURE_LINK_TCP == 1

#define TCP_CONFIG_TOUT_STR "tout"

// ── Typed config (de)serialization ───────────────────────────────────────────
//
// These operate on the strongly-typed `_z_tcp_config_t` and have no dependency
// on the generic string intmap. The wire format is unchanged: a `;`-separated
// list of `key=value` pairs (here just the optional `tout=<ms>`).

// Parse the config portion of a `tcp#...` endpoint into a typed struct.
z_result_t _z_tcp_config_typed_from_strn(_z_tcp_config_t *cfg, const char *s, size_t n);
// Serialize a typed TCP config into its `key=value;...` string form (heap).
// Returns an empty heap string when no option is set; NULL on allocation error.
char *_z_tcp_config_typed_to_str(const _z_tcp_config_t *cfg);
#endif

#ifdef __cplusplus
}
#endif

#endif /* ZENOH_PICO_LINK_CONFIG_TCP_H */
