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

#include <stddef.h>
#include <string.h>

#include "zenoh-pico/config.h"
#include "zenoh-pico/link/link.h"
#include "zenoh-pico/link/manager.h"
#include "zenoh-pico/link/transport/tcp.h"
#include "zenoh-pico/link/transport/tls_stream.h"
#include "zenoh-pico/utils/config.h"
#include "zenoh-pico/utils/logging.h"
#include "zenoh-pico/utils/string.h"

#if Z_FEATURE_LINK_TLS == 1

uint16_t _z_get_link_mtu_tls(void) { return 65535; }

z_result_t _z_endpoint_tls_valid(_z_endpoint_t *endpoint) {
    _z_string_t tls_str = _z_string_alias_str(TLS_SCHEMA);
    if (!_z_string_equals(&endpoint->_locator._protocol, &tls_str)) {
        _Z_ERROR_LOG(_Z_ERR_CONFIG_LOCATOR_INVALID);
        return _Z_ERR_CONFIG_LOCATOR_INVALID;
    }

    z_result_t ret = _z_tcp_address_valid(&endpoint->_locator._address);
    if (ret != _Z_RES_OK) {
        _Z_ERROR_LOG(_Z_ERR_CONFIG_LOCATOR_INVALID);
    }
    return ret;
}

// If `cred` is unset, take its value from the session config: prefer the path
// form, falling back to the base64 form.
static void _z_tls_cred_from_session(_z_tls_cert_source_t *cred, const _z_config_t *session_cfg, uint8_t path_key,
                                     uint8_t base64_key) {
    if (_z_tls_cert_source_is_path(cred) || _z_tls_cert_source_is_base64(cred)) {
        return;
    }
    const char *path_val = _z_config_get(session_cfg, path_key);
    if (path_val != NULL) {
        _z_string_t s = _z_string_copy_from_str(path_val);
        *cred = _z_tls_cert_source_from_path(&s);
        return;
    }
    const char *b64_val = _z_config_get(session_cfg, base64_key);
    if (b64_val != NULL) {
        _z_string_t s = _z_string_copy_from_str(b64_val);
        *cred = _z_tls_cert_source_from_base64(&s);
    }
}

static bool _z_tls_session_opt_is_true(const char *val) {
    if (val == NULL) {
        return true;
    }
    char c = val[0];
    return !(c == '0' || c == 'n' || c == 'N' || c == 'f' || c == 'F');
}

// Fill any field not already set in the typed endpoint TLS config from the
// session config. Endpoint-provided values take precedence. The endpoint config
// is upgraded to the `tls` alternative if it was not already.
static void _z_tls_apply_session_config(_z_endpoint_config_t *endpoint_cfg, const _z_config_t *session_cfg) {
    if (session_cfg == NULL) {
        return;
    }
    if (!_z_endpoint_config_is_tls(endpoint_cfg)) {
        _z_endpoint_config_destroy(endpoint_cfg);
        _z_tls_config_t empty;
        memset(&empty, 0, sizeof(empty));
        empty._enable_mtls = false;
        empty._verify_name_on_connect = true;
        *endpoint_cfg = _z_endpoint_config_from_tls(&empty);
    }
    _z_tls_config_t *t = _z_endpoint_config_get_tls(endpoint_cfg);

    _z_tls_cred_from_session(&t->_root_ca_certificate, session_cfg, Z_CONFIG_TLS_ROOT_CA_CERTIFICATE_KEY,
                             Z_CONFIG_TLS_ROOT_CA_CERTIFICATE_BASE64_KEY);
    _z_tls_cred_from_session(&t->_listen_private_key, session_cfg, Z_CONFIG_TLS_LISTEN_PRIVATE_KEY_KEY,
                             Z_CONFIG_TLS_LISTEN_PRIVATE_KEY_BASE64_KEY);
    _z_tls_cred_from_session(&t->_listen_certificate, session_cfg, Z_CONFIG_TLS_LISTEN_CERTIFICATE_KEY,
                             Z_CONFIG_TLS_LISTEN_CERTIFICATE_BASE64_KEY);
    _z_tls_cred_from_session(&t->_connect_private_key, session_cfg, Z_CONFIG_TLS_CONNECT_PRIVATE_KEY_KEY,
                             Z_CONFIG_TLS_CONNECT_PRIVATE_KEY_BASE64_KEY);
    _z_tls_cred_from_session(&t->_connect_certificate, session_cfg, Z_CONFIG_TLS_CONNECT_CERTIFICATE_KEY,
                             Z_CONFIG_TLS_CONNECT_CERTIFICATE_BASE64_KEY);
    if (!t->_has_enable_mtls) {
        const char *value = _z_config_get(session_cfg, Z_CONFIG_TLS_ENABLE_MTLS_KEY);
        if (value != NULL) {
            t->_enable_mtls = _z_tls_session_opt_is_true(value);
            t->_has_enable_mtls = true;
        }
    }
    if (!t->_has_verify_name_on_connect) {
        const char *value = _z_config_get(session_cfg, Z_CONFIG_TLS_VERIFY_NAME_ON_CONNECT_KEY);
        if (value != NULL) {
            t->_verify_name_on_connect = _z_tls_session_opt_is_true(value);
            t->_has_verify_name_on_connect = true;
        }
    }
}

static z_result_t _z_f_link_open_tls(_z_link_t *self) {
    z_result_t ret = _Z_RES_OK;

    char *hostname = _z_tcp_address_parse_host(&self->_endpoint._locator._address);
    if (hostname == NULL) {
        _Z_ERROR("Failed to parse TLS endpoint address");
        z_free(hostname);
        return _Z_ERR_GENERIC;
    }

    _z_sys_net_endpoint_t rep = {0};
    ret = _z_tcp_endpoint_init_from_address(&rep, &self->_endpoint._locator._address);
    if (ret != _Z_RES_OK) {
        z_free(hostname);
        return ret;
    }

    _z_tls_config_t *cfg = _z_endpoint_config_get_tls(&self->_endpoint._config);
    ret = _z_open_tls(&self->_socket._tls, &rep, hostname, cfg, false);
    _z_tcp_endpoint_clear(&rep);
    z_free(hostname);
    if (ret != _Z_RES_OK) {
        _Z_ERROR("TLS open failed");
    }
    return ret;
}

static z_result_t _z_f_link_listen_tls(_z_link_t *self) {
    z_result_t ret = _Z_RES_OK;

    char *host = _z_tcp_address_parse_host(&self->_endpoint._locator._address);
    if (host == NULL) {
        _Z_ERROR("Invalid TLS endpoint");
        z_free(host);
        return _Z_ERR_GENERIC;
    }

    _z_sys_net_endpoint_t rep = {0};
    ret = _z_tcp_endpoint_init_from_address(&rep, &self->_endpoint._locator._address);
    if (ret != _Z_RES_OK) {
        z_free(host);
        return ret;
    }

    _z_tls_config_t *cfg_map = _z_endpoint_config_get_tls(&self->_endpoint._config);
    ret = _z_listen_tls(&self->_socket._tls, &rep, cfg_map);
    _z_tcp_endpoint_clear(&rep);
    if (ret != _Z_RES_OK) {
        _Z_ERROR("TLS listen failed");
    }

    z_free(host);
    return ret;
}

static void _z_f_link_close_tls(_z_link_t *self) { _z_close_tls(&self->_socket._tls); }

static size_t _z_f_link_write_tls(const _z_link_t *self, const uint8_t *ptr, size_t len, _z_sys_net_socket_t *socket) {
    // Use provided socket if available, otherwise fall back to link socket
    if (socket != NULL && socket->_tls_sock != NULL) {
        return _z_write_tls((_z_tls_socket_t *)socket->_tls_sock, ptr, len);
    } else {
        return _z_write_tls(&self->_socket._tls, ptr, len);
    }
}

static size_t _z_f_link_write_all_tls(const _z_link_t *self, const uint8_t *ptr, size_t len) {
    return _z_write_all_tls(&self->_socket._tls, ptr, len);
}

static size_t _z_f_link_read_tls(const _z_link_t *self, uint8_t *ptr, size_t len, _z_slice_t *addr) {
    _ZP_UNUSED(addr);
    return _z_read_tls(&self->_socket._tls, ptr, len);
}

static size_t _z_f_link_read_exact_tls(const _z_link_t *self, uint8_t *ptr, size_t len, _z_slice_t *addr,
                                       _z_sys_net_socket_t *socket) {
    _ZP_UNUSED(addr);

    size_t n = (size_t)0;
    do {
        size_t rb;
        if (socket != NULL && socket->_tls_sock != NULL) {
            rb = _z_read_tls((_z_tls_socket_t *)socket->_tls_sock, &ptr[n], len - n);
        } else {
            rb = _z_read_tls(&self->_socket._tls, &ptr[n], len - n);
        }

        if (rb == SIZE_MAX) {
            n = rb;
            break;
        }
        n += rb;
    } while (n != len);

    return n;
}

static size_t _z_f_link_tls_read_socket(const _z_sys_net_socket_t socket, uint8_t *ptr, size_t len) {
    if (socket._tls_sock == NULL) {
        _Z_ERROR("TLS context not found in socket");
        return SIZE_MAX;
    }
    return _z_read_tls((_z_tls_socket_t *)socket._tls_sock, ptr, len);
}

static void _z_f_link_free_tls(_z_link_t *self) { _ZP_UNUSED(self); }

z_result_t _z_new_link_tls(_z_link_t *zl, _z_endpoint_t *endpoint, const _z_config_t *session_cfg) {
    zl->_type = _Z_LINK_TYPE_TLS;
    zl->_cap._transport = Z_LINK_CAP_TRANSPORT_UNICAST;
    zl->_cap._flow = Z_LINK_CAP_FLOW_STREAM;
    zl->_cap._is_reliable = true;

    zl->_mtu = _z_get_link_mtu_tls();

    // Move the endpoint into the link, then merge any session-provided TLS
    // options into its typed config (endpoint values take precedence).
    zl->_endpoint = *endpoint;
    _z_endpoint_init(endpoint);
    _z_tls_apply_session_config(&zl->_endpoint._config, session_cfg);
    _Z_DEBUG("TLS locator: '%.*s'", (int)_z_string_len(&zl->_endpoint._locator._address),
             _z_string_data(&zl->_endpoint._locator._address));

    zl->_open_f = _z_f_link_open_tls;
    zl->_listen_f = _z_f_link_listen_tls;
    zl->_close_f = _z_f_link_close_tls;
    zl->_write_f = _z_f_link_write_tls;
    zl->_write_all_f = _z_f_link_write_all_tls;
    zl->_read_f = _z_f_link_read_tls;
    zl->_read_exact_f = _z_f_link_read_exact_tls;
    zl->_read_socket_f = _z_f_link_tls_read_socket;
    zl->_free_f = _z_f_link_free_tls;

    return _Z_RES_OK;
}

z_result_t _z_new_peer_tls(_z_endpoint_t *endpoint, _z_sys_net_socket_t *socket, const _z_config_t *session_cfg) {
    _z_sys_net_endpoint_t sys_endpoint = {0};
    char *hostname = _z_tcp_address_parse_host(&endpoint->_locator._address);
    z_result_t ret = _Z_RES_OK;
    if (hostname == NULL) {
        ret = _Z_ERR_CONFIG_LOCATOR_INVALID;
        goto cleanup;
    }

    ret = _z_tcp_endpoint_init_from_address(&sys_endpoint, &endpoint->_locator._address);
    if (ret != _Z_RES_OK) {
        goto cleanup;
    }

    socket->_tls_sock = z_malloc(sizeof(_z_tls_socket_t));
    if (socket->_tls_sock == NULL) {
        ret = _Z_ERR_SYSTEM_OUT_OF_MEMORY;
        goto cleanup;
    }

    _z_tls_apply_session_config(&endpoint->_config, session_cfg);
    _z_tls_config_t *cfg = _z_endpoint_config_get_tls(&endpoint->_config);
    ret = _z_open_tls((_z_tls_socket_t *)socket->_tls_sock, &sys_endpoint, hostname, cfg, true);
    if (ret != _Z_RES_OK) {
        z_free(socket->_tls_sock);
        socket->_tls_sock = NULL;
        _z_endpoint_config_destroy(&endpoint->_config);
        goto cleanup;
    }

    socket->_fd = ((_z_tls_socket_t *)socket->_tls_sock)->_sock._fd;
    _z_endpoint_config_destroy(&endpoint->_config);

cleanup:
    z_free(hostname);
    _z_tcp_endpoint_clear(&sys_endpoint);
    return ret;
}

#endif  // Z_FEATURE_LINK_TLS == 1
