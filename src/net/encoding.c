//
// Copyright (c) 2024 ZettaScale Technology
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

#include "zenoh-pico/net/encoding.h"

#include <string.h>

#include "zenoh-pico/api/constants.h"
#include "zenoh-pico/utils/logging.h"

_z_encoding_t _z_encoding_make(z_encoding_id_t id, const char *schema) {
    return (_z_encoding_t){
        .id = id, .schema = _z_bytes_wrap((const uint8_t *)schema, (schema == NULL) ? (size_t)0 : strlen(schema))};
}

_z_encoding_t _z_encoding_null(void) { return _z_encoding_make(Z_ENCODING_ID_DEFAULT, NULL); }
