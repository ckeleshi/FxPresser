#ifndef MAINWINDOW_H
#define MAINWINDOW_H

// =============================================================================
//  FxMainWindow —— 外置魔手（自动按键工具）主界面
//
//  功能概述：
//   1. 扫描游戏（qqffo.exe）窗口，并对窗口内的角色名区域截图生成特征值(hash)；
//   2. 通过特征值自动匹配并选中上次使用的游戏窗口，可修改该窗口标题；
//   3. 提供 F1~F10 共 10 个技能的启用开关、施放间隔与缺省技能设置；
//   4. 通过定时器向游戏窗口投递 WM_KEYDOWN/WM_KEYUP 以实现自动按键；
//   5. 配置以 JSON 形式保存在 exe 目录/config/<exe名>.json。
// =============================================================================

#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QDoubleSpinBox>
#include <QHBoxLayout>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QLabel>
#include <QLineEdit>
#include <QMainWindow>
#include <QPushButton>
#include <QSpacerItem>
#include <QTimer>
#include <QVector>

#include <Windows.h>

#include <array>
#include <chrono>
#include <utility>

// 配置文件对应的数据结构（保存到 JSON，键名见 configToJson/jsonToConfig）
struct SConfigData
{
    bool   fxSwitch[10];   // Fx开关
    double fxCD[10];       // 单个按键的间隔
    double globalInterval; // Fx排队间隔
    int    defaultKey;     // 缺省技能的位置，没有则为-1

    QString    title; // 游戏窗口标题
    QByteArray hash;  // 角色名图片hash

    int x, y; // 程序窗口位置

    // 构造时给出各字段的默认值（与用户首次运行时的缺省表现一致）
    SConfigData()
    {
        std::fill(fxSwitch, fxSwitch + 10, false);
        std::fill(fxCD, fxCD + 10, 1.0);
        globalInterval = 0.8;
        defaultKey     = -1;

        x = -1;
        y = -1;
    }
};

// 主窗口类：负责界面搭建、配置读写、游戏窗口扫描与按键调度
class FxMainWindow : public QMainWindow
{
    Q_OBJECT

  public:
    FxMainWindow(QWidget *parent = nullptr);
    ~FxMainWindow();

  private:
    // ---- 界面控件 ----
    QPushButton                     *btn_scan;             // 「扫描游戏窗口」按钮
    QComboBox                       *combo_windows;        // 游戏窗口下拉框（显示角色名截图）
    QLineEdit                       *line_title;           // 要设置的窗口标题输入框
    QPushButton                     *btn_change_title;     // 「修改窗口标题」按钮
    QPushButton                     *btn_switch_to_window; // 「切换到游戏窗口」按钮
    QCheckBox                       *check_global_switch;  // 「全局开关」复选框
    QDoubleSpinBox                  *spin_global_interval; // 「全局间隔」数值框
    std::array<QCheckBox *, 10>      key_checks;           // F1~F10 启用复选框
    std::array<QDoubleSpinBox *, 10> key_intervals;        // F1~F10 间隔数值框
    std::array<QCheckBox *, 10>      key_defaults;         // F1~F10 缺省技能复选框

    // 搭建界面（仅在构造函数中调用一次）
    void setupUI();

    // 获取图片特征值
    static QByteArray imageHash(QImage image);

    // 计时部分
    QTimer pressTimer; // 固定间隔按键的计时器

    // 每个按键上次触发的时间点，用于计算单个按键的间隔
    std::array<std::chrono::steady_clock::time_point, 10> lastPressedTimePoint;
    // 最后一次按键的时间点，用于确定实际按键的时机
    std::chrono::steady_clock::time_point                 lastAnyPressedTimePoint;

    // 扫描到的游戏窗口数据（三个容器按同一索引一一对应）
    QVector<HWND>       gameWindows;      // 游戏窗口句柄
    QVector<QImage>     playerNameImages; // 角色名区域截图
    QVector<QByteArray> playerNameHashes; // 角色名区域截图特征值

    // 当前游戏窗口的数据
    QByteArray currentHash;         // 当前选中窗口的角色名特征值（写入配置）
    int        currentDefaultKey;   // 当前缺省技能索引，-1 表示未设置
    bool       defaultKeyTriggered; // 本次开启全局开关后是否已触发过缺省技能

    // 在启动的时候运行一次，根据保存的hash查找对应游戏窗口并设置窗口标题
    void autoSelectAndRenameGameWindow(const QByteArray &hash);

    // 定时器执行的函数
    void pressProc();

    // 重置按键的计时
    void resetTimeStamp(int index);
    void resetAllTimeStamps();

    // 扫描游戏窗口
    void scanGameWindows();

    // 将 line_title 中的文本设置为当前游戏窗口的标题
    void changeWindowTitle();

    // 尝试执行某个按键
    void tryPressKey(HWND window, int key_index, bool force);
    // 执行某个按键
    void pressKey(HWND window, UINT code);

    // 截取游戏窗口的某个区域
    static QImage getGamePicture(HWND window, QRect rect);

    // 计算配置文件路径
    QString getConfigPath();

    // 读取配置文件
    SConfigData readConfig(const QString &filename);
    // 写入配置文件
    void        writeConfig(const QString &filename, const SConfigData &config);
    // 加载配置文件到UI
    void        loadConfig();
    // 保存当前配置
    void        autoWriteConfig();
    // 从UI生成配置
    SConfigData makeConfigFromUI();
    // 将配置应用到UI
    void        applyConfigToUI(const SConfigData &config);
    // 应用默认配置
    void        applyDefaultConfigToUI();

    // 读写json配置文件用
    QJsonObject configToJson(const SConfigData &config);
    SConfigData jsonToConfig(QJsonObject json);
};
#endif // MAINWINDOW_H
