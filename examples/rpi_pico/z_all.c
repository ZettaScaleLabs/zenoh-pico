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

#include <stdio.h>
#include <zenoh-pico.h>

#include "config.h"
#include "zenoh-pico/api/types.h"
#include "zenoh-pico/system/common/platform.h"

#define QUERY_COUNT 10
#define PUBLICATION_COUNT 10
#define QUERYABLE_COUNT 10
#define SUBSCRIPTION_COUNT 10

#define BASE_TIMEOUT_MS 1000

#if Z_FEATURE_QUERY == 1
#define QUERY_KEYEXPR "demo/example/zenoh-pico-query"
#define QUERY_VALUE ""

void reply_dropper(void *ctx) {
    (void)(ctx);
    printf(">> Received query final notification\n");
}

void reply_handler(z_loaned_reply_t *reply, void *ctx) {
    (void)(ctx);
    if (z_reply_is_ok(reply)) {
        const z_loaned_sample_t *sample = z_reply_ok(reply);
        z_view_string_t keystr;
        z_keyexpr_as_view_string(z_sample_keyexpr(sample), &keystr);
        z_owned_string_t replystr;
        z_bytes_to_string(z_sample_payload(sample), &replystr);

        printf(">> Received ('%.*s': '%.*s')\n", (int)z_string_len(z_loan(keystr)), z_string_data(z_loan(keystr)),
               (int)z_string_len(z_loan(replystr)), z_string_data(z_loan(replystr)));
        z_drop(z_move(replystr));
    } else {
        printf(">> Received an error\n");
    }
}
#endif

#if Z_FEATURE_PUBLICATION == 1
#define PUBLICATION_KEYEXPR "demo/example/zenoh-pico-pub"
#define PUBLICATION_VALUE "[RPI] Pub from Zenoh-Pico!"
#endif

#if Z_FEATURE_QUERYABLE == 1
#define QUERYABLE_KEYEXPR "demo/example/zenoh-pico-queryable"
#define QUERYABLE_VALUE "[RPI] Queryable from Zenoh-Pico!"

void query_handler(z_loaned_query_t *query, void *ctx) {
    (void)(ctx);
    z_view_string_t keystr;
    z_keyexpr_as_view_string(z_query_keyexpr(query), &keystr);
    z_view_string_t params;
    z_query_parameters(query, &params);
    printf(" >> [Queryable handler] Received Query '%.*s%.*s'\n", (int)z_string_len(z_loan(keystr)),
           z_string_data(z_loan(keystr)), (int)z_string_len(z_loan(params)), z_string_data(z_loan(params)));
    // Process value
    z_owned_string_t payload_string;
    z_bytes_to_string(z_query_payload(query), &payload_string);
    if (z_string_len(z_loan(payload_string)) > 0) {
        printf("     with value '%.*s'\n", (int)z_string_len(z_loan(payload_string)),
               z_string_data(z_loan(payload_string)));
    }
    z_drop(z_move(payload_string));

    z_query_reply_options_t options;
    z_query_reply_options_default(&options);
    // Reply value encoding
    z_owned_bytes_t reply_payload;
    z_bytes_from_static_str(&reply_payload, QUERYABLE_VALUE);

    z_query_reply(query, z_query_keyexpr(query), z_move(reply_payload), &options);
}
#endif

#if Z_FEATURE_SUBSCRIPTION == 1
#define SUBSCRIPTION_KEYEXPR "demo/example/**"

void data_handler(z_loaned_sample_t *sample, void *ctx) {
    (void)(ctx);
    z_view_string_t keystr;
    z_keyexpr_as_view_string(z_sample_keyexpr(sample), &keystr);
    z_owned_string_t value;
    z_bytes_to_string(z_sample_payload(sample), &value);
    printf(">> [Subscriber] Received ('%.*s': '%.*s')\n", (int)z_string_len(z_loan(keystr)),
           z_string_data(z_loan(keystr)), (int)z_string_len(z_loan(value)), z_string_data(z_loan(value)));
    z_drop(z_move(value));
}
#endif

void print_memory_usage(void) {
    size_t free_heap_size = xPortGetFreeHeapSize();
    size_t min_free_heap_size = xPortGetMinimumEverFreeHeapSize();

    printf("Current free heap size: %zu bytes\n", free_heap_size);
    printf("Minimum ever free heap size: %zu bytes\n", min_free_heap_size);
}

void app_main(void) {
    print_memory_usage();

    static z_task_attr_t task_attr = {
        .name = "",
        .priority = configMAX_PRIORITIES / 2,
        .stack_depth = 512,
    };

    z_owned_config_t config;
    z_config_default(&config);
    zp_config_insert(z_loan_mut(config), Z_CONFIG_MODE_KEY, ZENOH_CONFIG_MODE);
    if (strcmp(ZENOH_CONFIG_CONNECT, "") != 0) {
        printf("Connect endpoint: %s\n", ZENOH_CONFIG_CONNECT);
        zp_config_insert(z_loan_mut(config), Z_CONFIG_CONNECT_KEY, ZENOH_CONFIG_CONNECT);
    }
    if (strcmp(ZENOH_CONFIG_LISTEN, "") != 0) {
        printf("Listen endpoint: %s\n", ZENOH_CONFIG_LISTEN);
        zp_config_insert(z_loan_mut(config), Z_CONFIG_LISTEN_KEY, ZENOH_CONFIG_LISTEN);
    }

    printf("Opening %s session ...\n", ZENOH_CONFIG_MODE);
    z_owned_session_t s;
    if (z_open(&s, z_move(config), NULL) < 0) {
        printf("Unable to open session!\n");
        return;
    }

    // Start read and lease tasks for zenoh-pico
    zp_task_read_options_t readopts = {.task_attributes = &task_attr};
    zp_task_lease_options_t leaseopts = {.task_attributes = &task_attr};
    if (zp_start_read_task(z_loan_mut(s), &readopts) < 0 || zp_start_lease_task(z_loan_mut(s), &leaseopts) < 0) {
        printf("Unable to start read and lease tasks\n");
        z_drop(z_move(s));
        return;
    }

#if Z_FEATURE_QUERY == 1
    z_view_keyexpr_t gke;
    if (z_view_keyexpr_from_str(&gke, QUERY_KEYEXPR) < 0) {
        printf("%s is not a valid key expression", QUERY_KEYEXPR);
        return;
    }
#endif

#if Z_FEATURE_PUBLICATION == 1
    z_view_keyexpr_t pke;
    z_view_keyexpr_from_str_unchecked(&pke, PUBLICATION_KEYEXPR);
    z_owned_publisher_t pub[PUBLICATION_COUNT];
    for (size_t i = 0; i != PUBLICATION_COUNT; ++i) {
        printf("Declaring publisher for '%s'...\n", PUBLICATION_KEYEXPR);
        if (z_declare_publisher(z_loan(s), &pub[i], z_loan(pke), NULL) < 0) {
            printf("Unable to declare publisher for key expression!\n");
            return;
        }
    }
#endif

#if Z_FEATURE_QUERYABLE == 1
    z_view_keyexpr_t ke;
    if (z_view_keyexpr_from_str(&ke, QUERYABLE_KEYEXPR) < 0) {
        printf("%s is not a valid key expression", QUERYABLE_KEYEXPR);
        return;
    }

    z_owned_queryable_t qable[QUERYABLE_COUNT];

    for (size_t i = 0; i != QUERYABLE_COUNT; ++i) {
        z_owned_closure_query_t callback;
        z_closure(&callback, query_handler, NULL, NULL);
        printf("Creating Queryable on '%s'...\n", QUERYABLE_KEYEXPR);
        if (z_declare_queryable(z_loan(s), &qable[i], z_loan(ke), z_move(callback), NULL) < 0) {
            printf("Unable to create queryable.\n");
            return;
        }
    }
#endif

#if Z_FEATURE_SUBSCRIPTION == 1
    z_view_keyexpr_t ske;
    z_view_keyexpr_from_str_unchecked(&ske, SUBSCRIPTION_KEYEXPR);

    for (size_t i = 0; i != SUBSCRIPTION_COUNT; ++i) {
        z_owned_closure_sample_t callback;
        z_closure(&callback, data_handler, NULL, NULL);
        printf("Declaring Subscriber on '%s'...\n", SUBSCRIPTION_KEYEXPR);
        z_owned_subscriber_t sub[SUBSCRIPTION_COUNT];
        if (z_declare_subscriber(z_loan(s), &sub[i], z_loan(ske), z_move(callback), NULL) < 0) {
            printf("Unable to declare subscriber.\n");
            return;
        }
    }
#endif

    char buf[256];
    for (int idx = 0; 1; ++idx) {
        z_sleep_ms(BASE_TIMEOUT_MS);
        print_memory_usage();

#if Z_FEATURE_QUERY == 1
        for (size_t i = 0; i != QUERY_COUNT; ++i) {
            printf("Sending Query '%s'...\n", QUERY_KEYEXPR);
            z_get_options_t opts;
            z_get_options_default(&opts);
            // Value encoding
            z_owned_bytes_t payload;
            if (strcmp(QUERY_VALUE, "") != 0) {
                z_bytes_from_static_str(&payload, QUERY_VALUE);
                opts.payload = z_move(payload);
            }
            z_owned_closure_reply_t callback;
            z_closure(&callback, reply_handler, reply_dropper, NULL);
            if (z_get(z_loan(s), z_loan(gke), "", z_move(callback), &opts) < 0) {
                printf("Unable to send query.\n");
                return;
            }
        }
#endif

#if Z_FEATURE_PUBLICATION == 1
        for (size_t i = 0; i != PUBLICATION_COUNT; ++i) {
            snprintf(buf, 256, "[%4d] %s", idx, PUBLICATION_VALUE);
            printf("Putting Data ('%s': '%s')...\n", PUBLICATION_KEYEXPR, buf);

            // Create payload
            z_owned_bytes_t payload;
            z_bytes_copy_from_str(&payload, buf);

            z_publisher_put_options_t options;
            z_publisher_put_options_default(&options);
            z_publisher_put(z_loan(pub[i]), z_move(payload), &options);
        }
#endif
    }
}
