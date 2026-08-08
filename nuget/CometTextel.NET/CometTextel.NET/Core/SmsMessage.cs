// CometTextel.NET - CometTextel API for .NET
// Copyright (c) 2026 Jiowcl. All rights reserved.

using CometTextel.NET.Core.Native;

namespace CometTextel.NET.Core
{
    /// <summary>
    /// Decoded or listed short message (UTF-8 text fields).
    /// </summary>
    public sealed class SmsMessage
    {
        public int Index { get; init; } = -1;

        public DataCoding Coding { get; init; } = DataCoding.Ucs2;

        public bool HasUdh { get; init; }

        public string ServiceCenter { get; init; } = string.Empty;

        public string PeerAddress { get; init; } = string.Empty;

        public string ServiceTimestamp { get; init; } = string.Empty;

        public string UserData { get; init; } = string.Empty;

        /// <summary>
        /// Create a new SmsMessage from a native message struct.
        /// </summary>
        internal static SmsMessage FromNative(in NativeMethods.CtMessage native) => new()
        {
            Index = native.Index,
            Coding = (DataCoding)native.Dcs,
            HasUdh = native.HasUdh != 0,
            ServiceCenter = NativeMethods.Utf8Z(native.ServiceCenter),
            PeerAddress = NativeMethods.Utf8Z(native.PeerAddress),
            ServiceTimestamp = NativeMethods.Utf8Z(native.ServiceTimestamp),
            UserData = NativeMethods.Utf8Z(native.UserData)
        };
    }
}
