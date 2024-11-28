//
// Copyright (c) 2022 ZettaScale Technology
// Copyright (c) 2023 Fictionlab sp. z o.o.
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
//   Błażej Sowa, <blazej@fictionlab.pl>

#include <stdlib.h>

#include "zenoh-pico/system/link/serial.h"
#include "zenoh-pico/system/platform.h"
#include "zenoh-pico/utils/checksum.h"
#include "zenoh-pico/utils/encoding.h"
#include "zenoh-pico/utils/pointers.h"
#include "zenoh-pico/utils/result.h"

// FreeRTOS includes
#include "FreeRTOS.h"

#if Z_FEATURE_LINK_TCP == 1
/*------------------ TCP sockets ------------------*/
z_result_t _z_create_endpoint_tcp(_z_sys_net_endpoint_t *ep, const char *s_address, const char *s_port) {
    z_result_t ret = _Z_RES_OK;

    if (FreeRTOS_getaddrinfo(s_address, NULL, NULL, &ep->_iptcp) < 0) {
        ret = _Z_ERR_GENERIC;
        return ret;
    }

    ep->_iptcp->ai_addr->sin_family = ep->_iptcp->ai_family;

    // Parse and check the validity of the port
    uint32_t port = strtoul(s_port, NULL, 10);
    if ((port > (uint32_t)0) && (port <= (uint32_t)65355)) {  // Port numbers should range from 1 to 65355
        ep->_iptcp->ai_addr->sin_port = FreeRTOS_htons((uint16_t)port);
    } else {
        ret = _Z_ERR_GENERIC;
    }

    return ret;
}

void _z_free_endpoint_tcp(_z_sys_net_endpoint_t *ep) { FreeRTOS_freeaddrinfo(ep->_iptcp); }

z_result_t _z_open_tcp(_z_sys_net_socket_t *sock, const _z_sys_net_endpoint_t rep, uint32_t tout) {
    z_result_t ret = _Z_RES_OK;

    sock->_socket = FreeRTOS_socket(rep._iptcp->ai_family, FREERTOS_SOCK_STREAM, FREERTOS_IPPROTO_TCP);
    if (sock->_socket != FREERTOS_INVALID_SOCKET) {
        TickType_t receive_timeout = pdMS_TO_TICKS(tout);

        if (FreeRTOS_setsockopt(sock->_socket, 0, FREERTOS_SO_RCVTIMEO, &receive_timeout, 0) != 0) {
            ret = _Z_ERR_GENERIC;
        } else if (FreeRTOS_connect(sock->_socket, rep._iptcp->ai_addr, rep._iptcp->ai_addrlen) < 0) {
            ret = _Z_ERR_GENERIC;
        }
    } else {
        ret = _Z_ERR_GENERIC;
    }

    return ret;
}

z_result_t _z_listen_tcp(_z_sys_net_socket_t *sock, const _z_sys_net_endpoint_t lep) {
    z_result_t ret = _Z_RES_OK;
    (void)sock;
    (void)lep;

    // @TODO: To be implemented
    ret = _Z_ERR_GENERIC;

    return ret;
}

void _z_close_tcp(_z_sys_net_socket_t *sock) { FreeRTOS_closesocket(sock->_socket); }

size_t _z_read_tcp(const _z_sys_net_socket_t sock, uint8_t *ptr, size_t len) {
    BaseType_t rb = FreeRTOS_recv(sock._socket, ptr, len, 0);
    if (rb < 0) {
        return SIZE_MAX;
    }

    return rb;
}

size_t _z_read_exact_tcp(const _z_sys_net_socket_t sock, uint8_t *ptr, size_t len) {
    size_t n = 0;
    uint8_t *pos = &ptr[0];

    do {
        size_t rb = _z_read_tcp(sock, pos, len - n);
        if (rb == SIZE_MAX) {
            n = rb;
            break;
        }

        n = n + rb;
        pos = _z_ptr_u8_offset(pos, n);
    } while (n != len);

    return n;
}

size_t _z_send_tcp(const _z_sys_net_socket_t sock, const uint8_t *ptr, size_t len) {
    return FreeRTOS_send(sock._socket, ptr, len, 0);
}
#endif

#if Z_FEATURE_LINK_UDP_UNICAST == 1 || Z_FEATURE_LINK_UDP_MULTICAST == 1
/*------------------ UDP sockets ------------------*/
z_result_t _z_create_endpoint_udp(_z_sys_net_endpoint_t *ep, const char *s_address, const char *s_port) {
    z_result_t ret = _Z_RES_OK;

    if (FreeRTOS_getaddrinfo(s_address, NULL, NULL, &ep->_iptcp) < 0) {
        ret = _Z_ERR_GENERIC;
        return ret;
    }

    ep->_iptcp->ai_addr->sin_family = ep->_iptcp->ai_family;

    // Parse and check the validity of the port
    uint32_t port = strtoul(s_port, NULL, 10);
    if ((port > (uint32_t)0) && (port <= (uint32_t)65355)) {  // Port numbers should range from 1 to 65355
        ep->_iptcp->ai_addr->sin_port = FreeRTOS_htons((uint16_t)port);
    } else {
        ret = _Z_ERR_GENERIC;
    }

    return ret;
}

void _z_free_endpoint_udp(_z_sys_net_endpoint_t *ep) { FreeRTOS_freeaddrinfo(ep->_iptcp); }
#endif

#if Z_FEATURE_LINK_UDP_UNICAST == 1
z_result_t _z_open_udp_unicast(_z_sys_net_socket_t *sock, const _z_sys_net_endpoint_t rep, uint32_t tout) {
    z_result_t ret = _Z_RES_OK;

    sock->_socket = FreeRTOS_socket(rep._iptcp->ai_family, FREERTOS_SOCK_DGRAM, FREERTOS_IPPROTO_UDP);
    if (sock->_socket != FREERTOS_INVALID_SOCKET) {
        TickType_t receive_timeout = pdMS_TO_TICKS(tout);

        if (FreeRTOS_setsockopt(sock->_socket, 0, FREERTOS_SO_RCVTIMEO, &receive_timeout, 0) != 0) {
            ret = _Z_ERR_GENERIC;
        }
    } else {
        ret = _Z_ERR_GENERIC;
    }

    return ret;
}

z_result_t _z_listen_udp_unicast(_z_sys_net_socket_t *sock, const _z_sys_net_endpoint_t rep, uint32_t tout) {
    z_result_t ret = _Z_RES_OK;
    (void)sock;
    (void)rep;
    (void)tout;

    // @TODO: To be implemented
    ret = _Z_ERR_GENERIC;

    return ret;
}

void _z_close_udp_unicast(_z_sys_net_socket_t *sock) { FreeRTOS_closesocket(sock->_socket); }

size_t _z_read_udp_unicast(const _z_sys_net_socket_t sock, uint8_t *ptr, size_t len) {
    struct freertos_sockaddr raddr;
    uint32_t addrlen = sizeof(struct freertos_sockaddr);

    int32_t rb = FreeRTOS_recvfrom(sock._socket, ptr, len, 0, &raddr, &addrlen);
    if (rb < 0) {
        return SIZE_MAX;
    }

    return rb;
}

size_t _z_read_exact_udp_unicast(const _z_sys_net_socket_t sock, uint8_t *ptr, size_t len) {
    size_t n = 0;
    uint8_t *pos = &ptr[0];

    do {
        size_t rb = _z_read_udp_unicast(sock, pos, len - n);
        if (rb == SIZE_MAX) {
            n = rb;
            break;
        }

        n = n + rb;
        pos = _z_ptr_u8_offset(pos, n);
    } while (n != len);

    return n;
}

size_t _z_send_udp_unicast(const _z_sys_net_socket_t sock, const uint8_t *ptr, size_t len,
                           const _z_sys_net_endpoint_t rep) {
    return FreeRTOS_sendto(sock._socket, ptr, len, 0, rep._iptcp->ai_addr, sizeof(struct freertos_sockaddr));
}
#endif

#if Z_FEATURE_LINK_UDP_MULTICAST == 1
#error "UDP Multicast not supported yet on FreeRTOS-Plus-TCP port of Zenoh-Pico"
#endif

#if Z_FEATURE_LINK_BLUETOOTH == 1
#error "Bluetooth not supported yet on FreeRTOS-Plus-TCP port of Zenoh-Pico"
#endif

#if Z_FEATURE_LINK_SERIAL == 1
#include "uart2.h"

z_result_t _z_open_serial_from_pins(_z_sys_net_socket_t *sock, uint32_t txpin, uint32_t rxpin, uint32_t baudrate) {
    z_result_t ret = _Z_RES_OK;
    (void)(sock);
    (void)(txpin);
    (void)(rxpin);
    (void)(baudrate);

    ret = _Z_ERR_GENERIC;

    return ret;
}

z_result_t _z_open_serial_from_dev(_z_sys_net_socket_t *sock, char *dev, uint32_t baudrate) {
    z_result_t ret = _Z_RES_OK;
    (void)(sock);
    (void)(dev);

    uart2Init(baudrate);

    return ret;
}

z_result_t _z_listen_serial_from_pins(_z_sys_net_socket_t *sock, uint32_t txpin, uint32_t rxpin, uint32_t baudrate) {
    (void)(sock);
    (void)(txpin);
    (void)(rxpin);
    (void)(baudrate);

    return _Z_ERR_GENERIC;
}

z_result_t _z_listen_serial_from_dev(_z_sys_net_socket_t *sock, char *dev, uint32_t baudrate) {
    (void)(sock);
    (void)(dev);
    (void)(baudrate);

    return _Z_ERR_GENERIC;
}

void _z_close_serial(_z_sys_net_socket_t *sock) {}

size_t _z_read_serial(const _z_sys_net_socket_t sock, uint8_t *ptr, size_t len) {
    z_result_t ret = _Z_RES_OK;

    uint8_t *before_cobs = (uint8_t *)z_malloc(_Z_SERIAL_MAX_COBS_BUF_SIZE);
    size_t rb = 0;
    for (size_t i = 0; i < _Z_SERIAL_MAX_COBS_BUF_SIZE; i++) {
        uart2Getchar(&before_cobs[i]);

        rb = rb + (size_t)1;
        if (before_cobs[i] == (uint8_t)0x00) {
            break;
        }
    }

    uint8_t *after_cobs = (uint8_t *)z_malloc(_Z_SERIAL_MFS_SIZE);
    size_t trb = _z_cobs_decode(before_cobs, rb, after_cobs);

    size_t i = 0;
    uint16_t payload_len = 0;
    for (; i < sizeof(payload_len); i++) {
        payload_len |= (after_cobs[i] << ((uint8_t)i * (uint8_t)8));
    }

    if (trb == (size_t)(payload_len + (uint16_t)6)) {
        (void)memcpy(ptr, &after_cobs[i], payload_len);
        i = i + (size_t)payload_len;

        uint32_t crc = 0;
        for (uint8_t j = 0; j < sizeof(crc); j++) {
            crc |= (uint32_t)(after_cobs[i] << (j * (uint8_t)8));
            i = i + (size_t)1;
        }

        uint32_t c_crc = _z_crc32(ptr, payload_len);
        if (c_crc != crc) {
            ret = _Z_ERR_GENERIC;
        }
    } else {
        ret = _Z_ERR_GENERIC;
    }

    z_free(before_cobs);
    z_free(after_cobs);

    rb = payload_len;
    if (ret != _Z_RES_OK) {
        rb = SIZE_MAX;
    }

    return rb;
}

size_t _z_read_exact_serial(const _z_sys_net_socket_t sock, uint8_t *ptr, size_t len) {
    size_t n = 0;
    uint8_t *pos = &ptr[0];

    do {
        size_t rb = _z_read_serial(sock, ptr, len - n);
        if (rb == SIZE_MAX) {
            n = rb;
            break;
        }

        n = n + rb;
        pos = _z_ptr_u8_offset(pos, n);
    } while (n != len);

    return n;
}

size_t _z_send_serial(const _z_sys_net_socket_t sock, const uint8_t *ptr, size_t len) {
    z_result_t ret = _Z_RES_OK;

    uint8_t *before_cobs = (uint8_t *)z_malloc(_Z_SERIAL_MFS_SIZE);
    size_t i = 0;
    for (; i < sizeof(uint16_t); ++i) {
        before_cobs[i] = (len >> (i * (size_t)8)) & (size_t)0XFF;
    }

    (void)memcpy(&before_cobs[i], ptr, len);
    i = i + len;

    uint32_t crc = _z_crc32(ptr, len);
    for (uint8_t j = 0; j < sizeof(crc); j++) {
        before_cobs[i] = (crc >> (j * (uint8_t)8)) & (uint32_t)0XFF;
        i = i + (size_t)1;
    }

    uint8_t *after_cobs = (uint8_t *)z_malloc(_Z_SERIAL_MAX_COBS_BUF_SIZE);
    size_t twb = _z_cobs_encode(before_cobs, i, after_cobs);
    after_cobs[twb] = 0x00;  // Manually add the COBS delimiter
    uart2SendData(twb + 1, after_cobs);

    z_free(before_cobs);
    z_free(after_cobs);

    size_t sb = len;
    if (ret != _Z_RES_OK) {
        sb = SIZE_MAX;
    }

    return sb;
}
#endif

#if Z_FEATURE_RAWETH_TRANSPORT == 1
#error "Raw ethernet transport not supported yet on FreeRTOS-Plus-TCP port of Zenoh-Pico"
#endif
