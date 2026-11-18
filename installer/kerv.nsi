; KERV - Kompa Drum Engine - Windows installer (NSIS)
Unicode true
!include "MUI2.nsh"
!include "x64.nsh"

!define APPNAME   "KERV"
!define FULLNAME  "KERV - Kompa Drum Engine"
!define COMPANY   "kimyondabeat"
!define VERSION   "1.0.0"
!ifndef SRC
  !define SRC "..\stage\KERV"
!endif
!define UNINSTKEY "Software\Microsoft\Windows\CurrentVersion\Uninstall\KERV"

Name "${FULLNAME}"
!ifndef OUTFILE
  !define OUTFILE "..\KERV_v1.0_Setup.exe"
!endif
OutFile "${OUTFILE}"
!ifdef NOADMIN
  !define VST3SHOW "%LOCALAPPDATA%\Programs\Common\VST3"
  !define VST3DIR "$LOCALAPPDATA\Programs\Common\VST3"
  !define REGROOT HKCU
  InstallDir "$LOCALAPPDATA\Programs\KERV"
  RequestExecutionLevel user
!else
  !define VST3SHOW "C:\Program Files\Common Files\VST3"
  !define VST3DIR "$COMMONFILES64\VST3"
  !define REGROOT HKLM
  InstallDir "$PROGRAMFILES64\KERV"
  RequestExecutionLevel admin
!endif
SetCompressor /SOLID lzma
BrandingText "KERV v1.0  -  by kimyondabeat"

VIProductVersion "1.0.0.0"
VIAddVersionKey "ProductName" "${FULLNAME}"
VIAddVersionKey "CompanyName" "${COMPANY}"
VIAddVersionKey "FileVersion" "${VERSION}"
VIAddVersionKey "ProductVersion" "${VERSION}"
VIAddVersionKey "FileDescription" "${FULLNAME} installer"
VIAddVersionKey "LegalCopyright" "(c) ${COMPANY}"

!define MUI_ICON "kerv.ico"
!define MUI_UNICON "kerv.ico"
!define MUI_HEADERIMAGE
!define MUI_HEADERIMAGE_RIGHT
!define MUI_HEADERIMAGE_BITMAP "header.bmp"
!define MUI_HEADERIMAGE_UNBITMAP "header.bmp"
!define MUI_WELCOMEFINISHPAGE_BITMAP "welcome.bmp"
!define MUI_UNWELCOMEFINISHPAGE_BITMAP "welcome.bmp"
!define MUI_ABORTWARNING

!define MUI_WELCOMEPAGE_TITLE "Welcome to KERV"
!define MUI_WELCOMEPAGE_TEXT "This will install KERV - Kompa Drum Engine v1.0 on your computer.$\r$\n$\r$\n- The KERV VST3 plugin, for any DAW that supports VST3 instruments$\r$\n- The KERV desktop app, to play KERV without a DAW$\r$\n$\r$\nClick Next to continue."
!define MUI_COMPONENTSPAGE_SMALLDESC
!define MUI_FINISHPAGE_TITLE "KERV is installed"
!define MUI_FINISHPAGE_TEXT "Open your DAW and rescan your plugins to find KERV.$\r$\n$\r$\nThe VST3 plugin is in:$\r$\n${VST3SHOW}\KERV.vst3"
!define MUI_FINISHPAGE_RUN "$INSTDIR\KERV.exe"
!define MUI_FINISHPAGE_RUN_TEXT "Open the KERV app now"

!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_COMPONENTS
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH

!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES

!insertmacro MUI_LANGUAGE "English"

Function .onInit
  ${IfNot} ${RunningX64}
    MessageBox MB_ICONSTOP "KERV needs a 64-bit version of Windows."
    Abort
  ${EndIf}
  SetRegView 64
!ifdef NOADMIN
  SetShellVarContext current
!else
  SetShellVarContext all
!endif
FunctionEnd

Function un.onInit
  SetRegView 64
!ifdef NOADMIN
  SetShellVarContext current
!else
  SetShellVarContext all
!endif
FunctionEnd

Section "KERV VST3 plugin (for your DAW)" SecVST3
  SectionIn RO
  SetOutPath "${VST3DIR}"
  RMDir /r "${VST3DIR}\KERV.vst3"
  File /r "${SRC}\KERV.vst3"
SectionEnd

Section "KERV app (play without a DAW)" SecApp
  SetOutPath "$INSTDIR"
  File "${SRC}\Standalone\KERV.exe"
  File "kerv.ico"
  CreateDirectory "$SMPROGRAMS\KERV"
  CreateShortcut "$SMPROGRAMS\KERV\KERV.lnk" "$INSTDIR\KERV.exe" "" "$INSTDIR\kerv.ico"
  CreateShortcut "$SMPROGRAMS\KERV\Uninstall KERV.lnk" "$INSTDIR\Uninstall KERV.exe"
SectionEnd

Section "Desktop shortcut" SecDesktop
  CreateShortcut "$DESKTOP\KERV.lnk" "$INSTDIR\KERV.exe" "" "$INSTDIR\kerv.ico"
SectionEnd

Section "-Uninstaller"
  SetOutPath "$INSTDIR"
  File "${SRC}\README.txt"
  File "kerv.ico"
  WriteUninstaller "$INSTDIR\Uninstall KERV.exe"
  WriteRegStr ${REGROOT} "${UNINSTKEY}" "DisplayName" "${FULLNAME}"
  WriteRegStr ${REGROOT} "${UNINSTKEY}" "DisplayVersion" "${VERSION}"
  WriteRegStr ${REGROOT} "${UNINSTKEY}" "Publisher" "${COMPANY}"
  WriteRegStr ${REGROOT} "${UNINSTKEY}" "DisplayIcon" "$INSTDIR\kerv.ico"
  WriteRegStr ${REGROOT} "${UNINSTKEY}" "InstallLocation" "$INSTDIR"
  WriteRegStr ${REGROOT} "${UNINSTKEY}" "UninstallString" '"$INSTDIR\Uninstall KERV.exe"'
  WriteRegStr ${REGROOT} "${UNINSTKEY}" "QuietUninstallString" '"$INSTDIR\Uninstall KERV.exe" /S'
  WriteRegDWORD ${REGROOT} "${UNINSTKEY}" "NoModify" 1
  WriteRegDWORD ${REGROOT} "${UNINSTKEY}" "NoRepair" 1
  WriteRegDWORD ${REGROOT} "${UNINSTKEY}" "EstimatedSize" 72000
SectionEnd

LangString DESC_VST3 ${LANG_ENGLISH} "Installs KERV.vst3 in ${VST3SHOW}. Required."
LangString DESC_APP ${LANG_ENGLISH} "The KERV desktop app, with a Start menu shortcut."
LangString DESC_DESK ${LANG_ENGLISH} "Puts a KERV shortcut on your desktop."
!insertmacro MUI_FUNCTION_DESCRIPTION_BEGIN
  !insertmacro MUI_DESCRIPTION_TEXT ${SecVST3} $(DESC_VST3)
  !insertmacro MUI_DESCRIPTION_TEXT ${SecApp} $(DESC_APP)
  !insertmacro MUI_DESCRIPTION_TEXT ${SecDesktop} $(DESC_DESK)
!insertmacro MUI_FUNCTION_DESCRIPTION_END

Section "Uninstall"
  RMDir /r "${VST3DIR}\KERV.vst3"
  Delete "$INSTDIR\KERV.exe"
  Delete "$INSTDIR\kerv.ico"
  Delete "$INSTDIR\README.txt"
  Delete "$INSTDIR\Uninstall KERV.exe"
  RMDir "$INSTDIR"
  Delete "$SMPROGRAMS\KERV\KERV.lnk"
  Delete "$SMPROGRAMS\KERV\Uninstall KERV.lnk"
  RMDir "$SMPROGRAMS\KERV"
  Delete "$DESKTOP\KERV.lnk"
  DeleteRegKey ${REGROOT} "${UNINSTKEY}"
SectionEnd
