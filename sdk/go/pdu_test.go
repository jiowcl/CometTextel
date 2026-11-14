// Copyright (c) Ji-Feng Tsai. All rights reserved.
// Code released under the MIT license.

//go:build windows && amd64

package comettextel

import (
	"errors"
	"testing"
)

// TestSelfCheck tests the self-check functionality.
func TestSelfCheck(t *testing.T) {
	if err := SelfCheck(); err != nil {
		t.Fatal(err)
	}
}

// TestAPIVersion tests the API version functionality.
func TestAPIVersion(t *testing.T) {
	version, err := APIVersion()
	if err != nil {
		t.Fatal(err)
	}
	if version < 4 {
		t.Fatalf("api version %d", version)
	}
}

// TestRunAtCommandNotOpen tests the RunAtCommand functionality when the modem is not open.
func TestRunAtCommandNotOpen(t *testing.T) {
	modem, err := NewModem()
	if err != nil {
		t.Fatal(err)
	}
	defer modem.Close()

	if _, err := modem.RunAtCommand("", 500); err == nil {
		t.Fatal("empty command accepted")
	} else {
		var ce *Error
		if !errors.As(err, &ce) || ce.Status != ErrInvalidArgument {
			t.Fatalf("empty command: %v", err)
		}
	}

	text, err := modem.RunAtCommand("AT", 500)
	if text != "" {
		t.Fatalf("not-open response %q", text)
	}
	var ce *Error
	if !errors.As(err, &ce) || ce.Status != ErrNotOpen {
		t.Fatalf("not-open: %v", err)
	}
}
