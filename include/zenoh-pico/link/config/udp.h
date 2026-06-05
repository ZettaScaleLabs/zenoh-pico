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

#ifndef ZENOH_PICO_LINK_CONFIG_UDP_H
#define ZENOH_PICO_LINK_CONFIG_UDP_H

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
// variant (in `endpoint_config.h`) only when a UDP link is enabled.

// udp#iface=<name>;tout=<ms>;join=<group>
typedef struct {
    _z_string_t _iface;
    _z_string_t _join;
    uint32_t _tout;
} _z_udp_config_t;

static inline void _z_udp_config_clear(_z_udp_config_t *c) {
    _z_string_clear(&c->_iface);
    _z_string_clear(&c->_join);
}

#define UDP_CONFIG_IFACE_STR "iface"
#define UDP_CONFIG_TOUT_STR "tout"
#define UDP_CONFIG_JOIN_STR "join"

#if Z_FEATURE_LINK_UDP_UNICAST == 1 || Z_FEATURE_LINK_UDP_MULTICAST == 1
// ── Typed config (de)serialization (intmap-free) ─────────────────────────────
// Parse the config portion of a `udp#...` endpoint into a typed struct.
z_result_t _z_udp_config_typed_from_strn(_z_udp_config_t *cfg, const char *s, size_t n);
// Serialize a typed UDP config into its `key=value;...` string form (heap).
char *_z_udp_config_typed_to_str(const _z_udp_config_t *cfg);
#endif

#ifdef __cplusplus
}
#endif

#endif /* ZENOH_PICO_LINK_CONFIG_UDP_H */
