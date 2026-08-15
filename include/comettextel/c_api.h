/**
 * @file c_api.h
 * @brief Stable C ABI for CometTextel (P/Invoke / NuGet friendly).
 *
 * @author Ji-Feng Tsai (Jiowcl)
 * @email jiowcl@gmail.com
 * @copyright Copyright (c) 2026 Jiowcl. All rights reserved.
 */

#ifndef COMETTEXTEL_C_API_H
#define COMETTEXTEL_C_API_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#if defined(COMETTEXTEL_STATIC)
#  define CT_API
#elif defined(_WIN32) || defined(__CYGWIN__)
#  if defined(COMETTEXTEL_BUILDING)
#    define CT_API __declspec(dllexport)
#  else
#    define CT_API __declspec(dllimport)
#  endif
#elif defined(__GNUC__) && __GNUC__ >= 4
#  define CT_API __attribute__((visibility("default")))
#else
#  define CT_API
#endif

/** @brief Status codes compatible with comettextel::Errc values. */
enum ct_status {
    CT_OK = 0,
    CT_ERR_INVALID_ARGUMENT = 1,
    CT_ERR_NOT_OPEN = 2,
    CT_ERR_ALREADY_OPEN = 3,
    CT_ERR_IO = 4,
    CT_ERR_TIMEOUT = 5,
    CT_ERR_MODEM_REJECTED = 6,
    CT_ERR_ENCODE = 7,
    CT_ERR_DECODE = 8,
    CT_ERR_UNSUPPORTED = 9,
    CT_ERR_UNKNOWN = 100
};

/** @brief TP-DCS values. */
enum ct_dcs {
    CT_DCS_GSM7 = 0,
    CT_DCS_8BIT = 4,
    CT_DCS_UCS2 = 8
};

/** @brief Opaque modem handle. */
typedef struct ct_modem ct_modem;

/** @brief Decoded / listed message (UTF-8 text fields). */
typedef struct ct_message {
    int32_t index;
    int32_t dcs;
    int32_t has_udh;
    char service_center[32];
    char peer_address[32];
    char service_timestamp[32];
    char user_data[512];
    /** @brief 1 when a concatenated SMS IE was found in the UDH. */
    int32_t is_concatenated;
    /** @brief Concatenation reference (8- or 16-bit IE). */
    int32_t concat_ref;
    /** @brief Total segment count (valid when is_concatenated != 0). */
    int32_t concat_total;
    /** @brief 1-based segment index; 0 when list/reassembly joined a complete set. */
    int32_t concat_seq;
} ct_message;

/**
 * @brief Returns a static English description for @p status.
 * @param status The status code.
 * @return The status string.
 */
CT_API const char* ct_status_string(int status);

/**
 * @brief Creates a modem instance. Free with @ref ct_modem_destroy.
 * @return The modem object.
 */
CT_API ct_modem* ct_modem_create(void);

/**
 * @brief Destroys a modem instance (safe with NULL).
 * @param modem The modem object.
 */
CT_API void ct_modem_destroy(ct_modem* modem);

/**
 * @brief Opens the serial port and initializes the modem (PDU mode).
 * @param port Device name, e.g. "COM3" or "/dev/ttyUSB0".
 * @param baud_rate Baud rate (for example 115200).
 * @return The status code.
 */
CT_API int ct_modem_open(ct_modem* modem, const char* port, uint32_t baud_rate);

/**
 * @brief Sends one SMS.
 * @param smsc SMSC digits (empty string allowed for modem default).
 * @param destination Destination digits.
 * @param text UTF-8 message body.
 * @param dcs One of @ref ct_dcs.
 * @param timeout_ms Final OK/ERROR wait after Ctrl-Z.
 * @return The status code.
 */
CT_API int ct_modem_send(ct_modem* modem,
                         const char* smsc,
                         const char* destination,
                         const char* text,
                         int dcs,
                         int timeout_ms);

/**
 * @brief Lists stored messages into @p out (up to @p max_count).
 * @param modem The modem object.
 * @param out The destination message list.
 * @param max_count The maximum number of messages to list.
 * @param out_count Receives the number of messages written.
 * @param timeout_ms The timeout in milliseconds.
 * @return The status code.
 *
 * @note Complete concatenated-SMS sets are rejoined (concat_seq == 0).
 */
CT_API int ct_modem_list(ct_modem* modem,
                         ct_message* out,
                         int max_count,
                         int* out_count,
                         int timeout_ms);

/**
 * @brief Deletes a stored message and waits for OK/ERROR.
 * @param modem The modem object.
 * @param index The index of the message to delete.
 * @param timeout_ms The timeout in milliseconds.
 * @return The status code.
 */
CT_API int ct_modem_delete(ct_modem* modem, int index, int timeout_ms);

/**
 * @brief Encodes a submit PDU hex string (no modem I/O).
 * @param smsc The service center address.
 * @param destination The destination address.
 * @param text The message text.
 * @param dcs The data coding scheme.
 * @param out_hex The destination PDU hex string.
 * @param out_hex_cap The destination PDU hex string capacity.
 * @return CT_OK on success; writes NUL-terminated hex into @p out_hex.
 */
CT_API int ct_pdu_encode_submit(const char* smsc,
                                const char* destination,
                                const char* text,
                                int dcs,
                                char* out_hex,
                                size_t out_hex_cap);

/**
 * @brief Encodes one or more submit PDUs (auto-splits with concat UDH when needed).
 * @param out_hex Receives newline-separated hex strings, NUL-terminated.
 * @param out_hex_cap Capacity of @p out_hex including the trailing NUL.
 * @param out_count Receives the number of segments (1 or more).
 * @return CT_OK on success.
 */
CT_API int ct_pdu_encode_submit_segments(const char* smsc,
                                         const char* destination,
                                         const char* text,
                                         int dcs,
                                         char* out_hex,
                                         size_t out_hex_cap,
                                         int* out_count);

/**
 * @brief Decodes a PDU hex string into @p out (no modem I/O).
 * @param pdu_hex The PDU hex string.
 * @param out The destination message struct.
 * @return The status code.
 */
CT_API int ct_pdu_decode(const char* pdu_hex, ct_message* out);

#ifdef __cplusplus
}
#endif

#endif /* COMETTEXTEL_C_API_H */
