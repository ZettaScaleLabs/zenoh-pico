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

#include "zenoh-pico/link/config/serial.h"

#include <string.h>

#include "zenoh-pico/link/config/parser.h"

#if Z_FEATURE_LINK_SERIAL == 1

// ── Typed config (de)serialization (intmap-free) ─────────────────────────────

z_result_t _z_serial_config_typed_from_strn(_z_serial_config_t *cfg, const char *s, size_t n) {
    memset(cfg, 0, sizeof(*cfg));

    _z_config_iter_t it = _z_config_iter_make(s, n);
    _z_config_kv_t kv;
    while (_z_config_iter_next(&it, &kv)) {
        if (_z_config_kv_key_eq(&kv, SERIAL_CONFIG_BAUDRATE_STR)) {
            uint32_t baudrate = 0;
            if (!_z_config_kv_value_as_u32(&kv, &baudrate)) {
                return _Z_ERR_CONFIG_LOCATOR_INVALID;
            }
            cfg->_baudrate = baudrate;
        }
        // Unknown keys are ignored for forward compatibility.
    }
    return _Z_RES_OK;
}

char *_z_serial_config_typed_to_str(const _z_serial_config_t *cfg) {
    _z_config_builder_t b;
    _z_config_builder_init(&b);
    if (cfg->_baudrate != 0) {
        _z_config_builder_add_u32(&b, SERIAL_CONFIG_BAUDRATE_STR, cfg->_baudrate);
    }
    return _z_config_builder_take(&b);
}
#endif
