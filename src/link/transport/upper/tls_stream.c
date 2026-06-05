//
// Copyright (c) 2025 ZettaScale Technology
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

#include "zenoh-pico/link/transport/tls_stream.h"

#if Z_FEATURE_LINK_TLS == 1

#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "mbedtls/base64.h"
#include "mbedtls/debug.h"
#include "mbedtls/entropy.h"
#include "mbedtls/error.h"
#include "mbedtls/hmac_drbg.h"
#include "mbedtls/md.h"
#include "mbedtls/net_sockets.h"
#include "mbedtls/pk.h"
#include "mbedtls/ssl.h"
#include "mbedtls/version.h"
#include "mbedtls/x509.h"
#include "mbedtls/x509_crt.h"
#include "zenoh-pico/config.h"
#include "zenoh-pico/link/config/tls.h"
#include "zenoh-pico/utils/logging.h"
#include "zenoh-pico/utils/pointers.h"

#define Z_TLS_BASE64_MAX_VALUE_LEN (64 * 1024)

// mbedTLS file APIs require a null-terminated path; copy into a conservative
// stack buffer to avoid a heap allocation for the common (short-path) case.
#define Z_TLS_PATH_BUF_LEN 256

#ifdef ZENOH_LOG_TRACE
static void _z_tls_debug(void *ctx, int level, const char *file, int line, const char *str) {
    _ZP_UNUSED(ctx);
    _Z_DEBUG_NONL("mbed TLS [%d] %s:%04d: %s", level, file, line, str);
}
#endif

static _z_tls_context_t *_z_tls_context_new(void);
static void _z_tls_context_free(_z_tls_context_t **ctx);

static z_result_t _z_tls_decode_base64(const char *label, const char *input, size_t input_len, unsigned char **output,
                                       size_t *output_len) {
    if (input == NULL) {
        return _Z_ERR_GENERIC;
    }

    if (label == NULL) {
        _Z_ERROR("TLS base64 value label is NULL");
        return _Z_ERR_GENERIC;
    }

    if (input_len > Z_TLS_BASE64_MAX_VALUE_LEN) {
        _Z_ERROR("%s exceeds maximum supported value length (%zu bytes)", label, (size_t)Z_TLS_BASE64_MAX_VALUE_LEN);
        return _Z_ERR_GENERIC;
    }

    size_t required = 0;
    int ret = mbedtls_base64_decode(NULL, 0, &required, (const unsigned char *)input, input_len);
    if (ret != 0 && ret != MBEDTLS_ERR_BASE64_BUFFER_TOO_SMALL) {
        _Z_ERROR("Failed to decode %s from base64: -0x%04x", label, -ret);
        return _Z_ERR_GENERIC;
    }

    size_t buffer_len = (required > 0) ? required : 1;
    unsigned char *buffer = (unsigned char *)z_malloc(buffer_len + 1);
    if (buffer == NULL) {
        _Z_ERROR("Failed to allocate buffer for %s base64 (%zu bytes)", label, buffer_len);
        return _Z_ERR_SYSTEM_OUT_OF_MEMORY;
    }

    ret = mbedtls_base64_decode(buffer, buffer_len, &required, (const unsigned char *)input, input_len);
    if (ret != 0) {
        _Z_ERROR("Failed to decode %s from base64: -0x%04x", label, -ret);
        z_free(buffer);
        return _Z_ERR_GENERIC;
    }

    buffer[required] = '\0';
    *output = buffer;
    if (output_len != NULL) {
        *output_len = required;
    }
    return _Z_RES_OK;
}

static z_result_t _z_tls_parse_cert_from_base64(mbedtls_x509_crt *cert, const char *base64, size_t base64_len,
                                                const char *label) {
    unsigned char *decoded = NULL;
    size_t decoded_len = 0;
    z_result_t res = _z_tls_decode_base64(label, base64, base64_len, &decoded, &decoded_len);
    if (res != _Z_RES_OK) {
        return res;
    }

    int ret = mbedtls_x509_crt_parse(cert, decoded, decoded_len + 1);
    z_free(decoded);
    if (ret != 0) {
        _Z_ERROR("Failed to parse %s from base64: -0x%04x", label, -ret);
        return _Z_ERR_GENERIC;
    }

    _Z_DEBUG("Loaded %s from base64", label);
    return _Z_RES_OK;
}

static z_result_t _z_tls_parse_key_from_base64(mbedtls_pk_context *key, const char *base64, size_t base64_len,
                                               const char *label, mbedtls_hmac_drbg_context *rng) {
    unsigned char *decoded = NULL;
    size_t decoded_len = 0;
    z_result_t res = _z_tls_decode_base64(label, base64, base64_len, &decoded, &decoded_len);
    if (res != _Z_RES_OK) {
        return res;
    }

#if MBEDTLS_VERSION_MAJOR >= 3
    int ret = mbedtls_pk_parse_key(key, decoded, decoded_len + 1, NULL, 0, mbedtls_hmac_drbg_random, rng);
#else
    _ZP_UNUSED(rng);
    int ret = mbedtls_pk_parse_key(key, decoded, decoded_len + 1, NULL, 0);
#endif
    z_free(decoded);
    if (ret != 0) {
        _Z_ERROR("Failed to parse %s from base64: -0x%04x", label, -ret);
        return _Z_ERR_GENERIC;
    }

    _Z_DEBUG("Loaded %s from base64", label);
    return _Z_RES_OK;
}

static int _z_tls_bio_send(void *ctx, const unsigned char *buf, size_t len) {
    int fd = *(int *)ctx;
    ssize_t n = send(fd, buf, len, MSG_NOSIGNAL);
    if (n < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return MBEDTLS_ERR_SSL_WANT_WRITE;
        }
        return MBEDTLS_ERR_NET_SEND_FAILED;
    }
    return (int)n;
}

static int _z_tls_bio_recv(void *ctx, unsigned char *buf, size_t len) {
    int fd = *(int *)ctx;
    ssize_t n = recv(fd, buf, len, 0);
    if (n < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return MBEDTLS_ERR_SSL_WANT_READ;
        }
        return MBEDTLS_ERR_NET_RECV_FAILED;
    }
    if (n == 0) {
        return 0;
    }
    return (int)n;
}

static _z_tls_context_t *_z_tls_context_new(void) {
    _z_tls_context_t *ctx = (_z_tls_context_t *)z_malloc(sizeof(_z_tls_context_t));
    if (ctx == NULL) {
        return NULL;
    }

    mbedtls_ssl_init(&ctx->_ssl);
    mbedtls_ssl_config_init(&ctx->_ssl_config);
    mbedtls_entropy_init(&ctx->_entropy);
    mbedtls_hmac_drbg_init(&ctx->_hmac_drbg);
    mbedtls_x509_crt_init(&ctx->_ca_cert);
    mbedtls_pk_init(&ctx->_listen_key);
    mbedtls_x509_crt_init(&ctx->_listen_cert);
    mbedtls_pk_init(&ctx->_client_key);
    mbedtls_x509_crt_init(&ctx->_client_cert);
    ctx->_enable_mtls = false;
#ifdef ZENOH_LOG_TRACE
    mbedtls_debug_set_threshold(4);
    mbedtls_ssl_conf_dbg(&ctx->_ssl_config, _z_tls_debug, NULL);
#endif

    int ret = mbedtls_hmac_drbg_seed(&ctx->_hmac_drbg, mbedtls_md_info_from_type(MBEDTLS_MD_SHA256),
                                     mbedtls_entropy_func, &ctx->_entropy, NULL, 0);
    if (ret != 0) {
        _Z_ERROR("Failed to seed HMAC_DRBG: -0x%04x", -ret);
        _z_tls_context_free(&ctx);
        return NULL;
    }

    return ctx;
}

static void _z_tls_context_free(_z_tls_context_t **ctx) {
    if (ctx != NULL && *ctx != NULL) {
        mbedtls_ssl_free(&(*ctx)->_ssl);
        mbedtls_ssl_config_free(&(*ctx)->_ssl_config);
        mbedtls_entropy_free(&(*ctx)->_entropy);
        mbedtls_hmac_drbg_free(&(*ctx)->_hmac_drbg);
        mbedtls_x509_crt_free(&(*ctx)->_ca_cert);
        mbedtls_pk_free(&(*ctx)->_listen_key);
        mbedtls_x509_crt_free(&(*ctx)->_listen_cert);
        mbedtls_pk_free(&(*ctx)->_client_key);
        mbedtls_x509_crt_free(&(*ctx)->_client_cert);
        z_free(*ctx);
        *ctx = NULL;
    }
}

// Copy the active `path` form of `cred` into `buf` as a null-terminated string.
// Returns true if `cred` is a path (and it fit in `buf`), false otherwise.
static bool _z_tls_cred_path_to_buf(const _z_string_t *path, char *buf, size_t buf_len) {
    size_t len = _z_string_len(path);
    if (len >= buf_len) {
        _Z_ERROR("TLS certificate path exceeds maximum supported length (%zu bytes)", (size_t)(buf_len - 1));
        return false;
    }
    memcpy(buf, _z_string_data(p), len);
    buf[len] = '\0';
    return true;
}

static z_result_t _z_tls_load_ca_certificate(_z_tls_context_t *ctx, const _z_tls_config_t *config) {
    const _z_tls_cert_source_t *ca = &config->_root_ca_certificate;

    if (_z_tls_cert_source_is_path(ca)) {
        char path[Z_TLS_PATH_BUF_LEN];
        if (!_z_tls_cred_path_to_buf(_z_tls_cert_source_const_get_path(ca), path, sizeof(path))) {
            return _Z_ERR_GENERIC;
        }
        int ret = mbedtls_x509_crt_parse_file(&ctx->_ca_cert, path);
        if (ret != 0) {
            _Z_ERROR("Failed to parse CA certificate file %s: -0x%04x", path, -ret);
            return _Z_ERR_GENERIC;
        }
        _Z_DEBUG("Loaded CA certificate from %s", path);
    } else if (_z_tls_cert_source_is_base64(ca)) {
        const _z_string_t *b64 = _z_tls_cert_source_const_get_base64(ca);
        z_result_t res =
            _z_tls_parse_cert_from_base64(&ctx->_ca_cert, _z_string_data(b64), _z_string_len(b64), "CA certificate");
        if (res != _Z_RES_OK) {
            return res;
        }
    } else {
        _Z_ERROR("TLS requires 'root_ca_certificate' to be set");
        return _Z_ERR_GENERIC;
    }

    return _Z_RES_OK;
}

// Load a private key from a credential (either a file path or inline base64).
static z_result_t _z_tls_load_key(mbedtls_pk_context *key, const _z_tls_cert_source_t *cred, const char *label,
                                  mbedtls_hmac_drbg_context *rng) {
    if (_z_tls_cert_source_is_base64(cred)) {
        const _z_string_t *b64 = _z_tls_cert_source_const_get_base64(cred);
        return _z_tls_parse_key_from_base64(key, _z_string_data(b64), _z_string_len(b64), label, rng);
    }
    char path[Z_TLS_PATH_BUF_LEN];
    if (!_z_tls_cred_path_to_buf(_z_tls_cert_source_const_get_path(cred), path, sizeof(path))) {
        return _Z_ERR_GENERIC;
    }
#if MBEDTLS_VERSION_MAJOR >= 3
    int ret = mbedtls_pk_parse_keyfile(key, path, NULL, mbedtls_hmac_drbg_random, rng);
#else
    _ZP_UNUSED(rng);
    int ret = mbedtls_pk_parse_keyfile(key, path, NULL);
#endif
    if (ret != 0) {
        _Z_ERROR("Failed to parse %s file %s: -0x%04x", label, path, -ret);
        return _Z_ERR_GENERIC;
    }
    _Z_DEBUG("Loaded %s from %s", label, path);
    return _Z_RES_OK;
}

// Load a certificate from a credential (either a file path or inline base64).
static z_result_t _z_tls_load_cert(mbedtls_x509_crt *cert, const _z_tls_cert_source_t *cred, const char *label) {
    if (_z_tls_cert_source_is_base64(cred)) {
        const _z_string_t *b64 = _z_tls_cert_source_const_get_base64(cred);
        return _z_tls_parse_cert_from_base64(cert, _z_string_data(b64), _z_string_len(b64), label);
    }
    char path[Z_TLS_PATH_BUF_LEN];
    if (!_z_tls_cred_path_to_buf(_z_tls_cert_source_const_get_path(cred), path, sizeof(path))) {
        return _Z_ERR_GENERIC;
    }
    int ret = mbedtls_x509_crt_parse_file(cert, path);
    if (ret != 0) {
        _Z_ERROR("Failed to parse %s file %s: -0x%04x", label, path, -ret);
        return _Z_ERR_GENERIC;
    }
    _Z_DEBUG("Loaded %s from %s", label, path);
    return _Z_RES_OK;
}

static z_result_t _z_tls_load_listen_cert(_z_tls_context_t *ctx, const _z_tls_config_t *config) {
    const _z_tls_cert_source_t *key = &config->_listen_private_key;
    const _z_tls_cert_source_t *cert = &config->_listen_certificate;
    bool has_key = !_z_tls_cert_source_is_none(key);
    bool has_cert = !_z_tls_cert_source_is_none(cert);

    if (!has_key || !has_cert) {
        _Z_ERROR("TLS server requires both private key and certificate to be configured");
        return _Z_ERR_GENERIC;
    }

    z_result_t res = _z_tls_load_key(&ctx->_listen_key, key, "listening private key", &ctx->_hmac_drbg);
    if (res != _Z_RES_OK) {
        return res;
    }
    return _z_tls_load_cert(&ctx->_listen_cert, cert, "listening side certificate");
}

static z_result_t _z_tls_load_client_cert(_z_tls_context_t *ctx, const _z_tls_config_t *config) {
    const _z_tls_cert_source_t *key = &config->_connect_private_key;
    const _z_tls_cert_source_t *cert = &config->_connect_certificate;
    bool has_key = !_z_tls_cert_source_is_none(key);
    bool has_cert = !_z_tls_cert_source_is_none(cert);

    if (!has_key || !has_cert) {
        _Z_ERROR("mTLS requires both client private key and certificate");
        return _Z_ERR_GENERIC;
    }

    z_result_t res = _z_tls_load_key(&ctx->_client_key, key, "client private key", &ctx->_hmac_drbg);
    if (res != _Z_RES_OK) {
        return res;
    }
    return _z_tls_load_cert(&ctx->_client_cert, cert, "client certificate");
}

z_result_t _z_open_tls(_z_tls_socket_t *sock, const _z_sys_net_endpoint_t *rep, const char *hostname,
                       const _z_tls_config_t *config, bool peer_socket) {
    if ((rep == NULL) || (rep->_iptcp == NULL)) {
        _Z_ERROR("Invalid TCP endpoint for TLS connection");
        return _Z_ERR_GENERIC;
    }

    sock->_is_peer_socket = peer_socket;

    bool verify_name = config->_verify_name_on_connect;
    bool enable_mtls = config->_enable_mtls;
    sock->_tls_ctx = _z_tls_context_new();
    if (sock->_tls_ctx == NULL) {
        _Z_ERROR("Failed to create TLS context");
        return _Z_ERR_SYSTEM_OUT_OF_MEMORY;
    }

    if (enable_mtls) {
        z_result_t ret_client = _z_tls_load_client_cert(sock->_tls_ctx, config);
        if (ret_client != _Z_RES_OK) {
            _z_tls_context_free(&sock->_tls_ctx);
            return ret_client;
        }
    }
    sock->_tls_ctx->_enable_mtls = enable_mtls;

    z_result_t ret = _z_tls_load_ca_certificate(sock->_tls_ctx, config);
    if (ret != _Z_RES_OK) {
        _Z_ERROR("Failed to load CA certificate");
        _z_tls_context_free(&sock->_tls_ctx);
        return ret;
    }

    ret = _z_tcp_open(&sock->_sock, *rep, Z_CONFIG_SOCKET_TIMEOUT);
    if (ret != _Z_RES_OK) {
        _Z_ERROR("Failed to open lower TCP socket: %d", ret);
        _z_tls_context_free(&sock->_tls_ctx);
        return ret;
    }

    // Needed for _read_socket_f callback which requires TLS context
    sock->_sock._tls_sock = (void *)sock;

    int mbedret = mbedtls_ssl_config_defaults(&sock->_tls_ctx->_ssl_config, MBEDTLS_SSL_IS_CLIENT,
                                              MBEDTLS_SSL_TRANSPORT_STREAM, MBEDTLS_SSL_PRESET_DEFAULT);
    if (mbedret != 0) {
        _Z_ERROR("Failed to set SSL config defaults: -0x%04x", -mbedret);
        _z_tcp_close(&sock->_sock);
        _z_tls_context_free(&sock->_tls_ctx);
        return _Z_ERR_GENERIC;
    }

    if (sock->_tls_ctx->_ca_cert.version != 0) {
        mbedtls_ssl_conf_ca_chain(&sock->_tls_ctx->_ssl_config, &sock->_tls_ctx->_ca_cert, NULL);
    }
    mbedtls_ssl_conf_authmode(&sock->_tls_ctx->_ssl_config,
                              verify_name ? MBEDTLS_SSL_VERIFY_REQUIRED : MBEDTLS_SSL_VERIFY_OPTIONAL);
    mbedtls_ssl_conf_rng(&sock->_tls_ctx->_ssl_config, mbedtls_hmac_drbg_random, &sock->_tls_ctx->_hmac_drbg);

    if (enable_mtls) {
        int own_ret = mbedtls_ssl_conf_own_cert(&sock->_tls_ctx->_ssl_config, &sock->_tls_ctx->_client_cert,
                                                &sock->_tls_ctx->_client_key);
        if (own_ret != 0) {
            _Z_ERROR("Failed to configure client certificate: -0x%04x", -own_ret);
            _z_tcp_close(&sock->_sock);
            _z_tls_context_free(&sock->_tls_ctx);
            return _Z_ERR_GENERIC;
        }
    }

    mbedret = mbedtls_ssl_setup(&sock->_tls_ctx->_ssl, &sock->_tls_ctx->_ssl_config);
    if (mbedret != 0) {
        _Z_ERROR("Failed to setup SSL: -0x%04x", -mbedret);
        _z_tcp_close(&sock->_sock);
        _z_tls_context_free(&sock->_tls_ctx);
        return _Z_ERR_GENERIC;
    }

    if (!hostname) {
        _Z_ERROR("No hostname is set");
        _z_tcp_close(&sock->_sock);
        _z_tls_context_free(&sock->_tls_ctx);
        return _Z_ERR_GENERIC;
    }

    mbedret = mbedtls_ssl_set_hostname(&sock->_tls_ctx->_ssl, hostname);
    if (mbedret != 0) {
        _Z_ERROR("Failed to set hostname: -0x%04x", -mbedret);
        _z_tcp_close(&sock->_sock);
        _z_tls_context_free(&sock->_tls_ctx);
        return _Z_ERR_GENERIC;
    }

    mbedtls_ssl_set_bio(&sock->_tls_ctx->_ssl, &sock->_sock._fd, _z_tls_bio_send, _z_tls_bio_recv, NULL);

    while ((mbedret = mbedtls_ssl_handshake(&sock->_tls_ctx->_ssl)) != 0) {
        if (mbedret != MBEDTLS_ERR_SSL_WANT_READ && mbedret != MBEDTLS_ERR_SSL_WANT_WRITE) {
            _Z_ERROR("TLS handshake failed: -0x%04x", -mbedret);
            _z_tcp_close(&sock->_sock);
            _z_tls_context_free(&sock->_tls_ctx);
            return _Z_ERR_GENERIC;
        }
    }

    uint32_t ignored_flags = verify_name ? 0u : MBEDTLS_X509_BADCERT_CN_MISMATCH;
    uint32_t verify_result = mbedtls_ssl_get_verify_result(&sock->_tls_ctx->_ssl);
    if (verify_result != 0) {
        if ((verify_result & ~ignored_flags) != 0u) {
            _Z_ERROR("TLS client certificate verification failed: 0x%08x", verify_result);
            _z_tcp_close(&sock->_sock);
            _z_tls_context_free(&sock->_tls_ctx);
            return _Z_ERR_GENERIC;
        }
        if (!verify_name) {
            _Z_INFO("TLS client name verification disabled; ignoring certificate name mismatch");
        }
    }

    return _Z_RES_OK;
}

z_result_t _z_listen_tls(_z_tls_socket_t *sock, const _z_sys_net_endpoint_t *rep, const _z_tls_config_t *config) {
    if ((rep == NULL) || (rep->_iptcp == NULL)) {
        _Z_ERROR("Invalid lower TCP endpoint for TLS listen");
        return _Z_ERR_GENERIC;
    }

    sock->_is_peer_socket = false;
    bool enable_mtls = config->_enable_mtls;
    sock->_tls_ctx = _z_tls_context_new();
    if (sock->_tls_ctx == NULL) {
        _Z_ERROR("Failed to create TLS context");
        return _Z_ERR_SYSTEM_OUT_OF_MEMORY;
    }

    z_result_t ret = _z_tls_load_ca_certificate(sock->_tls_ctx, config);
    if (ret != _Z_RES_OK) {
        _Z_ERROR("Failed to load CA certificate");
        _z_tls_context_free(&sock->_tls_ctx);
        return ret;
    }

    ret = _z_tls_load_listen_cert(sock->_tls_ctx, config);
    if (ret != _Z_RES_OK) {
        _Z_ERROR("Failed to load listening side certificate");
        _z_tls_context_free(&sock->_tls_ctx);
        return ret;
    }
    sock->_tls_ctx->_enable_mtls = enable_mtls;

    ret = _z_tcp_listen(&sock->_sock, *rep);
    if (ret != _Z_RES_OK) {
        _Z_ERROR("Failed to listen on lower TCP socket for TLS, ret=%d", ret);
        _z_tls_context_free(&sock->_tls_ctx);
        return ret;
    }

    // Needed for _read_socket_f callback which requires TLS context
    sock->_sock._tls_sock = (void *)sock;

    int mbedret = mbedtls_ssl_config_defaults(&sock->_tls_ctx->_ssl_config, MBEDTLS_SSL_IS_SERVER,
                                              MBEDTLS_SSL_TRANSPORT_STREAM, MBEDTLS_SSL_PRESET_DEFAULT);
    if (mbedret != 0) {
        _Z_ERROR("Failed to set SSL config defaults for server: -0x%04x", -mbedret);
        _z_tcp_close(&sock->_sock);
        _z_tls_context_free(&sock->_tls_ctx);
        return _Z_ERR_GENERIC;
    }

    if (sock->_tls_ctx->_ca_cert.version != 0) {
        mbedtls_ssl_conf_ca_chain(&sock->_tls_ctx->_ssl_config, &sock->_tls_ctx->_ca_cert, NULL);
    }

    if (sock->_tls_ctx->_listen_cert.version != 0) {
        mbedret = mbedtls_ssl_conf_own_cert(&sock->_tls_ctx->_ssl_config, &sock->_tls_ctx->_listen_cert,
                                            &sock->_tls_ctx->_listen_key);
        if (mbedret != 0) {
            _Z_ERROR("Failed to configure server certificate: -0x%04x", -mbedret);
            _z_tcp_close(&sock->_sock);
            _z_tls_context_free(&sock->_tls_ctx);
            return _Z_ERR_GENERIC;
        }
    }

    mbedtls_ssl_conf_authmode(&sock->_tls_ctx->_ssl_config,
                              enable_mtls ? MBEDTLS_SSL_VERIFY_REQUIRED : MBEDTLS_SSL_VERIFY_NONE);
    mbedtls_ssl_conf_rng(&sock->_tls_ctx->_ssl_config, mbedtls_hmac_drbg_random, &sock->_tls_ctx->_hmac_drbg);

    return _Z_RES_OK;
}

z_result_t _z_tls_accept(_z_sys_net_socket_t *socket, const _z_sys_net_socket_t *listen_sock) {
    socket->_tls_sock = z_malloc(sizeof(_z_tls_socket_t));
    if (socket->_tls_sock == NULL) {
        _Z_ERROR("Failed to allocate TLS socket structure");
        return _Z_ERR_SYSTEM_OUT_OF_MEMORY;
    }

    _z_tls_socket_t *tls_sock = (_z_tls_socket_t *)socket->_tls_sock;
    tls_sock->_tls_ctx = _z_tls_context_new();
    if (tls_sock->_tls_ctx == NULL) {
        _Z_ERROR("Failed to create TLS context");
        z_free(socket->_tls_sock);
        socket->_tls_sock = NULL;
        return _Z_ERR_SYSTEM_OUT_OF_MEMORY;
    }

    if (listen_sock == NULL) {
        _Z_ERROR("Listening TLS socket is NULL");
        _z_tls_context_free(&tls_sock->_tls_ctx);
        z_free(socket->_tls_sock);
        socket->_tls_sock = NULL;
        return _Z_ERR_GENERIC;
    }

    tls_sock->_sock = *socket;

    mbedtls_ssl_init(&tls_sock->_tls_ctx->_ssl);
    // Setup SSL context using the listen socket's configuration
    _z_tls_socket_t *listen_tls_sock = (_z_tls_socket_t *)listen_sock->_tls_sock;
    if (listen_tls_sock == NULL || listen_tls_sock->_tls_ctx == NULL) {
        _Z_ERROR("Listening TLS socket's TLS context is NULL");
        _z_tls_context_free(&tls_sock->_tls_ctx);
        z_free(socket->_tls_sock);
        socket->_tls_sock = NULL;
        return _Z_ERR_GENERIC;
    }

    tls_sock->_tls_ctx->_enable_mtls = listen_tls_sock->_tls_ctx->_enable_mtls;

    mbedtls_ssl_config *listen_conf = &listen_tls_sock->_tls_ctx->_ssl_config;
    int mbedret = mbedtls_ssl_setup(&tls_sock->_tls_ctx->_ssl, listen_conf);
    if (mbedret != 0) {
        _Z_ERROR("Failed to setup SSL: -0x%04x", -mbedret);
        _z_tls_context_free(&tls_sock->_tls_ctx);
        z_free(socket->_tls_sock);
        socket->_tls_sock = NULL;
        return _Z_ERR_GENERIC;
    }

    mbedtls_ssl_set_bio(&tls_sock->_tls_ctx->_ssl, &tls_sock->_sock._fd, _z_tls_bio_send, _z_tls_bio_recv, NULL);

    while ((mbedret = mbedtls_ssl_handshake(&tls_sock->_tls_ctx->_ssl)) != 0) {
        if (mbedret != MBEDTLS_ERR_SSL_WANT_READ && mbedret != MBEDTLS_ERR_SSL_WANT_WRITE) {
            _Z_ERROR("TLS server handshake failed: -0x%04x", -mbedret);
            _z_tls_context_free(&tls_sock->_tls_ctx);
            z_free(socket->_tls_sock);
            socket->_tls_sock = NULL;
            return _Z_ERR_GENERIC;
        }
    }
    uint32_t verify_result = mbedtls_ssl_get_verify_result(&tls_sock->_tls_ctx->_ssl);
    if (verify_result != 0) {
        uint32_t allowed_flags = 0u;
        if (!tls_sock->_tls_ctx->_enable_mtls) {
            allowed_flags |= MBEDTLS_X509_BADCERT_SKIP_VERIFY;
        }
        if ((verify_result & ~allowed_flags) != 0u) {
            _Z_ERROR("TLS client certificate verification failed: 0x%08x", verify_result);
            _z_tls_context_free(&tls_sock->_tls_ctx);
            z_free(socket->_tls_sock);
            socket->_tls_sock = NULL;
            return _Z_ERR_GENERIC;
        }
    }

    tls_sock->_is_peer_socket = true;
    tls_sock->_sock._tls_sock = (void *)tls_sock;
    socket->_fd = tls_sock->_sock._fd;
    socket->_tls_sock = (void *)tls_sock;
    return _Z_RES_OK;
}

void _z_close_tls_socket(_z_sys_net_socket_t *socket) {
    if (socket == NULL) {
        return;
    }
    if (socket->_tls_sock == NULL) {
        return;
    }

    _z_tls_socket_t *tls_sock = (_z_tls_socket_t *)socket->_tls_sock;
    bool peer_socket = tls_sock->_is_peer_socket;
    _z_close_tls(tls_sock);
    if (peer_socket) {
        z_free(tls_sock);
    }

    socket->_tls_sock = NULL;
    socket->_fd = -1;
}

void _z_close_tls(_z_tls_socket_t *sock) {
    if (sock->_tls_ctx != NULL) {
        mbedtls_ssl_close_notify(&sock->_tls_ctx->_ssl);
        _z_tls_context_free(&sock->_tls_ctx);
    }
    _z_tcp_close(&sock->_sock);
    sock->_sock._tls_sock = NULL;
}

size_t _z_read_tls(const _z_tls_socket_t *sock, uint8_t *ptr, size_t len) {
    if (sock->_tls_ctx == NULL) {
        _Z_ERROR("TLS context is NULL");
        return SIZE_MAX;
    }

    int ret = mbedtls_ssl_read(&sock->_tls_ctx->_ssl, ptr, len);
    if (ret > 0) {
        return (size_t)ret;
    }

    if (ret == MBEDTLS_ERR_SSL_WANT_READ || ret == MBEDTLS_ERR_SSL_WANT_WRITE) {
        return 0;
    }

    if (ret == 0 || ret == MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY || ret == MBEDTLS_ERR_SSL_CONN_EOF) {
        return SIZE_MAX;
    }

    _Z_ERROR("TLS read error: %d", ret);
    return SIZE_MAX;
}

size_t _z_write_tls(const _z_tls_socket_t *sock, const uint8_t *ptr, size_t len) {
    if (sock->_tls_ctx == NULL) {
        _Z_ERROR("TLS context is NULL");
        return SIZE_MAX;
    }
    int ret = mbedtls_ssl_write(&sock->_tls_ctx->_ssl, ptr, len);
    if (ret > 0) {
        return (size_t)ret;
    }

    if (ret == MBEDTLS_ERR_SSL_WANT_READ || ret == MBEDTLS_ERR_SSL_WANT_WRITE) {
        return 0;
    }

    _Z_ERROR("TLS write error: -0x%04x", -ret);
    return SIZE_MAX;
}

size_t _z_write_all_tls(const _z_tls_socket_t *sock, const uint8_t *ptr, size_t len) {
    size_t n = 0;
    do {
        size_t wb = _z_write_tls(sock, &ptr[n], len - n);
        if (wb == SIZE_MAX) {
            return wb;
        }
        n += wb;
    } while (n < len);
    return n;
}

#endif  // Z_FEATURE_LINK_TLS == 1
