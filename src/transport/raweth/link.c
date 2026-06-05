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

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "zenoh-pico/config.h"
#include "zenoh-pico/link/config/parser.h"
#include "zenoh-pico/link/config/raweth.h"
#include "zenoh-pico/link/manager.h"
#include "zenoh-pico/link/transport/raweth.h"
#include "zenoh-pico/protocol/codec/core.h"
#include "zenoh-pico/system/platform.h"
#include "zenoh-pico/utils/logging.h"
#include "zenoh-pico/utils/pointers.h"

#if Z_FEATURE_RAWETH_TRANSPORT == 1

#define RAWETH_CFG_TUPLE_SEPARATOR '#'
#define RAWETH_CFG_LIST_SEPARATOR ","

#define RAWETH_CONFIG_IFACE_STR "iface"
#define RAWETH_CONFIG_ETHTYPE_STR "ethtype"
#define RAWETH_CONFIG_MAPPING_STR "mapping"
#define RAWETH_CONFIG_WHITELIST_STR "whitelist"

// Ethtype must be at least 0x600 in network order
#define RAWETH_ETHTYPE_MIN_VALUE 0x600U

const uint16_t _ZP_RAWETH_DEFAULT_ETHTYPE = 0x72e0;
const char *_ZP_RAWETH_DEFAULT_INTERFACE = "lo";
const uint8_t _ZP_RAWETH_DEFAULT_SMAC[_ZP_MAC_ADDR_LENGTH] = {0x30, 0x03, 0xc8, 0x37, 0x25, 0xa1};
const _zp_raweth_mapping_entry_t _ZP_RAWETH_DEFAULT_MAPPING = {
    ._keyexpr = {0}, ._vlan = 0x0000, ._dmac = {0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff}, ._has_vlan = false};

static size_t _z_valid_mapping_raweth(const _z_string_t *cfg_mapping);
static z_result_t _z_get_mapping_raweth(const _z_string_t *cfg_mapping, _zp_raweth_mapping_array_t *array, size_t size);
static size_t _z_valid_whitelist_raweth(const _z_string_t *cfg_whitelist);
static z_result_t _z_get_whitelist_raweth(const _z_string_t *cfg_whitelist, _zp_raweth_whitelist_array_t *array,
                                          size_t size);
static z_result_t _z_get_mapping_entry(char *entry, _zp_raweth_mapping_entry_t *storage);
static bool _z_valid_mapping_entry(char *entry);
static bool _z_valid_address_raweth_inner(const _z_string_t *address);
static bool _z_valid_address_raweth(const char *address);
static uint8_t *_z_parse_address_raweth(const char *address);
static z_result_t _z_f_link_open_raweth(_z_link_t *self);
static z_result_t _z_f_link_listen_raweth(_z_link_t *self);
static void _z_f_link_close_raweth(_z_link_t *self);
static void _z_f_link_free_raweth(_z_link_t *self);
static size_t _z_f_link_write_raweth(const _z_link_t *self, const uint8_t *ptr, size_t len,
                                     _z_sys_net_socket_t *socket);
static size_t _z_f_link_write_all_raweth(const _z_link_t *self, const uint8_t *ptr, size_t len);
static size_t _z_f_link_read_raweth(const _z_link_t *self, uint8_t *ptr, size_t len, _z_slice_t *addr);
static size_t _z_f_link_read_exact_raweth(const _z_link_t *self, uint8_t *ptr, size_t len, _z_slice_t *addr,
                                          _z_sys_net_socket_t *socket);
static uint16_t _z_get_link_mtu_raweth(void);

static size_t _z_valid_mapping_raweth(const _z_string_t *cfg_mapping) {
    // Retrieve list
    if (!_z_string_check(cfg_mapping)) {
        return 0;
    }
    char *s_mapping = _z_str_clone(_z_string_data(cfg_mapping));
    if (s_mapping == NULL) {
        return 0;
    }
    size_t size = 0;
    // Parse list
    const char *delim = RAWETH_CFG_LIST_SEPARATOR;
    char *entry = strtok(s_mapping, delim);
    while (entry != NULL) {
        // Check entry
        if (!_z_valid_mapping_entry(entry)) {
            z_free(s_mapping);
            return 0;
        }
        size++;
        entry = strtok(NULL, delim);
    }
    // Clean up
    z_free(s_mapping);
    return size;
}

static z_result_t _z_get_mapping_raweth(const _z_string_t *cfg_mapping, _zp_raweth_mapping_array_t *array,
                                        size_t size) {
    // Retrieve data
    if (!_z_string_check(cfg_mapping)) {
        _Z_ERROR_RETURN(_Z_ERR_GENERIC);
    }
    // Copy data
    char *s_mapping = _z_str_clone(_z_string_data(cfg_mapping));
    if (s_mapping == NULL) {
        _Z_ERROR_RETURN(_Z_ERR_SYSTEM_OUT_OF_MEMORY);
    }
    // Allocate array
    *array = _zp_raweth_mapping_array_make(size);
    if (_zp_raweth_mapping_array_len(array) == 0) {
        _Z_ERROR_RETURN(_Z_ERR_SYSTEM_OUT_OF_MEMORY);
    }
    size_t idx = 0;
    // Parse list
    const char *delim = RAWETH_CFG_LIST_SEPARATOR;
    char *entry = strtok(s_mapping, delim);
    while ((entry != NULL) && (idx < _zp_raweth_mapping_array_len(array))) {
        // Copy data into array
        _Z_CLEAN_RETURN_IF_ERR(_z_get_mapping_entry(entry, _zp_raweth_mapping_array_get(array, idx)),
                               z_free(s_mapping));
        // Next iteration
        idx++;
        entry = strtok(NULL, delim);
    }
    // Clean up
    z_free(s_mapping);
    return _Z_RES_OK;
}

static size_t _z_valid_whitelist_raweth(const _z_string_t *cfg_whitelist) {
    // Retrieve data
    if (!_z_string_check(cfg_whitelist)) {
        return 0;
    }
    // Copy data
    char *s_whitelist = _z_str_clone(_z_string_data(cfg_whitelist));
    if (s_whitelist == NULL) {
        return 0;
    }
    // Parse list
    size_t size = 0;
    const char *delim = RAWETH_CFG_LIST_SEPARATOR;
    char *entry = strtok(s_whitelist, delim);
    while (entry != NULL) {
        // Check entry
        if (!_z_valid_address_raweth(entry)) {
            z_free(s_whitelist);
            return 0;
        }
        size++;
        entry = strtok(NULL, delim);
    }
    // Parse last entry

    // Clean up
    z_free(s_whitelist);
    return size;
}

static z_result_t _z_get_whitelist_raweth(const _z_string_t *cfg_whitelist, _zp_raweth_whitelist_array_t *array,
                                          size_t size) {
    // Retrieve data
    if (!_z_string_check(cfg_whitelist)) {
        _Z_ERROR_RETURN(_Z_ERR_GENERIC);
    }
    // Copy data
    char *s_whitelist = _z_str_clone(_z_string_data(cfg_whitelist));
    if (s_whitelist == NULL) {
        _Z_ERROR_RETURN(_Z_ERR_SYSTEM_OUT_OF_MEMORY);
    }
    // Allocate array
    *array = _zp_raweth_whitelist_array_make(size);
    if (_zp_raweth_whitelist_array_len(array) == 0) {
        _Z_ERROR_RETURN(_Z_ERR_SYSTEM_OUT_OF_MEMORY);
    }
    size_t idx = 0;
    // Parse list
    const char *delim = RAWETH_CFG_LIST_SEPARATOR;
    char *entry = strtok(s_whitelist, delim);
    while ((entry != NULL) && (idx < _zp_raweth_whitelist_array_len(array))) {
        // Convert address from string to int array
        uint8_t *addr = _z_parse_address_raweth(entry);
        if (addr == NULL) {
            _Z_ERROR_RETURN(_Z_ERR_SYSTEM_OUT_OF_MEMORY);
        }
        // Copy address to entry
        _zp_raweth_whitelist_entry_t *elem = _zp_raweth_whitelist_array_get(array, idx);
        // Flawfinder: ignore [CWE-120] - fixed-size MAC copy, both operands are _ZP_MAC_ADDR_LENGTH bytes.
        memcpy(elem->_mac, addr, _ZP_MAC_ADDR_LENGTH);
        z_free(addr);
        // Next iteration
        idx++;
        entry = strtok(NULL, delim);
    }
    // Clean up
    z_free(s_whitelist);
    return _Z_RES_OK;
}

static z_result_t _z_get_mapping_entry(char *entry, _zp_raweth_mapping_entry_t *storage) {
    // Flawfinder: ignore [CWE-126] - entry is a '\0'-terminated token produced from a cloned config string.
    size_t len = strlen(entry);
    const char *entry_end = &entry[len - (size_t)1];

    // Get first tuple member (keyexpr)
    char *p_start = &entry[0];
    char *p_end = strchr(p_start, RAWETH_CFG_TUPLE_SEPARATOR);
    size_t ke_len = (uintptr_t)p_end - (uintptr_t)p_start;
    storage->_keyexpr = _z_string_copy_from_substr(p_start, ke_len);
    if (!_z_string_check(&storage->_keyexpr)) {
        return _Z_ERR_SYSTEM_OUT_OF_MEMORY;
    }

    // Check second entry (address)
    p_start = p_end;
    p_start++;
    p_end = strchr(p_start, RAWETH_CFG_TUPLE_SEPARATOR);
    *p_end = '\0';
    uint8_t *addr = _z_parse_address_raweth(p_start);
    // Flawfinder: ignore [CWE-120] - fixed-size MAC copy, both operands are _ZP_MAC_ADDR_LENGTH bytes.
    memcpy(storage->_dmac, addr, _ZP_MAC_ADDR_LENGTH);
    z_free(addr);
    *p_end = RAWETH_CFG_TUPLE_SEPARATOR;

    // Check optional third entry (vlan id)
    p_start = p_end;
    p_start++;
    if (p_start >= entry_end) {  // No entry
        storage->_has_vlan = false;
    } else {
        storage->_has_vlan = true;
        storage->_vlan = (uint16_t)strtol(p_start, NULL, 16);
    }
    return _Z_RES_OK;
}
static bool _z_valid_mapping_entry(char *entry) {
    // Flawfinder: ignore [CWE-126] - entry is a '\0'-terminated token produced from a cloned config string.
    size_t len = strlen(entry);
    const char *entry_end = &entry[len - (size_t)1];

    // Check first tuple member (keyexpr)
    char *p_start = &entry[0];
    char *p_end = strchr(p_start, RAWETH_CFG_TUPLE_SEPARATOR);
    if (p_end == NULL) {
        return false;
    }
    // Check second entry (address)
    p_start = p_end;
    p_start++;
    if (p_start > entry_end) {
        return false;
    }
    p_end = strchr(p_start, RAWETH_CFG_TUPLE_SEPARATOR);
    if (p_end == NULL) {
        return false;
    }
    *p_end = '\0';
    if (!_z_valid_address_raweth(p_start)) {
        *p_end = RAWETH_CFG_TUPLE_SEPARATOR;
        return false;
    }
    *p_end = RAWETH_CFG_TUPLE_SEPARATOR;
    return true;
}

static bool _z_valid_address_raweth_inner(const _z_string_t *address) {
    // Check if the string has the correct length
    size_t len = _z_string_len(address);
    const char *str_data = _z_string_data(address);
    if (len != 17) {  // 6 pairs of hexadecimal digits and 5 colons
        return false;
    }
    // Check if the colons are at the correct positions
    for (size_t i = 2; i < len; i += 3) {
        if (str_data[i] != ':') {
            return false;
        }
    }
    // Check if each character is a valid hexadecimal digit
    for (size_t i = 0; i < len; ++i) {
        if (i % 3 != 2) {
            char c = str_data[i];
            if (!((c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f'))) {
                return false;
            }
        }
    }
    return true;
}

static bool _z_valid_address_raweth(const char *address) {
    _z_string_t addr_str = _z_string_alias_str(address);
    return _z_valid_address_raweth_inner(&addr_str);
}

static uint8_t *_z_parse_address_raweth(const char *address) {
    // Allocate data
    uint8_t *ret = (uint8_t *)z_malloc(_ZP_MAC_ADDR_LENGTH);
    if (ret == NULL) {
        return ret;
    }
    for (size_t i = 0; i < _ZP_MAC_ADDR_LENGTH; ++i) {
        // Flawfinder: ignore [CWE-120] - fixed 2-digit hex byte plus explicit terminator.
        char byte_string[3] = {address[i * 3], address[(i * 3) + 1], '\0'};
        ret[i] = (uint8_t)strtol(byte_string, NULL, 16);
    }
    return ret;
}

static z_result_t _z_f_link_open_raweth(_z_link_t *self) {
    // Retrieve typed config (may be absent)
    const _z_raweth_config_t *cfg = NULL;
    if (_z_endpoint_config_is_raweth(&self->_endpoint._config)) {
        cfg = _z_endpoint_config_get_raweth((_z_endpoint_config_t *)&self->_endpoint._config);
    }
    _z_string_t empty_str = _z_string_null();
    const _z_string_t *cfg_iface = (cfg != NULL) ? &cfg->_interface : &empty_str;
    const _z_string_t *cfg_mapping = (cfg != NULL) ? &cfg->_mapping : &empty_str;
    const _z_string_t *cfg_whitelist = (cfg != NULL) ? &cfg->_whitelist : &empty_str;

    // Init arrays
    self->_socket._raweth._mapping = _zp_raweth_mapping_array_empty();
    self->_socket._raweth._whitelist = _zp_raweth_whitelist_array_empty();
    // Init socket smac
    if (_z_valid_address_raweth_inner(&self->_endpoint._locator._address)) {
        uint8_t *addr = _z_parse_address_raweth(_z_string_data(&self->_endpoint._locator._address));
        // Flawfinder: ignore [CWE-120] - fixed-size MAC copy, both operands are _ZP_MAC_ADDR_LENGTH bytes.
        memcpy(self->_socket._raweth._smac, addr, _ZP_MAC_ADDR_LENGTH);
        z_free(addr);
    } else {
        _Z_DEBUG("Invalid locator source mac addr, using default value.");
        // Flawfinder: ignore [CWE-120] - fixed-size MAC copy, both operands are _ZP_MAC_ADDR_LENGTH bytes.
        memcpy(self->_socket._raweth._smac, _ZP_RAWETH_DEFAULT_SMAC, _ZP_MAC_ADDR_LENGTH);
    }
    // Init socket interface
    if (_z_string_check(cfg_iface)) {
        self->_socket._raweth._interface = _z_string_data(cfg_iface);
    } else {
        _Z_DEBUG("Invalid locator interface, using default value %s", _ZP_RAWETH_DEFAULT_INTERFACE);
        self->_socket._raweth._interface = _ZP_RAWETH_DEFAULT_INTERFACE;
    }
    // Init socket ethtype
    if ((cfg != NULL) && (cfg->_ethtype != 0) && (_z_raweth_htons(cfg->_ethtype) > RAWETH_ETHTYPE_MIN_VALUE)) {
        self->_socket._raweth._ethtype = cfg->_ethtype;
    } else {
        _Z_DEBUG("Invalid locator ethtype, using default value 0x%04x", _ZP_RAWETH_DEFAULT_ETHTYPE);
        self->_socket._raweth._ethtype = _ZP_RAWETH_DEFAULT_ETHTYPE;
    }
    // Init socket mapping
    size_t size = _z_valid_mapping_raweth(cfg_mapping);
    if (size != (size_t)0) {
        _Z_RETURN_IF_ERR(_z_get_mapping_raweth(cfg_mapping, &self->_socket._raweth._mapping, size));
    } else {
        _Z_DEBUG("Invalid locator mapping, using default value.");
        self->_socket._raweth._mapping = _zp_raweth_mapping_array_make(1);
        if (_zp_raweth_mapping_array_len(&self->_socket._raweth._mapping) == 0) {
            _Z_ERROR_RETURN(_Z_ERR_SYSTEM_OUT_OF_MEMORY);
        }
        _zp_raweth_mapping_entry_t *entry = _zp_raweth_mapping_array_get(&self->_socket._raweth._mapping, 0);
        *entry = _ZP_RAWETH_DEFAULT_MAPPING;
    }
    // Init socket whitelist
    size = _z_valid_whitelist_raweth(cfg_whitelist);
    if (size != (size_t)0) {
        _Z_RETURN_IF_ERR(_z_get_whitelist_raweth(cfg_whitelist, &self->_socket._raweth._whitelist, size));
    } else {
        _Z_DEBUG("Invalid locator whitelist, filtering deactivated.");
    }
    // Open raweth link
    return _z_open_raweth(&self->_socket._raweth._sock, self->_socket._raweth._interface);
}

static z_result_t _z_f_link_listen_raweth(_z_link_t *self) { return _z_f_link_open_raweth(self); }

static void _z_f_link_close_raweth(_z_link_t *self) {
    // Close connection
    _z_close_raweth(&self->_socket._raweth._sock);
    // Clear config
    _zp_raweth_mapping_array_clear(&self->_socket._raweth._mapping);
    if (_zp_raweth_whitelist_array_len(&self->_socket._raweth._whitelist) != 0) {
        _zp_raweth_whitelist_array_clear(&self->_socket._raweth._whitelist);
    }
}

static void _z_f_link_free_raweth(_z_link_t *self) { _ZP_UNUSED(self); }

static size_t _z_f_link_write_raweth(const _z_link_t *self, const uint8_t *ptr, size_t len,
                                     _z_sys_net_socket_t *socket) {
    _ZP_UNUSED(self);
    _ZP_UNUSED(ptr);
    _ZP_UNUSED(len);
    _ZP_UNUSED(socket);
    return SIZE_MAX;
}

static size_t _z_f_link_write_all_raweth(const _z_link_t *self, const uint8_t *ptr, size_t len) {
    _ZP_UNUSED(self);
    _ZP_UNUSED(ptr);
    _ZP_UNUSED(len);
    return SIZE_MAX;
}

static size_t _z_f_link_read_raweth(const _z_link_t *self, uint8_t *ptr, size_t len, _z_slice_t *addr) {
    _ZP_UNUSED(self);
    _ZP_UNUSED(ptr);
    _ZP_UNUSED(len);
    _ZP_UNUSED(addr);
    return SIZE_MAX;
}

static size_t _z_f_link_read_exact_raweth(const _z_link_t *self, uint8_t *ptr, size_t len, _z_slice_t *addr,
                                          _z_sys_net_socket_t *socket) {
    _ZP_UNUSED(self);
    _ZP_UNUSED(ptr);
    _ZP_UNUSED(len);
    _ZP_UNUSED(addr);
    _ZP_UNUSED(socket);
    return SIZE_MAX;
}

static uint16_t _z_get_link_mtu_raweth(void) { return _ZP_MAX_ETH_FRAME_SIZE; }

z_result_t _z_endpoint_raweth_valid(_z_endpoint_t *endpoint) {
    z_result_t ret = _Z_RES_OK;

    // Check the root
    _z_string_t str_cmp = _z_string_alias_str(RAWETH_SCHEMA);
    if (!_z_string_equals(&endpoint->_locator._protocol, &str_cmp)) {
        _Z_ERROR_LOG(_Z_ERR_CONFIG_LOCATOR_INVALID);
        ret = _Z_ERR_CONFIG_LOCATOR_INVALID;
    }
    return ret;
}

z_result_t _z_new_link_raweth(_z_link_t *zl, _z_endpoint_t endpoint) {
    z_result_t ret = _Z_RES_OK;
    zl->_type = _Z_LINK_TYPE_RAWETH;
    zl->_cap._transport = Z_LINK_CAP_TRANSPORT_RAWETH;
    zl->_cap._is_reliable = false;
    zl->_mtu = _z_get_link_mtu_raweth();

    zl->_endpoint = endpoint;

    // Init socket
    memset(&zl->_socket._raweth, 0, sizeof(zl->_socket._raweth));

    zl->_open_f = _z_f_link_open_raweth;
    zl->_listen_f = _z_f_link_listen_raweth;
    zl->_close_f = _z_f_link_close_raweth;
    zl->_free_f = _z_f_link_free_raweth;

    zl->_write_f = _z_f_link_write_raweth;
    zl->_write_all_f = _z_f_link_write_all_raweth;
    zl->_read_f = _z_f_link_read_raweth;
    zl->_read_exact_f = _z_f_link_read_exact_raweth;
    zl->_read_socket_f = _z_noop_link_read_socket;

    return ret;
}

// ── Typed config (de)serialization (intmap-free) ─────────────────────────────
//
// Only `iface` and `ethtype` are modeled by the typed struct. `mapping` and
// `whitelist` remain handled via the intmap path inside the transport.

// Parse a hexadecimal value (no "0x" prefix, matching the intmap path which
// calls strtol(..., 16)). Returns false on empty/invalid input.
static bool _z_raweth_hex_as_u16(const char *v, size_t n, uint16_t *out) {
    if (n == 0) {
        return false;
    }
    uint32_t acc = 0;
    for (size_t i = 0; i < n; i++) {
        char c = v[i];
        uint32_t digit;
        if (c >= '0' && c <= '9') {
            digit = (uint32_t)(c - '0');
        } else if (c >= 'a' && c <= 'f') {
            digit = (uint32_t)(c - 'a') + 10u;
        } else if (c >= 'A' && c <= 'F') {
            digit = (uint32_t)(c - 'A') + 10u;
        } else {
            return false;
        }
        acc = (acc << 4) | digit;
        if (acc > 0xFFFFu) {
            return false;
        }
    }
    *out = (uint16_t)acc;
    return true;
}

z_result_t _z_raweth_config_typed_from_strn(_z_raweth_config_t *cfg, const char *s, size_t n) {
    memset(cfg, 0, sizeof(*cfg));

    _z_config_iter_t it = _z_config_iter_make(s, n);
    _z_config_kv_t kv;
    while (_z_config_iter_next(&it, &kv)) {
        if (_z_config_kv_key_eq(&kv, RAWETH_CONFIG_IFACE_STR)) {
            _z_string_clear(&cfg->_interface);
            cfg->_interface = _z_string_copy_from_substr(kv._value, kv._value_len);
        } else if (_z_config_kv_key_eq(&kv, RAWETH_CONFIG_ETHTYPE_STR)) {
            uint16_t ethtype = 0;
            if (!_z_raweth_hex_as_u16(kv._value, kv._value_len, &ethtype)) {
                _z_raweth_config_clear(cfg);
                return _Z_ERR_CONFIG_LOCATOR_INVALID;
            }
            cfg->_ethtype = ethtype;
        } else if (_z_config_kv_key_eq(&kv, RAWETH_CONFIG_MAPPING_STR)) {
            _z_string_clear(&cfg->_mapping);
            cfg->_mapping = _z_string_copy_from_substr(kv._value, kv._value_len);
        } else if (_z_config_kv_key_eq(&kv, RAWETH_CONFIG_WHITELIST_STR)) {
            _z_string_clear(&cfg->_whitelist);
            cfg->_whitelist = _z_string_copy_from_substr(kv._value, kv._value_len);
        }
        // unknown keys are ignored.
    }
    return _Z_RES_OK;
}

char *_z_raweth_config_typed_to_str(const _z_raweth_config_t *cfg) {
    _z_config_builder_t b;
    _z_config_builder_init(&b);
    if (_z_string_check(&cfg->_interface)) {
        _z_config_builder_add_substr(&b, RAWETH_CONFIG_IFACE_STR, _z_string_data(&cfg->_interface),
                                     _z_string_len(&cfg->_interface));
    }
    if (cfg->_ethtype != 0) {
        char tmp[5];  // up to 4 hex digits
        int written = snprintf(tmp, sizeof(tmp), "%x", cfg->_ethtype);
        if (written > 0) {
            _z_config_builder_add_substr(&b, RAWETH_CONFIG_ETHTYPE_STR, tmp, (size_t)written);
        }
    }
    if (_z_string_check(&cfg->_mapping)) {
        _z_config_builder_add_substr(&b, RAWETH_CONFIG_MAPPING_STR, _z_string_data(&cfg->_mapping),
                                     _z_string_len(&cfg->_mapping));
    }
    if (_z_string_check(&cfg->_whitelist)) {
        _z_config_builder_add_substr(&b, RAWETH_CONFIG_WHITELIST_STR, _z_string_data(&cfg->_whitelist),
                                     _z_string_len(&cfg->_whitelist));
    }
    return _z_config_builder_take(&b);
}

#else
z_result_t _z_endpoint_raweth_valid(_z_endpoint_t *endpoint) {
    _ZP_UNUSED(endpoint);
    _Z_ERROR_RETURN(_Z_ERR_TRANSPORT_NOT_AVAILABLE);
}

z_result_t _z_new_link_raweth(_z_link_t *zl, _z_endpoint_t endpoint) {
    _ZP_UNUSED(zl);
    _ZP_UNUSED(endpoint);
    _Z_ERROR_RETURN(_Z_ERR_TRANSPORT_NOT_AVAILABLE);
}

z_result_t _z_raweth_config_typed_from_strn(_z_raweth_config_t *cfg, const char *s, size_t n) {
    _ZP_UNUSED(cfg);
    _ZP_UNUSED(s);
    _ZP_UNUSED(n);
    _Z_ERROR_RETURN(_Z_ERR_TRANSPORT_NOT_AVAILABLE);
}

char *_z_raweth_config_typed_to_str(const _z_raweth_config_t *cfg) {
    _ZP_UNUSED(cfg);
    return NULL;
}
#endif
