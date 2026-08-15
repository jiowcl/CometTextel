/'--------------------------------------------------------------------------------------------
 '  CometTextel — thin FreeBASIC FFI for the C ABI (PDU helpers).
 '
 '  Windows x64: DyLibLoad + cdecl function pointers. Runtime-loads comettextel.dll
 '  from the C SDK — no import library step.
 '
 '  Mirrors include/comettextel/c_api.h — do not add a second ABI here.
 '
 '  Copyright (c) Ji-Feng Tsai. All rights reserved.
 '  Code released under the MIT license.
 '--------------------------------------------------------------------------------------------
'/

#ifndef COMETTEXTEL_BI
#define COMETTEXTEL_BI

#include once "windows.bi"
#include once "crt.bi"

#if Not Defined(__FB_WIN32__)
  #error "CometTextel FreeBASIC sample targets Windows (comettextel.dll)."
#endif

#if Not Defined(__FB_64BIT__)
  #error "CometTextel FreeBASIC sample requires x64 (win-x64 native DLL)."
#endif

Const CT_OK As Long = 0
Const CT_ERR_INVALID_ARGUMENT As Long = 1
Const CT_ERR_NOT_OPEN As Long = 2
Const CT_ERR_ALREADY_OPEN As Long = 3
Const CT_ERR_IO As Long = 4
Const CT_ERR_TIMEOUT As Long = 5
Const CT_ERR_MODEM_REJECTED As Long = 6
Const CT_ERR_ENCODE As Long = 7
Const CT_ERR_DECODE As Long = 8
Const CT_ERR_UNSUPPORTED As Long = 9
Const CT_ERR_UNKNOWN As Long = 100

Const CT_DCS_GSM7 As Long = 0
Const CT_DCS_8BIT As Long = 4
Const CT_DCS_UCS2 As Long = 8

/' Layout must match struct ct_message in c_api.h (MSVC x64 C alignment).
   Use UByte arrays — ZString * N is NOT the same size as char[N]. '/
Type CtMessage
	index As Long
	dcs As Long
	has_udh As Long
	service_center(0 To 31) As UByte
	peer_address(0 To 31) As UByte
	service_timestamp(0 To 31) As UByte
	user_data(0 To 511) As UByte
	is_concatenated As Long
	concat_ref As Long
	concat_total As Long
	concat_seq As Long
End Type

Type CtStatusStringFn As Function CDecl (ByVal status As Long) As ZString Ptr
Type CtPduEncodeSubmitFn As Function CDecl ( _
	ByVal smsc As ZString Ptr, _
	ByVal destination As ZString Ptr, _
	ByVal text As ZString Ptr, _
	ByVal dcs As Long, _
	ByVal out_hex As Any Ptr, _
	ByVal out_hex_cap As UInteger _
	) As Long
Type CtPduEncodeSubmitSegmentsFn As Function CDecl ( _
	ByVal smsc As ZString Ptr, _
	ByVal destination As ZString Ptr, _
	ByVal text As ZString Ptr, _
	ByVal dcs As Long, _
	ByVal out_hex As Any Ptr, _
	ByVal out_hex_cap As UInteger, _
	ByVal out_count As Long Ptr _
	) As Long
Type CtPduDecodeFn As Function CDecl ( _
	ByVal pdu_hex As ZString Ptr, _
	ByVal msg As CtMessage Ptr _
	) As Long

Dim Shared CtLib As Any Ptr = 0
Dim Shared CtLoadError As String
Dim Shared CtLoadedPath As String
Dim Shared ct_status_string As CtStatusStringFn = 0
Dim Shared ct_pdu_encode_submit As CtPduEncodeSubmitFn = 0
Dim Shared ct_pdu_encode_submit_segments As CtPduEncodeSubmitSegmentsFn = 0
Dim Shared ct_pdu_decode As CtPduDecodeFn = 0

' <summary>
' CtJoinPath
' </summary>
' <param name="dir">String</param>
' <param name="file">String</param>
' <returns>Returns String.</returns>
Private Function CtJoinPath(ByRef folder As String, ByRef leaf As String) As String
	If Len(folder) = 0 Then Return leaf

	Dim As String lastCh = Right(folder, 1)

	If lastCh <> "\" AndAlso lastCh <> "/" Then
		Return folder & "\" & leaf
	End If

	Return folder & leaf
End Function

' <summary>
' CtBindExport
' </summary>
' <param name="name">WString</param>
' <returns>Returns Any Ptr.</returns>
Private Function CtBindExport(ByRef exportName As String) As Any Ptr
	Dim As Any Ptr fn = DyLibSymbol(CtLib, exportName)

	If fn = 0 Then
		CtLoadError = "DyLibSymbol('" & exportName & "') failed in " & CtLoadedPath & _
			". Need a C ABI build (ct_* exports), not an old DLL."
		Return 0
	End If

	Return fn
End Function

' <summary>
' CtUtf8FromW - Wide → UTF-8 bytes in a String (safe for C ABI text args).
' </summary>
' <param name="w">WString</param>
' <returns>Returns String.</returns>
Function CtUtf8FromW(ByRef w As WString) As String
	Dim As Integer nbytes = WideCharToMultiByte(CP_UTF8, 0, StrPtr(w), -1, NULL, 0, NULL, NULL)

	If nbytes <= 1 Then Return ""

	Dim As String utf8 = String(nbytes - 1, 0)
	WideCharToMultiByte(CP_UTF8, 0, StrPtr(w), -1, StrPtr(utf8), nbytes, NULL, NULL)

	Return utf8
End Function

' <summary>
' CtUtf8Z - Read a NUL-terminated UTF-8 field into a String (keeps UTF-8 bytes).
' </summary>
' <param name="p">UByte Ptr</param>
' <param name="maxBytes">Integer</param>
' <returns>Returns String.</returns>
Function CtUtf8Z(ByVal p As UByte Ptr, ByVal maxBytes As Integer) As String
	If p = 0 OrElse maxBytes <= 0 Then Return ""

	Dim As Integer n = 0

	Do While n < maxBytes AndAlso p[n] <> 0
		n += 1
	Loop

	If n = 0 Then Return ""

	Dim As String utf8 = String(n, 0)
	memcpy(StrPtr(utf8), p, n)

	Return utf8
End Function

' <summary>
' CtPeekAsciiZ
' </summary>
' <param name="p">ZString Ptr</param>
' <param name="maxBytes">Integer</param>
' <returns>Returns String.</returns>
Function CtPeekAsciiZ(ByVal p As ZString Ptr, ByVal maxBytes As Integer) As String
	If p = 0 OrElse maxBytes <= 0 Then Return ""

	Dim As Integer n = 0
	Dim As UByte Ptr b = Cast(UByte Ptr, p)

	Do While n < maxBytes AndAlso b[n] <> 0
		n += 1
	Loop

	If n = 0 Then Return ""

	Dim As String ascii = String(n, 0)
	memcpy(StrPtr(ascii), b, n)

	Return ascii
End Function

' <summary>
' CtInit
' </summary>
' <param name="dllPath">String</param>
' <returns>Returns Long.</returns>
Function CtInit(ByRef dllPath As String = "comettextel.dll") As Long
	If CtLib <> 0 Then Return 1

	CtLoadError = ""
	CtLoadedPath = ""

	Dim As String cand(0 To 2)
	Dim As Integer n = 0
	Dim As Integer i

	If InStr(dllPath, "\") > 0 OrElse InStr(dllPath, "/") > 0 OrElse _
	   (Len(dllPath) >= 2 AndAlso Mid(dllPath, 2, 1) = ":") Then
		cand(0) = dllPath
		n = 1
	Else
		cand(0) = CtJoinPath(Exepath(), dllPath)
		cand(1) = CtJoinPath(CurDir(), dllPath)
		cand(2) = dllPath
		n = 3
	End If

	For i = 0 To n - 1
		If Len(cand(i)) = 0 Then Continue For

		If i < n - 1 Then
			If Dir(cand(i)) = "" Then Continue For
		End If

		CtLib = DyLibLoad(cand(i))

		If CtLib <> 0 Then
			CtLoadedPath = cand(i)
			Exit For
		End If
	Next

	If CtLib = 0 Then
		CtLoadError = "DyLibLoad failed for '" & dllPath & "'"
		Return 0
	End If

	ct_status_string = Cast(CtStatusStringFn, CtBindExport("ct_status_string"))

	If ct_status_string = 0 Then
		DyLibFree(CtLib) : CtLib = 0
		Return 0
	End If

	ct_pdu_encode_submit = Cast(CtPduEncodeSubmitFn, CtBindExport("ct_pdu_encode_submit"))

	If ct_pdu_encode_submit = 0 Then
		DyLibFree(CtLib) : CtLib = 0
		Return 0
	End If

	ct_pdu_encode_submit_segments = Cast(CtPduEncodeSubmitSegmentsFn, CtBindExport("ct_pdu_encode_submit_segments"))

	If ct_pdu_encode_submit_segments = 0 Then
		DyLibFree(CtLib) : CtLib = 0
		Return 0
	End If

	ct_pdu_decode = Cast(CtPduDecodeFn, CtBindExport("ct_pdu_decode"))

	If ct_pdu_decode = 0 Then
		DyLibFree(CtLib) : CtLib = 0
		Return 0
	End If

	Return 1
End Function

' <summary>
' CtShutdown
' </summary>
' <returns>Returns void.</returns>
Sub CtShutdown()
	If CtLib <> 0 Then
		DyLibFree(CtLib)
		CtLib = 0
	End If

	ct_status_string = 0
	ct_pdu_encode_submit = 0
	ct_pdu_encode_submit_segments = 0
	ct_pdu_decode = 0
End Sub

' <summary>
' CtStatusString
' </summary>
' <param name="status">Long</param>
' <returns>Returns String.</returns>
Function CtStatusString(ByVal status As Long) As String
	If CtLib = 0 OrElse ct_status_string = 0 Then
		Return "status " & Str(status)
	End If

	Dim As ZString Ptr msg = ct_status_string(status)
	If msg = 0 Then Return "status " & Str(status)

	Return *msg
End Function

Function CtMessagePeer(ByVal msg As CtMessage Ptr) As String
	Return CtUtf8Z(@msg->peer_address(0), 32)
End Function

' <summary>
' CtMessageUserData
' </summary>
' <param name="msg">CtMessage Ptr</param>
' <returns>Returns String.</returns>
Function CtMessageUserData(ByVal msg As CtMessage Ptr) As String
	Return CtUtf8Z(@msg->user_data(0), 512)
End Function

' <summary>
' CtEncodeSubmit
' </summary>
' <param name="destination">String</param>
' <param name="text">String</param>
' <param name="serviceCenter">String</param>
' <param name="dcs">Long</param>
' <param name="pStatus"> Long Ptr</param>
' <returns>Returns String.</returns>
Function CtEncodeSubmit( _
	ByRef destination As String, _
	ByRef text As String, _
	ByRef serviceCenter As String, _
	ByVal dcs As Long, _
	ByVal pStatus As Long Ptr _
	) As String

	If CtLib = 0 Then
		If pStatus <> 0 Then *pStatus = CT_ERR_NOT_OPEN
		Return ""
	End If

	Const cap As Integer = 1024
	Dim As ZString * 1024 hexbuf
	Clear hexbuf, 0, SizeOf(hexbuf)

	Dim As String smsc = serviceCenter
	Dim As String dest = destination
	Dim As String body = text

	Dim As Long result = ct_pdu_encode_submit(StrPtr(smsc), StrPtr(dest), StrPtr(body), dcs, @hexbuf, cap)

	If pStatus <> 0 Then *pStatus = result
	If result <> CT_OK Then Return ""

	Return CtPeekAsciiZ(@hexbuf, cap)
End Function

' <summary>
' CtEncodeSubmitSegments
' </summary>
' <param name="destination">String</param>
' <param name="text">String</param>
' <param name="serviceCenter">String</param>
' <param name="dcs">Long</param>
' <param name="pStatus"> Long Ptr</param>
' <param name="pOutCount"> Long Ptr</param>
' <returns>Returns String.</returns>
Function CtEncodeSubmitSegments( _
	ByRef destination As String, _
	ByRef text As String, _
	ByRef serviceCenter As String, _
	ByVal dcs As Long, _
	ByVal pStatus As Long Ptr, _
	ByVal pOutCount As Long Ptr _
	) As String

	If CtLib = 0 Then
		If pStatus <> 0 Then *pStatus = CT_ERR_NOT_OPEN
		Return ""
	End If

	Const cap As Integer = 131072
	Dim As String hexbuf = String(cap, 0)
	Dim As Long count = 0

	Dim As String smsc = serviceCenter
	Dim As String dest = destination
	Dim As String body = text

	Dim As Long result = ct_pdu_encode_submit_segments( _
		StrPtr(smsc), StrPtr(dest), StrPtr(body), dcs, StrPtr(hexbuf), cap, @count)

	If pOutCount <> 0 Then *pOutCount = count
	If pStatus <> 0 Then *pStatus = result
	If result <> CT_OK Then Return ""

	Return CtPeekAsciiZ(StrPtr(hexbuf), cap)
End Function

' <summary>
' CtDecode
' </summary>
' <param name="pduHex">String</param>
' <param name="msg">CtMessage Ptr</param>
' <returns>Returns Long.</returns>
Function CtDecode(ByRef pduHex As String, ByVal msg As CtMessage Ptr) As Long
	If CtLib = 0 Then Return CT_ERR_NOT_OPEN
	If msg = 0 Then Return CT_ERR_INVALID_ARGUMENT

	Dim As String pdu = pduHex
	Clear *msg, 0, SizeOf(CtMessage)

	Return ct_pdu_decode(StrPtr(pdu), msg)
End Function

' <summary>
' CtEncodeSubmitW
' </summary>
' <param name="destination">WString</param>
' <param name="text">WString</param>
' <param name="serviceCenter">WString</param>
' <param name="dcs">Long</param>
' <param name="pStatus">Long Ptr</param>
' <returns>Returns string.</returns>
Function CtEncodeSubmitW( _
	ByRef destination As WString, _
	ByRef text As WString, _
	ByRef serviceCenter As WString, _
	ByVal dcs As Long, _
	ByVal pStatus As Long Ptr _
	) As String

	Return CtEncodeSubmit(CtUtf8FromW(destination), CtUtf8FromW(text), CtUtf8FromW(serviceCenter), dcs, pStatus)
End Function

' <summary>
' CtEncodeSubmitSegmentsW
' </summary>
' <param name="destination">WString</param>
' <param name="text">WString</param>
' <param name="serviceCenter">WString</param>
' <param name="dcs">Long</param>
' <param name="status">Long Ptr</param>
' <param name="outCount">Long Ptr</param>
' <returns>Returns string.</returns>
Function CtEncodeSubmitSegmentsW( _
	ByRef destination As WString, _
	ByRef text As WString, _
	ByRef serviceCenter As WString, _
	ByVal dcs As Long, _
	ByVal pStatus As Long Ptr, _
	ByVal pOutCount As Long Ptr _
	) As String

	Return CtEncodeSubmitSegments( _
		CtUtf8FromW(destination), CtUtf8FromW(text), CtUtf8FromW(serviceCenter), dcs, pStatus, pOutCount)
End Function

#endif /' COMETTEXTEL_BI '/
