#include "FxMainWindow.h"

#include <QApplication>

// 程序入口：创建 Qt 应用与主窗口，并提升自身权限以便读取游戏进程信息。
int main(int argc, char *argv[])
{
    // 全局统一使用「微软雅黑」10 号字体
    QFont        fnt(QStringLiteral("微软雅黑"), 10);
    QApplication a(argc, argv);
    QApplication::setFont(fnt);

    // 主窗口（内部负责界面搭建、配置读写、窗口扫描与按键定时器）
    FxMainWindow w;

    // 获取读进程文件名的权限
    // 说明：扫描游戏窗口时需要通过进程句柄获取其可执行文件路径，
    // 因此需为当前进程开启 SE_DEBUG_NAME 特权（进程已通过清单以管理员身份运行）。
    HANDLE           hToken;
    TOKEN_PRIVILEGES tp;
    OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES, &hToken);
    tp.PrivilegeCount = 1;
    LookupPrivilegeValue(nullptr, SE_DEBUG_NAME, &tp.Privileges[0].Luid);
    tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
    AdjustTokenPrivileges(hToken, FALSE, &tp, sizeof(tp), nullptr, nullptr);
    CloseHandle(hToken);

    // 显示主窗口并进入 Qt 事件循环
    w.show();
    return a.exec();
}
