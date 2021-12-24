#include "FxMainWindow.h"

#include <QApplication>

int main(int argc, char *argv[])
{
    QFont fnt(QStringLiteral("΢���ź�"),10);
    QApplication a(argc, argv);
    QApplication::setFont(fnt);
    FxMainWindow w;

    //��ȡ�������ļ�����Ȩ��
    HANDLE hToken;
    TOKEN_PRIVILEGES tp;
    OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES, &hToken);
    tp.PrivilegeCount = 1;
    LookupPrivilegeValueW(nullptr, SE_DEBUG_NAME, &tp.Privileges[0].Luid);
    tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
    AdjustTokenPrivileges(hToken, FALSE, &tp, sizeof(tp), nullptr, nullptr);
    CloseHandle(hToken);

    w.show();
    return a.exec();
}
