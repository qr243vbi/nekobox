!ifdef SOFTWARE_NAME
    Name "${SOFTWARE_NAME}"
!else
	!define SOFTWARE_NAME Iblis
    Name "Iblis"
!endif

!ifdef OUTFILE
    OutFile "${OUTFILE}.exe"
!else
    OutFile "nekobox_setup.exe"
!endif
InstallDir $WINDIR\qr243vbi\non_exists_directory
RequestExecutionLevel user

!ifdef SOFTWARE_VERSION
!else
!define SOFTWARE_VERSION "1.0.0"
!endif


!include FileFunc.nsh
!include MUI2.nsh
!include LogicLib.nsh
!include x64.nsh
!include WinMessages.nsh ; Required for BM_SETCHECK constant

; NSIS PROCESS INFO LIBRARY - GetProcessInfo.nsh
; Version 1.1 - Mar 28th, 2011
;
; Description:
;   Gets process information.
;
; Usage example:
;   ${GetProcessInfo} 0 $0 $1 $2 $3 $4
;   DetailPrint "pid=$0 parent_pid=$1 priority=$2 process_name=$3 exe=$4"
;
; History:
;   1.1 - 28/03/2011 - Added uninstall function, include guards, file header. Fixed getting full exe path on pre vista systems. (Sergius)
;

!ifndef GETPROCESSINFO_INCLUDED
!define GETPROCESSINFO_INCLUDED

!define PROCESSINFO.TH32CS_SNAPPROCESS      2
!define PROCESSINFO.INVALID_HANDLE_VALUE    -1

!define GetProcessInfo '!insertmacro GetProcessInfo'

;@in pid_in - if 0 - get current process info
;@out pid_out - real process id (may be useful, if pid_in=0)
;@out ppid - parent process id
;@out priority
;@out name - name of process
;@out fullname - fully-qualified path of process
!macro GetProcessInfo pid_in pid_out ppid priority name fullname
    Push ${pid_in}
    !ifdef __UNINSTALL__
        Call un._GetProcessInfo
    !else
        Call _GetProcessInfo
    !endif
    ;name;pri;ppid;fname;pid;
    Pop ${name}
    Pop ${priority}
    Pop ${ppid}
    Pop ${fullname}
    Pop ${pid_out}
!macroend

!macro FUNC_GETPROCESSINFO
    Exch $R3 ;pid
    Push $0
    Push $1
    Push $2
    Push $3
    Push $4
    Push $5
    Push $R0 ;hSnapshot
    Push $R1 ;result
    Push $R9 ;PROCESSENTRY32;MODULEENTRY32 and so on
    Push $R8

    ;zero registers to waste trash, if error occurred
    StrCpy $0 ""
    StrCpy $1 ""
    StrCpy $2 ""
    StrCpy $3 ""
    StrCpy $4 ""
    StrCpy $5 ""

    IntCmp $R3 0 0 skip_pid_detection skip_pid_detection
    System::Call 'kernel32::GetCurrentProcess() i.R0'
    System::Call "Kernel32::GetProcessId(i R0) i.R3"

skip_pid_detection:
    System::Call 'Kernel32::CreateToolhelp32Snapshot(i ${PROCESSINFO.TH32CS_SNAPPROCESS},i R3) i.R0'

    IntCmp $R0 ${PROCESSINFO.INVALID_HANDLE_VALUE} end ;someting wrong

    ;$R9=PROCESSENTRY32
    ;typedef struct tagPROCESSENTRY32 {
    ;  DWORD     dwSize;
    ;  DWORD     cntUsage;
    ;  DWORD     th32ProcessID;
    ;  ULONG_PTR th32DefaultHeapID;
    ;  DWORD     th32ModuleID;
    ;  DWORD     cntThreads;
    ;  DWORD     th32ParentProcessID;
    ;  LONG      pcPriClassBase;
    ;  DWORD     dwFlags;
    ;  TCHAR     szExeFile[MAX_PATH];
    ;}PROCESSENTRY32, *PPROCESSENTRY32;
    ;dwSize=4*9+260

    System::Alloc 1024
    pop $R9
    System::Call "*$R9(i 296)"

    System::Call 'Kernel32::Process32First(i R0, i $R9) i.R1'
    StrCmp $R1 0 end

nnext_iteration:
    System::Call "*$R9(i,i,i.R1)" ;get PID
    IntCmp $R1 $R3 exitloop

    System::Call 'Kernel32::Process32Next(i R0, i $R9) i.R1'
    IntCmp $R1 0 0 nnext_iteration nnext_iteration

exitloop:
    ;$0 - pid
    ;$1 - threads
    ;$2 - ppid
    ;$3 - priority
    ;$4 - process name
    System::Call "*$R9(i,i,i.r0,i,i,i.r1,i.r2,i.r3,i,&t256.r4)" ; Get next module

    ;free:
    System::Free $R9
    System::Call "Kernel32::CloseToolhelp32Snapshot(i R0)"

    ;===============
    ;now get full path and commandline

    System::Call "Kernel32::OpenProcess(i 1040, i 0, i r0)i .R0"

    StrCmp $R0 0 end

    IntOp $R8 0 + 256
    System::Call "psapi::GetModuleFileNameEx(i R0,i 0,t .r5, *i $R8)i .R1"

end:
    Pop $R8
    Pop $R9
    Pop $R1
    Pop $R0
    Exch $5
    Exch 1
    Exch $4
    Exch 2
    Exch $3
    Exch 3
    Exch $2
    Exch 4
    Pop $1
    Exch 4
    Exch $0
    Exch 5
    Pop $R3
!macroend ;FUNC_GETPROCESSINFO

Function _GetProcessInfo
    !insertmacro FUNC_GETPROCESSINFO
FunctionEnd

Function un._GetProcessInfo
    !insertmacro FUNC_GETPROCESSINFO
FunctionEnd

!endif ;GETPROCESSINFO_INCLUDED
;Version for Unicode NSIS
; NSIS PROCESS INFO LIBRARY - GetProcessInfo.nsh
; Version 1.1 - Mar 28th, 2011
;
; Description:
;   Gets process information.
;
; Usage example:
;   ${GetProcessInfo} 0 $0 $1 $2 $3 $4
;   DetailPrint "pid=$0 parent_pid=$1 priority=$2 process_name=$3 exe=$4"
;
; History:
;   1.1 - 28/03/2011 - Added uninstall function, include guards, file header. Fixed getting full exe path on pre vista systems. (Sergius)
;

!ifndef GETPROCESSINFO_INCLUDED
!define GETPROCESSINFO_INCLUDED

!define PROCESSINFO.TH32CS_SNAPPROCESS      2
!define PROCESSINFO.INVALID_HANDLE_VALUE    -1

!define GetProcessInfo '!insertmacro GetProcessInfo'

;@in pid_in - if 0 - get current process info
;@out pid_out - real process id (may be useful, if pid_in=0)
;@out ppid - parent process id
;@out priority
;@out name - name of process
;@out fullname - fully-qualified path of process
!macro GetProcessInfo pid_in pid_out ppid priority name fullname
    Push ${pid_in}
    !ifdef __UNINSTALL__
        Call un._GetProcessInfo
    !else
        Call _GetProcessInfo
    !endif
    ;name;pri;ppid;fname;pid;
    Pop ${name}
    Pop ${priority}
    Pop ${ppid}
    Pop ${fullname}
    Pop ${pid_out}
!macroend

!macro FUNC_GETPROCESSINFO
    Exch $R3 ;pid
    Push $0
    Push $1
    Push $2
    Push $3
    Push $4
    Push $5
    Push $R0 ;hSnapshot
    Push $R1 ;result
    Push $R9 ;PROCESSENTRY32;MODULEENTRY32 and so on
    Push $R8

    ;zero registers to waste trash, if error occurred
    StrCpy $0 ""
    StrCpy $1 ""
    StrCpy $2 ""
    StrCpy $3 ""
    StrCpy $4 ""
    StrCpy $5 ""

    IntCmp $R3 0 0 skip_pid_detection skip_pid_detection
    System::Call 'kernel32::GetCurrentProcess() i.R0'
    System::Call "Kernel32::GetProcessId(i R0) i.R3"

skip_pid_detection:
    System::Call 'Kernel32::CreateToolhelp32Snapshot(i ${PROCESSINFO.TH32CS_SNAPPROCESS},i R3) i.R0'

    IntCmp $R0 ${PROCESSINFO.INVALID_HANDLE_VALUE} end ;someting wrong

    ;$R9=PROCESSENTRY32
    ;typedef struct tagPROCESSENTRY32 {
    ;  DWORD     dwSize;
    ;  DWORD     cntUsage;
    ;  DWORD     th32ProcessID;
    ;  ULONG_PTR th32DefaultHeapID;
    ;  DWORD     th32ModuleID;
    ;  DWORD     cntThreads;
    ;  DWORD     th32ParentProcessID;
    ;  LONG      pcPriClassBase;
    ;  DWORD     dwFlags;
    ;  TCHAR     szExeFile[MAX_PATH];
    ;}PROCESSENTRY32, *PPROCESSENTRY32;
    ;dwSize=4*9+2*260

    System::Alloc 1024
    pop $R9
    System::Call "*$R9(i 556)"

    System::Call 'Kernel32::Process32FirstW(i R0, i $R9) i.R1'
    StrCmp $R1 0 end

nnext_iteration:
    System::Call "*$R9(i,i,i.R1)" ;get PID
    IntCmp $R1 $R3 exitloop

    System::Call 'Kernel32::Process32NextW(i R0, i $R9) i.R1'
    IntCmp $R1 0 0 nnext_iteration nnext_iteration

exitloop:
    ;$0 - pid
    ;$1 - threads
    ;$2 - ppid
    ;$3 - priority
    ;$4 - process name
    System::Call "*$R9(i,i,i.r0,i,i,i.r1,i.r2,i.r3,i,&w256.r4)" ; Get next module

    ;free:
    System::Free $R9
    System::Call "Kernel32::CloseToolhelp32Snapshot(i R0)"

    ;===============
    ;now get full path and commandline

    System::Call "Kernel32::OpenProcess(i 1040, i 0, i r0)i .R0"

    StrCmp $R0 0 end

    IntOp $R8 0 + 256
    System::Call "psapi::GetModuleFileNameExW(i R0,i 0,t .r5, *i $R8)i .R1"

end:
    Pop $R8
    Pop $R9
    Pop $R1
    Pop $R0
    Exch $5
    Exch 1
    Exch $4
    Exch 2
    Exch $3
    Exch 3
    Exch $2
    Exch 4
    Pop $1
    Exch 4
    Exch $0
    Exch 5
    Pop $R3
!macroend ;FUNC_GETPROCESSINFO

Function _GetProcessInfo
    !insertmacro FUNC_GETPROCESSINFO
FunctionEnd

Function un._GetProcessInfo
    !insertmacro FUNC_GETPROCESSINFO
FunctionEnd

!endif ;GETPROCESSINFO_INCLUDED

;%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
; MoveFile and MoveFolder macros
;
; Author:  theblazingangel@aol.com (for the AutoPatcher project - www.autopatcher.com)
; Created: June 2007  
;%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
 
;==================
; MoveFile macro
;==================
 
    !macro MoveFile sourceFile destinationFile
 
	!define MOVEFILE_JUMP ${__LINE__}
 
	; Check source actually exists
 
	    IfFileExists "${sourceFile}" +3 0
	    SetErrors
	    goto done_${MOVEFILE_JUMP}
 
	; Add message to details-view/install-log
 
	    DetailPrint "Moving/renaming file: ${sourceFile} to ${destinationFile}"
 
	; If destination does not already exists simply move file
 
	    IfFileExists "${destinationFile}" +3 0
	    rename "${sourceFile}" "${destinationFile}"
	    goto done_${MOVEFILE_JUMP}
 
	; If overwriting without 'ifnewer' check
 
	    ${If} $switch_overwrite == 1
		delete "${destinationFile}"
		rename "${sourceFile}" "${destinationFile}"
		delete "${sourceFile}"
		goto done_${MOVEFILE_JUMP}
	    ${EndIf}
 
	; If destination already exists
 
	    Push $R0
	    Push $R1
	    Push $R2
	    push $R3
 
	    GetFileTime "${sourceFile}" $R0 $R1
	    GetFileTime "${destinationFile}" $R2 $R3
 
	    IntCmp $R0 $R2 0 older_${MOVEFILE_JUMP} newer_${MOVEFILE_JUMP}
	    IntCmp $R1 $R3 older_${MOVEFILE_JUMP} older_${MOVEFILE_JUMP} newer_${MOVEFILE_JUMP}
 
	    older_${MOVEFILE_JUMP}:
	    delete "${sourceFile}"
	    goto time_check_done_${MOVEFILE_JUMP}
 
	    newer_${MOVEFILE_JUMP}:
	    delete "${destinationFile}"
	    rename "${sourceFile}" "${destinationFile}"
	    delete "${sourceFile}" ;incase above failed!
 
	    time_check_done_${MOVEFILE_JUMP}:
 
	    Pop $R3
	    Pop $R2
	    Pop $R1
	    Pop $R0
 
	done_${MOVEFILE_JUMP}:
 
	!undef MOVEFILE_JUMP
 
    !macroend
 
;==================
; MoveFolder macro
;==================
 
    !macro MoveFolder source destination mask
 
	!define MOVEFOLDER_JUMP ${__LINE__}
 
	Push $R0
	Push $R1
 
	; Move path parameters into registers so they can be altered if necessary
 
	    StrCpy $R0 "${source}"
	    StrCpy $R1 "${destination}"
 
	; Sort out paths - remove final backslash if supplied
 
	    Push $0
 
	    ; Source
	    StrCpy $0 "$R0" 1 -1
	    StrCmp $0 '\' 0 +2
	    StrCpy $R0 "$R0" -1
 
	    ; Destination
	    StrCpy $0 "$R1" 1 -1
	    StrCmp $0 '\' 0 +2
	    StrCpy $R1 "$R1" -1
 
	    Pop $0
 
	; Create destination dir
 
	    CreateDirectory "$R1\"
 
	; Add message to details-view/install-log
 
	    DetailPrint "Moving files: $R0\${mask} to $R1\"
 
	; Push registers used by ${Locate} onto stack
 
	    Push $R6
	    Push $R7
	    Push $R8
	    Push $R9
 
	; Duplicate dir structure (to preserve empty folders and such)
 
	    ${Locate} "$R0" "/L=D" ".MoveFolder_Locate_createDir"
 
	; Locate files and move (via callback function)
 
	    ${Locate} "$R0" "/L=F /M=${mask} /S= /G=1" ".MoveFolder_Locate_moveFile"
 
	; Delete subfolders left over after move
 
	    Push $R2
	    deldir_loop_${MOVEFOLDER_JUMP}:
	    StrCpy $R2 0
	    ${Locate} "$R0" "/L=DE" ".MoveFolder_Locate_deleteDir"
	    StrCmp $R2 0 0 deldir_loop_${MOVEFOLDER_JUMP}
	    Pop $R2
 
	; Delete empty subfolders moved - say the user just wanted to move *.apm files, they now also have a load of empty dir's from dir structure duplication!
 
	    Push $R2
	    delnewdir_loop_${MOVEFOLDER_JUMP}:
	    StrCpy $R2 0
	    ${Locate} "$R1" "/L=DE" ".MoveFolder_Locate_deleteDir"
	    StrCmp $R2 0 0 delnewdir_loop_${MOVEFOLDER_JUMP}
	    Pop $R2
 
	; Pop registers used by ${Locate} off the stack again
 
	    Pop $R9
	    Pop $R8
	    Pop $R7
	    Pop $R6
 
	; Delete source folder if empty
 
	    rmdir "$R0"
 
	Pop $R1
	Pop $R0
 
	!undef MOVEFOLDER_JUMP
 
    !macroend
 
;==================
; MoveFolder macro's ${Locate} callback functions
;==================
 
	Function .MoveFolder_Locate_createDir
 
	    ${If} $R6 == ""
		Push $R2
		StrLen $R2 "$R0"
		StrCpy $R2 $R9 '' $R2
		CreateDirectory "$R1$R2"
		Pop $R2
	    ${EndIf}
 
	    Push $R1
 
	FunctionEnd
 
	Function .MoveFolder_Locate_moveFile
 
	    Push $R2
 
	    ; Get path to file
 
		StrLen $R2 "$R0"
		StrCpy $R2 $R9 '' $R2
		StrCpy $R2 "$R1$R2"
 
	    ; If destination does not already exists simply move file
 
		IfFileExists "$R2" +3 0
		rename "$R9" "$R2"
		goto done
 
	    ; If overwriting without 'ifnewer' check
 
		${If} $switch_overwrite == 1
		    delete "$R2"
		    rename "$R9" "$R2"
		    delete "$R9"
		    goto done
		${EndIf}
 
	    ; If destination already exists
 
		Push $0
		Push $1
		Push $2
		push $3
 
		GetFileTime "$R9" $0 $1
		GetFileTime "$R2" $2 $3
 
		IntCmp $0 $2 0 older newer
		IntCmp $1 $3 older older newer
 
		older:
		delete "$R9"
		goto time_check_done
 
		newer:
		delete "$R2"
		rename "$R9" "$R2"
		delete "$R9" ;incase above failed!
 
		time_check_done:
 
		Pop $3
		Pop $2
		Pop $1
		Pop $0
 
	    done:
 
	    Pop $R2
 
	    Push $R1
 
	FunctionEnd
 
	Function .MoveFolder_Locate_deleteDir
 
	    ${If} $R6 == ""
		RMDir $R9
		IntOp $R2 $R2 + 1
	    ${EndIf}
 
	    Push $R1
 
	FunctionEnd

; Optional: version info for Windows file properties 
VIProductVersion "${SOFTWARE_VERSION}.0" 
VIAddVersionKey "ProductName" "${SOFTWARE_NAME}" 
VIAddVersionKey "ProductVersion" "${SOFTWARE_VERSION}" 
VIAddVersionKey "CompanyName" "qr243vbi" 
VIAddVersionKey "FileDescription" "Installer for ${SOFTWARE_NAME}" 
VIAddVersionKey "LegalCopyright" "© 2026 qr243vbi"


; --- Global Variables declared at the top level ---
Var VCRedistNeeded
;Var VCRedistDownload
;Var VCRedistFile
Var isInstalled
Var isAdmin
Var Winget
Var UnpackOnly
Var NoScript

Var RandomGUID
Var PID
Var PPID
Var PPPID

!ifndef PSEXEC_INCLUDED
!define PSEXEC_INCLUDED
 
!macro PowerShellExecMacro PSCommand
  InitPluginsDir
  ;Save command in a temp file
  Push $R1
  FileOpen $R1 $PLUGINSDIR\tempfile.ps1 w
  FileWrite $R1 "${PSCommand}"
  FileClose $R1
  Pop $R1
 
  !insertmacro PowerShellExecFileMacro "$PLUGINSDIR\tempfile.ps1"
!macroend
 
!macro PowerShellExecFileMacro PSFile
  !define PSExecID ${__LINE__}
  Push $R0
 
  nsExec::ExecToLog 'powershell -NoLogo -InputFormat none -NoProfile -ExecutionPolicy Bypass -File "${PSFile}"  '
  Pop $R0 ;return value is on stack
  IntCmp $R0 0 finish_${PSExecID}
  SetErrorLevel 2
 
finish_${PSExecID}:
  Pop $R0
  !undef PSExecID
!macroend
 
!define PowerShellExec `!insertmacro PowerShellExecMacro`
!define PowerShellExecFile `!insertmacro PowerShellExecFileMacro`
 
!endif

Function GenerateGUID
    System::Call 'ole32::CoCreateGuid(g .r0)'
    System::Call 'ole32::StringFromGUID2(g r0, w .r1, i 39)'
    System::Call 'ole32::CoTaskMemFree(p r0)'
    Push $1
FunctionEnd

!define CreateGUID `!insertmacro _CreateGUID`
!macro _CreateGUID _RetVar
    Call GenerateGUID
    !if ${_RetVar} != s
        Pop ${_RetVar}
    !endif
!macroend

Function openLinkNewWindow
  ; (Function body remains the same, it works fine)
  Push $3
  Exch
  Push $2
  Exch
  Push $1
  Exch
  Push $0
  Exch

  ReadRegStr $0 HKCR "http\shell\open\command" ""
# Get browser path
    DetailPrint $0
  StrCpy $2 '"'
  StrCpy $1 $0 1
  StrCmp $1 $2 +2 # if path is not enclosed in " look for space as final char
    StrCpy $2 ' '
  StrCpy $3 1
  loop:
    StrCpy $1 $0 1 $3
    DetailPrint $1
    StrCmp $1 $2 found
    StrCmp $1 "" found
    IntOp $3 $3 + 1
    Goto loop

  found:
    StrCpy $1 $0 $3
    StrCmp $2 " " +2
      StrCpy $1 '$1"'

  Pop $0
  Exec '$1 $0'
  Pop $0
  Pop $1
  Pop $2
  Pop $3
FunctionEnd

!macro _OpenURL URL
Push "${URL}"
Call openLinkNewWindow
!macroend

!define OpenURL '!insertmacro "_OpenURL"'


!macro checkVcRedist
  ; Variables VCRedistDownload and isInstalled are now declared globally at the top
${IfNot} ${IsNativeARM64}
  ${If} ${RunningX64}
    ;check H-KEY registry
    ReadRegDWORD $isInstalled HKLM "SOFTWARE\Wow6432Node\Microsoft\VisualStudio\14.0\VC\Runtimes\x64" "Installed"
;    StrCpy $VCRedistDownload "https://aka.ms/vc14/vc_redist.x64.exe"
;	StrCpy $VCRedistFile "vc14_redist.x64.exe"
  ${Else}
    ReadRegDWORD $isInstalled HKLM "SOFTWARE\Microsoft\VisualStudio\14.0\VC\Runtimes\x86" "Installed"
;    StrCpy $VCRedistDownload "https://aka.ms/vc14/vc_redist.x86.exe"
;	StrCpy $VCRedistFile "vc14_redist.x86.exe"
  ${EndIf}
${Else}
    ReadRegDWORD $isInstalled HKLM "SOFTWARE\Microsoft\VisualStudio\14.0\VC\Runtimes\ARM64" "Installed"
;    StrCpy $VCRedistDownload "https://aka.ms/vc14/vc_redist.arm64.exe"
;	StrCpy $VCRedistFile "vc14_redist.arm64.exe"
${EndIf}


  ${If} $isInstalled != "1"
    MessageBox MB_YESNO "NOTE: This application requires$\r$\n\
      'Microsoft Visual C++ Redistributable'$\r$\n\
      to function properly.$\r$\n$\r$\n\
      Download and install now?" /SD IDYES IDNO VSRedistInstalled

   ; ${OpenURL} "$VCRedistDownload"
    StrCpy $VCRedistNeeded "1" ; Mark that it was needed
  ${Else}
    StrCpy $VCRedistNeeded "0" ; Mark that it was not needed
  ${EndIf}


  VSRedistInstalled:
!macroend


!macro HasFlag FLAG OUTVAR
    ${GetParameters} $R9
    ${GetOptions} $R9 "${FLAG}" $R8
    StrCpy ${OUTVAR} $R8
!macroend


Function .onInit
	;get own process info
	${GetProcessInfo} 0 $0 $1 $2 $3 $4
	StrCpy $PID "$0"
	StrCpy $PPID "$1"

	;and now get parent process info
	${GetProcessInfo} $1 $0 $1 $2 $3 $5
	StrCpy $PPPID "$1"

    !insertmacro HasFlag "/WINGET=" $Winget
	!insertmacro HasFlag "/UNPACK=" $UnpackOnly
	!insertmacro HasFlag "/NOSCRIPT=" $NoScript
	UserInfo::GetAccountType 
	Pop $0 
	${If} $0 != "Admin" 
		StrCpy $isAdmin "0"
	${Else}
		StrCpy $isAdmin "1"
	${EndIf}
	${If} "$INSTDIR" != "$WINDIR\qr243vbi\non_exists_directory"
		
	${Else}
		${If} "$isAdmin" == "1"
			${If} ${RunningX64}
				StrCpy $INSTDIR "$PROGRAMFILES64\nekobox"
			${Else}
				StrCpy $INSTDIR "$PROGRAMFILES\nekobox"
			${EndIf}
		${Else}
			StrCpy $INSTDIR "$APPDATA\NekoBox"
		${EndIf}
	${EndIf}

	${CreateGUID} $RandomGUID
FunctionEnd

!define MUI_ICON "res\nekobox.ico"
!define MUI_ABORTWARNING
!define MUI_WELCOMEPAGE_TITLE "Welcome to ${SOFTWARE_NAME} Installer"
!define MUI_WELCOMEPAGE_TEXT "This wizard will guide you through the installation of ${SOFTWARE_NAME}."

; --- Finish Page Settings ---
!define MUI_FINISHPAGE_RUN "$INSTDIR\nekobox.exe"
!define MUI_FINISHPAGE_RUN_TEXT "Launch ${SOFTWARE_NAME}"
;!define MUI_FINISHPAGE_RUN_NOTCHECKED

!addplugindir .\script\

!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH
!insertmacro MUI_LANGUAGE "English"

!macro RegString NAME TARGET ICON 
	${If} "$isAdmin" == "0" 
		WriteRegStr HKCU ${NAME} ${TARGET} ${ICON}
	${Else}
		WriteRegStr HKLM ${NAME} ${TARGET} ${ICON}
	${EndIf}
!macroend

!macro RegInteger NAME TARGET ICON 
	${If} "$isAdmin" == "0" 
		WriteRegDWORD HKCU ${NAME} ${TARGET} ${ICON}
	${Else}
		WriteRegDWORD HKLM ${NAME} ${TARGET} ${ICON}
	${EndIf}
!macroend

!macro DropReg NAME 
	${If} "$isAdmin" == "0" 
		DeleteRegKey HKCU ${NAME}
	${Else}
		DeleteRegKey HKLM ${NAME}
	${EndIf}
!macroend


Function WriteToFile
Exch $0 ;file to write to
Exch
Exch $1 ;text to write
 
  FileOpen $0 $0 a #open file
  FileSeek $0 0 END #go to end
  FileWrite $0 $1 #write to file
  FileClose $0
 
Pop $1
Pop $0
FunctionEnd
 
!macro WriteToFile NewLine File String
  !if `${NewLine}` == true
  Push `${String}$\r$\n`
  !else
  Push `${String}`
  !endif
  Push `${File}`
  Call WriteToFile
!macroend
!define WriteToFile `!insertmacro WriteToFile false`
!define WriteLineToFile `!insertmacro WriteToFile true`

Section "Install"

  !insertmacro "checkVcRedist"
  
  ${If} "$NoScript" != "1"

  SetOutPath "$INSTDIR\$RandomGUID"
  SetOverwrite on

  !ifdef DIRECTORY
    File /r  "${DIRECTORY}\nekobox_core.exe"
  !else
    File /r  ".\deployment\windows64\nekobox_core.exe"
  !endif

  ${If} "$VCRedistNeeded" != "1"
    nsExec::ExecToLog '"$INSTDIR\$RandomGUID\nekobox_core.exe" -installer-mode -kill-processes "$INSTDIR" -ignore-pid "$PID" -ignore-pid "$PPID" -ignore-pid "$PPPID" '
  ${Else}
    nsExec::ExecToLog '"$INSTDIR\$RandomGUID\nekobox_core.exe" -installer-mode -kill-processes "$INSTDIR" -vcredist-install -ignore-pid "$PID" -ignore-pid "$PPID" -ignore-pid "$PPPID" '
  ${EndIf}
  
  !insertmacro MoveFile "$INSTDIR\$RandomGUID\nekobox_core.exe" "$INSTDIR\nekobox_core.exe" 
;  ${PowerShellExec} "\
;    Write-Host $\"=> $INSTDIR$\" ;				\
;	if ($\"$VCRedistNeeded$\" -eq $\"1$\") {					\
;		$$url = $\"$VCRedistDownload$\" ;						\
;		$$download=$$env:USERPROFILE + $\"\Downloads$\"	;		\
;		$$path = $\"$$download\$VCRedistFile$\" ;				\
;		if (-not (Test-Path $$path)) {							\
;			Invoke-WebRequest -Uri $\"$$url$\" -OutFile $$path;	\
;		} ;														\
;		Start-Process $$path -Wait ;							\
;	}; 															\
;	$$procs = Get-CimInstance Win32_Process | Where-Object { 	\
;		$$_.ExecutablePath -like $\"$INSTDIR\*.exe$\" ;			\
;	}; 															\
;	foreach ($$proc in $$procs) { 								\
;		Write-Host $\"!! $$proc$\" ;							\
;		Stop-Process -Id $$proc.ProcessId -Force ; 				\
;	}; 															\
;	"
  ${EndIf}

  SetOutPath "$INSTDIR"
  SetOverwrite on

  ${If} "$NoScript" != "1"
  RMDir  "$INSTDIR\$RandomGUID"
  !ifdef DIRECTORY
    File /r /x nekobox_core.exe "${DIRECTORY}\*"
  !else
    File /r /x nekobox_core.exe ".\deployment\windows64\*"
  !endif
  ${Else}
  !ifdef DIRECTORY
    File /r  "${DIRECTORY}\*"
  !else
    File /r  ".\deployment\windows64\*"
  !endif
  ${EndIf}

  ${If} "$Winget" == "1"
    ${If} ${FileExists} "$INSTDIR\global.ini"
    ${Else}
        FileOpen $0 "$INSTDIR\global.ini" w
        FileWrite $0 "[General]$\n"
		FileClose $0
    ${EndIf}
    ${WriteLineToFile} "$INSTDIR\global.ini" "winget_package=true$\n"
  ${EndIf}

  ${If} "$UnpackOnly" != "1"
    FileOpen $0 "$INSTDIR\settings" w
    FileWrite $0 "DeleteToUseThisDirectoryForConfig"
	FileClose $0

    CreateShortcut "$desktop\${SOFTWARE_NAME}.lnk" "$INSTDIR\nekobox.exe" "" "$INSTDIR\nekobox.exe" 0
    CreateShortcut "$SMPROGRAMS\${SOFTWARE_NAME}.lnk" "$INSTDIR\nekobox.exe" "" "$INSTDIR\nekobox.exe" 0

    !insertmacro RegString "Software\nekobox" "InstallPath" "$INSTDIR"
    !insertmacro RegString "Software\Microsoft\Windows\CurrentVersion\Uninstall\nekobox" "DisplayName" "${SOFTWARE_NAME}"
	!insertmacro RegString "Software\Microsoft\Windows\CurrentVersion\Uninstall\nekobox" "DisplayIcon" "$INSTDIR\nekobox.exe"
    !insertmacro RegString "Software\Microsoft\Windows\CurrentVersion\Uninstall\nekobox" "UninstallString" "$INSTDIR\uninstall.exe"
    !insertmacro RegString "Software\Microsoft\Windows\CurrentVersion\Uninstall\nekobox" "InstallLocation" "$INSTDIR"
    !insertmacro RegString "Software\Microsoft\Windows\CurrentVersion\Uninstall\nekobox" "Publisher" "qr243vbi"
	${GetSize} "$INSTDIR" "/S=0K" $0 $1 $2
    !insertmacro RegInteger "Software\Microsoft\Windows\CurrentVersion\Uninstall\nekobox" "EstimatedSize" $0
    !insertmacro RegInteger "Software\Microsoft\Windows\CurrentVersion\Uninstall\nekobox" "NoModify" 1
    !insertmacro RegInteger "Software\Microsoft\Windows\CurrentVersion\Uninstall\nekobox" "NoRepair" 1
    WriteUninstaller "uninstall.exe"
  ${EndIf}
SectionEnd

Section "Uninstall"

  Delete "$SMPROGRAMS\nekobox.lnk"
  Delete "$desktop\nekobox.lnk"
  RMDir "$SMPROGRAMS\nekobox"

  RMDir /r "$INSTDIR"

  Delete "$INSTDIR\uninstall.exe"

  !insertmacro DropReg "Software\Microsoft\Windows\CurrentVersion\Uninstall\nekobox"
  
SectionEnd
