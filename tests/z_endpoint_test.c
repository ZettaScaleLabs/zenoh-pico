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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "zenoh-pico/collections/string.h"
#include "zenoh-pico/link/config/udp.h"
#include "zenoh-pico/link/endpoint.h"
#include "zenoh-pico/utils/result.h"
#if Z_FEATURE_LINK_WS == 1
#include "zenoh-pico/link/config/ws.h"
#endif
#if Z_FEATURE_LINK_TLS == 1
#include "zenoh-pico/link/config/tls.h"
#endif

#undef NDEBUG
#include <assert.h>

int main(void) {
    // Locator
    printf(">>> Testing locators...\n");

    _z_locator_t lc;

    _z_string_t str = _z_string_alias_str("tcp/127.0.0.1:7447");
    assert(_z_locator_from_string(&lc, &str) == _Z_RES_OK);

    str = _z_string_alias_str("tcp");
    assert(_z_string_equals(&lc._protocol, &str) == true);
    str = _z_string_alias_str("127.0.0.1:7447");
    assert(_z_string_equals(&lc._address, &str) == true);
    // @TODO: assert protocol-level metadata once it is implemented
    _z_locator_clear(&lc);

    str = _z_string_alias_str("");
    assert(_z_locator_from_string(&lc, &str) == _Z_ERR_CONFIG_LOCATOR_INVALID);
    _z_locator_clear(&lc);

    str = _z_string_alias_str("/");
    assert(_z_locator_from_string(&lc, &str) == _Z_ERR_CONFIG_LOCATOR_INVALID);
    _z_locator_clear(&lc);

    str = _z_string_alias_str("tcp");
    assert(_z_locator_from_string(&lc, &str) == _Z_ERR_CONFIG_LOCATOR_INVALID);
    _z_locator_clear(&lc);

    str = _z_string_alias_str("tcp/");
    assert(_z_locator_from_string(&lc, &str) == _Z_ERR_CONFIG_LOCATOR_INVALID);
    _z_locator_clear(&lc);

    str = _z_string_alias_str("127.0.0.1:7447");
    assert(_z_locator_from_string(&lc, &str) == _Z_ERR_CONFIG_LOCATOR_INVALID);
    _z_locator_clear(&lc);

    str = _z_string_alias_str("tcp/127.0.0.1:7447?");
    assert(_z_locator_from_string(&lc, &str) == _Z_RES_OK);
    _z_locator_clear(&lc);

    // No metadata defined so far... but this is a valid syntax in principle
    str = _z_string_alias_str("tcp/127.0.0.1:7447?invalid=ctrl");
    assert(_z_locator_from_string(&lc, &str) == _Z_RES_OK);
    _z_locator_clear(&lc);

    // Endpoint
    printf(">>> Testing endpoints...\n");

    _z_endpoint_t ep;

    str = _z_string_alias_str("tcp/127.0.0.1:7447");
    assert(_z_endpoint_from_string(&ep, &str) == _Z_RES_OK);

    str = _z_string_alias_str("tcp");
    assert(_z_string_equals(&ep._locator._protocol, &str) == true);
    str = _z_string_alias_str("127.0.0.1:7447");
    assert(_z_string_equals(&ep._locator._address, &str) == true);
    // @TODO: assert protocol-level metadata once it is implemented
    assert(_z_endpoint_config_is_none(&ep._config) == true);
    _z_endpoint_clear(&ep);

    str = _z_string_alias_str("");
    assert(_z_endpoint_from_string(&ep, &str) == _Z_ERR_CONFIG_LOCATOR_INVALID);
    _z_endpoint_clear(&ep);

    str = _z_string_alias_str("/");
    assert(_z_endpoint_from_string(&ep, &str) == _Z_ERR_CONFIG_LOCATOR_INVALID);
    _z_endpoint_clear(&ep);

    str = _z_string_alias_str("tcp");
    assert(_z_endpoint_from_string(&ep, &str) == _Z_ERR_CONFIG_LOCATOR_INVALID);
    _z_endpoint_clear(&ep);

    str = _z_string_alias_str("tcp/");
    assert(_z_endpoint_from_string(&ep, &str) == _Z_ERR_CONFIG_LOCATOR_INVALID);
    _z_endpoint_clear(&ep);

    str = _z_string_alias_str("127.0.0.1:7447");
    assert(_z_endpoint_from_string(&ep, &str) == _Z_ERR_CONFIG_LOCATOR_INVALID);
    _z_endpoint_clear(&ep);

    str = _z_string_alias_str("tcp/127.0.0.1:7447?");
    assert(_z_endpoint_from_string(&ep, &str) == _Z_RES_OK);
    _z_endpoint_clear(&ep);

    // No metadata defined so far... but this is a valid syntax in principle
    str = _z_string_alias_str("tcp/127.0.0.1:7447?invalid=ctrl");
    assert(_z_endpoint_from_string(&ep, &str) == _Z_RES_OK);
    _z_endpoint_clear(&ep);

    str = _z_string_alias_str("udp/127.0.0.1:7447#iface=eth0");
    assert(_z_endpoint_from_string(&ep, &str) == _Z_RES_OK);

    str = _z_string_alias_str("udp");
    assert(_z_string_equals(&ep._locator._protocol, &str) == true);
    str = _z_string_alias_str("127.0.0.1:7447");
    assert(_z_string_equals(&ep._locator._address, &str) == true);
    // @TODO: assert protocol-level metadata once it is implemented
    assert(_z_endpoint_config_is_udp(&ep._config) == true);
    const _z_udp_config_t *udp_cfg = _z_endpoint_config_get_udp(&ep._config);
    assert(_z_string_check(&udp_cfg->_iface) == true);
    assert(_z_str_eq(_z_string_data(&udp_cfg->_iface), "eth0") == true);
    _z_endpoint_clear(&ep);

    str = _z_string_alias_str("udp/127.0.0.1:7447#invalid=eth0");
    assert(_z_endpoint_from_string(&ep, &str) == _Z_RES_OK);
    _z_endpoint_clear(&ep);

    str = _z_string_alias_str("udp/127.0.0.1:7447?invalid=ctrl#iface=eth0");
    assert(_z_endpoint_from_string(&ep, &str) == _Z_RES_OK);
    _z_endpoint_clear(&ep);

    str = _z_string_alias_str("udp/127.0.0.1:7447?invalid=ctrl#invalid=eth0");
    assert(_z_endpoint_from_string(&ep, &str) == _Z_RES_OK);
    _z_endpoint_clear(&ep);

#if Z_FEATURE_LINK_WS == 1
    str = _z_string_alias_str("ws/localhost:7447#tout=1000");
    assert(_z_endpoint_from_string(&ep, &str) == _Z_RES_OK);

    str = _z_string_alias_str("ws");
    assert(_z_string_equals(&ep._locator._protocol, &str) == true);
    str = _z_string_alias_str("localhost:7447");
    assert(_z_string_equals(&ep._locator._address, &str) == true);
    assert(_z_endpoint_config_is_ws(&ep._config) == true);
    const _z_ws_config_t *ws_cfg = _z_endpoint_config_get_ws(&ep._config);
    assert(ws_cfg->_tout == 1000);
    _z_endpoint_clear(&ep);

    str = _z_string_alias_str("ws/[::1]:7447");
    assert(_z_endpoint_from_string(&ep, &str) == _Z_RES_OK);
    _z_endpoint_clear(&ep);

    str = _z_string_alias_str("ws/");
    assert(_z_endpoint_from_string(&ep, &str) == _Z_ERR_CONFIG_LOCATOR_INVALID);
    _z_endpoint_clear(&ep);
#endif

#if Z_FEATURE_LINK_TLS == 1
    str = _z_string_alias_str("tls/localhost:7447#root_ca_certificate=/path/ca.pem");
    assert(_z_endpoint_from_string(&ep, &str) == _Z_RES_OK);

    str = _z_string_alias_str("tls");
    assert(_z_string_equals(&ep._locator._protocol, &str) == true);
    str = _z_string_alias_str("localhost:7447");
    assert(_z_string_equals(&ep._locator._address, &str) == true);
    assert(_z_endpoint_config_is_tls(&ep._config) == true);
    const _z_tls_config_t *tls_cfg = _z_endpoint_config_get_tls(&ep._config);
    assert(_z_tls_cert_source_is_path(&tls_cfg->_root_ca_certificate) == true);
    const _z_string_t *ca_path = _z_tls_cert_source_get_path((_z_tls_cert_source_t *)&tls_cfg->_root_ca_certificate);
    assert(_z_str_eq(_z_string_data(ca_path), "/path/ca.pem") == true);
    _z_endpoint_clear(&ep);

    str = _z_string_alias_str("tls/[::1]:7447");
    assert(_z_endpoint_from_string(&ep, &str) == _Z_RES_OK);
    _z_endpoint_clear(&ep);

    str = _z_string_alias_str("tls/localhost:7447#invalid=value");
    assert(_z_endpoint_from_string(&ep, &str) == _Z_RES_OK);
    _z_endpoint_clear(&ep);

    str = _z_string_alias_str("tls/");
    assert(_z_endpoint_from_string(&ep, &str) == _Z_ERR_CONFIG_LOCATOR_INVALID);
    _z_endpoint_clear(&ep);

    str = _z_string_alias_str("tls");
    assert(_z_endpoint_from_string(&ep, &str) == _Z_ERR_CONFIG_LOCATOR_INVALID);
    _z_endpoint_clear(&ep);
#endif

    return 0;
}
