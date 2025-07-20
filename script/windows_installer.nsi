Name "Throne"
OutFile "ThroneSetup.exe"
InstallDir $APPDATA\Throne
RequestExecutionLevel user

!include MUI2.nsh
!define MUI_ICON "res\Throne.ico"
!define MUI_ABORTWARNING
!define MUI_WELCOMEPAGE_TITLE "Welcome to Throne Installer"
!define MUI_WELCOMEPAGE_TEXT "This wizard will guide you through the installation of Throne."
!define MUI_FINISHPAGE_RUN "$INSTDIR\Throne.exe"
!define MUI_FINISHPAGE_RUN_TEXT "Launch Throne"
!addplugindir .\

!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH

!insertmacro MUI_LANGUAGE "English"

UninstallText "This will uninstall Throne. Do you wish to continue?"
UninstallIcon "res\ThroneDel.ico"

Section "Install"
  SetOutPath "$INSTDIR"
  SetOverwrite on

  FindProcDLL::KillProc "$INSTDIR\Throne.exe"

  File /r ".\deployment\windows64\*"

  CreateShortcut "$desktop\Throne.lnk" "$instdir\Throne.exe"

  CreateDirectory "$SMPROGRAMS\Throne"
  CreateShortcut "$SMPROGRAMS\Throne\Throne.lnk" "$INSTDIR\Throne.exe" "" "$INSTDIR\Throne.exe" 0
  CreateShortcut "$SMPROGRAMS\Throne\Uninstall Throne.lnk" "$INSTDIR\uninstall.exe"

  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\Throne" "DisplayName" "Throne"
  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\Throne" "UninstallString" "$INSTDIR\uninstall.exe"
  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\Throne" "InstallLocation" "$INSTDIR"
  WriteRegDWORD HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\Throne" "NoModify" 1
  WriteRegDWORD HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\Throne" "NoRepair" 1
  WriteUninstaller "uninstall.exe"
SectionEnd

Section "Uninstall"

  FindProcDLL::KillProc "$INSTDIR\Throne.exe"

  Delete "$SMPROGRAMS\Throne\Throne.lnk"
  Delete "$SMPROGRAMS\Throne\Uninstall Throne.lnk"
  Delete "$desktop\Throne.lnk"
  RMDir "$SMPROGRAMS\Throne"

  RMDir /r "$INSTDIR"

  Delete "$INSTDIR\uninstall.exe"

  DeleteRegKey HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\Throne"
SectionEnd
