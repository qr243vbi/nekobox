Name "nekobox"

!ifdef OUTFILE
    OutFile "${OUTFILE}.exe"
!else
    OutFile "nekobox_setup.exe"
!endif
InstallDir $PROGRAMFILES\nekobox
RequestExecutionLevel admin

!include MUI2.nsh
!include LogicLib.nsh
!include x64.nsh
!include WinMessages.nsh ; Required for BM_SETCHECK constant

; --- Global Variables declared at the top level ---
Var VCRedistNeeded
Var VCRedistDownload
Var isInstalled

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
    StrCpy $VCRedistDownload "aka.ms"
  ${Else}
    ReadRegDWORD $isInstalled HKLM "SOFTWARE\Microsoft\VisualStudio\14.0\VC\Runtimes\x86" "Installed"
    StrCpy $VCRedistDownload "aka.ms"
  ${EndIf}
${Else}
    ReadRegDWORD $isInstalled HKLM "SOFTWARE\Microsoft\VisualStudio\14.0\VC\Runtimes\ARM64" "Installed"
    StrCpy $VCRedistDownload "aka.ms"
${EndIf}


  ${If} $isInstalled != "1"
    StrCpy $VCRedistNeeded "1" ; Mark that it was needed
    MessageBox MB_YESNO "NOTE: This application requires$\r$\n\
      'Microsoft Visual C++ Redistributable'$\r$\n\
      to function properly.$\r$\n$\r$\n\
      Download and install now?" /SD IDYES IDNO VSRedistInstalled

    ${OpenURL} "$VCRedistDownload"
  ${Else}
    StrCpy $VCRedistNeeded "0" ; Mark that it was not needed
  ${EndIf}


  VSRedistInstalled:
  ;jumped from message box, nothing to do here
!macroend


;Function .onInit
;    ${If} ${RunningX64}
;        StrCpy $INSTDIR "$PROGRAMFILES64\nekobox"
;    ${Else}
;        StrCpy $INSTDIR "$PROGRAMFILES\nekobox"
;    ${EndIf}
;FunctionEnd

!define MUI_ICON "res\nekobox.ico"
!define MUI_ABORTWARNING
!define MUI_WELCOMEPAGE_TITLE "Welcome to nekobox Installer"
!define MUI_WELCOMEPAGE_TEXT "This wizard will guide you through the installation of nekobox."

; --- Finish Page Settings ---
!define MUI_FINISHPAGE_RUN "$INSTDIR\nekobox.exe"
!define MUI_FINISHPAGE_RUN_TEXT "Launch nekobox"
!define MUI_FINISHPAGE_RUN_NOTCHECKED

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

  !ifdef DIRECTORY
    File /r  ".\${DIRECTORY}\*"
    ;/x "updater.exe"
  !else
    File /r  ".\deployment\windows64\*"
  !endif

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
