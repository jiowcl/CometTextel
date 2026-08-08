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
        private IntPtr _handle;
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

        ~GsmModem()
        {
            Dispose(disposing: false);
        }

        /// <summary>
        /// Opens the port and initializes PDU mode.
        /// </summary>
        public void Open(string port, uint baudRate = 115200)
        {
            ObjectDisposedException.ThrowIf(_disposed, this);
            ArgumentException.ThrowIfNullOrEmpty(port);
            Pdu.EnsureOk(NativeMethods.ct_modem_open(_handle, port, baudRate));
        }

        /// <summary>
        /// Sends one SMS and waits for the modem final result.
        /// </summary>
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
        /// Lists stored messages.
        /// </summary>
        public IReadOnlyList<SmsMessage> List(int maxCount = 64, int timeoutMs = 8000)
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
        public void Delete(int index, int timeoutMs = 5000)
        {
            ObjectDisposedException.ThrowIf(_disposed, this);
            Pdu.EnsureOk(NativeMethods.ct_modem_delete(_handle, index, timeoutMs));
        }

        /// <inheritdoc />
        public void Dispose()
        {
            Dispose(disposing: true);
            GC.SuppressFinalize(this);
        }

        private void Dispose(bool disposing)
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
