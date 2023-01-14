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

#include "zenoh-pico/link/config/lora.h"

#include <stddef.h>
#include <string.h>

#include "zenoh-pico/config.h"
#include "zenoh-pico/link/manager.h"
#include "zenoh-pico/system/link/bt.h"

#if Z_LINK_LORA == 1

#define SPP_MAXIMUM_PAYLOAD 128

int8_t _z_f_link_open_lora(_z_link_t *self) {
    int8_t ret = _Z_RES_OK;

    const char *frequency = _z_str_intmap_get(&self->_endpoint._config, LORA_CONFIG_FREQUENCY_KEY);
    self->_socket._lora._sock = _z_open_lora(frequency);
    if (self->_socket._lora._sock._err == true) {
        ret = -1;
    }

    return ret;
}

int8_t _z_f_link_listen_lora(_z_link_t *self) {
    int8_t ret = _Z_RES_OK;

    const char *frequency = _z_str_intmap_get(&self->_endpoint._config, LORA_CONFIG_FREQUENCY_KEY);
    self->_socket._lora._sock = _z_listen_lora(frequency);
    if (self->_socket._lora._sock._err == true) {
        ret = -1;
    }

    return ret;
}

void _z_f_link_close_lora(_z_link_t *self) { _z_close_lora(self->_socket._lora._sock); }

void _z_f_link_free_lora(_z_link_t *self) { }

size_t _z_f_link_write_lora(const _z_link_t *self, const uint8_t *ptr, size_t len) {
    return _z_send_lora(self->_socket._lora._sock, ptr, len);
}

size_t _z_f_link_write_all_lora(const _z_link_t *self, const uint8_t *ptr, size_t len) {
    return _z_send_lora(self->_socket._lora._sock, ptr, len);
}

size_t _z_f_link_read_lora(const _z_link_t *self, uint8_t *ptr, size_t len, _z_bytes_t *addr) {
    size_t rb = _z_read_lora(self->_socket._lora._sock, ptr, len);
    if ((rb > (size_t)0) && (addr != NULL)) {
        *addr = _z_bytes_make(strlen("abc"));
        (void)memcpy((uint8_t *)addr->start, "abc", addr->len);
    }

    return rb;
}

size_t _z_f_link_read_exact_lora(const _z_link_t *self, uint8_t *ptr, size_t len, _z_bytes_t *addr) {
    size_t rb = _z_read_exact_lora(self->_socket._lora._sock, ptr, len);
    if ((rb == len) && (addr != NULL)) {
        *addr = _z_bytes_make(strlen("abc"));
        (void)memcpy((uint8_t *)addr->start, "abc", addr->len);
    }

    return rb;
}

uint16_t _z_get_link_mtu_lora(void) { return SPP_MAXIMUM_PAYLOAD; }

_z_link_t *_z_new_link_lora(_z_endpoint_t endpoint) {
    _z_link_t *lt = (_z_link_t *)z_malloc(sizeof(_z_link_t));

    lt->_capabilities = Z_LINK_CAPABILITY_STREAMED | Z_LINK_CAPABILITY_MULTICAST;
    lt->_mtu = _z_get_link_mtu_lora();

    lt->_endpoint = endpoint;
    lt->_socket._lora._sock._err = true;

    lt->_open_f = _z_f_link_open_lora;
    lt->_listen_f = _z_f_link_listen_lora;
    lt->_close_f = _z_f_link_close_lora;
    lt->_free_f = _z_f_link_free_lora;

    lt->_write_f = _z_f_link_write_lora;
    lt->_write_all_f = _z_f_link_write_all_lora;
    lt->_read_f = _z_f_link_read_lora;
    lt->_read_exact_f = _z_f_link_read_exact_lora;

    return lt;
}
#endif
