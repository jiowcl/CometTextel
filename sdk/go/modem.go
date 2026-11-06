// Copyright (c) Ji-Feng Tsai. All rights reserved.
// Code released under the MIT license.

//go:build windows && amd64

package comettextel

import (
	"runtime"
	"unsafe"
)

// Modem owns a native ct_modem handle.
type Modem struct {
	handle uintptr
}

// NewModem creates a modem. Call Close when finished.
// It creates a new modem handle and returns it.
func NewModem() (*Modem, error) {
	if err := ensure(); err != nil {
		return nil, err
	}
	handle, _, _ := api.modemCreate.Call()
	if handle == 0 {
		return nil, &Error{Status: ErrUnknown, Op: "ct_modem_create"}
	}
	return &Modem{handle: handle}, nil
}

// Close destroys the native modem. It is safe to call more than once.
// It destroys the modem handle.
func (m *Modem) Close() {
	if m == nil || m.handle == 0 {
		return
	}
	if api.modemDestroy != nil {
		api.modemDestroy.Call(m.handle)
	}
	m.handle = 0
}

// Open opens the serial port and initializes PDU mode.
func (m *Modem) Open(port string, baud uint32) error {
	if m == nil || m.handle == 0 {
		return &Error{Status: ErrNotOpen, Op: "ct_modem_open"}
	}
	portB := cstr(port)
	status := call(api.modemOpen, m.handle, ptr(portB), uintptr(baud))
	runtime.KeepAlive(portB)
	return check(status, "ct_modem_open")
}

// Send submits one SMS. TP-VP and TP-SRR are omitted. timeoutMs is the OK/ERROR wait.
func (m *Modem) Send(smsc, destination, text string, dcs, timeoutMs int) error {
	if m == nil || m.handle == 0 {
		return &Error{Status: ErrNotOpen, Op: "ct_modem_send"}
	}
	smscB := cstr(smsc)
	destB := cstr(destination)
	textB := cstr(text)
	status := call(api.modemSend,
		m.handle, ptr(smscB), ptr(destB), ptr(textB),
		uintptr(int32(dcs)), uintptr(int32(timeoutMs)),
	)
	runtime.KeepAlive(smscB)
	runtime.KeepAlive(destB)
	runtime.KeepAlive(textB)
	return check(status, "ct_modem_send")
}

// SendEx submits one SMS with explicit TP-VP / TP-SRR.
// relativeValidityPeriod -1 omits TP-VP. requestStatusReport non-zero sets TP-SRR.
func (m *Modem) SendEx(smsc, destination, text string, dcs, relativeValidityPeriod, requestStatusReport, timeoutMs int) error {
	if m == nil || m.handle == 0 {
		return &Error{Status: ErrNotOpen, Op: "ct_modem_send_ex"}
	}
	if api.modemSendEx == nil {
		return unsupported("ct_modem_send_ex")
	}
	smscB := cstr(smsc)
	destB := cstr(destination)
	textB := cstr(text)
	status := call(api.modemSendEx,
		m.handle, ptr(smscB), ptr(destB), ptr(textB),
		uintptr(int32(dcs)),
		uintptr(int32(relativeValidityPeriod)),
		uintptr(int32(requestStatusReport)),
		uintptr(int32(timeoutMs)),
	)
	runtime.KeepAlive(smscB)
	runtime.KeepAlive(destB)
	runtime.KeepAlive(textB)
	return check(status, "ct_modem_send_ex")
}

// List reads stored messages. Complete concat sets are rejoined (ConcatSeq == 0).
func (m *Modem) List(timeoutMs int) ([]Message, error) {
	if m == nil || m.handle == 0 {
		return nil, &Error{Status: ErrNotOpen, Op: "ct_modem_list"}
	}
	raw := make([]ctMessage, listMax)
	var count int32
	status := call(api.modemList,
		m.handle,
		uintptr(unsafe.Pointer(&raw[0])),
		uintptr(listMax),
		uintptr(unsafe.Pointer(&count)),
		uintptr(int32(timeoutMs)),
	)
	runtime.KeepAlive(raw)
	runtime.KeepAlive(&count)
	if err := check(status, "ct_modem_list"); err != nil {
		return nil, err
	}
	if count < 0 {
		count = 0
	}
	if int(count) > len(raw) {
		count = int32(len(raw))
	}
	out := make([]Message, 0, count)
	for i := int32(0); i < count; i++ {
		out = append(out, messageFrom(raw[i]))
	}
	return out, nil
}

// Delete removes one stored message by modem index.
func (m *Modem) Delete(index, timeoutMs int) error {
	if m == nil || m.handle == 0 {
		return &Error{Status: ErrNotOpen, Op: "ct_modem_delete"}
	}
	status := call(api.modemDelete, m.handle, uintptr(int32(index)), uintptr(int32(timeoutMs)))
	return check(status, "ct_modem_delete")
}

// PollStatusReport drains +CDS URCs and returns one SMS-STATUS-REPORT.
// timeoutMs 0 is non-blocking. Requires C ABI version 3.
func (m *Modem) PollStatusReport(timeoutMs int) (StatusReport, error) {
	if m == nil || m.handle == 0 {
		return StatusReport{}, &Error{Status: ErrNotOpen, Op: "ct_modem_poll_status_report"}
	}
	if api.modemPoll == nil {
		return StatusReport{}, unsupported("ct_modem_poll_status_report")
	}
	var raw ctStatusReport
	status := call(api.modemPoll, m.handle, uintptr(unsafe.Pointer(&raw)), uintptr(int32(timeoutMs)))
	runtime.KeepAlive(&raw)
	if err := check(status, "ct_modem_poll_status_report"); err != nil {
		return StatusReport{}, err
	}
	return reportFrom(raw), nil
}

// RunAtCommand sends one AT command and returns accumulated modem text.
// A trailing CR is appended by the native library when missing.
// The text is truncated to responseCap-1 bytes with a trailing NUL; truncation
// does not change the status code. Requires C ABI version 3.
func (m *Modem) RunAtCommand(command string, timeoutMs int) (string, error) {
	if m == nil || m.handle == 0 {
		return "", &Error{Status: ErrNotOpen, Op: "ct_modem_run_at_command"}
	}
	if command == "" {
		return "", &Error{Status: ErrInvalidArgument, Op: "ct_modem_run_at_command"}
	}
	if api.modemRunAt == nil {
		return "", unsupported("ct_modem_run_at_command")
	}
	cmd := cstr(command)
	out := make([]byte, responseCap)
	status := call(api.modemRunAt,
		m.handle, ptr(cmd), ptr(out), uintptr(len(out)), uintptr(int32(timeoutMs)),
	)
	runtime.KeepAlive(cmd)
	runtime.KeepAlive(out)
	if err := check(status, "ct_modem_run_at_command"); err != nil {
		return cString(out), err
	}
	return cString(out), nil
}
