// CometTextel.NET - CometTextel API for .NET
// Copyright (c) 2026 Jiowcl. All rights reserved.

using System.Runtime.InteropServices;
using System.Text;

namespace CometTextel.NET.Core.Native
{
    /// <summary>
    /// Native methods for the CometTextel C ABI.
    /// </summary>
    internal static class NativeMethods
    {
        internal const string DllName = "comettextel";

        /// <summary>
        /// Blittable mirror of <c>ct_message</c> (UTF-8 byte fields).
        /// </summary>
        [StructLayout(LayoutKind.Sequential)]
        public struct CtMessage
        {
            public int Index;
            public int Dcs;
            public int HasUdh;

            [MarshalAs(UnmanagedType.ByValArray, SizeConst = 32)]
            public byte[] ServiceCenter;

            [MarshalAs(UnmanagedType.ByValArray, SizeConst = 32)]
            public byte[] PeerAddress;

            [MarshalAs(UnmanagedType.ByValArray, SizeConst = 32)]
            public byte[] ServiceTimestamp;

            [MarshalAs(UnmanagedType.ByValArray, SizeConst = 512)]
            public byte[] UserData;
        }

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        public static extern IntPtr ct_status_string(int status);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        public static extern IntPtr ct_modem_create();

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        public static extern void ct_modem_destroy(IntPtr modem);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        public static extern int ct_modem_open(
            IntPtr modem,
            [MarshalAs(UnmanagedType.LPUTF8Str)] string port,
            uint baudRate);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        public static extern int ct_modem_send(
            IntPtr modem,
            [MarshalAs(UnmanagedType.LPUTF8Str)] string? smsc,
            [MarshalAs(UnmanagedType.LPUTF8Str)] string destination,
            [MarshalAs(UnmanagedType.LPUTF8Str)] string text,
            int dcs,
            int timeoutMs);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        public static extern int ct_modem_list(
            IntPtr modem,
            [Out] CtMessage[] outMessages,
            int maxCount,
            out int outCount,
            int timeoutMs);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        public static extern int ct_modem_delete(IntPtr modem, int index, int timeoutMs);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        public static extern int ct_pdu_encode_submit(
            [MarshalAs(UnmanagedType.LPUTF8Str)] string? smsc,
            [MarshalAs(UnmanagedType.LPUTF8Str)] string destination,
            [MarshalAs(UnmanagedType.LPUTF8Str)] string text,
            int dcs,
            byte[] outHex,
            UIntPtr outHexCap);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        public static extern int ct_pdu_decode(
            [MarshalAs(UnmanagedType.LPUTF8Str)] string pduHex,
            out CtMessage outMessage);

        /// <summary>
        /// Decodes a NUL-terminated UTF-8 byte field from native memory.
        /// </summary>
        public static string Utf8Z(byte[]? bytes)
        {
            if (bytes == null || bytes.Length == 0)
            {
                return string.Empty;
            }

            int end = Array.IndexOf(bytes, (byte)0);
            if (end < 0)
            {
                end = bytes.Length;
            }

            return end == 0 ? string.Empty : Encoding.UTF8.GetString(bytes, 0, end);
        }
    }
}
