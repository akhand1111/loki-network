; Script generated by the Inno Script Studio Wizard.
; SEE THE DOCUMENTATION FOR DETAILS ON CREATING INNO SETUP SCRIPT FILES!

#define MyAppName "loki-network"
#define MyAppVersion "0.3.1"
#define MyAppPublisher "Loki Project"
#define MyAppURL "https://loki.network"
#define MyAppExeName "lokinet.exe"
; change this to avoid compiler errors  -despair
#define DevPath "D:\dev\external\llarp\"
#include <idp.iss>

; see ../LICENSE

[Setup]
; NOTE: The value of AppId uniquely identifies this application.
; Do not use the same AppId value in installers for other applications.
; (To generate a new GUID, click Tools | Generate GUID inside the IDE.)
AppId={{11335EAC-0385-4C78-A3AA-67731326B653}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
;AppVerName={#MyAppName} {#MyAppVersion}
AppPublisher={#MyAppPublisher}
AppPublisherURL={#MyAppURL}
AppSupportURL={#MyAppURL}
AppUpdatesURL={#MyAppURL}
DefaultDirName={pf}\{#MyAppPublisher}\{#MyAppName}
DefaultGroupName={#MyAppName}
AllowNoIcons=yes
LicenseFile={#DevPath}LICENSE
OutputDir={#DevPath}win32-setup
OutputBaseFilename=lokinet-win32
Compression=lzma
SolidCompression=yes
VersionInfoVersion=0.3.1
VersionInfoCompany=Loki Project
VersionInfoDescription=lokinet for windows
VersionInfoTextVersion=0.3.1-dev
VersionInfoProductName=loki-network
VersionInfoProductVersion=0.3.1
VersionInfoProductTextVersion=0.3.1-dev
InternalCompressLevel=ultra64
MinVersion=0,5.0
ArchitecturesInstallIn64BitMode=x64
VersionInfoCopyright=Copyright �2018 Loki Project
AlwaysRestart=yes

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked
Name: "quicklaunchicon"; Description: "{cm:CreateQuickLaunchIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked; OnlyBelowVersion: 0,6.1

[Files]
; only one of these is installed
Source: "{#DevPath}build\lokinet.exe"; DestDir: "{app}"; Flags: ignoreversion 32bit; Check: not IsWin64
Source: "{#DevPath}build64\lokinet.exe"; DestDir: "{app}"; Flags: ignoreversion 64bit; Check: IsWin64
; eh, might as well ship the 32-bit port of everything else
; do we _need_ to ship these?
Source: "{#DevPath}build\dns.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#DevPath}build\llarpc.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#DevPath}build\rcutil.exe"; DestDir: "{app}"; Flags: ignoreversion
; delet this after finishing setup, we only need it to extract the drivers
; and download an initial RC
Source: "{#DevPath}lokinet-bootstrap.exe"; DestDir: "{tmp}"; Flags: deleteafterinstall
Source: "{#DevPath}win32-setup\7z.exe"; DestDir: "{tmp}"; Flags: deleteafterinstall
Source: "{tmp}\inet6.7z"; DestDir: "{app}"; Flags: ignoreversion external deleteafterinstall; MinVersion: 0,5.0; OnlyBelowVersion: 0,5.1
; Copy the correct tuntap driver for the selected platform
Source: "{tmp}\tuntapv9.7z"; DestDir: "{app}"; Flags: ignoreversion external deleteafterinstall; OnlyBelowVersion: 0, 6.0
Source: "{tmp}\tuntapv9_n6.7z"; DestDir: "{app}"; Flags: ignoreversion external deleteafterinstall; MinVersion: 0,6.0

; NOTE: Don't use "Flags: ignoreversion" on any shared system files

[UninstallDelete]
Type: filesandordirs; Name: "{app}\tap-windows*"
Type: filesandordirs; Name: "{app}\inet6_driver"; MinVersion: 0,5.0; OnlyBelowVersion: 0,5.1
Type: filesandordirs; Name: "{userappdata}\.lokinet"

[UninstallRun]
Filename: "{app}\tap-windows-9.21.2\remove.bat"; WorkingDir: "{app}\tap-windows-9.21.2"; MinVersion: 0,6.0; Flags: runascurrentuser
Filename: "{app}\tap-windows-9.9.2\remove.bat"; WorkingDir: "{app}\tap-windows-9.9.2"; OnlyBelowVersion: 0,6.0; Flags: runascurrentuser
 
[Code]
procedure CurStepChanged(CurStep: TSetupStep);
var
  Version: TWindowsVersion;
begin
  if CurStep = ssPostInstall then
  begin
  GetWindowsVersionEx(Version);
  if Version.NTPlatform and (Version.Major = 5) and (Version.Minor = 0) then
     // I need a better message...
    MsgBox('Set up IPv6 in Network Connections after reboot.', mbInformation, MB_OK);
  end;
end;

procedure InitializeWizard();
var
  Version: TWindowsVersion;
  S: String;
begin
  GetWindowsVersionEx(Version);
  if Version.NTPlatform and
     (Version.Major < 6) then
  begin
    // Windows 2000, XP, .NET Svr 2003
    // these have a horribly crippled WinInet that issues Triple-DES as its most secure
    // cipher suite
    idpAddFile('http://www.rvx86.net/files/tuntapv9.7z', ExpandConstant('{tmp}\tuntapv9.7z'));
    // Windows 2000 only, we need to install inet6 separately
    if (FileExists(ExpandConstant('{sys}\drivers\tcpip6.sys')) = false) and (Version.Major = 5) and (Version.Minor = 0) then
    begin
    idpAddFile('http://www.rvx86.net/files/inet6.7z', ExpandConstant('{tmp}\inet6.7z'));
    end;
  end
  else
  begin
    // current versions of windows :-)
    // (Arguably, one could pull this from any of the forks.)
    idpAddFile('https://github.com/despair86/loki-network/raw/master/contrib/tuntapv9-ndis/tap-windows-9.21.2.7z', ExpandConstant('{tmp}\tuntapv9_n6.7z'));
  end;
    idpDownloadAfter(wpReady);
end;

[Icons]
Name: "{group}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"
Name: "{group}\{cm:ProgramOnTheWeb,{#MyAppName}}"; Filename: "{#MyAppURL}"
Name: "{group}\{cm:UninstallProgram,{#MyAppName}}"; Filename: "{uninstallexe}"
Name: "{commondesktop}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; Tasks: desktopicon
Name: "{userappdata}\Microsoft\Internet Explorer\Quick Launch\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; Tasks: quicklaunchicon; OnlyBelowVersion: 0, 6.1
Name: "{userappdata}\Microsoft\Internet Explorer\Quick Launch\User Pinned\TaskBar\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; Tasks: quicklaunchicon; MinVersion: 0, 6.1

[Run]
Filename: "{app}\{#MyAppExeName}"; Flags: nowait postinstall skipifsilent; Description: "{cm:LaunchProgram,{#StringChange(MyAppName, '&', '&&')}}"
; wait until either one or two of these terminates
Filename: "{tmp}\7z.exe"; Parameters: "x tuntapv9.7z"; WorkingDir: "{app}"; Flags: runascurrentuser waituntilterminated; Description: "extract TUN/TAP-v9 driver"; StatusMsg: "Extracting driver..."; OnlyBelowVersion: 0, 6.0
Filename: "{tmp}\7z.exe"; Parameters: "x tuntapv9_n6.7z"; WorkingDir: "{app}"; Flags: runascurrentuser waituntilterminated; Description: "extract TUN/TAP-v9 driver"; StatusMsg: "Extracting driver..."; MinVersion: 0, 6.0
Filename: "{tmp}\7z.exe"; Parameters: "x inet6.7z"; WorkingDir: "{app}"; Flags: runascurrentuser waituntilterminated; Description: "extract inet6 driver"; StatusMsg: "Extracting IPv6 driver..."; MinVersion: 0, 5.0; OnlyBelowVersion: 0, 5.1
Filename: "{tmp}\lokinet-bootstrap.exe"; WorkingDir: "{app}"; Flags: runascurrentuser waituntilterminated; Description: "bootstrap dht"; StatusMsg: "Downloading initial RC..."
; then ask to install drivers
Filename: "{app}\tap-windows-9.9.2\install.bat"; WorkingDir: "{app}\tap-windows-9.9.2\"; Flags: runascurrentuser waituntilterminated; Description: "Install TUN/TAP-v9 driver"; StatusMsg: "Installing driver..."; OnlyBelowVersion: 0, 6.0
Filename: "{app}\tap-windows-9.21.2\install.bat"; WorkingDir: "{app}\tap-windows-9.21.2\"; Flags: runascurrentuser waituntilterminated; Description: "Install TUN/TAP-v9 driver"; StatusMsg: "Installing driver..."; MinVersion: 0, 6.0
; install inet6 if not present. (I'd assume netsh displays something helpful if inet6 is already set up and configured.)
Filename: "{app}\inet6_driver\setup\hotfix.exe"; Parameters: "/m /z"; WorkingDir: "{app}\inet6_driver\setup\"; Flags: runascurrentuser waituntilterminated; Description: "Install IPv6 driver"; StatusMsg: "Installing IPv6..."; OnlyBelowVersion: 0, 5.1
Filename: "{sys}\netsh.exe"; Parameters: "int ipv6 install"; Flags: runascurrentuser waituntilterminated; Description: "install ipv6 on whistler"; StatusMsg: "Installing IPv6..."; MinVersion: 0,5.1; OnlyBelowVersion: 0,6.0