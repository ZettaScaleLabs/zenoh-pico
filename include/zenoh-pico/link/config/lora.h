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

#ifndef ZENOH_PICO_LINK_CONFIG_LORA_H
#define ZENOH_PICO_LINK_CONFIG_LORA_H

#include "zenoh-pico/collections/intmap.h"
#include "zenoh-pico/collections/string.h"
#include "zenoh-pico/config.h"
#include "zenoh-pico/system/platform.h"

#if Z_LINK_LORA == 1

#define LORA_CONFIG_FREQUENCY_KEY 0x01
#define LORA_CONFIG_FREQUENCY_STR "frequency"

#define LORA_CONFIG_MAPPING_BUILD             \
    uint8_t argc = 1;                         \
    _z_str_intmapping_t args[argc];           \
    args[0]._key = LORA_CONFIG_FREQUENCY_KEY; \
    args[0]._str = LORA_CONFIG_FREQUENCY_STR;

size_t _z_lora_config_strlen(const _z_str_intmap_t *s);

void _z_lora_config_onto_str(char *dst, size_t dst_len, const _z_str_intmap_t *s);
char *_z_lora_config_to_str(const _z_str_intmap_t *s);

_z_str_intmap_result_t _z_lora_config_from_str(const char *s);
_z_str_intmap_result_t _z_lora_config_from_strn(const char *s, size_t n);
#endif

#endif /* ZENOH_PICO_LINK_CONFIG_LORA_H */