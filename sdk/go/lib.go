// CometTextel — thin Go FFI for the C ABI (PDU + modem).
//
// Windows amd64: syscall.LoadDLL. Runtime-loads comettextel.dll from the
// C SDK. No cgo and no import library.
//
// Mirrors include/comettextel/c_api.h — do not add a second ABI here.
//
// Copyright (c) Ji-Feng Tsai. All rights reserved.
// Code released under the MIT license.

//go:build windows && amd64

package comettextel

import (
	"fmt"
	"os"
	"path/filepath"
	"runtime"
	"sync"
	"syscall"
	"unsafe"
)

// Version is the version of the CometTextel SDK.
const (
	OK                 = 0
	ErrInvalidArgument = 1
	ErrNotOpen         = 2
	ErrAlreadyOpen     = 3
	ErrIO              = 4
	ErrTimeout         = 5
	ErrModemRejected   = 6
	ErrEncode          = 7
	ErrDecode          = 8
	ErrUnsupported     = 9
	ErrUnknown         = 100

	DCSGSM7 = 0
	DCS8Bit = 4
	DCSUCS2 = 8

	hexCap      = 65536
	responseCap = 4096
	listMax     = 64
)

// Error is a non-OK ct_status from the native library.
type Error struct {
	Status int
	Op     string
}

// Error returns the error message for the Error.
func (e *Error) Error() string {
	detail := StatusString(e.Status)
	if detail == "" {
		return fmt.Sprintf("%s failed (%d)", e.Op, e.Status)
	}
	return fmt.Sprintf("%s failed (%d): %s", e.Op, e.Status, detail)
}

// Message is a decoded or listed SMS. Text fields are UTF-8.
type Message struct {
	Index               int32
	DCS                 int32
	HasUDH              bool
	ServiceCenter       string
	PeerAddress         string
	ServiceTimestamp    string
	UserData            string
	IsConcatenated      bool
	ConcatRef           int32
	ConcatTotal         int32
	ConcatSeq           int32
	IsReassembledConcat bool
}

// StatusReport is a decoded SMS-STATUS-REPORT.
type StatusReport struct {
	MessageReference int32
	TpStatus         int32
	RecipientAddress string
	ServiceTimestamp string
	DischargeTime    string
}

// Layout matches struct ct_message in c_api.h (natural alignment, no padding).
type ctMessage struct {
	Index            int32
	DCS              int32
	HasUDH           int32
	ServiceCenter    [32]byte
	PeerAddress      [32]byte
	ServiceTimestamp [32]byte
	UserData         [512]byte
	IsConcatenated   int32
	ConcatRef        int32
	ConcatTotal      int32
	ConcatSeq        int32
}

// Layout matches struct ct_status_report in c_api.h.
type ctStatusReport struct {
	MessageReference int32
	TpStatus         int32
	RecipientAddress [32]byte
	ServiceTimestamp [32]byte
	DischargeTime    [32]byte
}

// Procs is a collection of procedure pointers.
type procs struct {
	dll *syscall.DLL

	statusString *syscall.Proc
	apiVersion   *syscall.Proc

	modemCreate  *syscall.Proc
	modemDestroy *syscall.Proc
	modemOpen    *syscall.Proc
	modemSend    *syscall.Proc
	modemSendEx  *syscall.Proc
	modemList    *syscall.Proc
	modemDelete  *syscall.Proc
	modemPoll    *syscall.Proc
	modemRunAt   *syscall.Proc

	encodeSubmit       *syscall.Proc
	encodeSubmitEx     *syscall.Proc
	encodeSegments     *syscall.Proc
	encodeSegmentsEx   *syscall.Proc
	decode             *syscall.Proc
	decodeStatusReport *syscall.Proc
}

// LoadOnce is a once.Do for loading the CometTextel SDK.
var (
	loadOnce sync.Once
	loadErr  error
	api      procs
)

// Ensure loads the CometTextel SDK if not already loaded.
func ensure() error {
	loadOnce.Do(func() {
		loadErr = load()
	})
	return loadErr
}

// load loads the CometTextel SDK.
// It finds the DLL, loads it, and sets the procedure pointers.
func load() error {
	path, err := findDLL()
	if err != nil {
		return err
	}
	dll, err := syscall.LoadDLL(path)
	if err != nil {
		return fmt.Errorf("load %s: %w", path, err)
	}
	api.dll = dll

	required := []struct {
		name string
		dest **syscall.Proc
	}{
		{"ct_status_string", &api.statusString},
		{"ct_modem_create", &api.modemCreate},
		{"ct_modem_destroy", &api.modemDestroy},
		{"ct_modem_open", &api.modemOpen},
		{"ct_modem_send", &api.modemSend},
		{"ct_modem_list", &api.modemList},
		{"ct_modem_delete", &api.modemDelete},
		{"ct_pdu_encode_submit", &api.encodeSubmit},
		{"ct_pdu_encode_submit_segments", &api.encodeSegments},
		{"ct_pdu_decode", &api.decode},
	}
	for _, item := range required {
		proc, err := dll.FindProc(item.name)
		if err != nil {
			return fmt.Errorf("%s: missing export %s", path, item.name)
		}
		*item.dest = proc
	}

	api.apiVersion = findOptional(dll, "ct_api_version")
	api.modemSendEx = findOptional(dll, "ct_modem_send_ex")
	api.modemPoll = findOptional(dll, "ct_modem_poll_status_report")
	api.modemRunAt = findOptional(dll, "ct_modem_run_at_command")
	api.encodeSubmitEx = findOptional(dll, "ct_pdu_encode_submit_ex")
	api.encodeSegmentsEx = findOptional(dll, "ct_pdu_encode_submit_segments_ex")
	api.decodeStatusReport = findOptional(dll, "ct_pdu_decode_status_report")
	return nil
}

// FindOptional finds an optional procedure in the DLL.
func findOptional(dll *syscall.DLL, name string) *syscall.Proc {
	proc, err := dll.FindProc(name)
	if err != nil {
		return nil
	}
	return proc
}

// FindDLL finds the CometTextel DLL.
func findDLL() (string, error) {
	var candidates []string
	add := func(path string) {
		if path == "" {
			return
		}
		candidates = append(candidates, path)
	}

	if env := os.Getenv("COMETTEXTEL_LIB"); env != "" {
		info, err := os.Stat(env)
		if err == nil && info.IsDir() {
			add(filepath.Join(env, "comettextel.dll"))
		} else {
			add(env)
		}
	}
	if exe, err := os.Executable(); err == nil {
		add(filepath.Join(filepath.Dir(exe), "comettextel.dll"))
	}
	add(filepath.Join(".", "comettextel.dll"))

	if _, file, _, ok := runtime.Caller(0); ok {
		dir := filepath.Dir(file)
		repo := filepath.Clean(filepath.Join(dir, "..", ".."))
		add(filepath.Join(dir, "comettextel.dll"))
		add(filepath.Join(repo, "build-c-sdk", "Release", "comettextel.dll"))
		add(filepath.Join(repo, "artifact", "comettextel-c-sdk-windows-x64", "bin", "comettextel.dll"))
		add(filepath.Join(repo, "build", "Release", "comettextel.dll"))
	}

	seen := map[string]struct{}{}
	for _, candidate := range candidates {
		abs, err := filepath.Abs(candidate)
		if err != nil {
			continue
		}
		if _, ok := seen[abs]; ok {
			continue
		}
		seen[abs] = struct{}{}
		info, err := os.Stat(abs)
		if err == nil && !info.IsDir() {
			return abs, nil
		}
	}
	return "", fmt.Errorf("comettextel.dll not found; set COMETTEXTEL_LIB to the DLL or its directory")
}

// APIVersion reports the native C ABI feature version.
// A DLL without ct_api_version is reported as version 1.
func APIVersion() (int, error) {
	if err := ensure(); err != nil {
		return 0, err
	}
	if api.apiVersion == nil {
		return 1, nil
	}
	r, _, _ := api.apiVersion.Call()
	return int(int32(r)), nil
}

// StatusString returns the English description for a ct_status code.
func StatusString(status int) string {
	if err := ensure(); err != nil || api.statusString == nil {
		return ""
	}
	r, _, _ := api.statusString.Call(uintptr(int32(status)))
	return goString(r)
}

// Check checks if the status is OK.
func check(status int, op string) error {
	if status == OK {
		return nil
	}
	return &Error{Status: status, Op: op}
}

// Unsupported returns an error for an unsupported operation.
func unsupported(op string) error {
	return &Error{Status: ErrUnsupported, Op: op}
}

// Call calls a procedure with the given arguments.
func call(proc *syscall.Proc, args ...uintptr) int {
	r, _, _ := syscall.SyscallN(proc.Addr(), args...)
	return int(int32(r))
}

// CStr converts a string to a null-terminated C string.
func cstr(s string) []byte {
	return append([]byte(s), 0)
}

// Ptr converts a byte slice to a uintptr.
func ptr(b []byte) uintptr {
	return uintptr(unsafe.Pointer(&b[0]))
}

// CString converts a byte slice to a null-terminated C string.
func cString(b []byte) string {
	for i, c := range b {
		if c == 0 {
			return string(b[:i])
		}
	}
	return string(b)
}

// GoString converts a null-terminated C string to a Go string.
func goString(p uintptr) string {
	if p == 0 {
		return ""
	}
	var n uintptr
	for *(*byte)(unsafe.Pointer(p + n)) != 0 {
		n++
	}
	if n == 0 {
		return ""
	}
	return unsafe.String((*byte)(unsafe.Pointer(p)), n)
}

// MessageFrom converts a ct_message to a Message.
func messageFrom(raw ctMessage) Message {
	return Message{
		Index:               raw.Index,
		DCS:                 raw.DCS,
		HasUDH:              raw.HasUDH != 0,
		ServiceCenter:       cString(raw.ServiceCenter[:]),
		PeerAddress:         cString(raw.PeerAddress[:]),
		ServiceTimestamp:    cString(raw.ServiceTimestamp[:]),
		UserData:            cString(raw.UserData[:]),
		IsConcatenated:      raw.IsConcatenated != 0,
		ConcatRef:           raw.ConcatRef,
		ConcatTotal:         raw.ConcatTotal,
		ConcatSeq:           raw.ConcatSeq,
		IsReassembledConcat: raw.IsConcatenated != 0 && raw.ConcatSeq == 0,
	}
}

// ReportFrom converts a ct_status_report to a StatusReport.
func reportFrom(raw ctStatusReport) StatusReport {
	return StatusReport{
		MessageReference: raw.MessageReference,
		TpStatus:         raw.TpStatus,
		RecipientAddress: cString(raw.RecipientAddress[:]),
		ServiceTimestamp: cString(raw.ServiceTimestamp[:]),
		DischargeTime:    cString(raw.DischargeTime[:]),
	}
}
