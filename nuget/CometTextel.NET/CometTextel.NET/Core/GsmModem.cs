// CometTextel.NET - CometTextel API for .NET
// Copyright (c) 2026 Jiowcl. All rights reserved.

using System.Text;
using CometTextel.NET.Core.Native;

namespace CometTextel.NET.Core
{
    /// <summary>
    /// GSM modem helper over the CometTextel C ABI.
    /// </summary>
    public sealed class GsmModem : IDisposable
    {
        // Handle to the modem instance.
        private IntPtr _handle;

        // True when the modem is disposed.
        private bool _disposed;

        /// <summary>
        /// Create a new GsmModem.
        /// </summary>
        public GsmModem()
        {
            _handle = NativeMethods.ct_modem_create();

            if (_handle == IntPtr.Zero)
            {
                throw new OutOfMemoryException("ct_modem_create failed");
            }
        }

        /// <summary>
        /// Finalizer for the GsmModem.
        /// </summary>
        ~GsmModem()
        {
            Dispose(disposing: false);
        }

        /// <summary>
        /// Opens the port and initializes PDU mode.
        /// </summary>
        /// <param name="port">The port to open.</param>
        /// <param name="baudRate">The baud rate to use.</param>
        public void Open(
            string port, 
            uint baudRate = 115200)
        {
            ObjectDisposedException.ThrowIf(_disposed, this);
            ArgumentException.ThrowIfNullOrEmpty(port);
            Pdu.EnsureOk(NativeMethods.ct_modem_open(_handle, port, baudRate));
        }

        /// <summary>
        /// Sends an SMS and waits for the modem final result.
        /// Long payloads are split and sent as concatenated segments.
        /// </summary>
        /// <param name="destination">The destination address.</param>
        /// <param name="text">The message text.</param>
        /// <param name="serviceCenter">The service center address.</param>
        /// <param name="coding">The data coding scheme.</param>
        /// <param name="timeoutMs">The timeout in milliseconds.</param>
        /// <param name="relativeValidityPeriod">Optional GSM relative TP-VP octet.
        /// Null omits TP-VP; 0 means five minutes.</param>
        /// <param name="requestStatusReport">Whether to set TP-SRR on each segment.</param>
        public void Send(
            string destination,
            string text,
            string? serviceCenter = null,
            DataCoding coding = DataCoding.Ucs2,
            int timeoutMs = 10000,
            byte? relativeValidityPeriod = null,
            bool requestStatusReport = false)
        {
            ObjectDisposedException.ThrowIf(_disposed, this);
            ArgumentException.ThrowIfNullOrEmpty(destination);
            ArgumentNullException.ThrowIfNull(text);

            Pdu.EnsureOk(NativeMethods.ct_modem_send_ex(
                _handle,
                serviceCenter ?? string.Empty,
                destination,
                text,
                (int)coding,
                relativeValidityPeriod.HasValue ? relativeValidityPeriod.Value : -1,
                requestStatusReport ? 1 : 0,
                timeoutMs));
        }

        /// <summary>
        /// Lists stored messages. Complete concatenated-SMS sets are rejoined
        /// (<see cref="SmsMessage.ConcatSeq"/> == 0 / <see cref="SmsMessage.IsReassembledConcat"/>).
        /// </summary>
        /// <param name="maxCount">The maximum number of messages to list.</param>
        /// <param name="timeoutMs">The timeout in milliseconds.</param>
        /// <returns>The list of messages.</returns>
        public IReadOnlyList<SmsMessage> List(
            int maxCount = 64, 
            int timeoutMs = 8000)
        {
            ObjectDisposedException.ThrowIf(_disposed, this);
            ArgumentOutOfRangeException.ThrowIfNegativeOrZero(maxCount);

            NativeMethods.CtMessage[] buffer = new NativeMethods.CtMessage[maxCount];
            int status = NativeMethods.ct_modem_list(_handle, buffer, maxCount, out var count, timeoutMs);
            Pdu.EnsureOk(status);

            List<SmsMessage> list = new List<SmsMessage>(count);
            
            for (var i = 0; i < count; i++)
            {
                list.Add(SmsMessage.FromNative(buffer[i]));
            }

            return list;
        }

        /// <summary>
        /// Deletes one stored message by index.
        /// </summary>
        /// <param name="index">The index of the message to delete.</param>
        /// <param name="timeoutMs">The timeout in milliseconds.</param>
        public void Delete(
            int index, 
            int timeoutMs = 5000)
        {
            ObjectDisposedException.ThrowIf(_disposed, this);
            Pdu.EnsureOk(NativeMethods.ct_modem_delete(_handle, index, timeoutMs));
        }

        /// <summary>
        /// Sends one AT command and returns accumulated modem text on success.
        /// </summary>
        /// <param name="command">Command bytes; a trailing CR is appended when missing.</param>
        /// <param name="timeoutMs">Maximum wait for OK/ERROR.</param>
        /// <param name="responseCap">Response buffer capacity in bytes.</param>
        /// <returns>Accumulated modem text including the final result.</returns>
        public string RunAtCommand(
            string command,
            int timeoutMs = 3000,
            int responseCap = 4096)
        {
            ObjectDisposedException.ThrowIf(_disposed, this);

            if (string.IsNullOrEmpty(command))
            {
                throw new ArgumentException("command must be non-empty", nameof(command));
            }

            if (responseCap <= 0)
            {
                throw new ArgumentOutOfRangeException(nameof(responseCap));
            }

            bool hasVersionExport = NativeMethods.TryGetApiVersion(out int apiVersion);

            if (!hasVersionExport || apiVersion < NativeMethods.ModemStatusReportApiVersion)
            {
                throw new CometTextelException(
                    Enums.ErrUnsupported,
                    $"native C ABI version {apiVersion} does not provide generic AT command execution");
            }

            byte[] buffer = new byte[responseCap];
            int status;

            try
            {
                status = NativeMethods.ct_modem_run_at_command(
                    _handle,
                    command,
                    buffer,
                    responseCap,
                    timeoutMs);
            }
            catch (EntryPointNotFoundException)
            {
                throw new CometTextelException(
                    Enums.ErrUnsupported,
                    "native C ABI does not export ct_modem_run_at_command");
            }

            string response = Encoding.UTF8.GetString(buffer).TrimEnd('\0');
            Pdu.EnsureOk(status);
            return response;
        }

        /// <summary>
        /// Drains unsolicited modem input and returns one SMS-STATUS-REPORT.
        /// </summary>
        /// <param name="timeoutMs">Wait after an initial drain; 0 is non-blocking.</param>
        /// <returns>The decoded status report.</returns>
        public StatusReport PollStatusReport(
            int timeoutMs = 0)
        {
            ObjectDisposedException.ThrowIf(_disposed, this);

            bool hasVersionExport = NativeMethods.TryGetApiVersion(out int apiVersion);

            if (!hasVersionExport || apiVersion < NativeMethods.ModemStatusReportApiVersion)
            {
                throw new CometTextelException(
                    Enums.ErrUnsupported,
                    $"native C ABI version {apiVersion} does not provide modem Status Report polling");
            }

            int status;
            NativeMethods.CtStatusReport native;

            try
            {
                status = NativeMethods.ct_modem_poll_status_report(_handle, out native, timeoutMs);
            }
            catch (EntryPointNotFoundException)
            {
                throw new CometTextelException(
                    Enums.ErrUnsupported,
                    "native C ABI does not export ct_modem_poll_status_report");
            }

            Pdu.EnsureOk(status);

            return StatusReport.FromNative(native);
        }

        /// <inheritdoc />
        /// <summary>
        /// Disposes of the GsmModem.
        /// </summary>
        public void Dispose()
        {
            Dispose(disposing: true);
            GC.SuppressFinalize(this);
        }

        /// <summary>
        /// Disposes of the GsmModem.
        /// </summary>
        /// <param name="disposing">True if the GsmModem is being disposed.</param>
        private void Dispose(
            bool disposing)
        {
            _ = disposing;

            if (_disposed)
            {
                return;
            }

            if (_handle != IntPtr.Zero)
            {
                NativeMethods.ct_modem_destroy(_handle);
                _handle = IntPtr.Zero;
            }

            _disposed = true;
        }
    }
}
