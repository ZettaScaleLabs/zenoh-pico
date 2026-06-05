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
//

#include "zenoh-pico/link/config/tls.h"

#include <string.h>

#include "zenoh-pico/link/config/parser.h"

#if Z_FEATURE_LINK_TLS == 1

// ── Typed config (de)serialization (intmap-free) ─────────────────────────────

// Mirrors `_z_opt_is_true` in tls_stream.c: NULL/empty => true; a leading
// '0'/'n'/'N'/'f'/'F' => false; otherwise true.
static bool _z_tls_opt_is_true(const char *val, size_t len) {
    if (len == 0) {
        return true;
    }
    char c = val[0];
    return !(c == '0' || c == 'n' || c == 'N' || c == 'f' || c == 'F');
}

// Set a credential to the `path` form, consuming the substring.
static void _z_tls_cred_set_path(_z_tls_cert_source_t *cred, const char *v, size_t n) {
    _z_tls_cert_source_destroy(cred);
    _z_string_t s = _z_string_copy_from_substr(v, n);
    *cred = _z_tls_cert_source_from_path(&s);
}

// Set a credential to the `base64` form, consuming the substring.
static void _z_tls_cred_set_base64(_z_tls_cert_source_t *cred, const char *v, size_t n) {
    _z_tls_cert_source_destroy(cred);
    _z_string_t s = _z_string_copy_from_substr(v, n);
    *cred = _z_tls_cert_source_from_base64(&s);
}

z_result_t _z_tls_config_typed_from_strn(_z_tls_config_t *cfg, const char *s, size_t n) {
    memset(cfg, 0, sizeof(*cfg));
    // Defaults: mTLS off, verify name on connect on.
    cfg->_enable_mtls = false;
    cfg->_verify_name_on_connect = true;

    _z_config_iter_t it = _z_config_iter_make(s, n);
    _z_config_kv_t kv;
    while (_z_config_iter_next(&it, &kv)) {
        if (_z_config_kv_key_eq(&kv, TLS_CONFIG_ROOT_CA_CERTIFICATE_STR)) {
            _z_tls_cred_set_path(&cfg->_root_ca_certificate, kv._value, kv._value_len);
        } else if (_z_config_kv_key_eq(&kv, TLS_CONFIG_ROOT_CA_CERTIFICATE_BASE64_STR)) {
            _z_tls_cred_set_base64(&cfg->_root_ca_certificate, kv._value, kv._value_len);
        } else if (_z_config_kv_key_eq(&kv, TLS_CONFIG_LISTEN_PRIVATE_KEY_STR)) {
            _z_tls_cred_set_path(&cfg->_listen_private_key, kv._value, kv._value_len);
        } else if (_z_config_kv_key_eq(&kv, TLS_CONFIG_LISTEN_PRIVATE_KEY_BASE64_STR)) {
            _z_tls_cred_set_base64(&cfg->_listen_private_key, kv._value, kv._value_len);
        } else if (_z_config_kv_key_eq(&kv, TLS_CONFIG_LISTEN_CERTIFICATE_STR)) {
            _z_tls_cred_set_path(&cfg->_listen_certificate, kv._value, kv._value_len);
        } else if (_z_config_kv_key_eq(&kv, TLS_CONFIG_LISTEN_CERTIFICATE_BASE64_STR)) {
            _z_tls_cred_set_base64(&cfg->_listen_certificate, kv._value, kv._value_len);
        } else if (_z_config_kv_key_eq(&kv, TLS_CONFIG_CONNECT_PRIVATE_KEY_STR)) {
            _z_tls_cred_set_path(&cfg->_connect_private_key, kv._value, kv._value_len);
        } else if (_z_config_kv_key_eq(&kv, TLS_CONFIG_CONNECT_PRIVATE_KEY_BASE64_STR)) {
            _z_tls_cred_set_base64(&cfg->_connect_private_key, kv._value, kv._value_len);
        } else if (_z_config_kv_key_eq(&kv, TLS_CONFIG_CONNECT_CERTIFICATE_STR)) {
            _z_tls_cred_set_path(&cfg->_connect_certificate, kv._value, kv._value_len);
        } else if (_z_config_kv_key_eq(&kv, TLS_CONFIG_CONNECT_CERTIFICATE_BASE64_STR)) {
            _z_tls_cred_set_base64(&cfg->_connect_certificate, kv._value, kv._value_len);
        } else if (_z_config_kv_key_eq(&kv, TLS_CONFIG_ENABLE_MTLS_STR)) {
            cfg->_enable_mtls = _z_tls_opt_is_true(kv._value, kv._value_len);
            cfg->_has_enable_mtls = true;
        } else if (_z_config_kv_key_eq(&kv, TLS_CONFIG_VERIFY_NAME_ON_CONNECT_STR)) {
            cfg->_verify_name_on_connect = _z_tls_opt_is_true(kv._value, kv._value_len);
            cfg->_has_verify_name_on_connect = true;
        }
        // Unknown keys are ignored for forward compatibility.
    }
    return _Z_RES_OK;
}

// Emit a credential as either its path or base64 key, depending on which form
// is active. No-op when the credential is unset.
static void _z_tls_cred_to_str(_z_config_builder_t *b, const _z_tls_cert_source_t *cred, const char *path_key,
                               const char *base64_key) {
    if (_z_tls_cert_source_is_path(cred)) {
        const _z_string_t *p = _z_tls_cert_source_get_path((_z_tls_cert_source_t *)cred);
        _z_config_builder_add_substr(b, path_key, _z_string_data(p), _z_string_len(p));
    } else if (_z_tls_cert_source_is_base64(cred)) {
        const _z_string_t *p = _z_tls_cert_source_get_base64((_z_tls_cert_source_t *)cred);
        _z_config_builder_add_substr(b, base64_key, _z_string_data(p), _z_string_len(p));
    }
}

char *_z_tls_config_typed_to_str(const _z_tls_config_t *cfg) {
    _z_config_builder_t b;
    _z_config_builder_init(&b);
    // Follow the intmap key ordering for stable output.
    _z_tls_cred_to_str(&b, &cfg->_root_ca_certificate, TLS_CONFIG_ROOT_CA_CERTIFICATE_STR,
                       TLS_CONFIG_ROOT_CA_CERTIFICATE_BASE64_STR);
    _z_tls_cred_to_str(&b, &cfg->_listen_private_key, TLS_CONFIG_LISTEN_PRIVATE_KEY_STR,
                       TLS_CONFIG_LISTEN_PRIVATE_KEY_BASE64_STR);
    _z_tls_cred_to_str(&b, &cfg->_listen_certificate, TLS_CONFIG_LISTEN_CERTIFICATE_STR,
                       TLS_CONFIG_LISTEN_CERTIFICATE_BASE64_STR);
    if (cfg->_enable_mtls) {
        _z_config_builder_add_str(&b, TLS_CONFIG_ENABLE_MTLS_STR, "true");
    }
    _z_tls_cred_to_str(&b, &cfg->_connect_private_key, TLS_CONFIG_CONNECT_PRIVATE_KEY_STR,
                       TLS_CONFIG_CONNECT_PRIVATE_KEY_BASE64_STR);
    _z_tls_cred_to_str(&b, &cfg->_connect_certificate, TLS_CONFIG_CONNECT_CERTIFICATE_STR,
                       TLS_CONFIG_CONNECT_CERTIFICATE_BASE64_STR);
    if (!cfg->_verify_name_on_connect) {
        _z_config_builder_add_str(&b, TLS_CONFIG_VERIFY_NAME_ON_CONNECT_STR, "false");
    }
    return _z_config_builder_take(&b);
}

#endif
