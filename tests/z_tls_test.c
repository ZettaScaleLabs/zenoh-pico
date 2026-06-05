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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "zenoh-pico/collections/string.h"
#include "zenoh-pico/link/config/tls.h"
#include "zenoh-pico/utils/config.h"
#include "zenoh-pico/utils/result.h"

#undef NDEBUG
#include <assert.h>

int main(void) {
#if Z_FEATURE_LINK_TLS == 1
    printf(">>> Testing TLS config parsing...\n");

    _z_tls_config_t config;
    const char *s = "root_ca_certificate=/etc/ssl/ca.pem";
    z_result_t res = _z_tls_config_typed_from_strn(&config, s, strlen(s));
    assert(res == _Z_RES_OK);
    assert(_z_tls_cert_source_is_path(&config._root_ca_certificate));
    const _z_string_t *ca_cert = _z_tls_cert_source_get_path(&config._root_ca_certificate);
    assert(_z_string_check(ca_cert));
    assert(strncmp(_z_string_data(ca_cert), "/etc/ssl/ca.pem", _z_string_len(ca_cert)) == 0);
    _z_tls_config_clear(&config);

    s = "root_ca_certificate=ca.pem;enable_mtls=1;connect_private_key=client.key;"
        "connect_certificate=client.pem;verify_name_on_connect=0;"
        "root_ca_certificate_base64=BASE64_CA;connect_private_key_base64=BASE64_KEY;"
        "connect_certificate_base64=BASE64_CERT";
    res = _z_tls_config_typed_from_strn(&config, s, strlen(s));
    assert(res == _Z_RES_OK);

    // root_ca_certificate appears as both path and base64; last one wins (base64).
    assert(_z_tls_cert_source_is_base64(&config._root_ca_certificate));
    const _z_string_t *ca_base64 = _z_tls_cert_source_get_base64(&config._root_ca_certificate);
    assert(strncmp(_z_string_data(ca_base64), "BASE64_CA", _z_string_len(ca_base64)) == 0);

    assert(config._has_enable_mtls);
    assert(config._enable_mtls == true);

    // connect_private_key appears as both path and base64; last one wins (base64).
    assert(_z_tls_cert_source_is_base64(&config._connect_private_key));
    const _z_string_t *client_key_inline = _z_tls_cert_source_get_base64(&config._connect_private_key);
    assert(strncmp(_z_string_data(client_key_inline), "BASE64_KEY", _z_string_len(client_key_inline)) == 0);

    // connect_certificate appears as both path and base64; last one wins (base64).
    assert(_z_tls_cert_source_is_base64(&config._connect_certificate));
    const _z_string_t *client_cert = _z_tls_cert_source_get_base64(&config._connect_certificate);
    assert(strncmp(_z_string_data(client_cert), "BASE64_CERT", _z_string_len(client_cert)) == 0);

    assert(config._has_verify_name_on_connect);
    assert(config._verify_name_on_connect == false);
    _z_tls_config_clear(&config);

    res = _z_tls_config_typed_from_strn(&config, "", 0);
    assert(res == _Z_RES_OK);
    assert(!_z_tls_cert_source_is_path(&config._root_ca_certificate));
    assert(!_z_tls_cert_source_is_base64(&config._root_ca_certificate));
    _z_tls_config_clear(&config);

    _z_config_t session_cfg;
    _z_config_init(&session_cfg);
    assert(_zp_config_insert(&session_cfg, Z_CONFIG_TLS_ROOT_CA_CERTIFICATE_KEY, "/session/ca.pem") == _Z_RES_OK);
    assert(_zp_config_insert(&session_cfg, Z_CONFIG_TLS_ROOT_CA_CERTIFICATE_BASE64_KEY, "SESSION_CA") == _Z_RES_OK);
    assert(_zp_config_insert(&session_cfg, Z_CONFIG_TLS_ENABLE_MTLS_KEY, "true") == _Z_RES_OK);
    assert(_zp_config_insert(&session_cfg, Z_CONFIG_TLS_CONNECT_CERTIFICATE_BASE64_KEY, "SESSION_CERT") == _Z_RES_OK);
    assert(_z_str_eq(_z_config_get(&session_cfg, Z_CONFIG_TLS_ROOT_CA_CERTIFICATE_KEY), "/session/ca.pem"));
    assert(_z_str_eq(_z_config_get(&session_cfg, Z_CONFIG_TLS_ENABLE_MTLS_KEY), "true"));
    _z_config_clear(&session_cfg);
#else
    printf("TLS feature not enabled, skipping tests\n");
#endif

    return 0;
}
