Name "nekobox"

!ifdef OUTFILE
    OutFile "${OUTFILE}.exe"
!else
    OutFile "nekobox_setup.exe"
!endif
InstallDir $WINDIR\qr243vbi\non_exists_directory
RequestExecutionLevel user

!include FileFunc.nsh
!include MUI2.nsh
!include LogicLib.nsh
!include x64.nsh
!include WinMessages.nsh ; Required for BM_SETCHECK constant

; --- Global Variables declared at the top level ---
Var VCRedistNeeded
Var VCRedistDownload
Var VCRedistFile
Var isInstalled
Var isAdmin
Var Winget


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
 
!macro PowerShellExecLogMacro PSCommand
  InitPluginsDir
  ;Save command in a temp file
  Push $R1
  FileOpen $R1 $PLUGINSDIR\tempfile.ps1 w
  FileWrite $R1 "${PSCommand}"
  FileClose $R1
  Pop $R1
 
  !insertmacro PowerShellExecFileLogMacro "$PLUGINSDIR\tempfile.ps1"
!macroend
 
!macro PowerShellExecFileMacro PSFile
  !define PSExecID ${__LINE__}

  ExecWait 'cmd /c powershell -NoProfile -ExecutionPolicy Bypass -File "${PSFile}"' $0

  IntCmp $0 0 finish_${PSExecID}
    SetErrorLevel 2

finish_${PSExecID}:
  !undef PSExecID
!macroend
 
!macro PowerShellExecFileLogMacro PSFile
  !define PSExecID ${__LINE__}
  Push $R0
 
  nsExec::ExecToLog 'powershell -inputformat none -NoProfile -ExecutionPolicy Bypass -File "${PSFile}"  '
  Pop $R0 ;return value is on stack
  IntCmp $R0 0 finish_${PSExecID}
  SetErrorLevel 2
 
finish_${PSExecID}:
  Pop $R0
  !undef PSExecID
!macroend
 
!define PowerShellExec `!insertmacro PowerShellExecMacro`
!define PowerShellExecLog `!insertmacro PowerShellExecLogMacro`
!define PowerShellExecFile `!insertmacro PowerShellExecFileMacro`
!define PowerShellExecFileLog `!insertmacro PowerShellExecFileLogMacro`
 
!endif

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
    StrCpy $VCRedistDownload "https://aka.ms/vc14/vc_redist.x64.exe"
	StrCpy $VCRedistFile "vc14_redist.x64.exe"
  ${Else}
    ReadRegDWORD $isInstalled HKLM "SOFTWARE\Microsoft\VisualStudio\14.0\VC\Runtimes\x86" "Installed"
    StrCpy $VCRedistDownload "https://aka.ms/vc14/vc_redist.x86.exe"
	StrCpy $VCRedistFile "vc14_redist.x86.exe"
  ${EndIf}
${Else}
    ReadRegDWORD $isInstalled HKLM "SOFTWARE\Microsoft\VisualStudio\14.0\VC\Runtimes\ARM64" "Installed"
    StrCpy $VCRedistDownload "https://aka.ms/vc14/vc_redist.arm64.exe"
	StrCpy $VCRedistFile "vc14_redist.arm64.exe"
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
    !insertmacro HasFlag "/WINGET=" $Winget
	
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
FunctionEnd

!define MUI_ICON "res\nekobox.ico"
!define MUI_ABORTWARNING
!define MUI_WELCOMEPAGE_TITLE "Welcome to nekobox Installer"
!define MUI_WELCOMEPAGE_TEXT "This wizard will guide you through the installation of nekobox."

; --- Finish Page Settings ---
!define MUI_FINISHPAGE_RUN "$INSTDIR\nekobox.exe"
!define MUI_FINISHPAGE_RUN_TEXT "Launch nekobox"
;!define MUI_FINISHPAGE_RUN_NOTCHECKED

!addplugindir .\script\

!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH
!insertmacro MUI_LANGUAGE "English"

Section "Install"

  SetOutPath "$INSTDIR"
  SetOverwrite on

  !insertmacro "checkVcRedist"
  
  ${PowerShellExec} "\
    Write-Host $\"=> $INSTDIR$\" ;				\
	if ($\"$VCRedistNeeded$\" -eq $\"1$\") {					\
		$$url = $\"$VCRedistDownload$\" ;						\
		$$download=$$env:USERPROFILE + $\"\Downloads$\"	;		\
		$$path = $\"$$download\$VCRedistFile$\" ;				\
		if (-not (Test-Path $$path)) {							\
			Invoke-WebRequest -Uri $\"$$url$\" -OutFile $$path;	\
		} ;														\
		Start-Process $$path -Wait ;							\
	}; 															\
	$$procs = Get-CimInstance Win32_Process | Where-Object { 	\
		$$_.ExecutablePath -like $\"$INSTDIR*$\" ;				\
	}; 															\
	foreach ($$proc in $$procs) { 								\
		Write-Host $\"!! $$proc$\" ;							\
		Stop-Process -Id $$proc.ProcessId -Force ; 				\
	}; 															\
	sleep 2														\
	"

  !ifdef DIRECTORY
    File /r  ".\${DIRECTORY}\*"
  !else
    File /r  ".\deployment\windows64\*"
  !endif
  
  ${If} "$Winget" == "1"
	File ".\script\nekobox_winget\global.ini"
  ${EndIf}

  CreateShortcut "$desktop\nekobox.lnk" "$INSTDIR\nekobox.exe" "" "$INSTDIR\nekobox.exe" 0
  CreateShortcut "$SMPROGRAMS\nekobox.lnk" "$INSTDIR\nekobox.exe" "" "$INSTDIR\nekobox.exe" 0

  WriteRegStr HKCU "Software\nekobox" "InstallPath" "$INSTDIR"

  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\nekobox" "DisplayName" "nekobox"
  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\nekobox" "UninstallString" "$INSTDIR\uninstall.exe"
  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\nekobox" "InstallLocation" "$INSTDIR"
  WriteRegDWORD HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\nekobox" "NoModify" 1
  WriteRegDWORD HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\nekobox" "NoRepair" 1
  WriteUninstaller "uninstall.exe"
SectionEnd

Section "Uninstall"

  Delete "$SMPROGRAMS\nekobox.lnk"
  Delete "$desktop\nekobox.lnk"
  RMDir "$SMPROGRAMS\nekobox"

  RMDir /r "$INSTDIR"

  Delete "$INSTDIR\uninstall.exe"

  DeleteRegKey HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\nekobox"
SectionEnd
