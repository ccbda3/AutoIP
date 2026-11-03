; AutoIP 安装包脚本
; 编译: ISCC.exe AutoIP.iss
; 依赖: stage\ 目录下是 windeployqt 部署好的完整运行库

#define AppName "AutoIP"
#define AppNameCN "AutoIP 自动配IP工具"
#define AppVersion "1.1.0"
#define AppPublisher "AutoIP"
#define AppExeName "AutoIP.exe"
#define AppUrl "https://github.com/ccbda3/AutoIP"

[Setup]
AppId={{7C3B9E14-2A5D-4F86-9C21-B0E5A7D43F92}
AppName={#AppNameCN}
AppVersion={#AppVersion}
AppVerName={#AppNameCN} {#AppVersion}
AppPublisher={#AppPublisher}
AppPublisherURL={#AppUrl}
AppSupportURL={#AppUrl}
AppUpdatesURL={#AppUrl}
DefaultDirName={autopf}\{#AppName}
DefaultGroupName={#AppNameCN}
UninstallDisplayName={#AppNameCN}
UninstallDisplayIcon={app}\{#AppExeName}
VersionInfoVersion={#AppVersion}
VersionInfoCompany={#AppPublisher}
VersionInfoDescription={#AppNameCN} 安装程序
VersionInfoProductName={#AppNameCN}
VersionInfoProductVersion={#AppVersion}
SetupIconFile=..\AutoIPQt\autoip.ico
OutputDir=..\dist
OutputBaseFilename=AutoIP-Setup-{#AppVersion}-x64
Compression=lzma2/max
SolidCompression=yes
WizardStyle=modern
PrivilegesRequired=admin
PrivilegesRequiredOverridesAllowed=dialog
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
DisableProgramGroupPage=yes
MinVersion=10.0
ShowLanguageDialog=no
AllowNoIcons=yes
LicenseFile=..\LICENSE
CloseApplications=yes
RestartApplications=no

[Languages]
Name: "chinesesimp"; MessagesFile: "ChineseSimplified.isl"
Name: "english"; MessagesFile: "compiler:Default.isl"

[CustomMessages]
chinesesimp.VerifyingSetup=正在验证安装程序...
chinesesimp.SelectDir=请选择安装位置
chinesesimp.SelectDirDesc=AutoIP 将安装到以下文件夹：
chinesesimp.Ready=准备安装
chinesesimp.ReadyDesc=即将开始安装 AutoIP 到：
chinesesimp.Finished=安装完成
chinesesimp.FinishedDesc=AutoIP 已安装到 {0}
chinesesimp.Installing=正在安装
chinesesimp.InstallingDesc=正在复制文件，请稍候...
english.VerifyingSetup=Verifying setup...
english.SelectDir=Select destination folder
english.SelectDirDesc=AutoIP will be installed into the following folder:
english.Ready=Ready to install
english.ReadyDesc=Setup is ready to install AutoIP into:
english.Finished=Setup completed
english.FinishedDesc=AutoIP has been installed into {0}
english.Installing=Installing
english.InstallingDesc=Please wait while Setup copies files...

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked

[Files]
Source: "stage\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{group}\{#AppNameCN}"; Filename: "{app}\{#AppExeName}"; WorkingDir: "{app}"; IconFilename: "{app}\{#AppExeName}"
Name: "{autodesktop}\{#AppNameCN}"; Filename: "{app}\{#AppExeName}"; WorkingDir: "{app}"; IconFilename: "{app}\{#AppExeName}"; Tasks: desktopicon

[Run]
Filename: "{app}\{#AppExeName}"; Description: "{cm:LaunchProgram,{#AppNameCN}}"; Flags: nowait postinstall skipifsilent

[UninstallDelete]
Type: filesandordirs; Name: "{app}"

[Code]
{ 兼容旧版卸载残留 }
procedure CurUninstallStepChanged(CurUninstallStep: TUninstallStep);
begin
  if CurUninstallStep = usPostUninstall then
  begin
    { 清理可能残留的旧版备份文件 }
    DeleteFile(ExpandConstant('{app}\{#AppExeName}.bak-20261027'));
  end;
end;
