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

#include "zenoh-pico/net/resource.h"

#include <stddef.h>

#include "zenoh-pico/protocol/core.h"

_z_keyexpr_t _z_rname(const char *rname) {
    return (_z_keyexpr_t){
        ._id = Z_RESOURCE_ID_NONE,
        ._uses_remote_mapping = false,
        ._owns_suffix = false,
        ._suffix = (char *)rname,
    };
}

_z_keyexpr_t _z_rid_with_suffix(_z_zint_t rid, const char *suffix) {
    return (_z_keyexpr_t){
        ._id = rid,
        ._uses_remote_mapping = false,
        ._owns_suffix = false,
        ._suffix = (char *)suffix,
    };
}
