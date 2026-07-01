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

#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "zenoh-pico/config.h"
#include "zenoh-pico/net/session.h"
#include "zenoh-pico/protocol/definitions/network.h"
#include "zenoh-pico/system/common/platform.h"
#include "zenoh-pico/transport/common/tx.h"
#include "zenoh-pico/transport/transport.h"
#include "zenoh-pico/transport/unicast/transport.h"

#if Z_FEATURE_UNICAST_TRANSPORT == 1

typedef struct {
    size_t write_count;
    size_t byte_count;
} _z_tx_fake_write_state_t;

typedef struct {
    _z_session_t session;
    _z_link_t link;
    _z_tx_fake_write_state_t default_link;
    _z_tx_fake_write_state_t peer_state[2];
    _z_transport_peer_unicast_t *peer[2];
} _z_tx_fixture_t;

static size_t _z_tx_fake_link_write(const _z_link_t *link, const uint8_t *ptr, size_t len) {
    _ZP_UNUSED(ptr);

    _z_tx_fake_write_state_t *state = (_z_tx_fake_write_state_t *)_z_link_state_const(link);
    state->write_count++;
    state->byte_count += len;
    return len;
}

static size_t _z_tx_fake_peer_write(const _z_link_t *link, const _z_link_peer_t *peer, const uint8_t *ptr, size_t len) {
    _ZP_UNUSED(link);
    _ZP_UNUSED(ptr);

    _z_tx_fake_write_state_t *state = (_z_tx_fake_write_state_t *)_z_link_peer_state_const(peer);
    state->write_count++;
    state->byte_count += len;
    return len;
}

static const _z_link_peer_ops_t _z_tx_fake_peer_ops = {NULL, _z_tx_fake_peer_write, NULL, NULL, NULL};

static void _z_tx_fake_link_init(_z_link_t *link, _z_tx_fake_write_state_t *state) {
    memset(link, 0, sizeof(*link));
    link->_state = state;
    link->_write_f = _z_tx_fake_link_write;
    link->_mtu = Z_BATCH_UNICAST_SIZE;
    link->_cap._transport = Z_LINK_CAP_TRANSPORT_UNICAST;
    link->_cap._flow = Z_LINK_CAP_FLOW_DATAGRAM;
    link->_cap._is_reliable = 1;
}

static _z_network_message_t _z_tx_test_message(void) {
    _z_network_message_t msg;
    _z_n_msg_make_response_final(&msg, 1);
    return msg;
}

static _z_transport_peer_unicast_t *_z_tx_fixture_add_peer(_z_tx_fixture_t *fixture, size_t idx) {
    _z_transport_unicast_t *ztu = &fixture->session._tp._transport._unicast;
    _z_transport_peer_unicast_slist_t *old_head = ztu->_peers;
    _z_transport_peer_unicast_slist_t *new_head = _z_transport_peer_unicast_slist_push_empty(old_head);
    assert(new_head != old_head);
    ztu->_peers = new_head;

    _z_transport_peer_unicast_t *peer = _z_transport_peer_unicast_slist_value(new_head);
    memset(peer, 0, sizeof(*peer));
    assert(_z_link_peer_init(&peer->_link_peer, &_z_tx_fake_peer_ops, &fixture->peer_state[idx], NULL) == _Z_RES_OK);
    fixture->peer[idx] = peer;
    return peer;
}

static void _z_tx_fixture_init(_z_tx_fixture_t *fixture) {
    memset(fixture, 0, sizeof(*fixture));

    _z_transport_unicast_establish_param_t param;
    memset(&param, 0, sizeof(param));
    param._batch_size = Z_BATCH_UNICAST_SIZE;
    param._seq_num_res = Z_SN_RESOLUTION;
    param._lease = Z_TRANSPORT_LEASE;

    _z_tx_fake_link_init(&fixture->link, &fixture->default_link);
    assert(_z_unicast_transport_create(&fixture->session._tp, &fixture->link, &param) == _Z_RES_OK);
    fixture->session._mode = Z_WHATAMI_PEER;

    _z_tx_fixture_add_peer(fixture, 0);
    _z_tx_fixture_add_peer(fixture, 1);
}

static void _z_tx_fixture_clear(_z_tx_fixture_t *fixture) {
    fixture->session._tp._transport._unicast._common._link = NULL;
    _z_transport_clear(&fixture->session._tp);
}

static void directed_send_reaches_only_selected_peer(void) {
    _z_tx_fixture_t fixture;
    _z_tx_fixture_init(&fixture);

    _z_network_message_t msg = _z_tx_test_message();
    assert(_z_send_n_msg(&fixture.session, &msg, Z_RELIABILITY_RELIABLE, Z_CONGESTION_CONTROL_BLOCK, fixture.peer[0]) ==
           _Z_RES_OK);

    assert(fixture.peer_state[0].write_count == 1);
    assert(fixture.peer_state[1].write_count == 0);
    assert(fixture.default_link.write_count == 0);

    _z_tx_fixture_clear(&fixture);
}

#if Z_FEATURE_BATCHING == 1
static void directed_send_flushes_shared_batch_once(void) {
    _z_tx_fixture_t fixture;
    _z_tx_fixture_init(&fixture);

    assert(_z_transport_start_batching(&fixture.session._tp) == _Z_RES_OK);

    _z_network_message_t broadcast_msg = _z_tx_test_message();
    assert(_z_send_n_msg(&fixture.session, &broadcast_msg, Z_RELIABILITY_RELIABLE, Z_CONGESTION_CONTROL_BLOCK, NULL) ==
           _Z_RES_OK);
    assert(fixture.peer_state[0].write_count == 0);
    assert(fixture.peer_state[1].write_count == 0);

    _z_network_message_t directed_msg = _z_tx_test_message();
    assert(_z_send_n_msg(&fixture.session, &directed_msg, Z_RELIABILITY_RELIABLE, Z_CONGESTION_CONTROL_BLOCK,
                         fixture.peer[0]) == _Z_RES_OK);
    assert(fixture.peer_state[0].write_count == 2);
    assert(fixture.peer_state[1].write_count == 1);

    assert(_z_send_n_batch(&fixture.session, Z_CONGESTION_CONTROL_BLOCK) == _Z_RES_OK);
    assert(fixture.peer_state[0].write_count == 2);
    assert(fixture.peer_state[1].write_count == 1);
    assert(fixture.default_link.write_count == 0);

    assert(_z_transport_stop_batching(&fixture.session._tp) == _Z_RES_OK);
    _z_tx_fixture_clear(&fixture);
}

static void broadcast_batching_reaches_all_peers(void) {
    _z_tx_fixture_t fixture;
    _z_tx_fixture_init(&fixture);

    assert(_z_transport_start_batching(&fixture.session._tp) == _Z_RES_OK);

    _z_network_message_t msg = _z_tx_test_message();
    assert(_z_send_n_msg(&fixture.session, &msg, Z_RELIABILITY_RELIABLE, Z_CONGESTION_CONTROL_BLOCK, NULL) ==
           _Z_RES_OK);
    assert(fixture.peer_state[0].write_count == 0);
    assert(fixture.peer_state[1].write_count == 0);

    assert(_z_send_n_batch(&fixture.session, Z_CONGESTION_CONTROL_BLOCK) == _Z_RES_OK);
    assert(fixture.peer_state[0].write_count == 1);
    assert(fixture.peer_state[1].write_count == 1);
    assert(fixture.default_link.write_count == 0);

    assert(_z_transport_stop_batching(&fixture.session._tp) == _Z_RES_OK);
    _z_tx_fixture_clear(&fixture);
}
#endif  // Z_FEATURE_BATCHING == 1

int main(void) {
    directed_send_reaches_only_selected_peer();

#if Z_FEATURE_BATCHING == 1
    directed_send_flushes_shared_batch_once();
    broadcast_batching_reaches_all_peers();
#else
    printf("Skipping batching-specific TX destination tests (Z_FEATURE_BATCHING disabled)\n");
#endif

    return 0;
}

#else
int main(void) {
    printf("Skipping TX destination tests (Z_FEATURE_UNICAST_TRANSPORT disabled)\n");
    return 0;
}
#endif  // Z_FEATURE_UNICAST_TRANSPORT == 1
