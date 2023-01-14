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

#ifndef ZENOH_PICO_SYSTEM_LINK_LORA_H
#define ZENOH_PICO_SYSTEM_LINK_LORA_H

#include <stdint.h>

#include "zenoh-pico/collections/string.h"
#include "zenoh-pico/config.h"

#if Z_LINK_LORA == 1

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    _z_sys_net_socket_t _sock;
} _z_lora_socket_t;

_z_sys_net_socket_t _z_open_lora(const char *frequency);
_z_sys_net_socket_t _z_listen_lora(const char *frequency);
void _z_close_lora(_z_sys_net_socket_t sock);
size_t _z_read_exact_lora(_z_sys_net_socket_t sock, uint8_t *ptr, size_t len);
size_t _z_read_lora(_z_sys_net_socket_t sock, uint8_t *ptr, size_t len);
size_t _z_send_lora(_z_sys_net_socket_t sock, const uint8_t *ptr, size_t len);

#ifdef __cplusplus
}
#endif

#endif

#endif /* ZENOH_PICO_SYSTEM_LINK_LORA_H */
