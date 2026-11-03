# -*- coding: utf-8 -*-
"""生成 Inno Setup 中文语言包（UTF-8 带 BOM，直接可读）。

源文件 ChineseSimplified.source.utf8.isl 是 UTF-8 中文原文，
本脚本做三件事：
  1. 删除 Inno Setup 6.x 已废弃的过时消息（编译会有 warning）
  2. 补齐 6.2+ 新增但 6.1.0 语言包里缺失的消息
  3. 以 UTF-8 BOM + CRLF 输出（Inno 6 编译器按 BOM 识别编码）
"""
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
SOURCE = os.path.join(HERE, 'ChineseSimplified.source.utf8.isl')
TARGET = os.path.join(HERE, 'ChineseSimplified.isl')

# Inno Setup 6.2+ 新增的消息，本语言包源自 6.1.0，需补齐
EXTRA_MESSAGES = {
    "DiskSpaceGBLabel": "需要 %1 GB 磁盘空间",
    "ComponentsDiskSpaceGBLabel": "需要 %1 GB 磁盘空间",
    "ReadyLabel2a": "点击「安装」继续安装程序。",
    "ReadyLabel2b": "点击「安装」开始安装程序。",
    "DownloadingLabel2": "正在下载...",
    "ButtonStopDownload": "停止下载",
    "StopDownload": "正在停止下载...",
    "ErrorDownloadAborted": "下载已中止。",
    "ErrorDownloadFailed": "下载失败。",
    "ErrorDownloadSizeFailed": "下载大小检查失败。",
    "ErrorProgress": "进度错误：%1",
    "ErrorFileSize": "文件大小错误。",
    "ExtractingLabel": "正在解压文件...",
    "ButtonStopExtraction": "停止解压",
    "StopExtraction": "正在停止解压...",
    "ErrorExtractionAborted": "解压已中止。",
    "ErrorExtractionFailed": "解压失败。",
    "ArchiveIncorrectPassword": "压缩包密码不正确。",
    "ArchiveIsCorrupted": "压缩包已损坏。",
    "ArchiveUnsupportedFormat": "不支持的压缩包格式。",
    "PrepareToInstallNeedsRestart": "安装程序准备安装 %1。安装过程中需要重启计算机。是否现在继续？",
    "RunEntryExec": "正在运行: %1",
    "RunEntryShellExec": "正在执行: %1",
    "RetryCancelSelectAction": "请选择要执行的操作。",
    "RetryCancelRetry": "请重试。",
    "RetryCancelCancel": "取消安装。",
    "StatusDownloadFiles": "正在下载文件...",
    "SourceVerificationFailed": "源文件验证失败。",
    "VerificationSignatureDoesntExist": "签名不存在。",
    "VerificationSignatureInvalid": "签名无效。",
    "VerificationKeyNotFound": "未找到验证密钥。",
    "VerificationFileNameIncorrect": "文件名不正确。",
    "VerificationFileTagIncorrect": "文件标记不正确。",
    "VerificationFileSizeIncorrect": "文件大小不正确。",
    "VerificationFileHashIncorrect": "文件哈希值不正确。",
    "ExistingFileReadOnly2": "文件 %1 是只读文件。",
    "ExistingFileReadOnlyRetry": "请重试。",
    "ExistingFileReadOnlyKeepExisting": "保留现有文件。",
    "ErrorReadingExistingDest": "读取现有文件时出错。",
    "FileExistsSelectAction": "请选择要执行的操作。",
    "FileExists2": "文件 %1 已存在。",
    "FileExistsOverwriteExisting": "覆盖现有文件。",
    "FileExistsKeepExisting": "保留现有文件。",
    "FileExistsOverwriteOrKeepAll": "全部覆盖或保留。",
    "ExistingFileNewerSelectAction": "请选择要执行的操作。",
    "ExistingFileNewer2": "文件 %1 的安装版本比现有版本更新。",
    "ExistingFileNewerOverwriteExisting": "覆盖现有文件。",
    "ExistingFileNewerKeepExisting": "保留现有文件。",
    "ExistingFileNewerOverwriteOrKeepAll": "全部覆盖或保留。",
    "ErrorDownloading": "下载时出错。",
    "ErrorExtracting": "解压时出错。",
    "ErrorRestartReplace": "重启替换文件时出错。",
    "ErrorRenamingTemp": "重命名临时文件时出错。",
}

# Inno Setup 6.x 已移除的过时消息名（6.1.0 语言包里还留着）
OBSOLETE = {
    "ReadyLabel2",
    "RunEntry",
    "ExistingFileReadOnly",
    "ErrorReadingExistingFile",
    "FileExists",
    "ExistingFileNewer",
    "UninstallRestarting",
    "UninstallShuttingDown",
    "UninstallStatusLabel2",
    "UninstallStatusLabel3",
    "UninstallStatusLabel4",
    "UninstallStatusLabel5",
}


def main():
    with open(SOURCE, encoding='utf-8') as f:
        src = f.read()

    out = []
    section = None
    leftover = dict(EXTRA_MESSAGES)
    for raw in src.split('\n'):
        line = raw.rstrip('\r')
        stripped = line.strip()

        if stripped.startswith('[') and stripped.endswith(']'):
            section = stripped[1:-1]
            out.append(line)
            continue

        if '=' in line and not stripped.startswith(';'):
            key = line.split('=', 1)[0].strip()
            if key in OBSOLETE:
                continue
            # [Messages] 段里已有同名旧译的，替换为新版本译法
            if section == 'Messages' and key in leftover:
                line = '%s=%s' % (key, leftover.pop(key))

        out.append(line)

    # 源文件里完全没有的新消息，追加到 [Messages] 段末尾
    if leftover:
        try:
            idx = out.index('[CustomMessages]')
        except ValueError:
            idx = len(out)
        extra = ['', '; --- 补充 Inno Setup 6.2+ 新增消息 ---']
        extra += ['%s=%s' % (k, v) for k, v in leftover.items()]
        out[idx:idx] = extra

    final = '\r\n'.join(out).rstrip('\r\n') + '\r\n'

    # UTF-8 带 BOM：Inno 6 编译器靠 BOM 识别编码，原子写入避免半截文件
    tmp = TARGET + '.tmp'
    with open(tmp, 'wb') as f:
        f.write(b'\xef\xbb\xbf')
        f.write(final.encode('utf-8'))
    os.replace(tmp, TARGET)

    data = open(TARGET, 'rb').read()
    assert data[:3] == b'\xef\xbb\xbf', '缺少 UTF-8 BOM'
    print('生成 %s' % TARGET)
    print('字节数   : %d' % len(data))
    i = data.find('LanguageName='.encode('utf-8'))
    print('样例     : %s' % data[i:i + 40].decode('utf-8', errors='replace'))
    j = data.find('ArchiveIsCorrupted='.encode('utf-8'))
    print('补充样例 : %s' % data[j:j + 60].decode('utf-8', errors='replace'))
    return 0


if __name__ == '__main__':
    sys.exit(main())
