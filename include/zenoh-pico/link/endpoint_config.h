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

// Strongly-typed representation of an endpoint's per-link configuration.
//
// `_z_endpoint_t._config` is a tagged union (variant) where each alternative is
// a typed struct holding exactly the fields a given link type understands. This
// replaces the former generic `_z_str_intmap_t` (key -> string) representation,
// improving type safety and removing runtime key errors.
//
// This file defines:
//   * one typed config struct per supported link type, and
//   * the variant instantiation (`_z_endpoint_config_t`) over those structs.
//
// The per-link string (de)serialization lives in `src/link/config/<link>.c`
// (and `src/transport/raweth/link.c`), dispatched from `src/link/endpoint.c`.
//
// NOTE: the TLS stream layer (`tls_stream.c`) still consumes a local
// `_z_str_intmap_t` projected from the typed TLS config at the point of use, to
// avoid rewriting security-sensitive credential-loading code.

#ifndef ZENOH_PICO_LINK_ENDPOINT_CONFIG_H
#define ZENOH_PICO_LINK_ENDPOINT_CONFIG_H

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "zenoh-pico/collections/string.h"
#include "zenoh-pico/config.h"
#include "zenoh-pico/utils/result.h"

// Per-link typed config structs and their clear helpers. Each header defines the
// `_z_<link>_config_t` struct (and `_z_tls_cert_source_t` for TLS) plus the
// matching `_z_<link>_config_clear()` callback used by the variant below. The
// structs themselves are defined unconditionally so dependent code can reference
// the types regardless of feature flags; only the variant *alternatives* (and
// the per-link (de)serializers) are gated by the corresponding `Z_FEATURE_*`.
#include "zenoh-pico/link/config/bt.h"
#include "zenoh-pico/link/config/raweth.h"
#include "zenoh-pico/link/config/serial.h"
#include "zenoh-pico/link/config/tcp.h"
#include "zenoh-pico/link/config/tls.h"
#include "zenoh-pico/link/config/udp.h"
#include "zenoh-pico/link/config/ws.h"

#ifdef __cplusplus
extern "C" {
#endif

// "Move" for owning config structs: shallow-copy the value and zero the source.
//
// This is only needed for the `from_X(val)` constructors, which take a *bare*
// struct (no tag) and would otherwise leave the caller's argument aliasing the
// stored value — a subsequent clear of that argument would double-free. Zeroing
// makes `from_X` consume its argument, matching the move convention used
// elsewhere in zenoh-pico.
//
// The variant-to-variant `move` and the `take_X` paths reset the source tag to
// NONE themselves, so they never double-destroy regardless of this macro. POD
// alternatives (tcp/ws/serial) own nothing and therefore use the template's
// default shallow-copy move.
//
// Defined here and (identically, behind the same guard) in `config/tls.h`, which
// needs it for its standalone `_z_tls_cert_source` variant.
#ifndef _Z_ENDPOINT_CONFIG_MOVE
#define _Z_ENDPOINT_CONFIG_MOVE(dst, src) \
    do {                                  \
        *(dst) = *(src);                  \
        memset((src), 0, sizeof(*(src))); \
    } while (0)
#endif

// ── Variant instantiation ────────────────────────────────────────────────────
//
// Generated symbols (NAME = _z_endpoint_config):
//   types : _z_endpoint_config_t, _z_endpoint_config_tag_t
//   ctor  : _z_endpoint_config_none(), _z_endpoint_config_from_<link>(...)
//   query : _z_endpoint_config_is_<link>(), _z_endpoint_config_tag()
//   access: _z_endpoint_config_get_<link>(), _z_endpoint_config_take_<link>()
//   misc  : _z_endpoint_config_destroy(), _z_endpoint_config_move()
//
// Each alternative is included only when its link type is enabled. The variant
// template supports a sparse set of alternatives, so each link keeps its fixed
// slot number (which determines its tag value and generated symbol names)
// regardless of which other links are compiled in. At least one link must be
// enabled for the variant to be instantiated.

#define _ZP_VARIANT_TEMPLATE_NAME _z_endpoint_config

#if Z_FEATURE_LINK_TCP == 1
#define _ZP_VARIANT_TEMPLATE_1_TYPE _z_tcp_config_t
#define _ZP_VARIANT_TEMPLATE_1_NAME tcp
#define _ZP_VARIANT_TEMPLATE_1_DESTROY_FN(ptr) _z_tcp_config_clear(ptr)
// tcp is POD: default shallow-copy move is sufficient.
#endif

#if Z_FEATURE_LINK_UDP_UNICAST == 1 || Z_FEATURE_LINK_UDP_MULTICAST == 1
#define _ZP_VARIANT_TEMPLATE_2_TYPE _z_udp_config_t
#define _ZP_VARIANT_TEMPLATE_2_NAME udp
#define _ZP_VARIANT_TEMPLATE_2_DESTROY_FN(ptr) _z_udp_config_clear(ptr)
#define _ZP_VARIANT_TEMPLATE_2_MOVE_FN(dst, src) _Z_ENDPOINT_CONFIG_MOVE(dst, src)
#endif

#if Z_FEATURE_LINK_BLUETOOTH == 1
#define _ZP_VARIANT_TEMPLATE_3_TYPE _z_bt_config_t
#define _ZP_VARIANT_TEMPLATE_3_NAME bt
#define _ZP_VARIANT_TEMPLATE_3_DESTROY_FN(ptr) _z_bt_config_clear(ptr)
#define _ZP_VARIANT_TEMPLATE_3_MOVE_FN(dst, src) _Z_ENDPOINT_CONFIG_MOVE(dst, src)
#endif

#if Z_FEATURE_LINK_SERIAL == 1
#define _ZP_VARIANT_TEMPLATE_4_TYPE _z_serial_config_t
#define _ZP_VARIANT_TEMPLATE_4_NAME serial
#define _ZP_VARIANT_TEMPLATE_4_DESTROY_FN(ptr) _z_serial_config_clear(ptr)
// serial is POD: default shallow-copy move is sufficient.
#endif

#if Z_FEATURE_LINK_WS == 1
#define _ZP_VARIANT_TEMPLATE_5_TYPE _z_ws_config_t
#define _ZP_VARIANT_TEMPLATE_5_NAME ws
#define _ZP_VARIANT_TEMPLATE_5_DESTROY_FN(ptr) _z_ws_config_clear(ptr)
// ws is POD: default shallow-copy move is sufficient.
#endif

#if Z_FEATURE_LINK_TLS == 1
#define _ZP_VARIANT_TEMPLATE_6_TYPE _z_tls_config_t
#define _ZP_VARIANT_TEMPLATE_6_NAME tls
#define _ZP_VARIANT_TEMPLATE_6_DESTROY_FN(ptr) _z_tls_config_clear(ptr)
#define _ZP_VARIANT_TEMPLATE_6_MOVE_FN(dst, src) _Z_ENDPOINT_CONFIG_MOVE(dst, src)
#endif

#if Z_FEATURE_RAWETH_TRANSPORT == 1
#define _ZP_VARIANT_TEMPLATE_7_TYPE _z_raweth_config_t
#define _ZP_VARIANT_TEMPLATE_7_NAME raweth
#define _ZP_VARIANT_TEMPLATE_7_DESTROY_FN(ptr) _z_raweth_config_clear(ptr)
#define _ZP_VARIANT_TEMPLATE_7_MOVE_FN(dst, src) _Z_ENDPOINT_CONFIG_MOVE(dst, src)
#endif

#include "zenoh-pico/collections/variant_template.h"

#ifdef __cplusplus
}
#endif

#endif /* ZENOH_PICO_LINK_ENDPOINT_CONFIG_H */
