;--------------------------------------------------------------------------------------------
;  CometTextel — thin PureBasic FFI for the C ABI (PDU + modem).
;
;  Windows x64: OpenLibrary + PrototypeC (cdecl). Runtime-loads comettextel.dll
;  from the C SDK — no import library / lld-link step.
;
;  Mirrors include/comettextel/c_api.h — do not add a second ABI here.
;
;  Copyright (c) Ji-Feng Tsai. All rights reserved.
;  Code released under the MIT license.
;--------------------------------------------------------------------------------------------

CompilerIf #PB_Compiler_OS <> #PB_OS_Windows
  CompilerError "CometTextel PureBasic sample targets Windows (comettextel.dll)."
CompilerEndIf

CompilerIf #PB_Compiler_Processor <> #PB_Processor_x64
  CompilerError "CometTextel PureBasic sample requires x64 (win-x64 native DLL)."
CompilerEndIf

#CT_OK = 0
#CT_ERR_INVALID_ARGUMENT = 1
#CT_ERR_NOT_OPEN = 2
#CT_ERR_ALREADY_OPEN = 3
#CT_ERR_IO = 4
#CT_ERR_TIMEOUT = 5
#CT_ERR_MODEM_REJECTED = 6
#CT_ERR_ENCODE = 7
#CT_ERR_DECODE = 8
#CT_ERR_UNSUPPORTED = 9
#CT_ERR_UNKNOWN = 100

#CT_DCS_GSM7 = 0
#CT_DCS_8BIT = 4
#CT_DCS_UCS2 = 8

; Layout must match struct ct_message in c_api.h (C alignment).
Structure CtMessage Align #PB_Structure_AlignC
  index.l
  dcs.l
  has_udh.l
  service_center.a[32]
  peer_address.a[32]
  service_timestamp.a[32]
  user_data.a[512]
  is_concatenated.l
  concat_ref.l
  concat_total.l
  concat_seq.l
EndStructure

PrototypeC.i Proto_ct_status_string(status.l)
PrototypeC.i Proto_ct_modem_create()
PrototypeC Proto_ct_modem_destroy(*modem)
PrototypeC.l Proto_ct_modem_open(*modem, *port, baudRate.l)
PrototypeC.l Proto_ct_modem_send(*modem, *smsc, *destination, *text, dcs.l, timeoutMs.l)
PrototypeC.l Proto_ct_modem_send_ex(*modem, *smsc, *destination, *text, dcs.l, relativeValidityPeriod.l, requestStatusReport.l, timeoutMs.l)
PrototypeC.l Proto_ct_modem_list(*modem, *outMessages, maxCount.l, *outCount, timeoutMs.l)
PrototypeC.l Proto_ct_modem_delete(*modem, index.l, timeoutMs.l)
PrototypeC.l Proto_ct_pdu_encode_submit(*smsc, *destination, *text, dcs.l, *out_hex, out_hex_cap.i)
PrototypeC.l Proto_ct_pdu_encode_submit_ex(*smsc, *destination, *text, dcs.l, relativeValidityPeriod.l, requestStatusReport.l, *out_hex, out_hex_cap.i)
PrototypeC.l Proto_ct_pdu_encode_submit_segments(*smsc, *destination, *text, dcs.l, *out_hex, out_hex_cap.i, *out_count)
PrototypeC.l Proto_ct_pdu_encode_submit_segments_ex(*smsc, *destination, *text, dcs.l, relativeValidityPeriod.l, requestStatusReport.l, *out_hex, out_hex_cap.i, *out_count)
PrototypeC.l Proto_ct_pdu_decode(*pdu_hex, *out)

; TODO: Using PureBasic's built-in functions.
Import "kernel32.lib"
  LoadLibraryW.i(lpFileName.p-unicode)
  GetProcAddress.i(hModule.i, lpProcName.p-ascii)
  FreeLibrary.i(hModule.i)
  GetLastError.l()
EndImport

Global CtLib.i = 0
Global CtLoadError.s = ""
Global CtLoadedPath.s = ""
Global ct_status_string.Proto_ct_status_string
Global ct_modem_create.Proto_ct_modem_create
Global ct_modem_destroy.Proto_ct_modem_destroy
Global ct_modem_open.Proto_ct_modem_open
Global ct_modem_send.Proto_ct_modem_send
Global ct_modem_send_ex.Proto_ct_modem_send_ex
Global ct_modem_list.Proto_ct_modem_list
Global ct_modem_delete.Proto_ct_modem_delete
Global ct_pdu_encode_submit.Proto_ct_pdu_encode_submit
Global ct_pdu_encode_submit_ex.Proto_ct_pdu_encode_submit_ex
Global ct_pdu_encode_submit_segments.Proto_ct_pdu_encode_submit_segments
Global ct_pdu_encode_submit_segments_ex.Proto_ct_pdu_encode_submit_segments_ex
Global ct_pdu_decode.Proto_ct_pdu_decode

; <summary>
; CtJoinPath
; </summary>
; <param name="dir">string</param>
; <param name="file">string</param>
; <returns>Returns string.</returns>
Procedure.s CtJoinPath(dir.s, file.s)
  If dir = ""
    ProcedureReturn file
  EndIf
  
  If Right(dir, 1) <> "\" And Right(dir, 1) <> "/"
    dir + "\"
  EndIf

  ProcedureReturn dir + file
EndProcedure

; <summary>
; CtBindExport
; </summary>
; <param name="name">string</param>
; <returns>Returns integer.</returns>
Procedure.i CtBindExport(name.s)
  Protected *fn = GetProcAddress(CtLib, name)

  If *fn = 0
    CtLoadError = "GetProcAddress('" + name + "') failed in " + CtLoadedPath +
                  " (GetLastError=" + Str(GetLastError()) + "). " +
                  "Need a C ABI build (ct_* exports), not an old DLL."
    ProcedureReturn 0
  EndIf

  ProcedureReturn *fn
EndProcedure

; <summary>
; CtInit
; </summary>
; <param name="dllPath">string</param>
; <returns>Returns integer.</returns>
Procedure.i CtInit(dllPath.s = "comettextel.dll")
  Protected i.i
  Protected n.i
  Protected Dim cand.s(3)
  Protected *fn

  If CtLib
    ProcedureReturn 1
  EndIf

  CtLoadError = ""
  CtLoadedPath = ""

  If FindString(dllPath, "\") Or FindString(dllPath, "/") Or (Len(dllPath) >= 2 And Mid(dllPath, 2, 1) = ":")
    cand(0) = dllPath
    n = 1
  Else
    cand(0) = CtJoinPath(GetPathPart(ProgramFilename()), dllPath)
    cand(1) = CtJoinPath(GetCurrentDirectory(), dllPath)
    cand(2) = dllPath
    n = 3
  EndIf

  For i = 0 To n - 1
    If cand(i) = ""
      Continue
    EndIf

    If i < n - 1 And FileSize(cand(i)) < 0
      Continue
    EndIf

    CtLib = LoadLibraryW(cand(i))

    If CtLib
      CtLoadedPath = cand(i)
      Break
    EndIf
  Next

  If CtLib = 0
    CtLoadError = "LoadLibraryW failed for '" + dllPath + "' (GetLastError=" + Str(GetLastError()) + ")"
    ProcedureReturn 0
  EndIf

  *fn = CtBindExport("ct_status_string")

  If *fn = 0
    FreeLibrary(CtLib) : CtLib = 0
    
    ProcedureReturn 0
  EndIf

  ct_status_string = *fn

  *fn = CtBindExport("ct_modem_create")
  If *fn = 0 : FreeLibrary(CtLib) : CtLib = 0 : ProcedureReturn 0 : EndIf
  ct_modem_create = *fn

  *fn = CtBindExport("ct_modem_destroy")
  If *fn = 0 : FreeLibrary(CtLib) : CtLib = 0 : ProcedureReturn 0 : EndIf
  ct_modem_destroy = *fn

  *fn = CtBindExport("ct_modem_open")
  If *fn = 0 : FreeLibrary(CtLib) : CtLib = 0 : ProcedureReturn 0 : EndIf
  ct_modem_open = *fn

  *fn = CtBindExport("ct_modem_send")
  If *fn = 0 : FreeLibrary(CtLib) : CtLib = 0 : ProcedureReturn 0 : EndIf
  ct_modem_send = *fn

  *fn = CtBindExport("ct_modem_send_ex")
  If *fn = 0 : FreeLibrary(CtLib) : CtLib = 0 : ProcedureReturn 0 : EndIf
  ct_modem_send_ex = *fn

  *fn = CtBindExport("ct_modem_list")
  If *fn = 0 : FreeLibrary(CtLib) : CtLib = 0 : ProcedureReturn 0 : EndIf
  ct_modem_list = *fn

  *fn = CtBindExport("ct_modem_delete")
  If *fn = 0 : FreeLibrary(CtLib) : CtLib = 0 : ProcedureReturn 0 : EndIf
  ct_modem_delete = *fn

  *fn = CtBindExport("ct_pdu_encode_submit")

  If *fn = 0
    FreeLibrary(CtLib) : CtLib = 0
    
    ProcedureReturn 0
  EndIf

  ct_pdu_encode_submit = *fn

  *fn = CtBindExport("ct_pdu_encode_submit_ex")
  If *fn = 0 : FreeLibrary(CtLib) : CtLib = 0 : ProcedureReturn 0 : EndIf
  ct_pdu_encode_submit_ex = *fn

  *fn = CtBindExport("ct_pdu_encode_submit_segments")

  If *fn = 0
    FreeLibrary(CtLib) : CtLib = 0
    
    ProcedureReturn 0
  EndIf

  ct_pdu_encode_submit_segments = *fn

  *fn = CtBindExport("ct_pdu_encode_submit_segments_ex")
  If *fn = 0 : FreeLibrary(CtLib) : CtLib = 0 : ProcedureReturn 0 : EndIf
  ct_pdu_encode_submit_segments_ex = *fn

  *fn = CtBindExport("ct_pdu_decode")

  If *fn = 0
    FreeLibrary(CtLib) : CtLib = 0
    
    ProcedureReturn 0
  EndIf
  
  ct_pdu_decode = *fn

  ProcedureReturn 1
EndProcedure

; <summary>
; CtShutdown
; </summary>
; <returns>Returns void.</returns>
Procedure CtShutdown()
  If CtLib
    FreeLibrary(CtLib)
    CtLib = 0
  EndIf

  ct_status_string = 0
  ct_modem_create = 0
  ct_modem_destroy = 0
  ct_modem_open = 0
  ct_modem_send = 0
  ct_modem_send_ex = 0
  ct_modem_list = 0
  ct_modem_delete = 0
  ct_pdu_encode_submit = 0
  ct_pdu_encode_submit_ex = 0
  ct_pdu_encode_submit_segments = 0
  ct_pdu_encode_submit_segments_ex = 0
  ct_pdu_decode = 0
EndProcedure

; <summary>
; CtStatusString
; </summary>
; <param name="status">integer</param>
; <returns>Returns string.</returns>
Procedure.s CtStatusString(status.l)
  Protected *msg
  
  If CtLib = 0
    ProcedureReturn "status " + Str(status)
  EndIf
  
  *msg = ct_status_string(status)
  
  If *msg = 0
    ProcedureReturn "status " + Str(status)
  EndIf
  
  ProcedureReturn PeekS(*msg, -1, #PB_UTF8)
EndProcedure

; <summary>
; CtUtf8Z
; </summary>
; <param name="bytes">pointer</param>
; <param name="maxBytes">long</param>
; <returns>Returns string.</returns>
Procedure.s CtUtf8Z(*bytes, maxBytes.l)
  Protected i.l

  If *bytes = 0 Or maxBytes <= 0
    ProcedureReturn ""
  EndIf

  For i = 0 To maxBytes - 1
    If PeekA(*bytes + i) = 0
      ProcedureReturn PeekS(*bytes, i, #PB_UTF8)
    EndIf
  Next

  ProcedureReturn PeekS(*bytes, maxBytes, #PB_UTF8)
EndProcedure

; <summary>
; CtPeekAsciiZ
; </summary>
; <param name="*bytes">pointer</param>
; <param name="maxBytes">integer</param>
; <returns>Returns string.</returns>
Procedure.s CtPeekAsciiZ(*bytes, maxBytes.i)
  Protected i.i

  If *bytes = 0 Or maxBytes <= 0
    ProcedureReturn ""
  EndIf

  For i = 0 To maxBytes - 1
    If PeekA(*bytes + i) = 0
      ProcedureReturn PeekS(*bytes, i, #PB_Ascii)
    EndIf
  Next

  ProcedureReturn PeekS(*bytes, maxBytes, #PB_Ascii)
EndProcedure

; <summary>
; CtUtf8Dup - Owned UTF-8 copy — do not use UTF8() across multiple arguments (C backend may reuse a temp buffer).
; </summary>
; <param name="text">string</param>
; <returns>Returns integer.</returns>
Procedure.i CtUtf8Dup(text.s)
  Protected n.i = StringByteLength(text, #PB_UTF8) + 1
  Protected *buf = AllocateMemory(n)

  If *buf = 0
    ProcedureReturn 0
  EndIf

  PokeS(*buf, text, -1, #PB_UTF8)
  ProcedureReturn *buf
EndProcedure

; <summary>
; CtMessagePeer
; </summary>
; <param name="*msg">struct</param>
; <returns>Returns string.</returns>
Procedure.s CtMessagePeer(*msg.CtMessage)
  ProcedureReturn CtUtf8Z(@*msg\peer_address[0], 32)
EndProcedure

; <summary>
; CtMessageUserData
; </summary>
; <param name="*msg">struct</param>
; <returns>Returns string.</returns>
Procedure.s CtMessageUserData(*msg.CtMessage)
  ProcedureReturn CtUtf8Z(@*msg\user_data[0], 512)
EndProcedure

; <summary>
; CtEncodeSubmit - Returns PDU hex. Status written to *status when non-zero.
; </summary>
; <param name="destination">string</param>
; <param name="text">string</param>
; <param name="serviceCenter">string</param>
; <param name="dcs">long</param>
; <param name="*status">pointer</param>
; <returns>Returns string.</returns>
Procedure.s CtEncodeSubmit(destination.s, text.s, serviceCenter.s, dcs.l, *status.Long, relativeValidityPeriod.l = -1, requestStatusReport.l = 0)
  Protected *hex
  Protected *smsc
  Protected *dest
  Protected *body
  Protected result.l
  Protected hex.s

  If CtLib = 0
    If *status : *status\l = #CT_ERR_NOT_OPEN : EndIf
    ProcedureReturn ""
  EndIf

  *hex = AllocateMemory(1024)
  *smsc = CtUtf8Dup(serviceCenter)
  *dest = CtUtf8Dup(destination)
  *body = CtUtf8Dup(text)

  If *hex = 0 Or *smsc = 0 Or *dest = 0 Or *body = 0
    If *hex : FreeMemory(*hex) : EndIf
    If *smsc : FreeMemory(*smsc) : EndIf
    If *dest : FreeMemory(*dest) : EndIf
    If *body : FreeMemory(*body) : EndIf
    If *status : *status\l = #CT_ERR_UNKNOWN : EndIf
    
    ProcedureReturn ""
  EndIf

  FillMemory(*hex, 1024)
  result = ct_pdu_encode_submit_ex(*smsc, *dest, *body, dcs, relativeValidityPeriod, requestStatusReport, *hex, 1024)

  If result = #CT_OK
    hex = CtPeekAsciiZ(*hex, 1024)
  EndIf

  FreeMemory(*hex)
  FreeMemory(*smsc)
  FreeMemory(*dest)
  FreeMemory(*body)

  If *status
    *status\l = result
  EndIf

  ProcedureReturn hex
EndProcedure

; <summary>
; CtEncodeSubmitSegments - Returns newline-separated PDU hex. *outCount receives segment count.
; </summary>
; <param name="destination">string</param>
; <param name="text">string</param>
; <param name="serviceCenter">string</param>
; <param name="dcs">long</param>
; <param name="*status">pointer</param>
; <param name="*outCount">pointer</param>
; <returns>Returns string.</returns>
Procedure.s CtEncodeSubmitSegments(destination.s, text.s, serviceCenter.s, dcs.l, *status.Long, *outCount.Long, relativeValidityPeriod.l = -1, requestStatusReport.l = 0)
  Protected cap.i = 131072
  Protected *hex
  Protected *smsc
  Protected *dest
  Protected *body
  Protected count.l
  Protected result.l
  Protected hex.s

  If CtLib = 0
    If *status : *status\l = #CT_ERR_NOT_OPEN : EndIf
    ProcedureReturn ""
  EndIf

  *hex = AllocateMemory(cap)
  *smsc = CtUtf8Dup(serviceCenter)
  *dest = CtUtf8Dup(destination)
  *body = CtUtf8Dup(text)
  count = 0

  If *hex = 0 Or *smsc = 0 Or *dest = 0 Or *body = 0
    If *hex : FreeMemory(*hex) : EndIf
    If *smsc : FreeMemory(*smsc) : EndIf
    If *dest : FreeMemory(*dest) : EndIf
    If *body : FreeMemory(*body) : EndIf
    If *status : *status\l = #CT_ERR_UNKNOWN : EndIf
    
    ProcedureReturn ""
  EndIf

  FillMemory(*hex, cap)
  result = ct_pdu_encode_submit_segments_ex(*smsc, *dest, *body, dcs, relativeValidityPeriod, requestStatusReport, *hex, cap, @count)

  If *outCount
    *outCount\l = count
  EndIf

  If result = #CT_OK
    hex = CtPeekAsciiZ(*hex, cap)
  EndIf

  FreeMemory(*hex)
  FreeMemory(*smsc)
  FreeMemory(*dest)
  FreeMemory(*body)

  If *status
    *status\l = result
  EndIf

  ProcedureReturn hex
EndProcedure

; <summary>
; CtDecode
; </summary>
; <param name="pduHex">string</param>
; <param name="*out">struct</param>
; <returns>Returns long.</returns>
Procedure.l CtDecode(pduHex.s, *out.CtMessage)
  Protected *hex
  Protected result.l

  If CtLib = 0
    ProcedureReturn #CT_ERR_NOT_OPEN
  EndIf

  If *out = 0
    ProcedureReturn #CT_ERR_INVALID_ARGUMENT
  EndIf

  *hex = CtUtf8Dup(pduHex)

  If *hex = 0
    ProcedureReturn #CT_ERR_UNKNOWN
  EndIf

  FillMemory(*out, SizeOf(CtMessage))
  result = ct_pdu_decode(*hex, *out)
  FreeMemory(*hex)
  
  ProcedureReturn result
EndProcedure

; <summary>
; CtModemCreate - Opaque ct_modem* handle (0 on failure).
; </summary>
; <returns>Returns integer.</returns>
Procedure.i CtModemCreate()
  If CtLib = 0
    ProcedureReturn 0
  EndIf

  ProcedureReturn ct_modem_create()
EndProcedure

; <summary>
; CtModemDestroy
; </summary>
; <param name="*modem">Pointer</param>
; <returns>Returns void.</returns>
Procedure CtModemDestroy(*modem)
  If CtLib = 0 Or *modem = 0
    ProcedureReturn
  EndIf

  ct_modem_destroy(*modem)
EndProcedure

; <summary>
; CtModemOpen
; </summary>
; <param name="*modem">Pointer</param>
; <param name="baudRate">long</param>
; <returns>Returns long.</returns>
Procedure.l CtModemOpen(*modem, port.s, baudRate.l = 115200)
  Protected *port
  Protected result.l

  If CtLib = 0 Or *modem = 0
    ProcedureReturn #CT_ERR_NOT_OPEN
  EndIf

  *port = CtUtf8Dup(port)

  If *port = 0
    ProcedureReturn #CT_ERR_UNKNOWN
  EndIf

  result = ct_modem_open(*modem, *port, baudRate)

  FreeMemory(*port)
  ProcedureReturn result
EndProcedure

; <summary>
; CtModemSend - Long text auto-splits with concat UDH.
; </summary>
; <param name="*modem">Pointer</param>
; <param name="destination">string</param>
; <param name="text">string</param>
; <param name="serviceCenter">string</param>
; <param name="dcs">long</param>
; <param name="timeoutMs">long</param>
; <returns>Returns long.</returns>
Procedure.l CtModemSend(*modem, destination.s, text.s, serviceCenter.s = "", dcs.l = #CT_DCS_UCS2, timeoutMs.l = 10000, relativeValidityPeriod.l = -1, requestStatusReport.l = 0)
  Protected *smsc
  Protected *dest
  Protected *body
  Protected result.l

  If CtLib = 0 Or *modem = 0
    ProcedureReturn #CT_ERR_NOT_OPEN
  EndIf

  *smsc = CtUtf8Dup(serviceCenter)
  *dest = CtUtf8Dup(destination)
  *body = CtUtf8Dup(text)

  If *smsc = 0 Or *dest = 0 Or *body = 0
    If *smsc : FreeMemory(*smsc) : EndIf
    If *dest : FreeMemory(*dest) : EndIf
    If *body : FreeMemory(*body) : EndIf
    ProcedureReturn #CT_ERR_UNKNOWN
  EndIf

  result = ct_modem_send_ex(*modem, *smsc, *dest, *body, dcs, relativeValidityPeriod, requestStatusReport, timeoutMs)

  FreeMemory(*smsc)
  FreeMemory(*dest)
  FreeMemory(*body)
  ProcedureReturn result
EndProcedure

; <summary>
; CtModemList - Fills *outMessages (array of CtMessage). Returns status; *outCount receives count.
; Complete concat sets are rejoined (concat_seq = 0).
; </summary>
; <param name="*modem">Pointer</param>
; <param name="*outMessages">struct</param>
; <param name="*outCount">long</param>
; <param name="timeoutMs">long</param>
; <returns>Returns long.</returns>
Procedure.l CtModemList(*modem, *outMessages.CtMessage, maxCount.l, *outCount.Long, timeoutMs.l = 8000)
  Protected count.l
  Protected result.l

  If CtLib = 0 Or *modem = 0
    ProcedureReturn #CT_ERR_NOT_OPEN
  EndIf

  If *outMessages = 0 Or maxCount <= 0
    ProcedureReturn #CT_ERR_INVALID_ARGUMENT
  EndIf

  count = 0
  result = ct_modem_list(*modem, *outMessages, maxCount, @count, timeoutMs)

  If *outCount
    *outCount\l = count
  EndIf

  ProcedureReturn result
EndProcedure

; <summary>
; CtModemDelete
; </summary>
; <param name="*modem">Pointer</param>
; <param name="index">long</param>
; <param name="timeoutMs">long</param>
; <returns>Returns long.</returns>
Procedure.l CtModemDelete(*modem, index.l, timeoutMs.l = 5000)
  If CtLib = 0 Or *modem = 0
    ProcedureReturn #CT_ERR_NOT_OPEN
  EndIf

  ProcedureReturn ct_modem_delete(*modem, index, timeoutMs)
EndProcedure

; IDE Options = PureBasic 6.41 (Windows - x64)
; CursorPosition = 193
; FirstLine = 82
; Folding = ---
; Optimizer
; EnableAsm
; EnableXP
; DPIAware
; DllProtection
; EnableOnError
; DisableDebugger
; CompileSourceDirectory
; Compiler = PureBasic 6.41 - C Backend (Windows - x64)