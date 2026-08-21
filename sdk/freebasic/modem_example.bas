/'--------------------------------------------------------------------------------------------
 '  CometTextel — FreeBASIC modem example (list / send / delete).
 '
 '  Usage:
 '    modem_example.exe list <port> [baud]
 '    modem_example.exe send <port> <smsc> <destination> <text> [baud]
 '    modem_example.exe delete <port> <index> [baud]
 '
 '  Requires comettextel.dll (C ABI) next to the exe. See README.md.
 '
 '  Copyright (c) Ji-Feng Tsai. All rights reserved.
 '  Code released under the MIT license.
 '--------------------------------------------------------------------------------------------
'/

#include once "comettextel.bi"

Sub PrintUsage()
	Print "Usage:"
	Print "  modem_example.exe list <port> [baud]"
	Print "  modem_example.exe send <port> <smsc> <destination> <text> [baud]"
	Print "  modem_example.exe delete <port> <index> [baud]"
End Sub

Function ParseBaud(ByRef text As String, ByVal fallback As Long = 115200) As Long
	Dim As Long v = Val(text)
	If v <= 0 Then Return fallback
	Return v
End Function

Function RunList(ByRef port As String, ByVal baud As ULong) As Integer
	Dim As Any Ptr modem = CtModemCreate()
	If modem = 0 Then
		Print "ct_modem_create failed"
		Return 2
	End If

	Dim As Long status = CtModemOpen(modem, port, baud)
	If status <> CT_OK Then
		Print "open: "; CtStatusString(status)
		CtModemDestroy(modem)
		Return 2
	End If

	Dim As CtMessage msgs(0 To 63)
	Dim As Long count = 0
	status = CtModemList(modem, @msgs(0), 64, @count)
	If status <> CT_OK Then
		Print "list: "; CtStatusString(status)
		CtModemDestroy(modem)
		Return 2
	End If

	Dim As Integer i
	For i = 0 To count - 1
		Dim As String flag = ""
		If msgs(i).is_concatenated <> 0 AndAlso msgs(i).concat_seq = 0 Then
			flag = " reassembled"
		End If
		Print "[" & Str(msgs(i).index) & "] " & CtMessagePeer(@msgs(i)) & ": " & _
			CtMessageUserData(@msgs(i)) & _
			" (dcs=" & Str(msgs(i).dcs) & _
			" concat=" & Str(msgs(i).is_concatenated) & _
			" seq=" & Str(msgs(i).concat_seq) & "/" & Str(msgs(i).concat_total) & flag & ")"
	Next
	Print "count=" & Str(count)

	CtModemDestroy(modem)
	Return 0
End Function

Function RunSend( _
	ByRef port As String, _
	ByRef smsc As String, _
	ByRef destination As String, _
	ByRef text As String, _
	ByVal baud As ULong _
	) As Integer

	Dim As Any Ptr modem = CtModemCreate()
	If modem = 0 Then
		Print "ct_modem_create failed"
		Return 2
	End If

	Dim As Long status = CtModemOpen(modem, port, baud)
	If status <> CT_OK Then
		Print "open: "; CtStatusString(status)
		CtModemDestroy(modem)
		Return 2
	End If

	status = CtModemSend(modem, destination, text, smsc, CT_DCS_UCS2)
	If status <> CT_OK Then
		Print "send: "; CtStatusString(status)
		CtModemDestroy(modem)
		Return 2
	End If

	Print "Sent."
	CtModemDestroy(modem)
	Return 0
End Function

Function RunDelete(ByRef port As String, ByVal index As Long, ByVal baud As ULong) As Integer
	Dim As Any Ptr modem = CtModemCreate()
	If modem = 0 Then
		Print "ct_modem_create failed"
		Return 2
	End If

	Dim As Long status = CtModemOpen(modem, port, baud)
	If status <> CT_OK Then
		Print "open: "; CtStatusString(status)
		CtModemDestroy(modem)
		Return 2
	End If

	status = CtModemDelete(modem, index)
	If status <> CT_OK Then
		Print "delete: "; CtStatusString(status)
		CtModemDestroy(modem)
		Return 2
	End If

	Print "Deleted."
	CtModemDestroy(modem)
	Return 0
End Function

If CtInit() = 0 Then
	Print CtLoadError
	End 2
End If

Dim As Integer argc = __FB_ARGC__
Dim As Integer exitCode = 1

If argc < 3 Then
	PrintUsage()
Else
	Dim As String cmd = LCase(*__FB_ARGV__[1])
	If cmd = "list" Then
		Dim As ULong baud = 115200
		If argc >= 4 Then baud = CULng(ParseBaud(*__FB_ARGV__[3]))
		exitCode = RunList(*__FB_ARGV__[2], baud)
	ElseIf cmd = "send" Then
		If argc < 6 Then
			PrintUsage()
		Else
			Dim As ULong baud = 115200
			If argc >= 7 Then baud = CULng(ParseBaud(*__FB_ARGV__[6]))
			exitCode = RunSend(*__FB_ARGV__[2], *__FB_ARGV__[3], *__FB_ARGV__[4], *__FB_ARGV__[5], baud)
		End If
	ElseIf cmd = "delete" Then
		If argc < 4 Then
			PrintUsage()
		Else
			Dim As ULong baud = 115200
			If argc >= 5 Then baud = CULng(ParseBaud(*__FB_ARGV__[4]))
			exitCode = RunDelete(*__FB_ARGV__[2], Val(*__FB_ARGV__[3]), baud)
		End If
	Else
		PrintUsage()
	End If
End If

CtShutdown()
End exitCode
