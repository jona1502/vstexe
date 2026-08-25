#ifndef AppVersion
  #define AppVersion "0.1.0"
#endif
#ifndef AppBinary
  #error AppBinary must point to the release VocalChain.exe
#endif
#ifndef OutputDirectory
  #define OutputDirectory "..\..\out\windows"
#endif

[Setup]
AppId={{B5A16131-A1B3-4886-A09E-2DA4B71F05DD}
AppName=VocalChain
AppVersion={#AppVersion}
AppPublisher=VocalChain
AppPublisherURL=https://github.com/jona1502/vstexe
AppSupportURL=https://github.com/jona1502/vstexe/issues
DefaultDirName={localappdata}\Programs\VocalChain
DefaultGroupName=VocalChain
DisableProgramGroupPage=yes
PrivilegesRequired=lowest
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
OutputDir={#OutputDirectory}
OutputBaseFilename=VocalChain-{#AppVersion}-Windows-x64-Setup
Compression=lzma2/max
SolidCompression=yes
WizardStyle=modern
UninstallDisplayIcon={app}\VocalChain.exe
VersionInfoVersion={#AppVersion}.0
VersionInfoCompany=VocalChain
VersionInfoDescription=VocalChain Windows Installer
VersionInfoProductName=VocalChain
VersionInfoProductVersion={#AppVersion}

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"
Name: "german"; MessagesFile: "compiler:Languages\German.isl"

[Files]
Source: "{#AppBinary}"; DestDir: "{app}"; DestName: "VocalChain.exe"; Flags: ignoreversion

[Icons]
Name: "{autoprograms}\VocalChain"; Filename: "{app}\VocalChain.exe"
Name: "{autodesktop}\VocalChain"; Filename: "{app}\VocalChain.exe"; Tasks: desktopicon

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked

[Run]
Filename: "{app}\VocalChain.exe"; Description: "{cm:LaunchProgram,VocalChain}"; Flags: nowait postinstall skipifsilent
