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
; The running application holds this mutex, so a silent update can close it
; through the restart manager instead of failing to replace a locked file.
AppMutex=VocalChainRunningInstance
CloseApplications=yes
RestartApplications=yes
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

[CustomMessages]
english.CableTitle=Virtual audio cable required
english.CableSubtitle=How other applications hear your chain
english.CableBody=VocalChain processes your microphone, but Windows only lets a driver publish a microphone device.%n%nTo use the processed signal in Discord, OBS or any other application, install a virtual audio cable such as VB-CABLE (https://vb-audio.com/Cable/). VocalChain does not include one.%n%nIn VocalChain, select the cable as the output device and enable Monitor. In the other application, select the cable's capture side as the microphone.
german.CableTitle=Virtuelles Audiokabel erforderlich
german.CableSubtitle=Wie andere Anwendungen deine Kette hören
german.CableBody=VocalChain bearbeitet dein Mikrofon, aber unter Windows kann nur ein Treiber ein Mikrofongerät bereitstellen.%n%nUm das bearbeitete Signal in Discord, OBS oder einer anderen Anwendung zu nutzen, installiere ein virtuelles Audiokabel wie VB-CABLE (https://vb-audio.com/Cable/). VocalChain bringt keines mit.%n%nWähle das Kabel in VocalChain als Ausgabegerät und aktiviere Monitor. Wähle in der anderen Anwendung die Aufnahmeseite des Kabels als Mikrofon.

[Files]
Source: "{#AppBinary}"; DestDir: "{app}"; DestName: "VocalChain.exe"; Flags: ignoreversion

[Icons]
Name: "{autoprograms}\VocalChain"; Filename: "{app}\VocalChain.exe"
Name: "{autodesktop}\VocalChain"; Filename: "{app}\VocalChain.exe"; Tasks: desktopicon

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked

[Run]
Filename: "{app}\VocalChain.exe"; Description: "{cm:LaunchProgram,VocalChain}"; Flags: nowait postinstall skipifsilent

[Code]
{ The application alone cannot publish a microphone, so the requirement is
  stated during setup rather than left for the user to discover in Discord. }
var
  CablePage: TOutputMsgWizardPage;

procedure InitializeWizard();
begin
  CablePage := CreateOutputMsgPage(wpWelcome,
    ExpandConstant('{cm:CableTitle}'),
    ExpandConstant('{cm:CableSubtitle}'),
    ExpandConstant('{cm:CableBody}'));
end;
