; OpenC3 Developer Toolkit — NSIS Installer Script
; Requires NSIS 3.x   (https://nsis.sourceforge.io/)
;
; Usage (called automatically by package.ps1):
;   makensis /DROOT_DIR="C:\..." /DDIST_DIR="C:\...\dist" installer.nsi

!ifndef ROOT_DIR
  !define ROOT_DIR ".."
!endif
!ifndef DIST_DIR
  !define DIST_DIR "..\dist"
!endif

; ── Metadata ──────────────────────────────────────────────────────────────────
!define APP_NAME     "OpenC3 Developer Toolkit"
!define APP_VERSION  "0.1.0"
!define APP_EXE      "OpenC3DevToolkit.exe"
!define PUBLISHER    "OpenC3 Toolkit Contributors"
!define HELP_URL     "https://github.com/underkim/cosmostool"
!define REGKEY       "Software\OpenC3DevToolkit"

; ── General settings ──────────────────────────────────────────────────────────
Name             "${APP_NAME} ${APP_VERSION}"
OutFile          "${ROOT_DIR}\OpenC3DevToolkit-${APP_VERSION}-Setup.exe"
InstallDir       "$PROGRAMFILES64\OpenC3 Developer Toolkit"
InstallDirRegKey HKLM "${REGKEY}" "InstallDir"
RequestExecutionLevel admin

; Modern UI
!include MUI2.nsh
!define MUI_ABORTWARNING
!define MUI_ICON   "${DIST_DIR}\${APP_EXE}"
!define MUI_UNICON "${DIST_DIR}\${APP_EXE}"

; Pages
!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_LICENSE "${ROOT_DIR}\LICENSE"
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH

!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES

!insertmacro MUI_LANGUAGE "English"

; ── Install ───────────────────────────────────────────────────────────────────
Section "MainSection" SecMain
  SetOutPath "$INSTDIR"
  File /r "${DIST_DIR}\*.*"

  ; Start menu shortcut
  CreateDirectory "$SMPROGRAMS\OpenC3 Developer Toolkit"
  CreateShortCut  "$SMPROGRAMS\OpenC3 Developer Toolkit\${APP_NAME}.lnk" \
                  "$INSTDIR\${APP_EXE}"
  CreateShortCut  "$DESKTOP\${APP_NAME}.lnk" "$INSTDIR\${APP_EXE}"

  ; Uninstaller
  WriteUninstaller "$INSTDIR\Uninstall.exe"

  ; Registry
  WriteRegStr   HKLM "${REGKEY}" "InstallDir" "$INSTDIR"
  WriteRegStr   HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\OpenC3DevToolkit" \
                      "DisplayName"     "${APP_NAME}"
  WriteRegStr   HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\OpenC3DevToolkit" \
                      "DisplayVersion"  "${APP_VERSION}"
  WriteRegStr   HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\OpenC3DevToolkit" \
                      "Publisher"       "${PUBLISHER}"
  WriteRegStr   HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\OpenC3DevToolkit" \
                      "HelpLink"        "${HELP_URL}"
  WriteRegStr   HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\OpenC3DevToolkit" \
                      "UninstallString" '"$INSTDIR\Uninstall.exe"'
  WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\OpenC3DevToolkit" \
                      "NoModify" 1
  WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\OpenC3DevToolkit" \
                      "NoRepair" 1
SectionEnd

; ── Uninstall ─────────────────────────────────────────────────────────────────
Section "Uninstall"
  Delete "$INSTDIR\Uninstall.exe"
  RMDir  /r "$INSTDIR"
  RMDir  /r "$SMPROGRAMS\OpenC3 Developer Toolkit"
  Delete "$DESKTOP\${APP_NAME}.lnk"
  DeleteRegKey HKLM "${REGKEY}"
  DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\OpenC3DevToolkit"
SectionEnd
