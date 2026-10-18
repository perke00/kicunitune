; Inno Setup 6.3+ script for KITSUNE Auto Tune (VST3)
; Build:  ISCC.exe /DMyVersion=0.1.0 /DBundleDir="..\build\KitsuneAutoTune_artefacts\Release\VST3\KITSUNE Auto Tune.vst3" installer.iss
#ifndef MyVersion
  #define MyVersion "0.1.0"
#endif
#ifndef BundleDir
  #define BundleDir "..\build\KitsuneAutoTune_artefacts\Release\VST3\KITSUNE Auto Tune.vst3"
#endif

[Setup]
AppId={{6E1B7C52-3B0A-4C55-9E64-5A1F0C4D7A21}
AppName=KITSUNE Auto Tune
AppVersion={#MyVersion}
AppVerName=KITSUNE Auto Tune {#MyVersion}
AppPublisher=prodbyperke
; The VST3 folder is shared with other plugins: we only add/remove our own bundle inside it.
DefaultDirName={commoncf64}\VST3
DisableProgramGroupPage=yes
DirExistsWarning=no
AppendDefaultDirName=no
UsePreviousAppDir=no
PrivilegesRequired=admin
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
UninstallFilesDir={autopf64}\KITSUNE\Auto Tune
UninstallDisplayName=KITSUNE Auto Tune {#MyVersion}
OutputDir=Output
OutputBaseFilename=KITSUNE-Auto-Tune-{#MyVersion}-Setup
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
DisableDirPage=no

[Files]
Source: "{#BundleDir}\*"; DestDir: "{app}\KITSUNE Auto Tune.vst3"; Flags: recursesubdirs createallsubdirs ignoreversion

[Messages]
SelectDirLabel3=Setup will install the KITSUNE Auto Tune.vst3 bundle into this VST3 folder. The default (C:\Program Files\Common Files\VST3) is scanned by FL Studio and most DAWs.
