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

#ifndef ZENOH_PICO_LINK_CONFIG_RAWETH_H
#define ZENOH_PICO_LINK_CONFIG_RAWETH_H

#include <stdint.h>

#include "zenoh-pico/collections/string.h"
#include "zenoh-pico/config.h"
#include "zenoh-pico/utils/result.h"

#ifdef __cplusplus
extern "C" {
#endif

#define RAWETH_SCHEMA "reth"

// ── Typed config struct ──────────────────────────────────────────────────────
//
// This alternative of the `_z_endpoint_config_t` variant is included into the
// variant (in `endpoint_config.h`) only when `Z_FEATURE_RAWETH_TRANSPORT` is
// enabled.
//
// reth#iface=<name>;...
// NOTE: `mapping` and `whitelist` are complex sub-formats parsed by the raw-eth
// transport layer. They are kept here as their raw string form so the typed
// config remains lossless; the transport projects them back to an intmap and
// reuses its existing parser.
typedef struct {
    _z_string_t _interface;
    _z_string_t _mapping;
    _z_string_t _whitelist;
    uint16_t _ethtype;
} _z_raweth_config_t;

static inline void _z_raweth_config_clear(_z_raweth_config_t *c) {
    _z_string_clear(&c->_interface);
    _z_string_clear(&c->_mapping);
    _z_string_clear(&c->_whitelist);
}

// ── Typed config (de)serialization (intmap-free) ─────────────────────────────
//
// NOTE: only the `iface` and `ethtype` options are modeled by the typed struct;
// the `mapping` and `whitelist` options remain parsed via the intmap path by
// the raw-ethernet transport for now.
//
// Parse the config portion of a `reth#...` endpoint into a typed struct.
z_result_t _z_raweth_config_typed_from_strn(_z_raweth_config_t *cfg, const char *s, size_t n);
// Serialize a typed raw-ethernet config into its `key=value;...` string (heap).
char *_z_raweth_config_typed_to_str(const _z_raweth_config_t *cfg);

#ifdef __cplusplus
}
#endif

#endif /* ZENOH_PICO_LINK_CONFIG_RAWETH_H */
