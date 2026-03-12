;--------------------------------
;Includes
;--------------------------------
!include "MUI2.nsh"


;--------------------------------
; publisher
;--------------------------------
VIProductVersion					"1.0.1.0"
VIAddVersionKey "ProductName"		"LLM-O-Meter"
VIAddVersionKey "CompanyName"		"Brekel"
VIAddVersionKey "LegalCopyright"	"(c) Brekel"
VIAddVersionKey "FileDescription"	"LLM-O-Meter - Display your Claude Code and Google AntiGravity usage in the tray"
VIAddVersionKey "FileVersion"		"1.0.1"


;--------------------------------
; general
;--------------------------------
; name and file
Unicode true
Name "LLM-O-Meter"
OutFile "LLM-O-Meter Windows Setup v1.01.exe"

; default installation folder
InstallDir "$PROGRAMFILES64\LLM-O-Meter"

; get installation folder from registry if available
InstallDirRegKey HKCU "Software\LLM-O-Meter" ""

; misc
BrandingText "Brekel.com"

;SetCompressor /SOLID lzma		; smaller file but may have negative effect on virus scanners
SetCompressor lzma
SetCompressorDictSize 64
SetDatablockOptimize ON

	

	
;--------------------------------
; variables
;--------------------------------
Var StartMenuFolder



  
;--------------------------------
; interface Settings
;--------------------------------
!define MUI_ABORTWARNING




;--------------------------------
;pages
;--------------------------------
;Start Menu Folder Page Configuration
!define MUI_STARTMENUPAGE_REGISTRY_ROOT			"HKCU" 
!define MUI_STARTMENUPAGE_REGISTRY_KEY			"Software\LLM-O-Meter" 
!define MUI_STARTMENUPAGE_REGISTRY_VALUENAME	"Start Menu Folder"

; Components page
!insertmacro MUI_PAGE_COMPONENTS

; Finish page settings
!define MUI_FINISHPAGE_RUN "$INSTDIR\LLM-O-Meter.exe"
!define MUI_FINISHPAGE_RUN_TEXT "Launch LLM-O-Meter"
!define MUI_FINISHPAGE_RUN_CHECKED 

!insertmacro MUI_PAGE_STARTMENU Application		$StartMenuFolder
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH

!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES




;--------------------------------
; languages
;--------------------------------
!insertmacro MUI_LANGUAGE "English"




;--------------------------------
; installer Sections
;--------------------------------
Section "!Main Program" SecMain
	;SetAutoClose true

	; make section mandatory
	SectionIn RO

	; set output path
	SetOutPath "$INSTDIR"

	; remove old files
	DetailPrint "Removing old files"
	RMDir /r "$INSTDIR"

	; main files
	File /r "..\dist_win\*"

	; store installation folder
	WriteRegStr HKCU "Software\Brekel LLM-O-Meter" "" $INSTDIR

	; start menu shortcuts
	!insertmacro MUI_STARTMENU_WRITE_BEGIN Application
		SetShellVarContext	All
		CreateDirectory		"$SMPROGRAMS\Brekel LLM-O-Meter"
		CreateShortCut		"$SMPROGRAMS\Brekel LLM-O-Meter\LLM-O-Meter.lnk"	"$INSTDIR\LLM-O-Meter.exe" ""
		CreateShortCut		"$SMPROGRAMS\Brekel LLM-O-Meter\Uninstall.lnk"		"$INSTDIR\Uninstall.exe"
	!insertmacro MUI_STARTMENU_WRITE_END
SectionEnd


Section "Uninstaller" SecUninstaller
	; make section mandatory
	SectionIn RO

	WriteRegStr   HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Brekel LLM-O-Meter"	"DisplayName"		"Brekel LLM-O-Meter"
	WriteRegStr   HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Brekel LLM-O-Meter"	"DisplayIcon"		"$INSTDIR\LLM-O-Meter.exe,0"
	WriteRegStr   HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Brekel LLM-O-Meter"	"UninstallString"	"$INSTDIR\Uninstall.exe"
	WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Brekel LLM-O-Meter"	"NoModify"			1
	WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Brekel LLM-O-Meter"	"NoRepair"			1
	WriteUninstaller "$INSTDIR\Uninstall.exe"
SectionEnd


Section "Desktop Shortcut" SecDesktopShortcuts
	CreateShortCut "$DESKTOP\LLM-O-Meter.lnk"	"$INSTDIR\LLM-O-Meter.exe" ""
SectionEnd


Section "Run on Windows Startup" SecStartup
	WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Run" "LLM-O-Meter" "$INSTDIR\LLM-O-Meter.exe"
SectionEnd




;--------------------------------
; descriptions
;--------------------------------
; language strings
LangString DESC_SecMain						${LANG_ENGLISH} "Main application"
LangString DESC_SecDesktopShortcuts			${LANG_ENGLISH} "Desktop shortcuts"
LangString DESC_SecStartup					${LANG_ENGLISH} "Automatically start LLM-O-Meter when Windows starts"
LangString DESC_SecUninstaller				${LANG_ENGLISH} "Uninstaller"

; assign language strings to sections
!insertmacro MUI_FUNCTION_DESCRIPTION_BEGIN
	!insertmacro MUI_DESCRIPTION_TEXT ${SecMain}				$(DESC_SecMain)
	!insertmacro MUI_DESCRIPTION_TEXT ${SecDesktopShortcuts}	$(DESC_SecDesktopShortcuts)
	!insertmacro MUI_DESCRIPTION_TEXT ${SecStartup}				$(DESC_SecStartup)
	!insertmacro MUI_DESCRIPTION_TEXT ${SecUninstaller}			$(DESC_SecUninstaller)
!insertmacro MUI_FUNCTION_DESCRIPTION_END


	

;--------------------------------
; uninstaller Section
;--------------------------------
Section "Uninstall"
	; Use nsExec to run taskkill silently.
	; /F forcefully terminates the process.
	DetailPrint "Attempting to close LLM-O-Meter..."
	nsExec::Exec 'taskkill /F /IM "LLM-O-Meter.exe"'
	Sleep 1000

	; main files
	RMDir /r "$INSTDIR"

	; delete start menu items
	!insertmacro MUI_STARTMENU_GETFOLDER Application $StartMenuFolder
	Delete "$SMPROGRAMS\Brekel LLM-O-Meter\LLM-O-Meter.lnk"
	Delete "$SMPROGRAMS\Brekel LLM-O-Meter\Uninstall.lnk"
	RMDir  "$SMPROGRAMS\Brekel LLM-O-Meter"
	SetShellVarContext All
	Delete "$SMPROGRAMS\Brekel LLM-O-Meter\LLM-O-Meter.lnk"
	Delete "$SMPROGRAMS\Brekel LLM-O-Meter\Uninstall.lnk"
	RMDir  "$SMPROGRAMS\Brekel LLM-O-Meter"
	
	; remove registry keys
	DeleteRegValue HKCU "Software\Microsoft\Windows\CurrentVersion\Run" "LLM-O-Meter"
	DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Brekel LLM-O-Meter"

	; delete desktop icon
	Delete "$DESKTOP\LLM-O-Meter.lnk"
SectionEnd
