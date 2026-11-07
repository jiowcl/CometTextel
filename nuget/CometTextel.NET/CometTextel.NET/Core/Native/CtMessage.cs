// CometTextel.NET - CometTextel API for .NET
// Copyright (c) 2026 Jiowcl. All rights reserved.

using System.Runtime.InteropServices;

namespace CometTextel.NET.Core.Native
{
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
}
