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

#ifndef ZENOH_PICO_MSGEXTCODEC_H
#define ZENOH_PICO_MSGEXTCODEC_H

#include "zenoh-pico/protocol/core.h"
#include "zenoh-pico/protocol/ext.h"
#include "zenoh-pico/protocol/iobuf.h"

/*------------------ Message Extension ------------------*/
int8_t _z_msg_ext_encode(_z_wbuf_t *wbf, const _z_msg_ext_t *ext);
int8_t _z_msg_ext_decode(_z_msg_ext_t *ext, _z_zbuf_t *zbf);
int8_t _z_msg_ext_decode_na(_z_msg_ext_t *ext, _z_zbuf_t *zbf);

#endif /* ZENOH_PICO_MSGEXTCODEC_H */

// NOTE: the following headers are for unit testing only
#ifdef ZENOH_PICO_TEST_H
// ------------------ Message Fields ------------------
int8_t _z_msg_ext_encode_unit(_z_wbuf_t *wbf, const _z_msg_ext_unit_t *pld);
int8_t _z_msg_ext_decode_unit(_z_msg_ext_unit_t *pld, _z_zbuf_t *zbf);
int8_t _z_msg_ext_decode_unit_na(_z_msg_ext_unit_t *pld, _z_zbuf_t *zbf);

int8_t _z_msg_ext_encode_zint(_z_wbuf_t *wbf, const _z_msg_ext_zint_t *pld);
int8_t _z_msg_ext_decode_zint(_z_msg_ext_zint_t *pld, _z_zbuf_t *zbf);
int8_t _z_msg_ext_decode_zint_na(_z_msg_ext_zint_t *pld, _z_zbuf_t *zbf);

int8_t _z_msg_ext_encode_zbuf(_z_wbuf_t *wbf, const _z_msg_ext_zbuf_t *pld);
int8_t _z_msg_ext_decode_zbuf(_z_msg_ext_zbuf_t *pld, _z_zbuf_t *zbf);
int8_t _z_msg_ext_decode_zbuf_na(_z_msg_ext_zbuf_t *pld, _z_zbuf_t *zbf);

#endif /* ZENOH_PICO_TEST_H */