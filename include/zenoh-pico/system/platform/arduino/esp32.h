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

#ifndef ZENOH_PICO_SYSTEM_ESP32_TYPES_H
#define ZENOH_PICO_SYSTEM_ESP32_TYPES_H

#include <Arduino.h>

#include "zenoh-pico/config.h"

#if Z_MULTI_THREAD == 1
#include <pthread.h>
#endif  // Z_MULTI_THREAD == 1

#if Z_MULTI_THREAD == 1
typedef TaskHandle_t _z_task_t;
typedef void *_z_task_attr_t;  // Not used in ESP32
typedef pthread_mutex_t _z_mutex_t;
typedef pthread_cond_t _z_condvar_t;
#endif  // Z_MULTI_THREAD == 1

typedef struct timespec z_clock_t;
typedef struct timeval z_time_t;

typedef struct BluetoothSerial BluetoothSerial;  // Forward declaration to be used in _z_sys_net_socket_t
typedef struct LoRaClass LoRaClass;  // Forward declaration to be used in _z_sys_net_socket_t

typedef struct {
    _Bool _err;
    union {
#if Z_LINK_TCP == 1 || Z_LINK_UDP_MULTICAST == 1 || Z_LINK_UDP_UNICAST == 1
        int _fd;
#endif
#if Z_LINK_BLUETOOTH == 1
        BluetoothSerial *_bts;  // As pointer to cross the boundary between C and C++
#endif
#if Z_LINK_LORA == 1
        LoRaClass *_lora;  // As pointer to cross the boundary between C and C++
#endif
    };
} _z_sys_net_socket_t;

typedef struct {
    _Bool _err;
    union {
#if Z_LINK_TCP == 1 || Z_LINK_UDP_MULTICAST == 1 || Z_LINK_UDP_UNICAST == 1
        struct addrinfo *_iptcp;
#endif
    };
} _z_sys_net_endpoint_t;

#endif /* ZENOH_PICO_SYSTEM_ESP32_TYPES_H */
