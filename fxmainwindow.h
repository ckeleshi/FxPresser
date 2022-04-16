#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QVector>
#include <QTimer>
#include <QImage>
#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QLabel>
#include <QComboBox>
#include <QSpacerItem>
#include <QPushButton>
#include <QLineEdit>
#include <QHBoxLayout>
#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QJsonArray>

#include <Windows.h>

#include <array>
#include <utility>
#include <chrono>

struct SConfigData
{
    bool fxSwitch[10]; //Fx开关
    double fxCD[10]; //单个按键的间隔
    double globalInterval; //Fx排队间隔
    int defaultKey; //缺省技能的位置，没有则为-1

    QString title; //游戏窗口标题
    QByteArray hash; //角色名图片hash

    int x, y; //程序窗口位置

    SConfigData()
    {
        std::fill(fxSwitch, fxSwitch + 10, false);
        std::fill(fxCD, fxCD + 10, 1.0);
        globalInterval = 0.8;
        defaultKey = -1;

        x = -1;
        y = -1;
    }
};

class FxMainWindow : public QMainWindow
{
    Q_OBJECT

public:
    FxMainWindow(QWidget* parent = nullptr);
    ~FxMainWindow();

private:
    QPushButton* btn_scan;
    QComboBox* combo_windows;
    QLineEdit* line_title;
    QPushButton* btn_change_title;
    QPushButton* btn_switch_to_window;
    QCheckBox* check_global_switch;
    QDoubleSpinBox* spin_global_interval;
    std::array<QCheckBox*, 10> key_checks;
    std::array<QDoubleSpinBox*, 10> key_intervals;
    std::array<QCheckBox*, 10> key_defaults;

    void setupUI();

    //获取图片特征值
    static QByteArray imageHash(QImage image);

    //计时部分
    QTimer pressTimer; //固定间隔按键的计时器

    //每个按键上次触发的时间点，用于计算单个按键的间隔
    std::array<std::chrono::steady_clock::time_point, 10> lastPressedTimePoint;
    //最后一次按键的时间点，用于确定实际按键的时机
    std::chrono::steady_clock::time_point lastAnyPressedTimePoint;

    //扫描到的游戏窗口数据
    QVector<HWND> gameWindows;
    QVector<QImage> playerNameImages;
    QVector<QByteArray> playerNameHashes;

    //当前游戏窗口的数据
    QByteArray currentHash;
    int currentDefaultKey;
    bool defaultKeyTriggered;

    //在启动的时候运行一次，根据保存的hash查找对应游戏窗口并设置窗口标题
    void autoSelectAndRenameGameWindow(const QByteArray& hash);

    //定时器执行的函数
    void pressProc();

    //重置按键的计时
    void resetTimeStamp(int index);
    void resetAllTimeStamps();

    //扫描游戏窗口
    void scanGameWindows();

    void changeWindowTitle();

    //尝试执行某个按键
    void tryPressKey(HWND window, int key_index, bool force);
    //执行某个按键
    void pressKey(HWND window, UINT code);

    //截取游戏窗口的某个区域
    static QImage getGamePicture(HWND window, QRect rect);

    //计算?配置文件的路径
    QString getConfigPath();

    //读取配置文件
    SConfigData readConfig(const QString& filename);
    //写入配置文件
    void writeConfig(const QString& filename, const SConfigData& config);
    //加载配置文件到UI
    void loadConfig();
    //保存当前配置
    void autoWriteConfig();
    //从UI生成配置
    SConfigData makeConfigFromUI();
    //将配置应用到UI
    void applyConfigToUI(const SConfigData& config);
    //应用默认配置
    void applyDefaultConfigToUI();

    //读写json配置文件用
    QJsonObject configToJson(const SConfigData& config);
    SConfigData jsonToConfig(QJsonObject json);
};
#endif // MAINWINDOW_H
