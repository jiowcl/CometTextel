/'--------------------------------------------------------------------------------------------
 '  CometTextel — FreeBASIC PDU example (no modem).
 '
 '  Usage:
 '    pdu_example.exe
 '    pdu_example.exe <destination> <text> [smsc]
 '
 '  Place comettextel.dll next to the executable (or the source folder when
 '  compiling). See README.md.
 '
 '  Copyright (c) Ji-Feng Tsai. All rights reserved.
 '  Code released under the MIT license.
 '--------------------------------------------------------------------------------------------
'/

#include once "comettextel.bi"

Sub PrintUsage()
	Print "Usage:"
	Print "  pdu_example.exe"
	Print "  pdu_example.exe <destination> <text> [smsc]"
End Sub

Function FailStatus(ByVal status As Long, ByRef what As String) As Integer
	Print what & " failed (" & Str(status) & "): " & CtStatusString(status)
	Return 2
End Function

Function PrintDecoded(ByRef pduHex As String) As Integer
	Dim As CtMessage msg
	Dim As Long status = CtDecode(pduHex, @msg)
	If status <> CT_OK Then Return FailStatus(status, "ct_pdu_decode")

	Print "peer=" & CtMessagePeer(@msg) & " text=" & CtMessageUserData(@msg) & _
		" dcs=" & Str(msg.dcs) & " has_udh=" & Str(msg.has_udh) & _
		" concat=" & Str(msg.is_concatenated) & " ref=" & Str(msg.concat_ref) & _
		" total=" & Str(msg.concat_total) & " seq=" & Str(msg.concat_seq)
	Return 0
End Function

Function RunPdu(ByRef destination As String, ByRef text As String, ByRef smsc As String) As Integer
	Dim As Long count = 0
	Dim As Long status = 0
	Dim As String joined = CtEncodeSubmitSegments(destination, text, smsc, CT_DCS_UCS2, @status, @count)

	If status <> CT_OK Then Return FailStatus(status, "ct_pdu_encode_submit_segments")
	If Len(joined) = 0 Then
		Print "encode returned empty hex (count=" & Str(count) & ")"
		Return 2
	End If

	Dim As Integer startPos = 1
	Dim As Integer nl
	Do
		nl = InStr(startPos, joined, !"\n")
		Dim As String part
		If nl = 0 Then
			part = Mid(joined, startPos)
		Else
			part = Mid(joined, startPos, nl - startPos)
		End If

		If Len(part) > 0 Then
			Print part
			If PrintDecoded(part) <> 0 Then Return 2
		End If

		If nl = 0 Then Exit Do
		startPos = nl + 1
	Loop

	Print "segments=" & Str(count)
	Return 0
End Function

Function RunSelfCheck() As Integer
	Dim As Long status = 0
	Dim As CtMessage msg
	Dim As String hexOne
	Dim As WString * 64 chineseW
	Dim As String chinese
	Dim As Long count = 0
	Dim As String joined
	Dim As String longText
	Dim As String payload
	Dim As Integer startPos
	Dim As Integer seq
	Dim As Integer nl
	Dim As String part

	/' Avoid CJK in source literals (file encoding). Use code points like PureBasic. '/
	chineseW = WChr(&h6E2C) & WChr(&h8A66) & WChr(&h4E2D) & WChr(&h6587) & WChr(&h7C21) & WChr(&h8A0A)
	chinese = CtUtf8FromW(chineseW)

	Print "-- self-check UCS-2 ASCII --"
	If RunPdu("886912345678", "Hello from FreeBASIC", "886932000000") <> 0 Then Return 2

	Print "-- self-check UCS-2 Chinese --"
	joined = CtEncodeSubmitSegments("886912345678", chinese, "886932000000", CT_DCS_UCS2, @status, @count)
	If status <> CT_OK Then Return FailStatus(status, "ct_pdu_encode_submit_segments")
	If InStr(joined, "FFFD") > 0 Then
		Print "Chinese encode produced U+FFFD replacement (bad UTF-8 input)"
		Return 2
	End If
	If InStr(UCase(joined), "6E2C") = 0 Then
		Print "Chinese UCS-2 hex missing 6E2C"
		Return 2
	End If
	Print joined
	status = CtDecode(joined, @msg)
	If status <> CT_OK Then Return FailStatus(status, "ct_pdu_decode")
	PrintDecoded(joined)
	If CtMessageUserData(@msg) <> chinese OrElse CtMessagePeer(@msg) <> "886912345678" Then
		Print "Chinese UCS-2 round-trip mismatch"
		Return 2
	End If
	Print "ok (6 CJK chars, UCS-2)"
	Print "segments=" & Str(count)

	Print "-- self-check UCS-2 concat (71 ASCII -> 2 segments) --"
	longText = String(71, Asc("B"))
	count = 0
	joined = CtEncodeSubmitSegments("886912345678", longText, "886932000000", CT_DCS_UCS2, @status, @count)
	If status <> CT_OK Then Return FailStatus(status, "ct_pdu_encode_submit_segments")
	If count <> 2 Then
		Print "expected 2 segments, got " & Str(count)
		Return 2
	End If

	payload = ""
	startPos = 1
	seq = 0
	Do
		nl = InStr(startPos, joined, !"\n")
		If nl = 0 Then
			part = Mid(joined, startPos)
		Else
			part = Mid(joined, startPos, nl - startPos)
		End If
		If Len(part) > 0 Then
			seq += 1
			Print part
			status = CtDecode(part, @msg)
			If status <> CT_OK Then Return FailStatus(status, "ct_pdu_decode")
			PrintDecoded(part)
			If msg.is_concatenated = 0 OrElse msg.concat_total <> 2 OrElse msg.concat_seq <> seq Then
				Print "concat metadata mismatch"
				Return 2
			End If
			payload &= CtMessageUserData(@msg)
		End If
		If nl = 0 Then Exit Do
		startPos = nl + 1
	Loop

	If payload <> longText Then
		Print "concat payload round-trip mismatch"
		Return 2
	End If
	Print "ok"

	Print "-- self-check single-segment EncodeSubmit --"
	hexOne = CtEncodeSubmit("886912345678", "Hello", "886932000000", CT_DCS_UCS2, @status)
	If status <> CT_OK Then Return FailStatus(status, "ct_pdu_encode_submit")
	If Len(hexOne) = 0 Then
		Print "EncodeSubmit returned empty hex"
		Return 2
	End If
	Print hexOne
	status = CtDecode(hexOne, @msg)
	If status <> CT_OK Then Return FailStatus(status, "ct_pdu_decode")
	If CtMessageUserData(@msg) <> "Hello" OrElse CtMessagePeer(@msg) <> "886912345678" Then
		Print "round-trip mismatch"
		Return 2
	End If
	Print "ok"
	Return 0
End Function

Dim As Integer rc = 1
Dim As String smsc = ""

If CtInit() = 0 Then
	Print CtLoadError
	Print "Use C SDK bin\comettextel.dll (must export ct_pdu_encode_submit_segments)."
	Print "Place it next to the .exe / source folder."
	End 1
End If

If Command(1) = "" Then
	rc = RunSelfCheck()
ElseIf Command(2) = "" Then
	PrintUsage()
	rc = 1
Else
	smsc = Command(3)
	rc = RunPdu(Command(1), Command(2), smsc)
End If

CtShutdown()
End rc
