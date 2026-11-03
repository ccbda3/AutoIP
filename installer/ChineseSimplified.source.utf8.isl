; *** Inno Setup version 6.1.0+ Chinese Simplified messages ***
;
; To download user-contributed translations of this file, go to:
;   https://jrsoftware.org/files/istrans/
;
; Note: When translating this text, do not add periods (.) to the end of
; messages that didn't have them already, because on those messages Inno
; Setup adds the periods automatically (appending a period would result in
; two periods being displayed).

[LangOptions]
LanguageName=简体中文
LanguageID=$0804
LanguageCodePage=936

[Messages]

; *** Application titles
SetupAppTitle=安装程序
SetupWindowTitle=安装程序 - %1
UninstallAppTitle=卸载程序
UninstallAppFullTitle=%1 卸载程序

; *** Misc. common
InformationTitle=信息
ConfirmTitle=确认
ErrorTitle=错误

; *** SetupLdr messages
SetupLdrStartupMessage=这将安装 %1。您想要继续吗？
LdrCannotCreateTemp=无法创建临时文件。安装程序已中止
LdrCannotExecTemp=无法执行临时目录中的文件。安装程序已中止
HelpTextNote=

; *** Startup error messages
LastErrorMessage=%1。%n%n错误 %2: %3
SetupFileMissing=安装目录中缺少文件 %1。请修正该问题或获取一个新的程序副本。
SetupFileCorrupt=安装文件已损坏。请获取一个新的程序副本。
SetupFileCorruptOrWrongVer=安装文件已损坏，或与该安装程序版本不兼容。请修正该问题或获取一个新的程序副本。
InvalidParameter=命令行出现无效参数：%n%n%1
SetupAlreadyRunning=安装程序正在运行。
WindowsVersionNotSupported=此程序不支持您的 Windows 版本。
WindowsServicePackRequired=此程序需要 %1 Service Pack %2 或更高版本。
NotOnThisPlatform=此程序无法在 %1 上运行。
OnlyOnThisPlatform=此程序必须在 %1 上运行。
OnlyOnTheseArchitectures=此程序只能安装到为以下处理器架构设计的 Windows 版本中：%n%n%1
WinVersionTooLowError=此程序需要 %1 版本 %2 或更高版本。
WinVersionTooHighError=此程序不能安装于 %1 版本 %2 或更高版本中。
AdminPrivilegesRequired=您必须以管理员身份登录才能安装此程序。
PowerUserPrivilegesRequired=您必须以管理员身份或有权限的用户组成员身份登录才能安装此程序。
SetupAppRunningError=安装程序检测到 %1 正在运行。%n%n请先关闭所有实例，然后单击“确定”继续，或单击“取消”退出。
UninstallAppRunningError=卸载程序检测到 %1 正在运行。%n%n请先关闭所有实例，然后单击“确定”继续，或单击“取消”退出。

; *** Startup questions
PrivilegesRequiredOverrideTitle=选择安装模式
PrivilegesRequiredOverrideInstruction=选择安装模式
PrivilegesRequiredOverrideText1=%1 可以为所有用户安装（需要管理员权限），或仅为当前用户安装。
PrivilegesRequiredOverrideText2=%1 可以仅为当前用户安装，或为所有用户安装（需要管理员权限）。
PrivilegesRequiredOverrideAllUsers=为所有用户安装(&A)
PrivilegesRequiredOverrideAllUsersRecommended=为所有用户安装(&A)（建议）
PrivilegesRequiredOverrideCurrentUser=仅为当前用户安装(&M)
PrivilegesRequiredOverrideCurrentUserRecommended=仅为当前用户安装(&M)（建议）

; *** Misc. errors
ErrorCreatingDir=安装程序无法创建目录 "%1"
ErrorTooManyFilesInDir=无法在目录 "%1" 中创建文件，因为该目录包含太多文件

; *** Setup common messages
ExitSetupTitle=退出安装程序
ExitSetupMessage=安装程序尚未完成。如果现在退出，将不会安装该程序。%n%n您可以在稍后再次运行安装程序完成安装。%n%n是否退出安装程序？
AboutSetupMenuItem=关于安装程序(&A)...
AboutSetupTitle=关于安装程序
AboutSetupMessage=%1 版本 %2%n%3%n%n%1 主页：%n%4
AboutSetupNote=
TranslatorNote=

; *** Buttons
ButtonBack=< 上一步(&B)
ButtonNext=下一步(&N) >
ButtonInstall=安装(&I)
ButtonOK=确定
ButtonCancel=取消
ButtonYes=是(&Y)
ButtonYesToAll=全部是(&A)
ButtonNo=否(&N)
ButtonNoToAll=全部否(&O)
ButtonFinish=完成(&F)
ButtonBrowse=浏览(&B)...
ButtonWizardBrowse=浏览(&R)...
ButtonNewFolder=新建文件夹(&M)

; *** "Select Language" dialog messages
SelectLanguageTitle=选择安装语言
SelectLanguageLabel=选择安装时要使用的语言。

; *** Common wizard text
ClickNext=单击“下一步”继续，或单击“取消”退出安装程序。
BeveledLabel=
BrowseDialogTitle=浏览文件夹
BrowseDialogLabel=在下面的列表中选择一个文件夹，然后单击“确定”。
NewFolderName=新建文件夹

; *** "Welcome" wizard page
WelcomeLabel1=欢迎使用 [name] 安装向导
WelcomeLabel2=本向导将指导您完成 [name] 的安装。%n%n建议在继续之前关闭其他应用程序。

; *** "Password" wizard page
WizardPassword=密码
PasswordLabel1=此安装程序受密码保护。
PasswordLabel3=请输入密码，然后单击“下一步”继续。密码区分大小写。
PasswordEditLabel=密码(&P)：
IncorrectPassword=输入的密码不正确。请重试。

; *** "License Agreement" wizard page
WizardLicense=许可协议
LicenseLabel=请阅读以下许可协议。
LicenseLabel3=请阅读以下许可协议。您必须接受协议条款才能继续安装。
LicenseAccepted=我接受协议(&A)
LicenseNotAccepted=我不接受协议(&D)

; *** "Information" wizard pages
WizardInfoBefore=信息
InfoBeforeLabel=请在继续安装之前阅读以下重要信息。
InfoBeforeClickLabel=准备好继续安装后，请单击“下一步”。
WizardInfoAfter=信息
InfoAfterLabel=请在继续安装之前阅读以下重要信息。
InfoAfterClickLabel=准备好继续安装后，请单击“下一步”。

; *** "User Information" wizard page
WizardUserInfo=用户信息
UserInfoDesc=请输入您的信息。
UserInfoName=用户名(&U)：
UserInfoOrg=组织(&O)：
UserInfoSerial=序列号(&S)：
UserInfoNameRequired=您必须输入用户名。

; *** "Select Destination Location" wizard page
WizardSelectDir=选择目标位置
SelectDirDesc=[name] 将安装到哪个文件夹？
SelectDirLabel3=[name] 将安装到以下文件夹。%n%n要继续，请单击“下一步”。如果要选择其他文件夹，请单击“浏览”。
SelectDirBrowseLabel=要继续，请单击“下一步”。如果要选择其他文件夹，请单击“浏览”。
DiskSpaceMBLabel=至少需要 [mb] MB 的可用磁盘空间。
CannotInstallToNetworkDrive=安装程序无法安装到网络驱动器。
CannotInstallToUNCPath=安装程序无法安装到 UNC 路径。
InvalidPath=您必须输入带驱动器号的完整路径，例如：%n%nC:\APP%n%n或 UNC 路径，例如：%n%n\\server\share
InvalidDrive=您选择的驱动器或 UNC 共享不存在或无法访问。请选择其他位置。
DiskSpaceWarningTitle=磁盘空间不足
DiskSpaceWarning=安装程序需要至少 %1 KB 的可用空间，但所选驱动器仅有 %2 KB 可用。%n%n您仍要继续吗？
DirNameTooLong=文件夹名称或路径太长。
InvalidDirName=文件夹名称无效。
BadDirName32=文件夹名称不能包含以下任何字符：%n%n%1
DirExistsTitle=文件夹已存在
DirExists=文件夹：%n%n%1%n%n已经存在。您仍要安装到该文件夹吗？
DirDoesntExistTitle=文件夹不存在
DirDoesntExist=文件夹：%n%n%1%n%n不存在。您要创建该文件夹吗？

; *** "Select Components" wizard page
WizardSelectComponents=选择组件
SelectComponentsDesc=要安装哪些组件？
SelectComponentsLabel2=选择要安装的组件；清除不想安装的组件。准备好继续时，请单击“下一步”。
FullInstallation=完全安装
CompactInstallation=简洁安装
CustomInstallation=自定义安装
NoUninstallWarningTitle=组件已存在
NoUninstallWarning=安装程序检测到以下组件已安装在您的计算机上：%n%n%1%n%n不卸载这些组件将不会安装它们。%n%n您仍要继续吗？
ComponentSize1=%1 KB
ComponentSize2=%1 MB
ComponentsDiskSpaceMBLabel=当前选择需要至少 [mb] MB 的磁盘空间。

; *** "Select Additional Tasks" wizard page
WizardSelectTasks=选择附加任务
SelectTasksDesc=要执行哪些附加任务？
SelectTasksLabel2=选择安装程序安装 [name] 时要执行的附加任务，然后单击“下一步”。

; *** "Select Start Menu Folder" wizard page
WizardSelectProgramGroup=选择开始菜单文件夹
SelectStartMenuFolderDesc=安装程序应将程序的快捷方式放在哪里？
SelectStartMenuFolderLabel3=[name] 的快捷方式将放在以下开始菜单文件夹中。%n%n要继续，请单击“下一步”。如果要选择其他文件夹，请单击“浏览”。
SelectStartMenuFolderBrowseLabel=要继续，请单击“下一步”。如果要选择其他文件夹，请单击“浏览”。
MustEnterGroupName=您必须输入文件夹名称。
GroupNameTooLong=文件夹名称或路径太长。
InvalidGroupName=文件夹名称无效。
BadGroupName=文件夹名称不能包含以下任何字符：%n%n%1
NoProgramGroupCheck2=不创建开始菜单文件夹(&D)

; *** "Ready to Install" wizard page
WizardReady=准备安装
ReadyLabel1=安装程序现在准备开始安装 [name] 到您的计算机。
ReadyLabel2=单击“安装”继续安装，或单击“上一步”查看或更改设置。
ReadyMemoUserInfo=用户信息：
ReadyMemoDir=目标位置：
ReadyMemoType=安装类型：
ReadyMemoComponents=所选组件：
ReadyMemoGroup=开始菜单文件夹：
ReadyMemoTasks=附加任务：

; *** "Preparing to Install" wizard page
WizardPreparing=准备安装
PreparingDesc=安装程序正在准备安装 [name] 到您的计算机。
PreviousInstallNotCompleted=先前程序的安装/卸载未完成。您需要重新启动计算机才能完成该安装。%n%n重新启动计算机后，请再次运行安装程序完成 [name] 的安装。
CannotContinue=安装程序无法继续。请单击“取消”退出。
ApplicationsFound=以下应用程序正在使用将由安装程序更新的文件。建议您允许安装程序自动关闭这些应用程序。
ApplicationsFound2=以下应用程序正在使用将由安装程序更新的文件。建议您允许安装程序自动关闭这些应用程序。安装完成后，安装程序将尝试重新启动这些应用程序。
CloseApplications=&自动关闭应用程序
CloseApplications=自动关闭应用程序
DontCloseApplications=&不关闭应用程序
ErrorCloseApplications=安装程序无法自动关闭所有应用程序。建议您在继续之前关闭所有使用需要由安装程序更新的文件的应用程序。

; *** "Installing" wizard page
WizardInstalling=正在安装
InstallingLabel=安装程序正在安装 [name] 到您的计算机，请稍候。

; *** "Setup Completed" wizard page
FinishedHeadingLabel=完成 [name] 安装向导
FinishedLabelNoIcons=安装程序已完成 [name] 的安装。
FinishedLabel=安装程序已完成 [name] 的安装。可以通过选择已安装的快捷方式运行该应用程序。
ClickFinish=单击“完成”退出安装程序。
FinishedRestartLabel=要完成 [name] 的安装，安装程序必须重新启动您的计算机。您想立即重新启动吗？
FinishedRestartMessage=要完成 [name] 的安装，安装程序必须重新启动您的计算机。%n%n您想立即重新启动吗？
ShowReadmeCheck=是，我想查看 README 文件
YesRadio=是，立即重新启动计算机(&Y)
NoRadio=否，稍后重新启动计算机(&N)
RunEntry=运行 %1

; *** "Setup Needs the Next Disk" stuff
ChangeDiskTitle=安装程序需要下一张磁盘
SelectDiskLabel2=请插入磁盘 %1，然后单击“确定”。%n%n如果该磁盘上的文件可以在下面显示的文件夹以外的文件夹中找到，请输入正确的路径或单击“浏览”。
PathLabel=路径(&P)：
FileNotInDir2=文件 "%1" 无法在 "%2" 中找到。请插入正确的磁盘或选择其他文件夹。
SelectDirectoryLabel=请指定下一张磁盘的位置。

; *** Installation phase messages
SetupAborted=安装未完成。%n%n请修正问题并重试。
AbortRetryIgnoreSelectAction=选择操作
AbortRetryIgnoreRetry=重试(&T)
AbortRetryIgnoreIgnore=忽略错误并继续(&I)
AbortRetryIgnoreCancel=取消安装

; *** Installation status messages
StatusClosingApplications=正在关闭应用程序...
StatusCreateDirs=正在创建目录...
StatusExtractFiles=正在提取文件...
StatusCreateIcons=正在创建快捷方式...
StatusCreateIniEntries=正在创建 INI 条目...
StatusCreateRegistryEntries=正在创建注册表条目...
StatusRegisterFiles=正在注册文件...
StatusSavingUninstall=正在保存卸载信息...
StatusRunProgram=正在完成安装...
StatusRestartingApplications=正在重新启动应用程序...
StatusRollback=正在回滚更改...

; *** Misc. errors
ErrorInternal2=内部错误：%1
ErrorFunctionFailedNoCode=%1 失败
ErrorFunctionFailed=%1 失败；代码 %2
ErrorFunctionFailedWithMessage=%1 失败；代码 %2。%n%3
ErrorExecutingProgram=无法执行文件：%n%1

; *** Registry errors
ErrorRegOpenKey=打开注册表项时出错：%n%1\%2
ErrorRegCreateKey=创建注册表项时出错：%n%1\%2
ErrorRegWriteKey=写入注册表项时出错：%n%1\%2

; *** INI errors
ErrorIniEntry=在文件 "%1" 中创建 INI 条目时出错。

; *** File copying errors
FileAbortRetryIgnoreSkipNotRecommended=跳过此文件（不推荐）(&S)
FileAbortRetryIgnoreIgnoreNotRecommended=忽略错误并继续（不推荐）(&I)
SourceIsCorrupted=源文件已损坏
SourceDoesntExist=源文件 "%1" 不存在
ExistingFileReadOnly=现有文件被标记为只读。%n%n单击“重试”删除只读属性并重试，单击“忽略”跳过此文件，或单击“中止”取消安装。
ErrorReadingExistingFile=读取现有文件时出错：%n%n%1
FileExists=文件已存在。%n%n您要覆盖它吗？
ExistingFileNewer=现有文件比安装程序试图安装的文件更新。建议您保留现有文件。%n%n您要保留现有文件吗？
ErrorChangingAttr=更改现有文件的属性时出错：%n%n%1
ErrorCreatingTemp=创建临时文件时出错：%n%n%1
ErrorReadingSource=读取源文件时出错：%n%n%1
ErrorCopying=复制文件时出错：%n%n%1
ErrorReplacingExistingFile=替换现有文件时出错：%n%n%1
ErrorRegisterServer=无法注册 DLL/OCX：%n%n%1
ErrorRegSvr32Failed=RegSvr32 失败，退出代码 %1
ErrorRegisterTypeLib=无法注册类型库：%n%n%1

; *** Uninstall display name markings
UninstallDisplayNameMark=%1 (%2)
UninstallDisplayNameMarks=%1 (%2, %3)
UninstallDisplayNameMark32Bit=32 位
UninstallDisplayNameMark64Bit=64 位
UninstallDisplayNameMarkAllUsers=所有用户
UninstallDisplayNameMarkCurrentUser=当前用户

; *** Post-installation errors
ErrorOpeningReadme=打开 README 文件时出错。
ErrorRestartingComputer=安装程序无法重新启动计算机。请手动重新启动。

; *** Uninstaller messages
UninstallNotFound=文件 "%1" 不存在。无法卸载。
UninstallOpenError=无法打开文件 "%1"。无法卸载。
UninstallUnsupportedVer=此版本的卸载程序无法识别卸载日志文件 "%1" 的格式。无法卸载。
UninstallUnknownEntry=卸载日志中遇到未知条目 (%1)
ConfirmUninstall=您确定要完全删除 %1 及其所有组件吗？
UninstallOnlyOnWin64=此安装只能在 64 位 Windows 上卸载。
OnlyAdminCanUninstall=只有具有管理员权限的用户才能卸载此安装。
UninstallStatusLabel=正在从您的计算机删除 %1，请稍候。
UninstalledAll=%1 已从您的计算机成功删除。
UninstalledMost=%1 卸载完成。%n%n某些元素无法删除。您可以手动删除它们。
UninstalledAndNeedsRestart=要完成 %1 的卸载，必须重新启动计算机。%n%n您想立即重新启动吗？
UninstallDataCorrupted=文件 "%1" 已损坏。无法卸载。

; *** Uninstallation phase messages
ConfirmDeleteSharedFileTitle=删除共享文件？
ConfirmDeleteSharedFile2=系统指示以下共享文件不再被任何程序使用。您要删除此共享文件吗？%n%n如果删除后仍有程序在使用此文件，这些程序可能无法正常运行。如果您不确定，请选择否。在系统中保留此文件不会导致任何损害。
SharedFileNameLabel=文件名：
SharedFileLocationLabel=位置：
WizardUninstalling=卸载状态
StatusUninstalling=正在卸载 %1...

; *** The new contents of the Uninstall page with the new Uninstall features
UninstallRestarting=正在重新启动
UninstallShuttingDown=正在关闭
UninstallStatusLabel=正在完成 %1 的卸载
UninstallStatusLabel2=请稍候...
UninstallStatusLabel3=不要关闭计算机...
UninstallStatusLabel4=正在清理...
UninstallStatusLabel5=正在回滚...

; *** Shutdown block reasons
ShutdownBlockReasonInstallingApp=正在安装 %1。
ShutdownBlockReasonUninstallingApp=正在卸载 %1。

; The custom messages below aren't used by Setup itself, but if you make
; use of them in your scripts, you'll want to translate them.

[CustomMessages]

NameAndVersion=%1 版本 %2
AdditionalIcons=附加快捷方式：
CreateDesktopIcon=创建桌面快捷方式(&D)
CreateQuickLaunchIcon=创建快速启动栏快捷方式(&Q)
ProgramOnTheWeb=%1 网站
UninstallProgram=卸载 %1
LaunchProgram=运行 %1
AssocFileExtension=将 %1 与 %2 文件扩展名关联(&A)
AssocingFileExtension=正在将 %1 与 %2 文件扩展名关联...
AutoStartProgramGroupDescription=启动：
AutoStartProgram=自动启动 %1
AddonHostProgramNotFound=%1 找不到。%n%n您想继续安装吗？
