//
// Copyright (c) 2025 ZettaScale Technology
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

#ifndef ZENOH_PICO_LINK_CONFIG_TLS_H
#define ZENOH_PICO_LINK_CONFIG_TLS_H

#include <stdbool.h>
#include <string.h>

#include "zenoh-pico/collections/string.h"
#include "zenoh-pico/config.h"
#include "zenoh-pico/utils/result.h"

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
#ifndef _Z_ENDPOINT_CONFIG_MOVE
#define _Z_ENDPOINT_CONFIG_MOVE(dst, src) \
    do {                                  \
        *(dst) = *(src);                  \
        memset((src), 0, sizeof(*(src))); \
    } while (0)
#endif

// ── TLS credential source variant ────────────────────────────────────────────
//
// Each TLS credential (CA, private key, certificate) can be supplied either as
// a filesystem path or as an inline base64-encoded blob. The two forms are
// mutually exclusive, so they are modeled as a variant<path, base64> rather than
// two independent (and potentially conflicting) string fields.
//
// Generated symbols (NAME = _z_tls_cert_source):
//   _z_tls_cert_source_t, _z_tls_cert_source_none(),
//   _z_tls_cert_source_from_path(...), _z_tls_cert_source_from_base64(...),
//   _z_tls_cert_source_is_path(...), _z_tls_cert_source_is_base64(...),
//   _z_tls_cert_source_get_path(...), _z_tls_cert_source_get_base64(...),
//   _z_tls_cert_source_destroy(...), _z_tls_cert_source_move(...)

#define _ZP_VARIANT_TEMPLATE_NAME _z_tls_cert_source

#define _ZP_VARIANT_TEMPLATE_1_TYPE _z_string_t
#define _ZP_VARIANT_TEMPLATE_1_NAME path
#define _ZP_VARIANT_TEMPLATE_1_DESTROY_FN(ptr) _z_string_clear(ptr)
#define _ZP_VARIANT_TEMPLATE_1_MOVE_FN(dst, src) _Z_ENDPOINT_CONFIG_MOVE(dst, src)

#define _ZP_VARIANT_TEMPLATE_2_TYPE _z_string_t
#define _ZP_VARIANT_TEMPLATE_2_NAME base64
#define _ZP_VARIANT_TEMPLATE_2_DESTROY_FN(ptr) _z_string_clear(ptr)
#define _ZP_VARIANT_TEMPLATE_2_MOVE_FN(dst, src) _Z_ENDPOINT_CONFIG_MOVE(dst, src)

#include "zenoh-pico/collections/variant_template.h"

// ── Typed config struct ──────────────────────────────────────────────────────
//
// This alternative of the `_z_endpoint_config_t` variant is included into the
// variant (in `endpoint_config.h`) only when `Z_FEATURE_LINK_TLS` is enabled.
//
// tls#... (certificates, keys and verification flags)
//
// Each credential is a path-or-base64 variant (the two encodings are mutually
// exclusive). `_none` on a credential means "not configured".
typedef struct {
    _z_tls_cert_source_t _root_ca_certificate;
    _z_tls_cert_source_t _listen_private_key;
    _z_tls_cert_source_t _listen_certificate;
    _z_tls_cert_source_t _connect_private_key;
    _z_tls_cert_source_t _connect_certificate;
    bool _enable_mtls;
    bool _verify_name_on_connect;
    // Track whether the boolean options were explicitly present in the config
    // string. Needed so a later merge with the session config does not get
    // shadowed by the typed defaults.
    bool _has_enable_mtls;
    bool _has_verify_name_on_connect;
} _z_tls_config_t;

static inline void _z_tls_config_clear(_z_tls_config_t *c) {
    _z_tls_cert_source_destroy(&c->_root_ca_certificate);
    _z_tls_cert_source_destroy(&c->_listen_private_key);
    _z_tls_cert_source_destroy(&c->_listen_certificate);
    _z_tls_cert_source_destroy(&c->_connect_private_key);
    _z_tls_cert_source_destroy(&c->_connect_certificate);
}

#if Z_FEATURE_LINK_TLS == 1

#define TLS_CONFIG_ROOT_CA_CERTIFICATE_STR "root_ca_certificate"
#define TLS_CONFIG_ROOT_CA_CERTIFICATE_BASE64_STR "root_ca_certificate_base64"
#define TLS_CONFIG_LISTEN_PRIVATE_KEY_STR "listen_private_key"
#define TLS_CONFIG_LISTEN_PRIVATE_KEY_BASE64_STR "listen_private_key_base64"
#define TLS_CONFIG_LISTEN_CERTIFICATE_STR "listen_certificate"
#define TLS_CONFIG_LISTEN_CERTIFICATE_BASE64_STR "listen_certificate_base64"
#define TLS_CONFIG_ENABLE_MTLS_STR "enable_mtls"
#define TLS_CONFIG_CONNECT_PRIVATE_KEY_STR "connect_private_key"
#define TLS_CONFIG_CONNECT_PRIVATE_KEY_BASE64_STR "connect_private_key_base64"
#define TLS_CONFIG_CONNECT_CERTIFICATE_STR "connect_certificate"
#define TLS_CONFIG_CONNECT_CERTIFICATE_BASE64_STR "connect_certificate_base64"
#define TLS_CONFIG_VERIFY_NAME_ON_CONNECT_STR "verify_name_on_connect"

// ── Typed config (de)serialization (intmap-free) ─────────────────────────────
// Parse the config portion of a `tls#...` endpoint into a typed struct.
//
// Each credential is path-or-base64. If both forms appear for the same
// credential, the last one seen wins (matching intmap's last-insert semantics).
z_result_t _z_tls_config_typed_from_strn(_z_tls_config_t *cfg, const char *s, size_t n);
// Serialize a typed TLS config into its `key=value;...` string form (heap).
char *_z_tls_config_typed_to_str(const _z_tls_config_t *cfg);
#endif

#ifdef __cplusplus
}
#endif

#endif /* ZENOH_PICO_LINK_CONFIG_TLS_H */
