//
// Copyright (c) 2024 ZettaScale Technology
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
#ifndef ZENOH_PICO_COLLECTIONS_RING_MT_H
#define ZENOH_PICO_COLLECTIONS_RING_MT_H

#include <stdint.h>

#include "zenoh-pico/collections/element.h"
#include "zenoh-pico/collections/fifo.h"
#include "zenoh-pico/system/platform.h"

/*-------- Ring Buffer Multithreaded --------*/
typedef struct {
    _z_ring_t _ring;
#if Z_FEATURE_MULTI_THREAD == 1
    z_mutex_t _mutex;
    z_condvar_t _cv_not_empty;
#endif
} _z_ring_mt_t;

int8_t _z_ring_mt_init(_z_ring_mt_t *ring, size_t capacity);
_z_ring_mt_t *_z_ring_mt_new(size_t capacity);

void _z_ring_mt_clear(_z_ring_mt_t *ring, z_element_free_f free_f);
void _z_ring_mt_free(_z_ring_mt_t *ring, z_element_free_f free_f);

int8_t _z_ring_mt_push(const void *src, void *context, z_element_free_f element_free);

int8_t _z_ring_mt_pull(void *dst, void *context, z_element_move_f element_move);
int8_t _z_ring_mt_try_pull(void *dst, void *context, z_element_move_f element_move);

#endif  // ZENOH_PICO_COLLECTIONS_RING_MT_H
