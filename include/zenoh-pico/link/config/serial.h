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

#ifndef ZENOH_PICO_LINK_CONFIG_SERIAL_H
#define ZENOH_PICO_LINK_CONFIG_SERIAL_H

#include <stdint.h>

#include "zenoh-pico/collections/string.h"
#include "zenoh-pico/config.h"
#include "zenoh-pico/system/platform.h"
#include "zenoh-pico/utils/result.h"

#ifdef __cplusplus
extern "C" {
#endif

// ── Typed config struct ──────────────────────────────────────────────────────
//
// This alternative of the `_z_endpoint_config_t` variant is included into the
// variant (in `endpoint_config.h`) only when `Z_FEATURE_LINK_SERIAL` is
// enabled.

// serial#baudrate=<bps>
typedef struct {
    uint32_t _baudrate;
} _z_serial_config_t;

// serial is POD: clear is a no-op.
static inline void _z_serial_config_clear(_z_serial_config_t *c) { (void)c; }

#if Z_FEATURE_LINK_SERIAL == 1

#define SERIAL_CONFIG_BAUDRATE_STR "baudrate"

// #define SERIAL_CONFIG_DATABITS_STR     "data_bits"
// #define SERIAL_CONFIG_FLOWCONTROL_STR  "flow_control"
// #define SERIAL_CONFIG_PARITY_STR       "parity"
// #define SERIAL_CONFIG_STOPBITS_STR     "stop_bits"
// #define SERIAL_CONFIG_TOUT_STR         "tout"

// ── Typed config (de)serialization (intmap-free) ─────────────────────────────
// Parse the config portion of a `serial#...` endpoint into a typed struct.
z_result_t _z_serial_config_typed_from_strn(_z_serial_config_t *cfg, const char *s, size_t n);
// Serialize a typed serial config into its `key=value;...` string form (heap).
char *_z_serial_config_typed_to_str(const _z_serial_config_t *cfg);
#endif

#ifdef __cplusplus
}
#endif

#endif /* ZENOH_PICO_LINK_CONFIG_SERIAL_H */
