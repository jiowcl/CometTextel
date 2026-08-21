// CometTextel.NET - CometTextel API for .NET
// Copyright (c) 2026 Jiowcl. All rights reserved.

namespace CometTextel.NET.Core
{
    /// <summary>
    /// TP-DCS alphabet selection.
    /// </summary>
    /// <remarks>
    /// <see cref="Gsm7Bit"/> maps UTF-8 via GSM 03.38 (default alphabet + ESC extension).
    /// Characters outside that set fail encode — use <see cref="Ucs2"/> for CJK and similar.
    /// </remarks>
    public enum DataCoding
    {
        /// <summary>GSM 03.38 7-bit default alphabet (+ ESC extension).</summary>
        Gsm7Bit = Enums.DcsGsm7,
        /// <summary>8-bit data.</summary>
        EightBit = Enums.DcsEightBit,
        /// <summary>UCS-2 (UTF-16BE) text.</summary>
        Ucs2 = Enums.DcsUcs2
    }
}
