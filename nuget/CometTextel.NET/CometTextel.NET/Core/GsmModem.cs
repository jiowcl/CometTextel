// CometTextel.NET - CometTextel API for .NET
// Copyright (c) 2026 Jiowcl. All rights reserved.

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
        public void Send(
            string destination,
            string text,
            string? serviceCenter = null,
            DataCoding coding = DataCoding.Ucs2,
            int timeoutMs = 10000)
        {
            ObjectDisposedException.ThrowIf(_disposed, this);
            ArgumentException.ThrowIfNullOrEmpty(destination);
            ArgumentNullException.ThrowIfNull(text);

            Pdu.EnsureOk(NativeMethods.ct_modem_send(
                _handle,
                serviceCenter ?? string.Empty,
                destination,
                text,
                (int)coding,
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
