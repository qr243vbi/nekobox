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

Function .onInit
    ${If} ${RunningX64}
        StrCpy $INSTDIR "$PROGRAMFILES64\nekobox"
    ${Else}
        StrCpy $INSTDIR "$PROGRAMFILES\nekobox"
    ${EndIf}
FunctionEnd

!macro customInit
  Var /GLOBAL VCRedistDownload
  Var /GLOBAL isInstalled
  ${IfNot} ${IsNativeARM64}
    ${If} ${RunningX64}
      ;check H-KEY registry
      ReadRegDWORD $isInstalled HKLM "SOFTWARE\Wow6432Node\Microsoft\VisualStudio\14.0\VC\Runtimes\x64" "Installed"
      StrCpy $VCRedistDownload "https://aka.ms/vc14/vc_redist.x64.exe"
    ${Else}
      ReadRegDWORD $isInstalled HKLM "SOFTWARE\Microsoft\VisualStudio\14.0\VC\Runtimes\x86" "Installed"
      StrCpy $VCRedistDownload "https://aka.ms/vc14/vc_redist.x86.exe"
    ${EndIf}
  ${Else}
      ReadRegDWORD $isInstalled HKLM "SOFTWARE\Microsoft\VisualStudio\14.0\VC\Runtimes\ARM64" "Installed"
      StrCpy $VCRedistDownload "https://aka.ms/vc14/vc_redist.arm64.exe"
  ${EndIf}


  ${If} $isInstalled != "1"
    MessageBox MB_YESNO "NOTE: This application requires$\r$\n\
      'Microsoft Visual C++ Redistributable'$\r$\n\
      to function properly.$\r$\n$\r$\n\
      Download and install now?" /SD IDYES IDNO VSRedistInstalled

    ;if no goto executed, install vcredist
    ;create temp dir
    CreateDirectory $TEMP\app-name-setup
    ;download installer
    StrCpy $DestinationPath "$TEMP\app-name-setup\vcppredist.exe"

    ; Define the PowerShell command to execute
    ; We use Invoke-WebRequest to download the file securely via HTTPS
    ; -OutFile specifies where to save the file
    ; -UseBasicParsing is often needed to run without a full IE DOM parser
    StrCpy $R0 "powershell.exe -Command &{Invoke-WebRequest -Uri '$VCRedistDownload' -OutFile '$DestinationPath' -UseBasicParsing}"

    ; Execute the PowerShell command and wait for it to finish
    ExecWait '"$R0"'
    ;exec installer
    ExecWait "$TEMP\app-name-setup\vcppredist.exe"
  ${EndIf}


  VSRedistInstalled:
  ;jumped from message box, nothing to do here
!macroend

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

  !insertmacro customInit

  SetOutPath "$INSTDIR"
  SetOverwrite on

  !ifdef DIRECTORY
    File /r  ".\deployment\${DIRECTORY}\*"
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




