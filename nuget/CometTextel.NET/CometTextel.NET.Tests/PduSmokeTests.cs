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
    public void EncodeSubmit_Decode_Gsm7_EscapeAndAtSign_RoundTrip()
    {
        const string destination = "886912345678";
        const string text = "Cost: 10€ [ok] @home";
        const string smsc = "886932000000";

        string hex = Pdu.EncodeSubmit(destination, text, smsc, DataCoding.Gsm7Bit);
        SmsMessage decoded = Pdu.Decode(hex);

        Assert.Equal(text, decoded.UserData);
        Assert.Equal(DataCoding.Gsm7Bit, decoded.Coding);
    }

    [Fact]
    public void EncodeSubmit_Gsm7_UnsupportedCjk_Throws()
    {
        Assert.ThrowsAny<Exception>(() =>
            Pdu.EncodeSubmit("886912345678", "你好", "886932000000", DataCoding.Gsm7Bit));
    }

    [Fact]
    public void EncodeSubmit_Default_OmitsValidityPeriod()
    {
        string hex = Pdu.EncodeSubmit(
            "886912345678", "Hi", "886932000000", DataCoding.Gsm7Bit);
        byte[] raw = Convert.FromHexString(hex);
        int firstOctetOffset = 1 + raw[0];

        Assert.Equal(0x01, raw[firstOctetOffset]);
    }

    [Fact]
    public void EncodeSubmit_RelativeValidity_AndStatusReport()
    {
        string hex = Pdu.EncodeSubmit(
            "886912345678",
            "Hi",
            "886932000000",
            DataCoding.Gsm7Bit,
            relativeValidityPeriod: 0x00,
            requestStatusReport: true);
        byte[] raw = Convert.FromHexString(hex);
        int firstOctetOffset = 1 + raw[0];

        Assert.Equal(0x31, raw[firstOctetOffset]);
        Assert.Equal("Hi", Pdu.Decode(hex).UserData);
    }

    [Fact]
    public void DecodeStatusReport_Fixture()
    {
        const string pdu =
            "00022A0C91889621436587215070418045232150704180452300";

        StatusReport report = Pdu.DecodeStatusReport(pdu);

        Assert.Equal(0x2A, report.MessageReference);
        Assert.Equal(0, report.TpStatus);
        Assert.Equal("886912345678", report.RecipientAddress);
        Assert.Equal("12050714085432", report.ServiceTimestamp);
        Assert.Equal("12050714085432", report.DischargeTime);
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

    [Fact]
    public void EncodeSubmitSegments_Short_MatchesEncodeSubmit()
    {
        const string destination = "886912345678";
        const string text = "Hello";
        const string smsc = "886932000000";

        string single = Pdu.EncodeSubmit(destination, text, smsc, DataCoding.Ucs2);
        string[] parts = Pdu.EncodeSubmitSegments(destination, text, smsc, DataCoding.Ucs2);

        Assert.Single(parts);
        Assert.Equal(single, parts[0]);
        Assert.False(Pdu.Decode(parts[0]).IsConcatenated);
    }

    [Fact]
    public void EncodeSubmitSegments_Ucs2_LongText_SplitsAndRoundTrips()
    {
        const string destination = "886912345678";
        string text = new string('B', 71);
        const string smsc = "886932000000";

        string[] parts = Pdu.EncodeSubmitSegments(destination, text, smsc, DataCoding.Ucs2);

        Assert.Equal(2, parts.Length);

        string joined = string.Empty;
        for (int i = 0; i < parts.Length; i++)
        {
            SmsMessage decoded = Pdu.Decode(parts[i]);
            Assert.True(decoded.IsConcatenated);
            Assert.Equal(2, decoded.ConcatTotal);
            Assert.Equal(i + 1, decoded.ConcatSeq);
            Assert.Equal(destination, decoded.PeerAddress);
            joined += decoded.UserData;
        }

        Assert.Equal(text, joined);
    }
}
