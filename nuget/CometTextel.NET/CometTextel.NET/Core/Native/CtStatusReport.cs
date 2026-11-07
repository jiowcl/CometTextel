// CometTextel.NET - CometTextel API for .NET
// Copyright (c) 2026 Jiowcl. All rights reserved.

using System.Runtime.InteropServices;

namespace CometTextel.NET.Core.Native
{
    /// <summary>
    /// Blittable mirror of <c>ct_status_report</c>.
    /// </summary>
    [StructLayout(LayoutKind.Sequential)]
    public struct CtStatusReport
    {
        // Message reference.
        public int MessageReference;

        // TP status.
        public int TpStatus;

        /// <summary>
        /// Recipient address.
        /// </summary>
        [MarshalAs(UnmanagedType.ByValArray, SizeConst = 32)]
        public byte[] RecipientAddress;

        /// <summary>
        /// Service timestamp.
        /// </summary>
        [MarshalAs(UnmanagedType.ByValArray, SizeConst = 32)]
        public byte[] ServiceTimestamp;

        /// <summary>
        /// Discharge time.
        /// </summary>
        [MarshalAs(UnmanagedType.ByValArray, SizeConst = 32)]
        public byte[] DischargeTime;
    }
}
