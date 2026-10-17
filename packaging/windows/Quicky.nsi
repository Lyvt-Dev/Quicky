Unicode True
!include "MUI2.nsh"
!ifndef VERSION
  !define VERSION "0.1.0-preview.1"
!endif
!ifndef STAGE
  !define STAGE "stage"
!endif
!ifndef UNINSTALL_MANIFEST
  !error "Build with packaging/windows/build.ps1 to generate the uninstall manifest"
!endif
Name "Quicky"
OutFile "Quicky-${VERSION}-windows-x64-setup.exe"
InstallDir "$LOCALAPPDATA\Programs\Quicky"
InstallDirRegKey HKCU "Software\Quicky" "InstallDir"
RequestExecutionLevel user
SetCompressor /SOLID lzma
!define MUI_ABORTWARNING
!define MUI_WELCOMEPAGE_TITLE "Willkommen bei Quicky"
!define MUI_WELCOMEPAGE_TEXT "Dateien direkt und verschlüsselt im lokalen Netzwerk teilen.$\r$\n$\r$\nDies ist eine Vorschauversion."
!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!define MUI_FINISHPAGE_RUN "$INSTDIR\Quicky.exe"
!insertmacro MUI_PAGE_FINISH
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES
!insertmacro MUI_LANGUAGE "German"
!insertmacro MUI_LANGUAGE "English"
Section "Quicky" SecMain
  SetShellVarContext current
  SetOutPath "$INSTDIR"
  File /r "${STAGE}\*.*"
  WriteUninstaller "$INSTDIR\Uninstall.exe"
  CreateDirectory "$SMPROGRAMS\Quicky"
  CreateShortcut "$SMPROGRAMS\Quicky\Quicky.lnk" "$INSTDIR\Quicky.exe"
  CreateShortcut "$SMPROGRAMS\Quicky\Deinstallieren.lnk" "$INSTDIR\Uninstall.exe"
  CreateShortcut "$DESKTOP\Quicky.lnk" "$INSTDIR\Quicky.exe"
  WriteRegStr HKCU "Software\Quicky" "InstallDir" "$INSTDIR"
  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\Quicky" "DisplayName" "Quicky"
  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\Quicky" "DisplayVersion" "${VERSION}"
  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\Quicky" "Publisher" "Lyvt-Dev"
  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\Quicky" "UninstallString" '"$INSTDIR\Uninstall.exe"'
  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\Quicky" "QuietUninstallString" '"$INSTDIR\Uninstall.exe" /S'
  WriteRegDWORD HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\Quicky" "NoModify" 1
  WriteRegDWORD HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\Quicky" "NoRepair" 1
SectionEnd
Section "Uninstall"
  SetShellVarContext current
  Delete "$DESKTOP\Quicky.lnk"
  Delete "$SMPROGRAMS\Quicky\Quicky.lnk"
  Delete "$SMPROGRAMS\Quicky\Deinstallieren.lnk"
  RMDir "$SMPROGRAMS\Quicky"
  !include "${UNINSTALL_MANIFEST}"
  Delete "$INSTDIR\Uninstall.exe"
  RMDir "$INSTDIR"
  DeleteRegKey HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\Quicky"
  DeleteRegKey HKCU "Software\Quicky"
  ; Device identity, history and received files intentionally survive uninstall.
SectionEnd
