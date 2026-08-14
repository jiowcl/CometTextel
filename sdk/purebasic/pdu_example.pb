;--------------------------------------------------------------------------------------------
;  CometTextel — PureBasic PDU example (no modem).
;
;  Usage:
;    pdu_example.exe
;    pdu_example.exe <destination> <text> [smsc]
;
;  Place comettextel.dll next to the executable (or the source folder when
;  compiling from the IDE). See README.md.
;
;  Copyright (c) Ji-Feng Tsai. All rights reserved.
;  Code released under the MIT license.
;--------------------------------------------------------------------------------------------

EnableExplicit

IncludeFile "comettextel.pbi"

Procedure PrintUsage()
  PrintN("Usage:")
  PrintN("  pdu_example.exe")
  PrintN("  pdu_example.exe <destination> <text> [smsc]")
EndProcedure

Procedure.i FailStatus(status.l, what.s)
  PrintN(what + " failed (" + Str(status) + "): " + CtStatusString(status))
  
  ProcedureReturn 2
EndProcedure

Procedure.i PrintDecoded(pduHex.s)
  Protected msg.CtMessage
  Protected status.l = CtDecode(pduHex, @msg)
  
  If status <> #CT_OK
    ProcedureReturn FailStatus(status, "ct_pdu_decode")
  EndIf
  
  PrintN("peer=" + CtMessagePeer(@msg) + " text=" + CtMessageUserData(@msg) +
         " dcs=" + Str(msg\dcs) + " has_udh=" + Str(msg\has_udh) +
         " concat=" + Str(msg\is_concatenated) + " ref=" + Str(msg\concat_ref) +
         " total=" + Str(msg\concat_total) + " seq=" + Str(msg\concat_seq))
  ProcedureReturn 0
EndProcedure

Procedure.i RunPdu(destination.s, text.s, smsc.s)
  Protected joined.s
  Protected count.l = 0
  Protected status.l
  Protected i.i
  Protected part.s

  joined = CtEncodeSubmitSegments(destination, text, smsc, #CT_DCS_UCS2, @status, @count)
  
  If status <> #CT_OK
    ProcedureReturn FailStatus(status, "ct_pdu_encode_submit_segments")
  EndIf
  
  If joined = ""
    PrintN("encode returned empty hex (count=" + Str(count) + ")")
    ProcedureReturn 2
  EndIf

  For i = 1 To CountString(joined, #LF$) + 1
    part = StringField(joined, i, #LF$)
    
    If part = ""
      Continue
    EndIf
    
    PrintN(part)
    
    If PrintDecoded(part) <> 0
      ProcedureReturn 2
    EndIf
  Next

  PrintN("segments=" + Str(count))
  ProcedureReturn 0
EndProcedure

Procedure.i RunSelfCheck()
  Protected hex.s
  Protected msg.CtMessage
  Protected status.l
  Protected chinese.s
  ; Do not put CJK in the .pb source: without UTF-8 BOM the C backend
  ; turns those literals into U+FFFD (PDU hex FFFD…). Use code points.
  chinese = Chr($6E2C) + Chr($8A66) + Chr($4E2D) + Chr($6587) + Chr($7C21) + Chr($8A0A)

  PrintN("-- self-check UCS-2 ASCII --")
  
  If RunPdu("886912345678", "Hello from PureBasic", "886932000000") <> 0
    ProcedureReturn 2
  EndIf

  PrintN("-- self-check UCS-2 Chinese --")
  
  If RunPdu("886912345678", chinese, "886932000000") <> 0
    ProcedureReturn 2
  EndIf
  
  PrintN("ok (6 CJK chars, UCS-2)")

  PrintN("-- self-check single-segment EncodeSubmit --")
  hex = CtEncodeSubmit("886912345678", "Hello", "886932000000", #CT_DCS_UCS2, @status)
  
  If status <> #CT_OK
    ProcedureReturn FailStatus(status, "ct_pdu_encode_submit")
  EndIf
  
  If hex = ""
    PrintN("EncodeSubmit returned empty hex")
    ProcedureReturn 2
  EndIf
  
  PrintN(hex)
  
  status = CtDecode(hex, @msg)
  
  If status <> #CT_OK
    ProcedureReturn FailStatus(status, "ct_pdu_decode")
  EndIf
  
  If CtMessageUserData(@msg) <> "Hello" Or CtMessagePeer(@msg) <> "886912345678"
    PrintN("round-trip mismatch")
    ProcedureReturn 2
  EndIf
  
  PrintN("ok")
  
  ProcedureReturn 0
EndProcedure

Define rc.i = 1
Define smsc.s = ""

If OpenConsole() = 0
  End 1
EndIf

If CtInit() = 0
  PrintN(CtLoadError)
  PrintN("Use C SDK bin\comettextel.dll (must export ct_pdu_encode_submit_segments).")
  PrintN("Place it next to the .exe / source folder. dumpbin /exports comettextel.dll")
  End 1
EndIf

Select CountProgramParameters()
  Case 0
    rc = RunSelfCheck()
  Case 1
    PrintUsage()
    rc = 1
  Default
    If CountProgramParameters() >= 3
      smsc = ProgramParameter(2)
    EndIf
    
    rc = RunPdu(ProgramParameter(0), ProgramParameter(1), smsc)
EndSelect

If rc <> 0 And CountProgramParameters() = 1
  PrintUsage()
EndIf

CtShutdown()
End rc

; IDE Options = PureBasic 6.41 (Windows - x64)
; ExecutableFormat = Console
; CursorPosition = 158
; FirstLine = 118
; Folding = -
; Optimizer
; EnableAsm
; EnableThread
; EnableXP
; DPIAware
; DllProtection
; SharedUCRT
; EnableOnError
; Executable = Output\pdu_example.exe
; DisableDebugger
; CompileSourceDirectory
; Compiler = PureBasic 6.41 - C Backend (Windows - x64)