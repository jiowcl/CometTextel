// Copyright (c) Ji-Feng Tsai. All rights reserved.
// Code released under the MIT license.

//go:build windows && amd64

package comettextel

import (
	"runtime"
	"strings"
	"unsafe"
)

// EncodeSubmit encodes one single-segment submit PDU. TP-VP and TP-SRR are omitted.
func EncodeSubmit(smsc, destination, text string, dcs int) (string, error) {
	if err := ensure(); err != nil {
		return "", err
	}
	smscB := cstr(smsc)
	destB := cstr(destination)
	textB := cstr(text)
	out := make([]byte, hexCap)
	status := call(api.encodeSubmit,
		ptr(smscB), ptr(destB), ptr(textB),
		uintptr(int32(dcs)),
		ptr(out), uintptr(len(out)),
	)
	runtime.KeepAlive(smscB)
	runtime.KeepAlive(destB)
	runtime.KeepAlive(textB)
	runtime.KeepAlive(out)
	if err := check(status, "ct_pdu_encode_submit"); err != nil {
		return "", err
	}
	return cString(out), nil
}

// EncodeSubmitEx encodes one submit PDU.
// relativeValidityPeriod is -1 to omit TP-VP, otherwise a GSM relative octet 0..255
// (0 means five minutes). requestStatusReport is non-zero to set TP-SRR.
func EncodeSubmitEx(smsc, destination, text string, dcs, relativeValidityPeriod, requestStatusReport int) (string, error) {
	if err := ensure(); err != nil {
		return "", err
	}
	if api.encodeSubmitEx == nil {
		return "", unsupported("ct_pdu_encode_submit_ex")
	}
	smscB := cstr(smsc)
	destB := cstr(destination)
	textB := cstr(text)
	out := make([]byte, hexCap)
	status := call(api.encodeSubmitEx,
		ptr(smscB), ptr(destB), ptr(textB),
		uintptr(int32(dcs)),
		uintptr(int32(relativeValidityPeriod)),
		uintptr(int32(requestStatusReport)),
		ptr(out), uintptr(len(out)),
	)
	runtime.KeepAlive(smscB)
	runtime.KeepAlive(destB)
	runtime.KeepAlive(textB)
	runtime.KeepAlive(out)
	if err := check(status, "ct_pdu_encode_submit_ex"); err != nil {
		return "", err
	}
	return cString(out), nil
}

// EncodeSubmitSegments encodes one or more submit PDUs.
// Long text is split with concat UDH. Returned hex strings have no trailing newline.
func EncodeSubmitSegments(smsc, destination, text string, dcs int) ([]string, error) {
	if err := ensure(); err != nil {
		return nil, err
	}
	smscB := cstr(smsc)
	destB := cstr(destination)
	textB := cstr(text)
	out := make([]byte, hexCap)
	var count int32
	status := call(api.encodeSegments,
		ptr(smscB), ptr(destB), ptr(textB),
		uintptr(int32(dcs)),
		ptr(out), uintptr(len(out)),
		uintptr(unsafe.Pointer(&count)),
	)
	runtime.KeepAlive(smscB)
	runtime.KeepAlive(destB)
	runtime.KeepAlive(textB)
	runtime.KeepAlive(out)
	runtime.KeepAlive(&count)
	if err := check(status, "ct_pdu_encode_submit_segments"); err != nil {
		return nil, err
	}
	return splitHex(cString(out), count), nil
}

// EncodeSubmitSegmentsEx encodes submit PDUs with the same TP-VP / TP-SRR rules as EncodeSubmitEx.
// requestStatusReport applies to every segment.
func EncodeSubmitSegmentsEx(smsc, destination, text string, dcs, relativeValidityPeriod, requestStatusReport int) ([]string, error) {
	if err := ensure(); err != nil {
		return nil, err
	}
	if api.encodeSegmentsEx == nil {
		return nil, unsupported("ct_pdu_encode_submit_segments_ex")
	}
	smscB := cstr(smsc)
	destB := cstr(destination)
	textB := cstr(text)
	out := make([]byte, hexCap)
	var count int32
	status := call(api.encodeSegmentsEx,
		ptr(smscB), ptr(destB), ptr(textB),
		uintptr(int32(dcs)),
		uintptr(int32(relativeValidityPeriod)),
		uintptr(int32(requestStatusReport)),
		ptr(out), uintptr(len(out)),
		uintptr(unsafe.Pointer(&count)),
	)
	runtime.KeepAlive(smscB)
	runtime.KeepAlive(destB)
	runtime.KeepAlive(textB)
	runtime.KeepAlive(out)
	runtime.KeepAlive(&count)
	if err := check(status, "ct_pdu_encode_submit_segments_ex"); err != nil {
		return nil, err
	}
	return splitHex(cString(out), count), nil
}

// Decode decodes a PDU hex string.
func Decode(pduHex string) (Message, error) {
	if err := ensure(); err != nil {
		return Message{}, err
	}
	hexB := cstr(pduHex)
	var raw ctMessage
	status := call(api.decode, ptr(hexB), uintptr(unsafe.Pointer(&raw)))
	runtime.KeepAlive(hexB)
	runtime.KeepAlive(&raw)
	if err := check(status, "ct_pdu_decode"); err != nil {
		return Message{}, err
	}
	return messageFrom(raw), nil
}

// DecodeStatusReport decodes an SMS-STATUS-REPORT PDU.
// A DLL without this export returns ErrUnsupported.
func DecodeStatusReport(pduHex string) (StatusReport, error) {
	if err := ensure(); err != nil {
		return StatusReport{}, err
	}
	if api.decodeStatusReport == nil {
		return StatusReport{}, unsupported("ct_pdu_decode_status_report")
	}
	hexB := cstr(pduHex)
	var raw ctStatusReport
	status := call(api.decodeStatusReport, ptr(hexB), uintptr(unsafe.Pointer(&raw)))
	runtime.KeepAlive(hexB)
	runtime.KeepAlive(&raw)
	if err := check(status, "ct_pdu_decode_status_report"); err != nil {
		return StatusReport{}, err
	}
	return reportFrom(raw), nil
}

// SplitHex splits a joined hex string into a slice of hex strings.
func splitHex(joined string, count int32) []string {
	if joined == "" {
		return nil
	}
	parts := strings.Split(joined, "\n")
	out := make([]string, 0, len(parts))
	for _, part := range parts {
		if part != "" {
			out = append(out, part)
		}
	}
	if count > 0 && int(count) < len(out) {
		out = out[:count]
	}
	return out
}
