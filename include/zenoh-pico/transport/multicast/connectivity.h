//
// Copyright (c) 2026 ZettaScale Technology
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
#ifndef INCLUDE_ZENOH_PICO_TRANSPORT_MULTICAST_CONNECTIVITY_H
#define INCLUDE_ZENOH_PICO_TRANSPORT_MULTICAST_CONNECTIVITY_H

#include "zenoh-pico/session/interest.h"
#include "zenoh-pico/transport/multicast/transport.h"

void _zp_multicast_report_disconnected_event(_z_transport_multicast_t *ztm,
                                             _z_transport_peer_multicast_t *dropped_peer);
void _zp_multicast_report_connected_event(_z_transport_multicast_t *ztm, _z_transport_peer_multicast_t *connected_peer);

void _zp_multicast_remove_peer_entry(_z_transport_multicast_t *ztm,
                                     _z_peer_id_to_transport_peer_multicast_hmap_elem_t *entry);
#endif  // INCLUDE_ZENOH_PICO_TRANSPORT_MULTICAST_CONNECTIVITY_H