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

#include "zenoh-pico/utils/config.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "zenoh-pico/collections/string.h"
#include "zenoh-pico/config.h"
#include "zenoh-pico/utils/pointers.h"
#include "zenoh-pico/utils/string.h"
#include "zenoh-pico/utils/uuid.h"

z_result_t _z_config_init(_z_config_t *ps) {
    memset(ps, 0, sizeof(_z_config_t));
    _z_config_connect_init(&ps->_connect);
    // Pre-fill the parsed default values of the typed properties. The `_str`
    // member is left NULL so that an otherwise-empty configuration is still
    // reported as empty (see `_z_config_len`), while consumers can read the
    // effective parsed value directly, whether or not the property was set.
    // `_connect_exit_on_failure` is intentionally left at its zero-value because
    // its default is mode-dependent and resolved when the session is opened, and
    // the TLS booleans default to `false` (no dedicated default value).
    ps->_mode._parsed = Z_WHATAMI_CLIENT;  // By default, zenoh-pico operates as a client
    ps->_session_zid._parsed = _z_id_empty();
    (void)_z_str_parse_bool(Z_CONFIG_MULTICAST_SCOUTING_DEFAULT, &ps->_multicast_scouting._parsed);
    (void)_z_str_parse_bool(Z_CONFIG_ADD_TIMESTAMP_DEFAULT, &ps->_add_timestamp._parsed);
    (void)_z_str_parse_bool(Z_CONFIG_LISTEN_EXIT_ON_FAILURE_DEFAULT, &ps->_listen_exit_on_failure._parsed);
    (void)_z_str_parse_i32(Z_CONFIG_SCOUTING_TIMEOUT_DEFAULT, &ps->_scouting_timeout._parsed);
    int32_t scouting_what_default = 0;
    (void)_z_str_parse_i32(Z_CONFIG_SCOUTING_WHAT_DEFAULT, &scouting_what_default);
    ps->_scouting_what._parsed = (z_what_t)scouting_what_default;
    (void)_z_str_parse_i32(Z_CONFIG_CONNECT_TIMEOUT_DEFAULT, &ps->_connect_timeout._parsed);
    (void)_z_str_parse_i32(Z_CONFIG_LISTEN_TIMEOUT_DEFAULT, &ps->_listen_timeout._parsed);
    return _Z_RES_OK;
}

static z_result_t _z_config_insert_bool(const char *value, _z_config_bool_t *bool_field) {
    // Boolean properties only accept "true"/"false": reject any other value
    // at insert time rather than when the session is opened.
    bool parsed = false;
    if (!_z_str_parse_bool(value, &parsed)) {
        _Z_ERROR_LOG(_Z_ERR_CONFIG_INVALID_VALUE);
        return _Z_ERR_CONFIG_INVALID_VALUE;
    }
    bool_field->_str = value;
    bool_field->_parsed = parsed;
    return _Z_RES_OK;
}

static z_result_t _z_config_insert_int(const char *value, _z_config_int_t *int_field) {
    // Integer properties only accept a valid base-10 int32 value: reject any
    // other value at insert time rather than when the session is opened.
    int32_t parsed = 0;
    if (!_z_str_parse_i32(value, &parsed)) {
        _Z_ERROR_LOG(_Z_ERR_CONFIG_INVALID_VALUE);
        return _Z_ERR_CONFIG_INVALID_VALUE;
    }
    int_field->_str = value;
    int_field->_parsed = parsed;
    return _Z_RES_OK;
}

static z_result_t _z_config_insert_mode(const char *value, _z_config_mode_t *mode_field) {
    // Mode only accepts "client"/"peer": reject any other value at insert time
    // rather than when the session is opened.
    z_whatami_t parsed;
    if (_z_str_eq(value, Z_CONFIG_MODE_CLIENT)) {
        parsed = Z_WHATAMI_CLIENT;
    } else if (_z_str_eq(value, Z_CONFIG_MODE_PEER)) {
        parsed = Z_WHATAMI_PEER;
    } else {
        _Z_ERROR("Trying to configure an invalid mode: %s", value);
        _Z_ERROR_LOG(_Z_ERR_CONFIG_INVALID_MODE);
        return _Z_ERR_CONFIG_INVALID_MODE;
    }
    mode_field->_str = value;
    mode_field->_parsed = parsed;
    return _Z_RES_OK;
}

static z_result_t _z_config_insert_what(const char *value, _z_config_what_t *what_field) {
    // The scouting "what" property is a bitmask encoded as a base-10 int32.
    int32_t parsed = 0;
    if (!_z_str_parse_i32(value, &parsed)) {
        _Z_ERROR_LOG(_Z_ERR_CONFIG_INVALID_VALUE);
        return _Z_ERR_CONFIG_INVALID_VALUE;
    }
    what_field->_str = value;
    what_field->_parsed = (z_what_t)parsed;
    return _Z_RES_OK;
}

static z_result_t _z_config_insert_zid(const char *value, _z_config_zid_t *zid_field) {
    _z_id_t parsed = _z_id_empty();
    _z_uuid_to_bytes(parsed.id, value);
    zid_field->_str = value;
    zid_field->_parsed = parsed;
    return _Z_RES_OK;
}

z_result_t _zp_config_insert(_z_config_t *ps, uint8_t key, const char *value) {
    if (value == NULL) {
        return _Z_ERR_NULL;
    }
    switch (key) {
        case Z_CONFIG_CONNECT_KEY:
            if (!_z_config_connect_push_back(&ps->_connect, &value)) {
                _Z_ERROR_LOG(_Z_ERR_CONFIG_FAILED_INSERT);
                return _Z_ERR_CONFIG_FAILED_INSERT;
            }
            return _Z_RES_OK;
        // String properties
        case Z_CONFIG_MODE_KEY:
            return _z_config_insert_mode(value, &ps->_mode);
        case Z_CONFIG_LISTEN_KEY:
            ps->_listen = value;
            return _Z_RES_OK;
        case Z_CONFIG_USER_KEY:
            ps->_user = value;
            return _Z_RES_OK;
        case Z_CONFIG_PASSWORD_KEY:
            ps->_password = value;
            return _Z_RES_OK;
        case Z_CONFIG_MULTICAST_LOCATOR_KEY:
            ps->_multicast_locator = value;
            return _Z_RES_OK;
        case Z_CONFIG_SCOUTING_TIMEOUT_KEY:
            return _z_config_insert_int(value, &ps->_scouting_timeout);
        case Z_CONFIG_SCOUTING_WHAT_KEY:
            return _z_config_insert_what(value, &ps->_scouting_what);
        case Z_CONFIG_SESSION_ZID_KEY:
            return _z_config_insert_zid(value, &ps->_session_zid);
        case Z_CONFIG_TLS_ROOT_CA_CERTIFICATE_KEY:
            ps->_tls_root_ca_certificate = value;
            return _Z_RES_OK;
        case Z_CONFIG_TLS_ROOT_CA_CERTIFICATE_BASE64_KEY:
            ps->_tls_root_ca_certificate_base64 = value;
            return _Z_RES_OK;
        case Z_CONFIG_TLS_LISTEN_PRIVATE_KEY_KEY:
            ps->_tls_listen_private_key = value;
            return _Z_RES_OK;
        case Z_CONFIG_TLS_LISTEN_PRIVATE_KEY_BASE64_KEY:
            ps->_tls_listen_private_key_base64 = value;
            return _Z_RES_OK;
        case Z_CONFIG_TLS_LISTEN_CERTIFICATE_KEY:
            ps->_tls_listen_certificate = value;
            return _Z_RES_OK;
        case Z_CONFIG_TLS_LISTEN_CERTIFICATE_BASE64_KEY:
            ps->_tls_listen_certificate_base64 = value;
            return _Z_RES_OK;
        case Z_CONFIG_TLS_CONNECT_PRIVATE_KEY_KEY:
            ps->_tls_connect_private_key = value;
            return _Z_RES_OK;
        case Z_CONFIG_TLS_CONNECT_PRIVATE_KEY_BASE64_KEY:
            ps->_tls_connect_private_key_base64 = value;
            return _Z_RES_OK;
        case Z_CONFIG_TLS_CONNECT_CERTIFICATE_KEY:
            ps->_tls_connect_certificate = value;
            return _Z_RES_OK;
        case Z_CONFIG_TLS_CONNECT_CERTIFICATE_BASE64_KEY:
            ps->_tls_connect_certificate_base64 = value;
            return _Z_RES_OK;
        case Z_CONFIG_CONNECT_TIMEOUT_KEY:
            return _z_config_insert_int(value, &ps->_connect_timeout);
        case Z_CONFIG_LISTEN_TIMEOUT_KEY:
            return _z_config_insert_int(value, &ps->_listen_timeout);
        // Boolean properties
        case Z_CONFIG_MULTICAST_SCOUTING_KEY:
            return _z_config_insert_bool(value, &ps->_multicast_scouting);
        case Z_CONFIG_ADD_TIMESTAMP_KEY:
            return _z_config_insert_bool(value, &ps->_add_timestamp);
        case Z_CONFIG_TLS_ENABLE_MTLS_KEY:
            return _z_config_insert_bool(value, &ps->_tls_enable_mtls);
        case Z_CONFIG_TLS_VERIFY_NAME_ON_CONNECT_KEY:
            return _z_config_insert_bool(value, &ps->_tls_verify_name_on_connect);
        case Z_CONFIG_CONNECT_EXIT_ON_FAILURE_KEY:
            return _z_config_insert_bool(value, &ps->_connect_exit_on_failure);
        case Z_CONFIG_LISTEN_EXIT_ON_FAILURE_KEY:
            return _z_config_insert_bool(value, &ps->_listen_exit_on_failure);
        default:
            return _Z_ERR_CONFIG_FAILED_INSERT;
    }
}

const char *_z_config_get(const _z_config_t *ps, uint8_t key) {
    switch (key) {
        case Z_CONFIG_CONNECT_KEY: {
            const _z_config_str_t *first = _z_config_connect_const_get(&ps->_connect, 0);
            return (first != NULL) ? *first : NULL;
        }
        case Z_CONFIG_MODE_KEY:
            return ps->_mode._str;
        case Z_CONFIG_LISTEN_KEY:
            return ps->_listen;
        case Z_CONFIG_USER_KEY:
            return ps->_user;
        case Z_CONFIG_PASSWORD_KEY:
            return ps->_password;
        case Z_CONFIG_MULTICAST_LOCATOR_KEY:
            return ps->_multicast_locator;
        case Z_CONFIG_SCOUTING_TIMEOUT_KEY:
            return ps->_scouting_timeout._str;
        case Z_CONFIG_SCOUTING_WHAT_KEY:
            return ps->_scouting_what._str;
        case Z_CONFIG_SESSION_ZID_KEY:
            return ps->_session_zid._str;
        case Z_CONFIG_TLS_ROOT_CA_CERTIFICATE_KEY:
            return ps->_tls_root_ca_certificate;
        case Z_CONFIG_TLS_ROOT_CA_CERTIFICATE_BASE64_KEY:
            return ps->_tls_root_ca_certificate_base64;
        case Z_CONFIG_TLS_LISTEN_PRIVATE_KEY_KEY:
            return ps->_tls_listen_private_key;
        case Z_CONFIG_TLS_LISTEN_PRIVATE_KEY_BASE64_KEY:
            return ps->_tls_listen_private_key_base64;
        case Z_CONFIG_TLS_LISTEN_CERTIFICATE_KEY:
            return ps->_tls_listen_certificate;
        case Z_CONFIG_TLS_LISTEN_CERTIFICATE_BASE64_KEY:
            return ps->_tls_listen_certificate_base64;
        case Z_CONFIG_TLS_CONNECT_PRIVATE_KEY_KEY:
            return ps->_tls_connect_private_key;
        case Z_CONFIG_TLS_CONNECT_PRIVATE_KEY_BASE64_KEY:
            return ps->_tls_connect_private_key_base64;
        case Z_CONFIG_TLS_CONNECT_CERTIFICATE_KEY:
            return ps->_tls_connect_certificate;
        case Z_CONFIG_TLS_CONNECT_CERTIFICATE_BASE64_KEY:
            return ps->_tls_connect_certificate_base64;
        case Z_CONFIG_CONNECT_TIMEOUT_KEY:
            return ps->_connect_timeout._str;
        case Z_CONFIG_LISTEN_TIMEOUT_KEY:
            return ps->_listen_timeout._str;
        case Z_CONFIG_MULTICAST_SCOUTING_KEY:
            return ps->_multicast_scouting._str;
        case Z_CONFIG_ADD_TIMESTAMP_KEY:
            return ps->_add_timestamp._str;
        case Z_CONFIG_TLS_ENABLE_MTLS_KEY:
            return ps->_tls_enable_mtls._str;
        case Z_CONFIG_TLS_VERIFY_NAME_ON_CONNECT_KEY:
            return ps->_tls_verify_name_on_connect._str;
        case Z_CONFIG_CONNECT_EXIT_ON_FAILURE_KEY:
            return ps->_connect_exit_on_failure._str;
        case Z_CONFIG_LISTEN_EXIT_ON_FAILURE_KEY:
            return ps->_listen_exit_on_failure._str;
        default:
            return NULL;
    }
}

z_result_t _z_config_get_all(const _z_config_t *ps, _z_string_svec_t *locators, uint8_t key) {
    if (key == Z_CONFIG_CONNECT_KEY) {
        size_t connect_len = _z_config_connect_size(&ps->_connect);
        for (size_t i = 0; i < connect_len; i++) {
            const char *val = *_z_config_connect_const_at(&ps->_connect, i);
            _z_string_t s = _z_string_copy_from_str(val);
            _Z_RETURN_IF_ERR(_z_string_svec_append(locators, &s, true));
        }
    } else {
        const char *val = _z_config_get(ps, key);
        if (val != NULL) {
            _z_string_t s = _z_string_copy_from_str(val);
            _Z_RETURN_IF_ERR(_z_string_svec_append(locators, &s, true));
        }
    }
    return _Z_RES_OK;
}

size_t _z_config_len(const _z_config_t *ps) {
    size_t len = _z_config_connect_size(&ps->_connect);
    // Single-value `const char *` properties: count the ones that are set.
    const char *const *fields[] = {
        &ps->_listen,
        &ps->_user,
        &ps->_password,
        &ps->_multicast_locator,
        &ps->_tls_root_ca_certificate,
        &ps->_tls_root_ca_certificate_base64,
        &ps->_tls_listen_private_key,
        &ps->_tls_listen_private_key_base64,
        &ps->_tls_listen_certificate,
        &ps->_tls_listen_certificate_base64,
        &ps->_tls_connect_private_key,
        &ps->_tls_connect_private_key_base64,
        &ps->_tls_connect_certificate,
        &ps->_tls_connect_certificate_base64,
    };
    for (size_t i = 0; i < sizeof(fields) / sizeof(fields[0]); i++) {
        if (*fields[i] != NULL) {
            len++;
        }
    }
    // Typed properties hold their effective value in a dedicated parsed member
    // and keep the original string in `_str`, which is non-NULL only when the
    // property was explicitly set: count those.
    const char *const typed_str_fields[] = {
        ps->_mode._str,
        ps->_session_zid._str,
        ps->_scouting_what._str,
        ps->_scouting_timeout._str,
        ps->_connect_timeout._str,
        ps->_listen_timeout._str,
        ps->_multicast_scouting._str,
        ps->_add_timestamp._str,
        ps->_tls_enable_mtls._str,
        ps->_tls_verify_name_on_connect._str,
        ps->_connect_exit_on_failure._str,
        ps->_listen_exit_on_failure._str,
    };
    for (size_t i = 0; i < sizeof(typed_str_fields) / sizeof(typed_str_fields[0]); i++) {
        if (typed_str_fields[i] != NULL) {
            len++;
        }
    }
    return len;
}

bool _z_config_is_empty(const _z_config_t *ps) { return _z_config_len(ps) == 0; }

z_result_t _z_config_copy(_z_config_t *dst, const _z_config_t *src) {
    // Configuration strings are non-owning pointers and the connect locators are
    // stored in a fixed-capacity value vector, so a plain shallow copy is enough.
    *dst = *src;
    return _Z_RES_OK;
}

_z_config_t _z_config_clone(const _z_config_t *src) {
    _z_config_t dst;
    if (_z_config_copy(&dst, src) != _Z_RES_OK) {
        _z_config_init(&dst);
    }
    return dst;
}

z_result_t _z_config_move(_z_config_t *dst, _z_config_t *src) {
    *dst = *src;
    return _z_config_init(src);
}

void _z_config_clear(_z_config_t *ps) {
    if (ps == NULL) {
        return;
    }
    // Configuration strings are non-owning, so there is nothing to free; just
    // reset the configuration to an empty state.
    _z_config_init(ps);
}

void _z_config_free(_z_config_t **ps) {
    _z_config_t *config = *ps;
    if (config != NULL) {
        _z_config_clear(config);
        z_free(config);
        *ps = NULL;
    }
}

/*------------------ int-string map ------------------*/
z_result_t _z_str_intmap_from_strn(_z_str_intmap_t *strint, const char *s, uint8_t argc, _z_str_intmapping_t argv[],
                                   size_t n) {
    z_result_t ret = _Z_RES_OK;
    *strint = _z_str_intmap_make();

    // Check the string contains only the right
    const char *start = s;
    const char *end = &s[n - 1];
    size_t curr_len = n;
    while (curr_len > 0) {
        const char *p_key_start = start;
        const char *p_key_end = memchr(p_key_start, INT_STR_MAP_KEYVALUE_SEPARATOR, curr_len);

        if (p_key_end != NULL) {
            // Verify the key is valid based on the provided mapping
            size_t p_key_len = _z_ptr_char_diff(p_key_end, p_key_start);
            bool found = false;
            uint8_t key = 0;
            for (uint8_t i = 0; i < argc; i++) {
                if (p_key_len != strlen(argv[i]._str)) {
                    continue;
                }
                if (strncmp(p_key_start, argv[i]._str, p_key_len) != 0) {
                    continue;
                }

                found = true;
                key = argv[i]._key;
                break;
            }

            if (found == false) {
                break;
            }

            // Read and populate the value
            const char *p_value_start = _z_cptr_char_offset(p_key_end, 1);
            size_t value_max_size = curr_len - _z_ptr_char_diff(p_value_start, start);
            const char *p_value_end = memchr(p_key_end, INT_STR_MAP_LIST_SEPARATOR, value_max_size);

            size_t p_value_len = 0;
            if (p_value_end == NULL) {
                p_value_end = end;
                p_value_len = value_max_size + 1;
            } else {
                p_value_len = _z_ptr_char_diff(p_value_end, p_value_start) + 1;
            }
            char *p_value = (char *)z_malloc(p_value_len);
            if (p_value != NULL) {
                _z_str_n_copy(p_value, p_value_start, p_value_len);
                _z_str_intmap_insert(strint, key, p_value);

                // Process next key value
                start = _z_cptr_char_offset(p_value_end, 1);
                curr_len = n - _z_ptr_char_diff(start, s);
            } else {
                _Z_ERROR_LOG(_Z_ERR_SYSTEM_OUT_OF_MEMORY);
                ret = _Z_ERR_SYSTEM_OUT_OF_MEMORY;
            }
        }
    }

    return ret;
}

z_result_t _z_str_intmap_from_str(_z_str_intmap_t *strint, const char *s, uint8_t argc, _z_str_intmapping_t argv[]) {
    return _z_str_intmap_from_strn(strint, s, argc, argv, strlen(s));
}

size_t _z_str_intmap_strlen(const _z_str_intmap_t *s, uint8_t argc, _z_str_intmapping_t argv[]) {
    // Calculate the string length to allocate
    size_t len = 0;
    for (size_t i = 0; i < argc; i++) {
        char *v = _z_str_intmap_get(s, argv[i]._key);
        if (v != NULL) {
            if (len != (size_t)0) {
                len = len + (size_t)1;  // List separator
            }
            len = len + strlen(argv[i]._str);  // Key
            len = len + (size_t)1;             // KeyValue separator
            len = len + strlen(v);             // Value
        }
    }

    return len;
}

void _z_str_intmap_onto_str(char *dst, size_t dst_len, const _z_str_intmap_t *s, uint8_t argc,
                            _z_str_intmapping_t argv[]) {
    if (dst == NULL || dst_len == 0) {
        return;
    }

    // Remaining length excluding '\0'
    size_t len = dst_len - (size_t)1;
    dst[0] = '\0';
    for (size_t i = 0; i < argc; i++) {
        char *v = _z_str_intmap_get(s, argv[i]._key);
        if (v != NULL) {
            if (len > (size_t)0 && dst[0] != '\0') {
                _z_str_append(dst, INT_STR_MAP_LIST_SEPARATOR);  // List separator
                len = len - (size_t)1;
            }

            size_t key_len = strnlen(argv[i]._str, len);
            if (len > (size_t)0) {
                size_t n = key_len < len ? key_len : len;
                // Flawfinder: ignore [CWE-120]
                (void)strncat(dst, argv[i]._str, n);  // Key
                len = len - n;
            }

            if (len > (size_t)0) {
                _z_str_append(dst, INT_STR_MAP_KEYVALUE_SEPARATOR);  // KeyValue separator
                len = len - (size_t)1;
            }

            size_t value_len = strnlen(v, len);
            if (len > (size_t)0) {
                size_t n = value_len < len ? value_len : len;
                // Flawfinder: ignore [CWE-120]
                (void)strncat(dst, v, n);  // Value
                len = len - n;
            }
        }
    }
}

char *_z_str_intmap_to_str(const _z_str_intmap_t *s, uint8_t argc, _z_str_intmapping_t argv[]) {
    // Calculate the string length to allocate
    size_t len = _z_str_intmap_strlen(s, argc, argv) + (size_t)1;
    // Build the string
    char *dst = (char *)z_malloc(len);
    if (dst != NULL) {
        _z_str_intmap_onto_str(dst, len, s, argc, argv);
    }
    return dst;
}
