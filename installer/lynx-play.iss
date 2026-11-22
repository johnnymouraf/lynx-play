; Inno Setup script for the Lynx Play installer. The build passes
; /DAppVersion and /DSourceDir from the workflow.

#ifndef AppVersion
  #define AppVersion "0.0.0"
#endif
#ifndef SourceDir
  #define SourceDir "."
#endif

[Setup]
AppId={{8E4B5F6A-3C2D-4B7A-9F1E-6A5D8C2B7E41}
AppName=Lynx Play
AppVersion={#AppVersion}
AppVerName=Lynx Play {#AppVersion}
AppPublisher=Lynx Play
DefaultDirName={autopf}\Lynx Play
DefaultGroupName=Lynx Play
DisableProgramGroupPage=yes
OutputDir=..\dist
OutputBaseFilename=LynxPlay-Setup-{#AppVersion}
Compression=lzma2
SolidCompression=yes
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
UninstallDisplayIcon={app}\lynx-play.exe
WizardStyle=modern
; A personal app installs for the current user, without a UAC prompt.
PrivilegesRequired=lowest

[Languages]
Name: "brazilianportuguese"; MessagesFile: "compiler:Languages\BrazilianPortuguese.isl"

[Tasks]
Name: "desktopicon"; Description: "Criar um atalho na area de trabalho"; GroupDescription: "Atalhos:"

[Files]
Source: "{#SourceDir}\*"; DestDir: "{app}"; Flags: recursesubdirs ignoreversion

[Icons]
Name: "{group}\Lynx Play"; Filename: "{app}\lynx-play.exe"
Name: "{autodesktop}\Lynx Play"; Filename: "{app}\lynx-play.exe"; Tasks: desktopicon

[Run]
Filename: "{app}\lynx-play.exe"; Description: "Abrir o Lynx Play"; Flags: nowait postinstall skipifsilent
