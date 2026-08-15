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
        /// <summary>
        /// Index of the message.
        /// </summary>
        public int Index { get; init; } = -1;

        /// <summary>
        /// Data coding scheme of the message.
        /// </summary>
        public DataCoding Coding { get; init; } = DataCoding.Ucs2;

        /// <summary>
        /// True when a UDH was present in the message.
        /// </summary>
        public bool HasUdh { get; init; }

        /// <summary>
        /// True when a concatenated SMS IE (0x00 / 0x08) was present in the UDH,
        /// or when <see cref="ListMessages"/> rejoined a complete multi-part set.
        /// </summary>
        public bool IsConcatenated { get; init; }

        /// <summary>
        /// Concatenation reference number (valid when <see cref="IsConcatenated"/>).
        /// </summary>
        public int ConcatRef { get; init; }

        /// <summary>
        /// Total segment count (valid when <see cref="IsConcatenated"/>).
        /// </summary>
        public int ConcatTotal { get; init; }

        /// <summary>
        /// 1-based segment index; <c>0</c> when the message is a reassembled full set.
        /// </summary>
        public int ConcatSeq { get; init; }

        /// <summary>
        /// True when this message is a full join of concat segments
        /// (<see cref="IsConcatenated"/> and <see cref="ConcatSeq"/> == 0).
        /// </summary>
        public bool IsReassembledConcat => IsConcatenated && ConcatSeq == 0 && ConcatTotal > 0;

        /// <summary>
        /// Service center of the message.
        /// </summary>
        public string ServiceCenter { get; init; } = string.Empty;

        /// <summary>
        /// Peer address of the message.
        /// </summary>
        public string PeerAddress { get; init; } = string.Empty;

        /// <summary>
        /// Service timestamp of the message.
        /// </summary>
        public string ServiceTimestamp { get; init; } = string.Empty;

        /// <summary>
        /// UTF-8 text of the message payload.
        /// </summary>
        public string UserData { get; init; } = string.Empty;

        /// <summary>
        /// Create a new SmsMessage from a native message struct.
        /// </summary>
        internal static SmsMessage FromNative(in NativeMethods.CtMessage native) => new()
        {
            Index = native.Index,
            Coding = (DataCoding)native.Dcs,
            HasUdh = native.HasUdh != 0,
            IsConcatenated = native.IsConcatenated != 0,
            ConcatRef = native.ConcatRef,
            ConcatTotal = native.ConcatTotal,
            ConcatSeq = native.ConcatSeq,
            ServiceCenter = NativeMethods.Utf8Z(native.ServiceCenter),
            PeerAddress = NativeMethods.Utf8Z(native.PeerAddress),
            ServiceTimestamp = NativeMethods.Utf8Z(native.ServiceTimestamp),
            UserData = NativeMethods.Utf8Z(native.UserData)
        };
    }
}
