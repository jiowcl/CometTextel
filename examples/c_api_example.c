/**
 * @file c_api_example.c
 * @brief Minimal C consumer of the CometTextel C ABI (PDU + optional modem).
 *
 * Usage:
 *   comettextel_c_api_example pdu <destination> <text> [smsc]
 *   comettextel_c_api_example list <port> [baud]
 *   comettextel_c_api_example send <port> <smsc> <destination> <text> [baud]
 *   comettextel_c_api_example delete <port> <index> [baud]
 *
 * @author Ji-Feng Tsai (Jiowcl)
 * @copyright Copyright (c) 2026 Jiowcl. All rights reserved.
 */

#include "comettextel/c_api.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * @brief Prints usage information.
 */
static void print_usage(void)
{
    fprintf(stderr,
            "Usage:\n"
            "  comettextel_c_api_example pdu <destination> <text> [smsc]\n"
            "  comettextel_c_api_example list <port> [baud]\n"
            "  comettextel_c_api_example send <port> <smsc> <destination> <text> [baud]\n"
            "  comettextel_c_api_example delete <port> <index> [baud]\n");
}

/**
 * @brief Prints a failure status and returns a failure code.
 * @param status The status code.
 * @param what The what.
 * @return The failure code.
 */
static int fail_status(int status, const char* what)
{
    fprintf(stderr, "%s failed (%d): %s\n", what, status, ct_status_string(status));
    return 2;
}

/**
 * @brief Encodes a submit PDU hex string.
 * @param argc The number of arguments.
 * @param argv The arguments.
 * @return The status code.
 */
static int run_pdu(int argc, char** argv)
{
    const char* destination = NULL;
    const char* text = NULL;
    const char* smsc = "";
    char hex[65536];
    int count = 0;
    int status = 0;
    char* cursor = NULL;

    if (argc < 4) {
        print_usage();
        return 1;
    }

    destination = argv[2];
    text = argv[3];
    if (argc >= 5) {
        smsc = argv[4];
    }

    status = ct_pdu_encode_submit_segments(smsc, destination, text, CT_DCS_UCS2, hex, sizeof(hex),
                                           &count);
    if (status != CT_OK) {
        return fail_status(status, "ct_pdu_encode_submit_segments");
    }

    cursor = hex;
    while (*cursor != '\0') {
        char* nl = strchr(cursor, '\n');
        ct_message msg;
        char saved = 0;

        if (nl != NULL) {
            saved = *nl;
            *nl = '\0';
        }

        printf("%s\n", cursor);

        memset(&msg, 0, sizeof(msg));
        status = ct_pdu_decode(cursor, &msg);
        if (status != CT_OK) {
            return fail_status(status, "ct_pdu_decode");
        }

        printf("peer=%s text=%s dcs=%d has_udh=%d concat=%d ref=%d total=%d seq=%d\n",
               msg.peer_address,
               msg.user_data,
               msg.dcs,
               msg.has_udh,
               msg.is_concatenated,
               msg.concat_ref,
               msg.concat_total,
               msg.concat_seq);

        if (nl == NULL) {
            break;
        }
        *nl = saved;
        cursor = nl + 1;
    }

    printf("segments=%d\n", count);
    return 0;
}

/**
 * @brief Parses a baud rate from a string.
 * @param text The text to parse.
 * @param fallback The fallback baud rate.
 * @return The baud rate.
 */
static uint32_t parse_baud(const char* text, uint32_t fallback)
{
    unsigned long value = 0;
    char* end = NULL;

    if (text == NULL || text[0] == '\0') {
        return fallback;
    }

    value = strtoul(text, &end, 10);
    if (end == text || *end != '\0' || value == 0UL || value > 0xFFFFFFFFUL) {
        return fallback;
    }
    return (uint32_t)value;
}

/**
 * @brief Lists stored messages.
 * @param argc The number of arguments.
 * @param argv The arguments.
 * @return The status code.
 */
static int run_list(int argc, char** argv)
{
    const char* port = NULL;
    uint32_t baud = 115200U;
    ct_modem* modem = NULL;
    ct_message messages[64];
    int count = 0;
    int status = 0;
    int i = 0;

    if (argc < 3) {
        print_usage();
        return 1;
    }

    port = argv[2];
    if (argc >= 4) {
        baud = parse_baud(argv[3], baud);
    }

    modem = ct_modem_create();
    if (modem == NULL) {
        fprintf(stderr, "ct_modem_create failed\n");
        return 2;
    }

    status = ct_modem_open(modem, port, baud);
    if (status != CT_OK) {
        ct_modem_destroy(modem);
        return fail_status(status, "ct_modem_open");
    }

    status = ct_modem_list(modem, messages, 64, &count, 8000);
    if (status != CT_OK) {
        ct_modem_destroy(modem);
        return fail_status(status, "ct_modem_list");
    }

    for (i = 0; i < count; ++i) {
        printf("[%d] %s: %s\n", messages[i].index, messages[i].peer_address, messages[i].user_data);
    }

    ct_modem_destroy(modem);
    return 0;
}

/**
 * @brief Sends a message to a destination.
 * @param argc The number of arguments.
 * @param argv The arguments.
 * @return The status code.
 */
static int run_send(int argc, char** argv)
{
    const char* port = NULL;
    const char* smsc = NULL;
    const char* destination = NULL;
    const char* text = NULL;
    uint32_t baud = 115200U;
    ct_modem* modem = NULL;
    int status = 0;

    if (argc < 6) {
        print_usage();
        return 1;
    }

    port = argv[2];
    smsc = argv[3];
    destination = argv[4];
    text = argv[5];
    if (argc >= 7) {
        baud = parse_baud(argv[6], baud);
    }

    modem = ct_modem_create();
    if (modem == NULL) {
        fprintf(stderr, "ct_modem_create failed\n");
        return 2;
    }

    status = ct_modem_open(modem, port, baud);
    if (status != CT_OK) {
        ct_modem_destroy(modem);
        return fail_status(status, "ct_modem_open");
    }

    status = ct_modem_send(modem, smsc, destination, text, CT_DCS_UCS2, 10000);
    if (status != CT_OK) {
        ct_modem_destroy(modem);
        return fail_status(status, "ct_modem_send");
    }

    printf("Sent.\n");
    ct_modem_destroy(modem);
    return 0;
}

/**
 * @brief Deletes a stored message and waits for OK/ERROR.
 * @param argc The number of arguments.
 * @param argv The arguments.
 * @return The status code.
 */
static int run_delete(int argc, char** argv)
{
    const char* port = NULL;
    int index = 0;
    uint32_t baud = 115200U;
    ct_modem* modem = NULL;
    int status = 0;

    if (argc < 4) {
        print_usage();
        return 1;
    }

    port = argv[2];
    index = atoi(argv[3]);
    if (argc >= 5) {
        baud = parse_baud(argv[4], baud);
    }

    modem = ct_modem_create();
    if (modem == NULL) {
        fprintf(stderr, "ct_modem_create failed\n");
        return 2;
    }

    status = ct_modem_open(modem, port, baud);
    if (status != CT_OK) {
        ct_modem_destroy(modem);
        return fail_status(status, "ct_modem_open");
    }

    status = ct_modem_delete(modem, index, 5000);
    if (status != CT_OK) {
        ct_modem_destroy(modem);
        return fail_status(status, "ct_modem_delete");
    }

    printf("Deleted index %d.\n", index);
    ct_modem_destroy(modem);
    return 0;
}

/**
 * @brief Main function.
 * @param argc The number of arguments.
 * @param argv The arguments.
 * @return The status code.
 */
int main(int argc, char** argv)
{
    if (argc < 2) {
        print_usage();
        return 1;
    }

    if (strcmp(argv[1], "pdu") == 0) {
        return run_pdu(argc, argv);
    }
    if (strcmp(argv[1], "list") == 0) {
        return run_list(argc, argv);
    }
    if (strcmp(argv[1], "send") == 0) {
        return run_send(argc, argv);
    }
    if (strcmp(argv[1], "delete") == 0) {
        return run_delete(argc, argv);
    }

    fprintf(stderr, "Unknown command: %s\n", argv[1]);
    print_usage();
    return 1;
}
