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

#ifndef ZENOH_PICO_UTILS_PROPERTY_H
#define ZENOH_PICO_UTILS_PROPERTY_H

#include <stdbool.h>
#include <stdint.h>

#include "zenoh-pico/api/constants.h"
#include "zenoh-pico/collections/string.h"
#include "zenoh-pico/config.h"
#include "zenoh-pico/protocol/core.h"
#include "zenoh-pico/utils/result.h"

#ifdef __cplusplus
extern "C" {
#endif

// Fixed-capacity vector of connect locator strings. A session can connect to at
// most `Z_MAX_NUM_PEERS` peers, so the connect locators are stored in a static
// vector of that capacity. Configuration strings are non-owning pointers to
// caller-provided storage (typically compile-time string constants), so the
// elements are never freed by the vector.
typedef const char *_z_config_str_t;
#define _ZP_STATIC_VECTOR_TEMPLATE_ELEM_TYPE _z_config_str_t
#define _ZP_STATIC_VECTOR_TEMPLATE_NAME _z_config_connect
#define _ZP_STATIC_VECTOR_TEMPLATE_SIZE Z_MAX_NUM_PEERS
#include "zenoh-pico/collections/static_vector_template.h"

typedef struct {
    const char *_str;
    bool _parsed;
} _z_config_bool_t;

// Returns the parsed boolean value of a boolean configuration property, or
// `default_val` when the property is not set. Boolean properties are validated
// at insert time, so a property that is set is always `TRUE` or `FALSE`.
static inline bool _z_config_bool_get_or_default(_z_config_bool_t field, bool default_val) {
    return (field._str == NULL) ? default_val : field._parsed;
}

typedef struct {
    const char *_str;
    int32_t _parsed;
} _z_config_int_t;

typedef struct {
    const char *_str;
    z_whatami_t _parsed;
} _z_config_mode_t;

typedef struct {
    const char *_str;
    z_what_t _parsed;
} _z_config_what_t;

typedef struct {
    const char *_str;
    _z_id_t _parsed;
} _z_config_zid_t;

/**
 * Zenoh-net configuration represented as a struct with one dedicated field per property.
 *
 * Plain string properties are stored as non-owning pointers to a null-terminated C
 * string (typically a compile-time constant) and are `NULL` when the corresponding
 * property is not set. Typed properties (boolean, integer, mode, what, zid) keep the
 * original string in their `_str` member (also non-owning, `NULL` when unset) alongside
 * the parsed value, which is validated at insert time and pre-filled with its default
 * by `_z_config_init`. The configuration does not take ownership of the strings, so no
 * copying or freeing is performed. The `_connect` property accepts multiple values and
 * is therefore stored as a fixed-capacity vector of strings.
 */
typedef struct {
    // Session properties
    _z_config_mode_t _mode;
    _z_config_connect_t _connect;
    const char *_listen;
    const char *_user;
    const char *_password;
    _z_config_bool_t _multicast_scouting;
    const char *_multicast_locator;
    _z_config_int_t _scouting_timeout;
    _z_config_what_t _scouting_what;
    _z_config_zid_t _session_zid;
    _z_config_bool_t _add_timestamp;
    // TLS properties
    const char *_tls_root_ca_certificate;
    const char *_tls_root_ca_certificate_base64;
    const char *_tls_listen_private_key;
    const char *_tls_listen_private_key_base64;
    const char *_tls_listen_certificate;
    const char *_tls_listen_certificate_base64;
    _z_config_bool_t _tls_enable_mtls;
    const char *_tls_connect_private_key;
    const char *_tls_connect_private_key_base64;
    const char *_tls_connect_certificate;
    const char *_tls_connect_certificate_base64;
    _z_config_bool_t _tls_verify_name_on_connect;
    // Connect/listen behaviour properties
    _z_config_int_t _connect_timeout;
    _z_config_bool_t _connect_exit_on_failure;
    _z_config_int_t _listen_timeout;
    _z_config_bool_t _listen_exit_on_failure;
} _z_config_t;

/**
 * Initialize a new empty configuration.
 */
z_result_t _z_config_init(_z_config_t *ps);

/**
 * Insert a property with a given key into the configuration.
 * If a property with the same key already exists, it is replaced.
 *
 * The configuration stores @p value as a non-owning pointer, so the caller must
 * ensure the referenced string outlives the configuration (typically a
 * compile-time string constant).
 *
 * Parameters:
 *   ps: A pointer to the configuration.
 *   key: The key of the property to add.
 *   value: The value of the property to add.
 */
z_result_t _zp_config_insert(_z_config_t *ps, uint8_t key, const char *value);

/**
 * Get the property with the given key from the configuration.
 *
 * Parameters:
 *     ps: A pointer to the configuration.
 *     key: The key of the property.
 *
 * Returns:
 *     The non-owning value of the property with key ``key`` in configuration ``ps``.
 */
const char *_z_config_get(const _z_config_t *ps, uint8_t key);
z_result_t _z_config_get_all(const _z_config_t *ps, _z_string_svec_t *locators, uint8_t key);

/**
 * Get the number of properties that are set in the given configuration.
 *
 * Parameters:
 *     ps: A pointer to the configuration.
 *
 * Returns:
 *     The number of set properties.
 */
size_t _z_config_len(const _z_config_t *ps);

/**
 * Clone a config.
 *
 * Parameters:
 *     dst: A pointer to the configuration to initialize with the clone.
 *     src: A pointer to the configuration to clone.
 */
z_result_t _z_config_copy(_z_config_t *dst, const _z_config_t *src);
_z_config_t _z_config_clone(const _z_config_t *src);

/**
 * Check if the given configuration has no property set.
 *
 * Parameters:
 *     ps: A pointer to the configuration.
 *
 * Returns:
 *     A boolean to indicate if properties are present.
 */
bool _z_config_is_empty(const _z_config_t *ps);

/**
 * Move a configuration, leaving the source empty.
 */
z_result_t _z_config_move(_z_config_t *dst, _z_config_t *src);

/**
 * Clear a configuration, resetting it to an empty state.
 *
 * Configuration strings are non-owning, so no memory is released.
 *
 * Parameters:
 *   ps: A pointer to the configuration.
 */
void _z_config_clear(_z_config_t *ps);

/**
 * Free a configuration and the pointer holding it.
 *
 * Parameters:
 *   ps: A pointer to a pointer of the configuration.
 */
void _z_config_free(_z_config_t **ps);

#ifdef __cplusplus
}
#endif

#endif /* ZENOH_PICO_UTILS_PROPERTY_H */
