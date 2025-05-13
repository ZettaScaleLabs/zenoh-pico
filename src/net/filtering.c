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

#include "zenoh-pico/net/filtering.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "zenoh-pico/config.h"
#include "zenoh-pico/net/primitives.h"
#include "zenoh-pico/session/session.h"

#if Z_FEATURE_INTEREST == 1
void _z_decl_id_elem_free(void **e) {
    uint32_t *ptr = (uint32_t *)*e;
    if (ptr != NULL) {
        z_free(ptr);
        *e = NULL;
    }
}

static bool _z_decl_id_eq(const void *left, const void *right) {
    uint32_t left_val = *(const uint32_t *)left;
    uint32_t right_val = *(const uint32_t *)right;
    return left_val == right_val;
}

static bool _z_write_filter_push_decl_id(_z_writer_filter_ctx_t *ctx, uint32_t id) {
    uint32_t *alloc_id = (uint32_t *)z_malloc(sizeof(uint32_t));
    if (alloc_id == NULL) {
        _Z_ERROR("Failed to allocate declare id.");
        return false;
    }
    *alloc_id = id;
    ctx->decl_ids = _z_decl_id_list_push(ctx->decl_ids, alloc_id);
    if (ctx->decl_ids == NULL) {  // Allocation can fail
        return false;
    }
    return true;
}

static void _z_write_filter_callback(const _z_interest_msg_t *msg, void *arg) {
    _z_writer_filter_ctx_t *ctx = (_z_writer_filter_ctx_t *)arg;

    switch (ctx->state) {
        // Update init state
        case WRITE_FILTER_INIT:
            switch (msg->type) {
                case _Z_INTEREST_MSG_TYPE_FINAL:
                    ctx->state = WRITE_FILTER_ACTIVE;  // No subscribers
                    break;

                case _Z_INTEREST_MSG_TYPE_DECL_SUBSCRIBER:
                case _Z_INTEREST_MSG_TYPE_DECL_QUERYABLE:
                    if (_z_write_filter_push_decl_id(ctx, msg->id)) {
                        ctx->state = WRITE_FILTER_OFF;
                    }
                    break;

                default:  // Nothing to do
                    break;
            }
            break;
        // Remove filter if we have targets
        case WRITE_FILTER_ACTIVE:
            if (msg->type == _Z_INTEREST_MSG_TYPE_DECL_SUBSCRIBER || msg->type == _Z_INTEREST_MSG_TYPE_DECL_QUERYABLE) {
                if (_z_write_filter_push_decl_id(ctx, msg->id)) {
                    ctx->state = WRITE_FILTER_OFF;
                }
            }
            break;
        // Remove target
        case WRITE_FILTER_OFF:
            if ((msg->type == _Z_INTEREST_MSG_TYPE_UNDECL_SUBSCRIBER ||
                 msg->type == _Z_INTEREST_MSG_TYPE_UNDECL_QUERYABLE)) {
                ctx->decl_ids = _z_decl_id_list_drop_filter(ctx->decl_ids, _z_decl_id_eq, &msg->id);
                // Activate filter if no more targets
                if (ctx->decl_ids == NULL) {
                    ctx->state = WRITE_FILTER_ACTIVE;
                }
            }
            break;
        // Nothing to do
        default:
            break;
    }
}

z_result_t _z_write_filter_create(_z_session_t *zn, _z_write_filter_t *filter, _z_keyexpr_t keyexpr,
                                  uint8_t interest_flag) {
    uint8_t flags = interest_flag | _Z_INTEREST_FLAG_KEYEXPRS | _Z_INTEREST_FLAG_RESTRICTED | _Z_INTEREST_FLAG_CURRENT |
                    _Z_INTEREST_FLAG_FUTURE | _Z_INTEREST_FLAG_AGGREGATE;
    _z_writer_filter_ctx_t *ctx = (_z_writer_filter_ctx_t *)z_malloc(sizeof(_z_writer_filter_ctx_t));

    if (ctx == NULL) {
        return _Z_ERR_SYSTEM_OUT_OF_MEMORY;
    }
    ctx->state = (zn->_mode == Z_WHATAMI_CLIENT) ? WRITE_FILTER_INIT : WRITE_FILTER_ACTIVE;
    ctx->decl_ids = _z_decl_id_list_new();

    filter->ctx = ctx;
    filter->_interest_id = _z_add_interest(zn, keyexpr, _z_write_filter_callback, flags, (void *)ctx);
    if (filter->_interest_id == 0) {
        z_free(ctx);
        return _Z_ERR_GENERIC;
    }
    return _Z_RES_OK;
}

z_result_t _z_write_filter_destroy(_z_session_t *zn, _z_write_filter_t *filter) {
    if (filter->ctx != NULL) {
        z_result_t res = _z_remove_interest(zn, filter->_interest_id);
        z_free(filter->ctx);
        filter->ctx = NULL;
        return res;
    }
    return _Z_RES_OK;
}

bool _z_write_filter_active(const _z_write_filter_t *filter) {
    return filter->ctx != NULL && filter->ctx->state == WRITE_FILTER_ACTIVE;
}

#else
z_result_t _z_write_filter_create(_z_session_t *zn, _z_write_filter_t *filter, _z_keyexpr_t keyexpr,
                                  uint8_t interest_flag) {
    _ZP_UNUSED(zn);
    _ZP_UNUSED(keyexpr);
    _ZP_UNUSED(filter);
    _ZP_UNUSED(interest_flag);
    return _Z_RES_OK;
}

z_result_t _z_write_filter_destroy(_z_session_t *zn, _z_write_filter_t *filter) {
    _ZP_UNUSED(zn);
    _ZP_UNUSED(filter);
    return _Z_RES_OK;
}

bool _z_write_filter_active(const _z_write_filter_t *filter) {
    _ZP_UNUSED(filter);
    return false;
}

#endif
