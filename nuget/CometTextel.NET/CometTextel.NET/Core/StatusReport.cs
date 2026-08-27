// CometTextel.NET - CometTextel API for .NET
// Copyright (c) 2026 Jiowcl. All rights reserved.

using CometTextel.NET.Core.Native;

namespace CometTextel.NET.Core;

/// <summary>
/// Decoded SMS-STATUS-REPORT fields.
/// </summary>
public sealed class StatusReport
{
    /// <summary>
    /// TP-MR matching the submitted message reference.
    /// </summary>
    public int MessageReference { get; init; }

    /// <summary>
    /// Raw TP-Status octet.
    /// </summary>
    public int TpStatus { get; init; }

    /// <summary>
    /// Recipient address from TP-RA.
    /// </summary>
    public string RecipientAddress { get; init; } = string.Empty;

    /// <summary>
    /// Serialized TP-SCTS timestamp.
    /// </summary>
    public string ServiceTimestamp { get; init; } = string.Empty;

    /// <summary>
    /// Serialized TP-DT discharge timestamp.
    /// </summary>
    public string DischargeTime { get; init; } = string.Empty;

    /// <summary>
    /// Creates a <c>StatusReport</c> from a native <c>ct_status_report</c> struct.
    /// </summary>
    /// <param name="native">The native <c>ct_status_report</c> struct.</param>
    /// <returns>The <c>StatusReport</c> instance.</returns>
    internal static StatusReport FromNative(in NativeMethods.CtStatusReport native) => new()
    {
        MessageReference = native.MessageReference,
        TpStatus = native.TpStatus,
        RecipientAddress = NativeMethods.Utf8Z(native.RecipientAddress),
        ServiceTimestamp = NativeMethods.Utf8Z(native.ServiceTimestamp),
        DischargeTime = NativeMethods.Utf8Z(native.DischargeTime)
    };
}
