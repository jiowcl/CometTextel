// CometTextel.NET - CometTextel API for .NET
// Copyright (c) 2026 Jiowcl. All rights reserved.

namespace CometTextel.NET.Core
{
    /// <summary>
    /// Shared numeric constants for the C ABI.
    /// </summary>
    internal static class Enums
    {
        // OK.
        public const int Ok = 0;

        // Invalid argument.
        public const int ErrInvalidArgument = 1;

        // Not open.
        public const int ErrNotOpen = 2;

        // Already open.
        public const int ErrAlreadyOpen = 3;

        // I/O failure.
        public const int ErrIo = 4;

        // Timeout.
        public const int ErrTimeout = 5;

        // Modem rejected.
        public const int ErrModemRejected = 6;

        // Encode failure.
        public const int ErrEncode = 7;

        // Decode failure.
        public const int ErrDecode = 8;

        // Unsupported.
        public const int ErrUnsupported = 9;

        // GSM 7-bit.
        public const int DcsGsm7 = 0;

        // 8-bit.
        public const int DcsEightBit = 4;

        // UCS-2.
        public const int DcsUcs2 = 8;
    }
}
