// CometTextel.NET - CometTextel API for .NET
// Copyright (c) 2026 Jiowcl. All rights reserved.

using CometTextel.NET.Core;
using Xunit;

namespace CometTextel.NET.Tests;

/// <summary>
/// Smoke tests for the P/Invoke PDU path (requires comettextel.dll next to the test host).
/// </summary>
public sealed class PduSmokeTests
{
    [Theory]
    [InlineData("886912345678", "Hello from CometTextel.NET", "886932000000")]
    [InlineData("886912345678", "測試中文簡訊", "886932000000")]
    public void EncodeSubmit_Decode_Ucs2_RoundTrip(string destination, string text, string smsc)
    {
        string hex = Pdu.EncodeSubmit(destination, text, smsc, DataCoding.Ucs2);

        Assert.False(string.IsNullOrWhiteSpace(hex));
        Assert.True(hex.All(char.IsAsciiHexDigit), $"non-hex PDU: {hex}");

        SmsMessage decoded = Pdu.Decode(hex);

        Assert.Equal(destination, decoded.PeerAddress);
        Assert.Equal(text, decoded.UserData);
        Assert.Equal(DataCoding.Ucs2, decoded.Coding);
        Assert.False(decoded.HasUdh);
        Assert.False(decoded.IsConcatenated);
    }

    [Fact]
    public void Decode_ConcatUdh_8BitRef_ExposesFields()
    {
        // Same fixture as C++ test_decode_skips_udh_ucs2
        const string pdu =
            "0044049121430008000000000000000A" +
            "050003AA0201" +
            "00480069";

        SmsMessage decoded = Pdu.Decode(pdu);

        Assert.True(decoded.HasUdh);
        Assert.True(decoded.IsConcatenated);
        Assert.Equal(0xAA, decoded.ConcatRef);
        Assert.Equal(2, decoded.ConcatTotal);
        Assert.Equal(1, decoded.ConcatSeq);
        Assert.Equal("Hi", decoded.UserData);
        Assert.Equal(DataCoding.Ucs2, decoded.Coding);
    }

    [Fact]
    public void EncodeSubmit_Decode_Gsm7_Ascii_RoundTrip()
    {
        const string destination = "886912345678";
        const string text = "Hello GSM7";
        const string smsc = "886932000000";

        string hex = Pdu.EncodeSubmit(destination, text, smsc, DataCoding.Gsm7Bit);
        SmsMessage decoded = Pdu.Decode(hex);

        Assert.Equal(destination, decoded.PeerAddress);
        Assert.Equal(text, decoded.UserData);
        Assert.Equal(DataCoding.Gsm7Bit, decoded.Coding);
    }

    [Fact]
    public void Decode_EmptyHex_Throws()
    {
        Assert.ThrowsAny<ArgumentException>(() => Pdu.Decode(""));
    }

    [Fact]
    public void EncodeSubmit_EmptyDestination_Throws()
    {
        Assert.ThrowsAny<ArgumentException>(() =>
            Pdu.EncodeSubmit("", "x", "886932000000", DataCoding.Ucs2));
    }
}
