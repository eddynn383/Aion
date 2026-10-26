; Installer-ul Windows (Inno Setup 6). Îl folosește scripts/package-windows.ps1, după windeployqt.
; Parametri: /DAppVersion=0.1.0 /DSourceDir=<folderul cu Aion.exe și DLL-urile> /DOutputDir=<dist>

#ifndef AppVersion
  #define AppVersion "0.1.0"
#endif

[Setup]
AppId={{6B0F4E7A-3C1D-4D8E-9A51-AE10A10A10A1}
AppName=Aion
AppVersion={#AppVersion}
AppPublisher=Aion
AppPublisherURL=https://github.com/eddynn383/Aion
DefaultDirName={autopf}\Aion
DefaultGroupName=Aion
UninstallDisplayIcon={app}\Aion.exe
OutputDir={#OutputDir}
OutputBaseFilename=Aion-{#AppVersion}-setup
Compression=lzma2
SolidCompression=yes
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
MinVersion=10.0
WizardStyle=modern
ChangesAssociations=yes
PrivilegesRequiredOverridesAllowed=dialog

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked
Name: "association"; Description: "Deschide fișierele .aion cu Aion"; GroupDescription: "Asocieri:"

[Files]
Source: "{#SourceDir}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{group}\Aion"; Filename: "{app}\Aion.exe"
Name: "{autodesktop}\Aion"; Filename: "{app}\Aion.exe"; Tasks: desktopicon

; Asocierea .aion → Aion (în HKCU pentru instalarea per utilizator, HKLM pentru toți).
[Registry]
Root: HKA; Subkey: "Software\Classes\.aion"; ValueType: string; ValueName: ""; ValueData: "Aion.Model"; Flags: uninsdeletevalue; Tasks: association
Root: HKA; Subkey: "Software\Classes\Aion.Model"; ValueType: string; ValueName: ""; ValueData: "Model Aion"; Flags: uninsdeletekey; Tasks: association
Root: HKA; Subkey: "Software\Classes\Aion.Model\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: "{app}\Aion.exe,0"; Tasks: association
Root: HKA; Subkey: "Software\Classes\Aion.Model\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\Aion.exe"" ""%1"""; Tasks: association

[Run]
Filename: "{app}\Aion.exe"; Description: "{cm:LaunchProgram,Aion}"; Flags: nowait postinstall skipifsilent
