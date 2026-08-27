// CometTextel.NET - CometTextel API for .NET
// Copyright (c) 2026 Jiowcl. All rights reserved.

using System.Runtime.InteropServices;
using System.Text;

namespace CometTextel.NET.Core.Native
{
    /// <summary>
    /// Native methods for the CometTextel C ABI.
    /// </summary>
    internal static class NativeMethods
    {
        // DLL name for the CometTextel library.
        internal const string DllName = "comettextel";

        /// <summary>
        /// Blittable mirror of <c>ct_message</c> (UTF-8 byte fields).
        /// </summary>
        [StructLayout(LayoutKind.Sequential)]
        public struct CtMessage
        {
            /// <summary>
            /// Index of the message.
            /// </summary>
            public int Index;

            /// <summary>
            /// Data coding scheme.
            /// </summary>
            public int Dcs;

            /// <summary>
            /// True when a UDH was found in the message.
            /// </summary>
            public int HasUdh;

            /// <summary>
            /// Service center.
            /// </summary>
            [MarshalAs(UnmanagedType.ByValArray, SizeConst = 32)]
            public byte[] ServiceCenter;

            /// <summary>
            /// Peer address.
            /// </summary>
            [MarshalAs(UnmanagedType.ByValArray, SizeConst = 32)]
            public byte[] PeerAddress;

            /// <summary>
            /// Service timestamp.
            /// </summary>
            [MarshalAs(UnmanagedType.ByValArray, SizeConst = 32)]
            public byte[] ServiceTimestamp;

            /// <summary>
            /// UTF-8 user data.
            /// </summary>
            [MarshalAs(UnmanagedType.ByValArray, SizeConst = 512)]
            public byte[] UserData;

            /// <summary>
            /// True when a concatenated SMS IE was found in the UDH.
            /// </summary>
            public int IsConcatenated;

            /// <summary>
            /// Concatenation reference number (valid when <see cref="IsConcatenated"/> != 0).
            /// </summary>
            public int ConcatRef;

            /// <summary>
            /// Total segment count (valid when <see cref="IsConcatenated"/> != 0).
            /// </summary>
            public int ConcatTotal;

            /// <summary>
            /// 1-based segment index (valid when <see cref="IsConcatenated"/> != 0).
            /// </summary>
            public int ConcatSeq;
        }

        /// <summary>
        /// Gets the status string for a given status code.
        /// </summary>
        /// <param name="status">The status code.</param>
        /// <returns>The status string.</returns>
        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        public static extern IntPtr ct_status_string(int status);

        /// <summary>
        /// Creates a modem.
        /// </summary>
        /// <returns>The modem.</returns>
        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        public static extern IntPtr ct_modem_create();

        /// <summary>
        /// Destroys a modem.
        /// </summary>
        /// <param name="modem">The modem to destroy.</param>
        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        public static extern void ct_modem_destroy(IntPtr modem);

        /// <summary>
        /// Opens a modem.
        /// </summary>
        /// <param name="modem">The modem to open.</param>
        /// <param name="port">The port to open.</param>
        /// <param name="baudRate">The baud rate to use.</param>
        /// <returns>The error code.</returns>
        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        public static extern int ct_modem_open(
            IntPtr modem,
            [MarshalAs(UnmanagedType.LPUTF8Str)] string port,
            uint baudRate);

        /// <summary>
        /// Sends a message to the modem.
        /// </summary>
        /// <param name="modem">The modem to send the message to.</param>
        /// <param name="smsc">The SMSC address.</param>
        /// <param name="destination">The destination address.</param>
        /// <param name="text">The text to send.</param>
        /// <param name="dcs">The data coding scheme.</param>
        /// <param name="timeoutMs">The timeout in milliseconds.</param>
        /// <returns>The error code.</returns>
        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        public static extern int ct_modem_send(
            IntPtr modem,
            [MarshalAs(UnmanagedType.LPUTF8Str)] string? smsc,
            [MarshalAs(UnmanagedType.LPUTF8Str)] string destination,
            [MarshalAs(UnmanagedType.LPUTF8Str)] string text,
            int dcs,
            int timeoutMs);

        /// <summary>
        /// Sends an SMS with explicit relative TP-VP and TP-SRR options.
        /// </summary>
        /// <param name="modem">The modem to send the message to.</param>
        /// <param name="smsc">The SMSC address.</param>
        /// <param name="destination">The destination address.</param>
        /// <param name="text">The text to send.</param>
        /// <param name="dcs">The data coding scheme.</param>
        /// <param name="relativeValidityPeriod">The relative validity period.</param>
        /// <param name="requestStatusReport">The request status report.</param>
        /// <param name="timeoutMs">The timeout in milliseconds.</param>
        /// <returns>The error code.</returns>
        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        public static extern int ct_modem_send_ex(
            IntPtr modem,
            [MarshalAs(UnmanagedType.LPUTF8Str)] string? smsc,
            [MarshalAs(UnmanagedType.LPUTF8Str)] string destination,
            [MarshalAs(UnmanagedType.LPUTF8Str)] string text,
            int dcs,
            int relativeValidityPeriod,
            int requestStatusReport,
            int timeoutMs);

        /// <summary>
        /// Lists messages from the modem.
        /// </summary>
        /// <param name="modem">The modem to list messages from.</param>
        /// <param name="outMessages">The output messages.</param>
        /// <param name="maxCount">The maximum number of messages to list.</param>
        /// <param name="outCount">The number of messages listed.</param>
        /// <param name="timeoutMs">The timeout in milliseconds.</param>
        /// <returns>The error code.</returns>
        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        public static extern int ct_modem_list(
            IntPtr modem,
            [Out] CtMessage[] outMessages,
            int maxCount,
            out int outCount,
            int timeoutMs);

        /// <summary>
        /// Deletes a message from the modem.
        /// </summary>
        /// <param name="modem">The modem to delete the message from.</param>
        /// <param name="index">The index of the message to delete.</param>
        /// <param name="timeoutMs">The timeout in milliseconds.</param>
        /// <returns>The error code.</returns>
        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        public static extern int ct_modem_delete(IntPtr modem, int index, int timeoutMs);

        /// <summary>
        /// Encodes a PDU hex string into a <c>ct_message</c>.
        /// </summary>
        /// <param name="smsc">The SMSC address.</param>
        /// <param name="destination">The destination address.</param>
        /// <param name="text">The text to encode.</param>
        /// <param name="dcs">The data coding scheme.</param>
        /// <param name="outHex">The encoded PDU hex string.</param>
        /// <param name="outHexCap">The capacity of the output buffer.</param>
        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        public static extern int ct_pdu_encode_submit(
            [MarshalAs(UnmanagedType.LPUTF8Str)] string? smsc,
            [MarshalAs(UnmanagedType.LPUTF8Str)] string destination,
            [MarshalAs(UnmanagedType.LPUTF8Str)] string text,
            int dcs,
            byte[] outHex,
            UIntPtr outHexCap);

        /// <summary>
        /// Encodes a submit PDU with explicit relative TP-VP and TP-SRR options.
        /// Pass -1 for <paramref name="relativeValidityPeriod"/> to omit TP-VP.
        /// </summary>
        /// <param name="smsc">The SMSC address.</param>
        /// <param name="destination">The destination address.</param>
        /// <param name="text">The text to encode.</param>
        /// <param name="dcs">The data coding scheme.</param>
        /// <param name="relativeValidityPeriod">The relative validity period.</param>
        /// <param name="requestStatusReport">The request status report.</param>
        /// <param name="outHex">The encoded PDU hex string.</param>
        /// <param name="outHexCap">The capacity of the output buffer.</param>
        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        public static extern int ct_pdu_encode_submit_ex(
            [MarshalAs(UnmanagedType.LPUTF8Str)] string? smsc,
            [MarshalAs(UnmanagedType.LPUTF8Str)] string destination,
            [MarshalAs(UnmanagedType.LPUTF8Str)] string text,
            int dcs,
            int relativeValidityPeriod,
            int requestStatusReport,
            byte[] outHex,
            UIntPtr outHexCap);

        /// <summary>
        /// Encodes one or more submit PDUs (newline-separated hex). Auto-splits with concat UDH.
        /// </summary>
        /// <param name="smsc">The SMSC address.</param>
        /// <param name="destination">The destination address.</param>
        /// <param name="text">The text to encode.</param>
        /// <param name="dcs">The data coding scheme.</param>
        /// <param name="outHex">The encoded PDU hex string.</param>
        /// <param name="outHexCap">The capacity of the output buffer.</param>
        /// <param name="outCount">The number of PDU hex strings encoded.</param>
        /// <returns>The error code.</returns>
        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        public static extern int ct_pdu_encode_submit_segments(
            [MarshalAs(UnmanagedType.LPUTF8Str)] string? smsc,
            [MarshalAs(UnmanagedType.LPUTF8Str)] string destination,
            [MarshalAs(UnmanagedType.LPUTF8Str)] string text,
            int dcs,
            byte[] outHex,
            UIntPtr outHexCap,
            out int outCount);

        /// <summary>
        /// Encodes submit PDUs with explicit relative TP-VP and TP-SRR options.
        /// </summary>
        /// <param name="smsc">The SMSC address.</param>
        /// <param name="destination">The destination address.</param>
        /// <param name="text">The text to encode.</param>
        /// <param name="dcs">The data coding scheme.</param>
        /// <param name="relativeValidityPeriod">The relative validity period.</param>
        /// <param name="requestStatusReport">The request status report.</param>
        /// <param name="outHex">The encoded PDU hex string.</param>
        /// <param name="outHexCap">The capacity of the output buffer.</param>
        /// <param name="outCount">The number of PDU hex strings encoded.</param>
        /// <returns>The error code.</returns>
        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        public static extern int ct_pdu_encode_submit_segments_ex(
            [MarshalAs(UnmanagedType.LPUTF8Str)] string? smsc,
            [MarshalAs(UnmanagedType.LPUTF8Str)] string destination,
            [MarshalAs(UnmanagedType.LPUTF8Str)] string text,
            int dcs,
            int relativeValidityPeriod,
            int requestStatusReport,
            byte[] outHex,
            UIntPtr outHexCap,
            out int outCount);

        /// <summary>
        /// Decodes a PDU hex string into a <c>ct_message</c>.
        /// </summary>
        /// <param name="pduHex">The PDU hex string to decode.</param>
        /// <param name="outMessage">The decoded message.</param>
        /// <returns>The error code.</returns>
        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        public static extern int ct_pdu_decode(
            [MarshalAs(UnmanagedType.LPUTF8Str)] string pduHex,
            out CtMessage outMessage);

        /// <summary>
        /// Decodes a NUL-terminated UTF-8 byte field from native memory.
        /// </summary>
        /// <param name="bytes">The bytes to decode.</param>
        /// <returns>The decoded string.</returns>
        public static string Utf8Z(byte[]? bytes)
        {
            if (bytes == null || bytes.Length == 0)
            {
                return string.Empty;
            }

            int end = Array.IndexOf(bytes, (byte)0);

            if (end < 0)
            {
                end = bytes.Length;
            }

            return end == 0 ? string.Empty : Encoding.UTF8.GetString(bytes, 0, end);
        }
    }
}
