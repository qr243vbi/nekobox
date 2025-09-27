Name "nekobox"

!ifdef OUTFILE
    OutFile "${OUTFILE}.exe"
!else
    OutFile "nekobox_setup.exe"
!endif
InstallDir $APPDATA\nekobox
RequestExecutionLevel user

!include MUI2.nsh
!define MUI_ICON "res\nekobox.ico"
!define MUI_ABORTWARNING
!define MUI_WELCOMEPAGE_TITLE "Welcome to nekobox Installer"
!define MUI_WELCOMEPAGE_TEXT "This wizard will guide you through the installation of nekobox."
!define MUI_FINISHPAGE_RUN "$INSTDIR\nekobox.exe"
!define MUI_FINISHPAGE_RUN_TEXT "Launch nekobox"
!addplugindir .\script\

!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH

!insertmacro MUI_LANGUAGE "English"

Section "Install"
  SetOutPath "$INSTDIR"
  SetOverwrite on

  !ifdef DIRECTORY
    File /r ".\deployment\${DIRECTORY}\*"
  !else
    File /r ".\deployment\windows64\*"
  !endif


  CreateShortcut "$desktop\nekobox.lnk" "$instdir\nekobox.exe"
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
