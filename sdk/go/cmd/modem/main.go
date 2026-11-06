// CometTextel — Go modem example (list / send / delete).
//
// Usage:
//
//	modem.exe list <port> [baud]
//	modem.exe send <port> <smsc> <destination> <text> [baud]
//	modem.exe delete <port> <index> [baud]
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
	"strconv"

	"github.com/jiowcl/CometTextel/sdk/go"
)

// Main is the main function of the modem program.
func main() {
	os.Exit(run(os.Args))
}

// Run runs the modem program.
func run(argv []string) int {
	if len(argv) < 3 {
		printUsage()
		return 1
	}
	cmd := argv[1]
	port := argv[2]
	switch cmd {
	case "list":
		baud := uint32(115200)
		if len(argv) >= 4 {
			baud = parseBaud(argv[3])
		}
		return runList(port, baud)
	case "send":
		if len(argv) < 6 {
			printUsage()
			return 1
		}
		baud := uint32(115200)
		if len(argv) >= 7 {
			baud = parseBaud(argv[6])
		}
		return runSend(port, argv[3], argv[4], argv[5], baud)
	case "delete":
		if len(argv) < 4 {
			printUsage()
			return 1
		}
		index, err := strconv.Atoi(argv[3])
		if err != nil {
			fmt.Fprintln(os.Stderr, "index must be an integer")
			return 1
		}
		baud := uint32(115200)
		if len(argv) >= 5 {
			baud = parseBaud(argv[4])
		}
		return runDelete(port, index, baud)
	default:
		printUsage()
		return 1
	}
}

// PrintUsage prints the usage of the modem program.
func printUsage() {
	fmt.Println("Usage:")
	fmt.Println("  modem.exe list <port> [baud]")
	fmt.Println("  modem.exe send <port> <smsc> <destination> <text> [baud]")
	fmt.Println("  modem.exe delete <port> <index> [baud]")
}

// ParseBaud parses a baud rate from a string.
func parseBaud(text string) uint32 {
	value, err := strconv.Atoi(text)
	if err != nil || value <= 0 {
		return 115200
	}
	return uint32(value)
}

// RunList lists the messages from the modem.
func runList(port string, baud uint32) int {
	modem, err := comettextel.NewModem()
	if err != nil {
		fmt.Fprintln(os.Stderr, err)
		return 2
	}
	defer modem.Close()
	if err := modem.Open(port, baud); err != nil {
		fmt.Fprintln(os.Stderr, err)
		return 2
	}
	messages, err := modem.List(8000)
	if err != nil {
		fmt.Fprintln(os.Stderr, err)
		return 2
	}
	for _, msg := range messages {
		flag := ""
		if msg.IsReassembledConcat {
			flag = " reassembled"
		}
		fmt.Printf("[%d] %s: %s (dcs=%d concat=%d seq=%d/%d%s)\n",
			msg.Index, msg.PeerAddress, msg.UserData, msg.DCS,
			b2i(msg.IsConcatenated), msg.ConcatSeq, msg.ConcatTotal, flag)
	}
	fmt.Printf("count=%d\n", len(messages))
	return 0
}

// RunSend sends a message to the modem.
func runSend(port, smsc, destination, text string, baud uint32) int {
	modem, err := comettextel.NewModem()
	if err != nil {
		fmt.Fprintln(os.Stderr, err)
		return 2
	}
	defer modem.Close()
	if err := modem.Open(port, baud); err != nil {
		fmt.Fprintln(os.Stderr, err)
		return 2
	}
	if err := modem.Send(smsc, destination, text, comettextel.DCSUCS2, 10000); err != nil {
		fmt.Fprintln(os.Stderr, err)
		return 2
	}
	fmt.Println("Sent.")
	return 0
}

// RunDelete deletes a message from the modem.
func runDelete(port string, index int, baud uint32) int {
	modem, err := comettextel.NewModem()
	if err != nil {
		fmt.Fprintln(os.Stderr, err)
		return 2
	}
	defer modem.Close()
	if err := modem.Open(port, baud); err != nil {
		fmt.Fprintln(os.Stderr, err)
		return 2
	}
	if err := modem.Delete(index, 8000); err != nil {
		fmt.Fprintln(os.Stderr, err)
		return 2
	}
	fmt.Println("Deleted.")
	return 0
}

// B2I converts a boolean to an integer.
func b2i(v bool) int {
	if v {
		return 1
	}
	return 0
}
