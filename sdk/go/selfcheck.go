// Copyright (c) Ji-Feng Tsai. All rights reserved.
// Code released under the MIT license.

//go:build windows && amd64

package comettextel

import (
	"fmt"
	"strings"
)

// SelfCheck encodes and decodes PDU fixtures against the loaded DLL.
// It does not open a modem.
func SelfCheck() error {
	const dest = "886912345678"
	const smsc = "886932000000"

	fmt.Println("-- self-check UCS-2 ASCII --")
	if err := printSegments(dest, "Hello from Go", smsc); err != nil {
		return err
	}

	fmt.Println("-- self-check UCS-2 Chinese --")
	chinese := "測試中文簡訊"
	parts, err := EncodeSubmitSegments(smsc, dest, chinese, DCSUCS2)
	if err != nil {
		return err
	}
	for _, part := range parts {
		fmt.Println(part)
		msg, err := Decode(part)
		if err != nil {
			return err
		}
		printMessage(msg)
		if msg.UserData != chinese || msg.PeerAddress != dest {
			return fmt.Errorf("Chinese UCS-2 round-trip mismatch")
		}
	}
	fmt.Printf("ok (6 CJK chars, UCS-2)\nsegments=%d\n", len(parts))

	fmt.Println("-- self-check UCS-2 concat (71 ASCII → 2 segments) --")
	longText := strings.Repeat("B", 71)
	parts, err = EncodeSubmitSegments(smsc, dest, longText, DCSUCS2)
	if err != nil {
		return err
	}
	if len(parts) != 2 {
		return fmt.Errorf("expected 2 segments, got %d", len(parts))
	}
	joined := ""
	for i, part := range parts {
		fmt.Println(part)
		msg, err := Decode(part)
		if err != nil {
			return err
		}
		printMessage(msg)
		if !msg.IsConcatenated || msg.ConcatTotal != 2 || msg.ConcatSeq != int32(i+1) {
			return fmt.Errorf("concat metadata mismatch")
		}
		joined += msg.UserData
	}
	if joined != longText {
		return fmt.Errorf("concat payload round-trip mismatch")
	}
	fmt.Println("ok")

	fmt.Println("-- self-check single-segment EncodeSubmit --")
	hexOne, err := EncodeSubmit(smsc, dest, "Hello", DCSUCS2)
	if err != nil {
		return err
	}
	fmt.Println(hexOne)
	msg, err := Decode(hexOne)
	if err != nil {
		return err
	}
	if msg.UserData != "Hello" || msg.PeerAddress != dest {
		return fmt.Errorf("round-trip mismatch")
	}
	fmt.Println("ok")

	fmt.Println("-- self-check SMS-STATUS-REPORT --")
	const reportHex = "00022A0C91889621436587215070418045232150704180452300"
	report, err := DecodeStatusReport(reportHex)
	if err != nil {
		return err
	}
	if report.MessageReference != 0x2A || report.TpStatus != 0 ||
		report.RecipientAddress != dest ||
		report.ServiceTimestamp != "12050714085432" ||
		report.DischargeTime != "12050714085432" {
		return fmt.Errorf("status report mismatch: %+v", report)
	}
	fmt.Printf("mr=%d status=%d recipient=%s\n", report.MessageReference, report.TpStatus, report.RecipientAddress)
	fmt.Println("ok")
	return nil
}

// PrintSegments prints the segments of a text.
func printSegments(dest, text, smsc string) error {
	parts, err := EncodeSubmitSegments(smsc, dest, text, DCSUCS2)
	if err != nil {
		return err
	}
	for _, part := range parts {
		fmt.Println(part)
		msg, err := Decode(part)
		if err != nil {
			return err
		}
		printMessage(msg)
	}
	fmt.Printf("segments=%d\n", len(parts))
	return nil
}

// PrintMessage prints a Message.
func printMessage(msg Message) {
	fmt.Printf("peer=%s text=%s dcs=%d has_udh=%d concat=%d ref=%d total=%d seq=%d\n",
		msg.PeerAddress, msg.UserData, msg.DCS, b2i(msg.HasUDH), b2i(msg.IsConcatenated),
		msg.ConcatRef, msg.ConcatTotal, msg.ConcatSeq)
}

// B2I converts a boolean to an integer.
func b2i(v bool) int {
	if v {
		return 1
	}
	return 0
}
