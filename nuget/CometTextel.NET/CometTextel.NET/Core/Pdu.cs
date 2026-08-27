// CometTextel.NET - CometTextel API for .NET
// Copyright (c) 2026 Jiowcl. All rights reserved.

using CometTextel.NET.Core.Native;
using System.Runtime.InteropServices;
using System.Text;

namespace CometTextel.NET.Core
{
    /// <summary>
    /// Stateless PDU helpers (no modem required).
    /// </summary>
    public static class Pdu
    {
        /// <summary>
        /// Encode a PDU submit message.
        /// </summary>
        /// <param name="destination">The destination address.</param>
        /// <param name="text">The message text.</param>
        /// <param name="serviceCenter">The service center address.</param>
        /// <param name="coding">The data coding scheme.</param>
        /// <param name="relativeValidityPeriod">Optional GSM relative TP-VP octet.
        /// Null omits TP-VP; 0 means five minutes.</param>
        /// <param name="requestStatusReport">Whether to set TP-SRR.</param>
        /// <returns>The encoded PDU hex string.</returns>
        public static string EncodeSubmit(
            string destination,
            string text,
            string? serviceCenter = null,
            DataCoding coding = DataCoding.Ucs2,
            byte? relativeValidityPeriod = null,
            bool requestStatusReport = false)
        {
            ArgumentException.ThrowIfNullOrEmpty(destination);
            ArgumentNullException.ThrowIfNull(text);

            byte[] buffer = new byte[1024];
            int status = NativeMethods.ct_pdu_encode_submit_ex(
                serviceCenter ?? string.Empty,
                destination,
                text,
                (int)coding,
                relativeValidityPeriod.HasValue ? relativeValidityPeriod.Value : -1,
                requestStatusReport ? 1 : 0,
                buffer,
                (UIntPtr)buffer.Length);

            EnsureOk(status);

            int end = Array.IndexOf(buffer, (byte)0);

            if (end < 0)
            {
                end = buffer.Length;
            }

            return Encoding.ASCII.GetString(buffer, 0, end);
        }

        /// <summary>
        /// Encode one or more submit PDUs. Longer text is split with concatenated-SMS UDH.
        /// </summary>
        /// <param name="destination">The destination address.</param>
        /// <param name="text">The message text.</param>
        /// <param name="serviceCenter">The service center address.</param>
        /// <param name="coding">The data coding scheme.</param>
        /// <param name="relativeValidityPeriod">Optional GSM relative TP-VP octet.
        /// Null omits TP-VP; 0 means five minutes.</param>
        /// <param name="requestStatusReport">Whether to set TP-SRR on each segment.</param>
        /// <returns>The encoded PDU hex strings.</returns>
        public static string[] EncodeSubmitSegments(
            string destination,
            string text,
            string? serviceCenter = null,
            DataCoding coding = DataCoding.Ucs2,
            byte? relativeValidityPeriod = null,
            bool requestStatusReport = false)
        {
            ArgumentException.ThrowIfNullOrEmpty(destination);
            ArgumentNullException.ThrowIfNull(text);

            byte[] buffer = new byte[131072];
            int status = NativeMethods.ct_pdu_encode_submit_segments_ex(
                serviceCenter ?? string.Empty,
                destination,
                text,
                (int)coding,
                relativeValidityPeriod.HasValue ? relativeValidityPeriod.Value : -1,
                requestStatusReport ? 1 : 0,
                buffer,
                (UIntPtr)buffer.Length,
                out int count);

            EnsureOk(status);

            int end = Array.IndexOf(buffer, (byte)0);
            
            if (end < 0)
            {
                end = buffer.Length;
            }

            string joined = Encoding.ASCII.GetString(buffer, 0, end);

            if (string.IsNullOrEmpty(joined))
            {
                return [];
            }

            string[] parts = joined.Split('\n', StringSplitOptions.RemoveEmptyEntries);

            if (parts.Length != count)
            {
                throw new CometTextelException(Enums.ErrEncode, "segment count mismatch");
            }

            return parts;
        }

        /// <summary>
        /// Decode a PDU message.
        /// </summary>
        /// <param name="pduHex">The PDU hex string.</param>
        /// <returns>The decoded PDU message.</returns>
        public static SmsMessage Decode(
            string pduHex)
        {
            ArgumentException.ThrowIfNullOrEmpty(pduHex);
            int status = NativeMethods.ct_pdu_decode(pduHex, out var native);
            EnsureOk(status);
            
            return SmsMessage.FromNative(native);
        }

        /// <summary>
        /// Decode an SMS-STATUS-REPORT PDU.
        /// </summary>
        /// <param name="pduHex">The status report PDU hex string.</param>
        /// <returns>The decoded status report.</returns>
        public static StatusReport DecodeStatusReport(
            string pduHex)
        {
            ArgumentException.ThrowIfNullOrEmpty(pduHex);
            int status = NativeMethods.ct_pdu_decode_status_report(pduHex, out var native);
            EnsureOk(status);
            
            return StatusReport.FromNative(native);
        }

        /// <summary>
        /// Throws <see cref="CometTextelException"/> unless <paramref name="status"/> is OK.
        /// </summary>
        /// <param name="status">The status code.</param>
        /// <exception cref="CometTextelException">Thrown when the status code is not OK.</exception>
        internal static void EnsureOk(
            int status)
        {
            if (status == Enums.Ok)
            {
                return;
            }

            nint ptr = NativeMethods.ct_status_string(status);
            string message = ptr == IntPtr.Zero
                ? $"status {status}"
                : Marshal.PtrToStringUTF8(ptr) ?? $"status {status}";

            throw new CometTextelException(status, message);
        }
    }
}
