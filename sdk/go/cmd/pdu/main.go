// CometTextel — Go PDU example (no modem).
//
// Usage:
//
//	pdu.exe
//	pdu.exe <destination> <text> [smsc]
//
// Place comettextel.dll next to the executable, or set COMETTEXTEL_LIB.
//
// Copyright (c) Ji-Feng Tsai. All rights reserved.
// Code released under the MIT license.

//go:build windows && amd64

package main

import (
	"fmt"
	"os"

	"github.com/jiowcl/CometTextel/sdk/go"
)

// Main is the main function of the pdu program.
func main() {
	os.Exit(run(os.Args))
}

// Run runs the pdu program.
func run(argv []string) int {
	if len(argv) == 1 {
		if err := comettextel.SelfCheck(); err != nil {
			fmt.Fprintln(os.Stderr, err)
			return 2
		}
		return 0
	}
	if len(argv) < 3 {
		fmt.Println("Usage:")
		fmt.Println("  pdu.exe")
		fmt.Println("  pdu.exe <destination> <text> [smsc]")
		return 1
	}
	destination := argv[1]
	text := argv[2]
	smsc := ""
	if len(argv) >= 4 {
		smsc = argv[3]
	}
	parts, err := comettextel.EncodeSubmitSegments(smsc, destination, text, comettextel.DCSUCS2)
	if err != nil {
		fmt.Fprintln(os.Stderr, err)
		return 2
	}
	for _, part := range parts {
		fmt.Println(part)
		msg, err := comettextel.Decode(part)
		if err != nil {
			fmt.Fprintln(os.Stderr, err)
			return 2
		}
		fmt.Printf("peer=%s text=%s dcs=%d concat=%d seq=%d/%d\n",
			msg.PeerAddress, msg.UserData, msg.DCS, b2i(msg.IsConcatenated), msg.ConcatSeq, msg.ConcatTotal)
	}
	fmt.Printf("segments=%d\n", len(parts))
	return 0
}

// B2I converts a boolean to an integer.
func b2i(v bool) int {
	if v {
		return 1
	}
	return 0
}
