;--------------------------------------------------------------------------------------------
;  CometTextel — PureBasic modem example (list / send / delete).
;
;  Usage:
;    modem_example.exe list <port> [baud]
;    modem_example.exe send <port> <smsc> <destination> <text> [baud]
;    modem_example.exe delete <port> <index> [baud]
;
;  Requires comettextel.dll (C ABI) next to the exe. See README.md.
;
;  Copyright (c) Ji-Feng Tsai. All rights reserved.
;  Code released under the MIT license.
;--------------------------------------------------------------------------------------------

XIncludeFile "comettextel.pbi"

Procedure PrintUsage()
  PrintN("Usage:")
  PrintN("  modem_example.exe list <port> [baud]")
  PrintN("  modem_example.exe send <port> <smsc> <destination> <text> [baud]")
  PrintN("  modem_example.exe delete <port> <index> [baud]")
EndProcedure

Procedure.l ParseBaud(text.s, fallback.l = 115200)
  Protected v.l = Val(text)
  If v <= 0
    ProcedureReturn fallback
  EndIf
  ProcedureReturn v
EndProcedure

Procedure.i RunList(port.s, baud.l)
  Protected *modem
  Protected status.l
  Protected count.l
  Protected i.l
  Protected Dim msgs.CtMessage(63)
  Protected flag.s

  *modem = CtModemCreate()
  If *modem = 0
    PrintN("ct_modem_create failed")
    ProcedureReturn 2
  EndIf

  status = CtModemOpen(*modem, port, baud)
  If status <> #CT_OK
    PrintN("open: " + CtStatusString(status))
    CtModemDestroy(*modem)
    ProcedureReturn 2
  EndIf

  status = CtModemList(*modem, @msgs(0), 64, @count)
  If status <> #CT_OK
    PrintN("list: " + CtStatusString(status))
    CtModemDestroy(*modem)
    ProcedureReturn 2
  EndIf

  For i = 0 To count - 1
    flag = ""
    If msgs(i)\is_concatenated And msgs(i)\concat_seq = 0
      flag = " reassembled"
    EndIf
    PrintN("[" + Str(msgs(i)\index) + "] " + CtMessagePeer(@msgs(i)) + ": " +
           CtMessageUserData(@msgs(i)) +
           " (dcs=" + Str(msgs(i)\dcs) +
           " concat=" + Str(msgs(i)\is_concatenated) +
           " seq=" + Str(msgs(i)\concat_seq) + "/" + Str(msgs(i)\concat_total) + flag + ")")
  Next
  PrintN("count=" + Str(count))

  CtModemDestroy(*modem)
  ProcedureReturn 0
EndProcedure

Procedure.i RunSend(port.s, smsc.s, destination.s, text.s, baud.l)
  Protected *modem
  Protected status.l

  *modem = CtModemCreate()
  If *modem = 0
    PrintN("ct_modem_create failed")
    ProcedureReturn 2
  EndIf

  status = CtModemOpen(*modem, port, baud)
  If status <> #CT_OK
    PrintN("open: " + CtStatusString(status))
    CtModemDestroy(*modem)
    ProcedureReturn 2
  EndIf

  status = CtModemSend(*modem, destination, text, smsc, #CT_DCS_UCS2)
  If status <> #CT_OK
    PrintN("send: " + CtStatusString(status))
    CtModemDestroy(*modem)
    ProcedureReturn 2
  EndIf

  PrintN("Sent.")
  CtModemDestroy(*modem)
  ProcedureReturn 0
EndProcedure

Procedure.i RunDelete(port.s, index.l, baud.l)
  Protected *modem
  Protected status.l

  *modem = CtModemCreate()
  If *modem = 0
    PrintN("ct_modem_create failed")
    ProcedureReturn 2
  EndIf

  status = CtModemOpen(*modem, port, baud)
  If status <> #CT_OK
    PrintN("open: " + CtStatusString(status))
    CtModemDestroy(*modem)
    ProcedureReturn 2
  EndIf

  status = CtModemDelete(*modem, index)
  If status <> #CT_OK
    PrintN("delete: " + CtStatusString(status))
    CtModemDestroy(*modem)
    ProcedureReturn 2
  EndIf

  PrintN("Deleted.")
  CtModemDestroy(*modem)
  ProcedureReturn 0
EndProcedure

OpenConsole()

If CtInit() = 0
  PrintN(CtLoadError)
  End 2
EndIf

Define argc.i = CountProgramParameters()
Define exitCode.i = 1
Define cmd.s
Define baud.l

If argc < 2
  PrintUsage()
Else
  cmd = LCase(ProgramParameter(0))
  If cmd = "list"
    baud = 115200
    If argc >= 3
      baud = ParseBaud(ProgramParameter(2))
    EndIf
    exitCode = RunList(ProgramParameter(1), baud)
  ElseIf cmd = "send"
    If argc < 5
      PrintUsage()
    Else
      baud = 115200
      If argc >= 6
        baud = ParseBaud(ProgramParameter(5))
      EndIf
      exitCode = RunSend(ProgramParameter(1), ProgramParameter(2), ProgramParameter(3), ProgramParameter(4), baud)
    EndIf
  ElseIf cmd = "delete"
    If argc < 3
      PrintUsage()
    Else
      baud = 115200
      If argc >= 4
        baud = ParseBaud(ProgramParameter(3))
      EndIf
      exitCode = RunDelete(ProgramParameter(1), Val(ProgramParameter(2)), baud)
    EndIf
  Else
    PrintUsage()
  EndIf
EndIf

CtShutdown()
End exitCode

; IDE Options = PureBasic 6.41 (Windows - x64)
; FirstLine = 134
; Folding = -
; Optimizer
; EnableAsm
; EnableXP
; DPIAware
; DllProtection
; EnableOnError
; Executable = Output\modem_example.exe
; DisableDebugger
; CompileSourceDirectory
; Compiler = PureBasic 6.41 - C Backend (Windows - x64)