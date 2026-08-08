// CometTextel.NET - CometTextel API for .NET
// Copyright (c) 2026 Jiowcl. All rights reserved.

namespace CometTextel.NET.Core
{
    /// <summary>
    /// Shared numeric constants for the C ABI.
    /// </summary>
    internal static class Enums
    {
        public const int Ok = 0;
        public const int ErrInvalidArgument = 1;
        public const int ErrNotOpen = 2;
        public const int ErrAlreadyOpen = 3;
        public const int ErrIo = 4;
        public const int ErrTimeout = 5;
        public const int ErrModemRejected = 6;
        public const int ErrEncode = 7;
        public const int ErrDecode = 8;
        public const int ErrUnsupported = 9;

        public const int DcsGsm7 = 0;
        public const int DcsEightBit = 4;
        public const int DcsUcs2 = 8;
    }
}
