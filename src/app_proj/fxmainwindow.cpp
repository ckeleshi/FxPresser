#include "fxmainwindow.h"
#include <Psapi.h>
#include <QApplication>
#include <QButtonGroup>
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QMessageBox>
#include <QPainter>
#include <QStyledItemDelegate>

#pragma comment(lib, "psapi.lib")

// =============================================================================
//  FxMainWindow 实现
//
//  工作流程：
//   - 扫描类名为 QQSwordWinClass 的窗口，筛选其进程为 qqffo.exe 的窗口；
//   - 截取每个窗口固定区域（角色名）得到图片，计算 MD5 作为特征值；
//   - 以特征值匹配上次使用过的角色，从而自动选中对应窗口；
//   - 定时器每 50ms 调用 pressProc()，按各技能的间隔/全局间隔向窗口投递按键消息。
// =============================================================================

// 角色名取样区域（相对于游戏窗口客户区的像素矩形）
static const QRect playerNameRect{80, 22, 90, 14};

// 下拉框项代理：让角色名截图按下拉框宽度等比展示
class CharacterBoxDelegate : public QStyledItemDelegate
{
  public:
    CharacterBoxDelegate(QObject *parent = nullptr) : QStyledItemDelegate(parent)
    {
    }

    // 绘制每一项时，把装饰图标（角色名截图）宽度撑满整个条目宽度
    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override
    {
        auto o = option;
        initStyleOption(&o, index);
        o.decorationSize.setWidth(o.rect.width());
        auto style = o.widget ? o.widget->style() : QApplication::style();
        style->drawControl(QStyle::CE_ItemViewItem, &o, painter, o.widget);
    }
};

// 构造函数：搭建界面 -> 读取配置 -> 扫描窗口 -> 自动选中 -> 启动按键定时器
FxMainWindow::FxMainWindow(QWidget *parent) : QMainWindow(parent)
{
    setupUI();

    connect(&pressTimer, &QTimer::timeout, this, &FxMainWindow::pressProc);

    QDir dir = QCoreApplication::applicationDirPath();
    dir.mkdir(QStringLiteral("config")); // 确保配置目录存在

    // 读取参数
    loadConfig();

    // 扫描游戏窗口
    scanGameWindows();

    // 首次自动选择游戏窗口
    autoSelectAndRenameGameWindow(currentHash);

    // 使用精确定时器，保证按键间隔相对准确
    pressTimer.setTimerType(Qt::PreciseTimer);
    pressTimer.start(50);
}

// 析构函数：停止定时器并保存当前配置
FxMainWindow::~FxMainWindow()
{
    pressTimer.stop();

    autoWriteConfig();
}

// 根据保存的角色名特征值，在已扫描到的窗口中查找匹配项并选中；
// 找到后立即把标题栏文本框的内容应用到该游戏窗口。
void FxMainWindow::autoSelectAndRenameGameWindow(const QByteArray &hash)
{
    int index = -1;

    if (!gameWindows.isEmpty())
    {
        if (!hash.isEmpty())
        {
            for (int hash_index = 0; hash_index < playerNameImages.size(); ++hash_index)
            {
                if (playerNameHashes[hash_index] == hash)
                {
                    index = hash_index;
                    break;
                }
            }
        }
    }

    combo_windows->setCurrentIndex(index);

    // 找到窗口之后自动更改窗口标题
    if (index != -1)
    {
        changeWindowTitle();
    }
}

// 定时器回调：全局开关打开时，先触发一次缺省技能，再轮询其余已启用的技能
void FxMainWindow::pressProc()
{
    // 全局开关未打开则不做任何事
    if (!check_global_switch->isChecked())
    {
        return;
    }

    int window_index = combo_windows->currentIndex();

    // 没有选中任何游戏窗口
    if (window_index == -1)
    {
        return;
    }

    // 缺省技能（如平A）：每个开关周期只强制触发一次
    if (currentDefaultKey != -1 && !defaultKeyTriggered && key_checks[currentDefaultKey]->isChecked())
    {
        tryPressKey(gameWindows[window_index], currentDefaultKey, true);
        defaultKeyTriggered = true;
    }

    // 其余技能按各自间隔与全局间隔判断是否触发
    for (int key_index = 0; key_index < 10; ++key_index)
    {
        if (key_index == currentDefaultKey || !key_checks[key_index]->isChecked())
        {
            continue;
        }

        tryPressKey(gameWindows[window_index], key_index, false);
    }
}

// 记录某个按键本次触发的时间点
void FxMainWindow::resetTimeStamp(int index)
{
    lastPressedTimePoint[index] = std::chrono::steady_clock::now();
}

// 重置所有时间戳（用于下次立即触发）
void FxMainWindow::resetAllTimeStamps()
{
    // 为了实现点击全局开关时自动触发一次，此处将每个按键的上次时间设为0
    lastPressedTimePoint.fill(std::chrono::steady_clock::time_point());
    lastAnyPressedTimePoint = std::chrono::steady_clock::time_point();
}

// 扫描所有 QQ 游戏窗口，筛选出 qqffo.exe 的窗口并采集角色名截图与特征值
void FxMainWindow::scanGameWindows()
{
    wchar_t c_string[512];

    int found = 0, invalid = 0;

    gameWindows.clear();
    playerNameImages.clear();
    playerNameHashes.clear();
    combo_windows->clear();
    check_global_switch->setChecked(false); // 重新扫描后关闭全局开关，避免误按

    HWND hWindow = FindWindowW(L"QQSwordWinClass", nullptr); // 暂不知道是不是FO/FFO独有类名

    // 批量增删下拉框项时先屏蔽信号，避免反复触发 currentIndexChanged
    combo_windows->blockSignals(true);

    while (hWindow != nullptr)
    {
        // 通过窗口句柄取得所属进程的可执行文件名，从而区分 FO 与 FFO
        DWORD pid;
        GetWindowThreadProcessId(hWindow, &pid);
        HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, pid);
        GetProcessImageFileNameW(hProcess, c_string, 512);
        CloseHandle(hProcess);

        if (QString::fromWCharArray(c_string).endsWith(
                QStringLiteral("\\qqffo.exe"))) // 此处要跟FO区分，不区分则两游戏通用。
        {
            QImage playerNameImage = getGamePicture(hWindow, playerNameRect);

            if (!playerNameImage.isNull())
            {
                ++found;
                gameWindows.push_back(hWindow);
                playerNameHashes.push_back(imageHash(playerNameImage));
                playerNameImages.push_back(playerNameImage);
                combo_windows->addItem(QIcon(QPixmap::fromImage(playerNameImage)), nullptr);
            }
            else
            {
                ++invalid; // 窗口不可见/最小化等原因导致截图失败
            }
        }

        hWindow = FindWindowExW(nullptr, hWindow, L"QQSwordWinClass", nullptr);
    }

    combo_windows->blockSignals(false);
}

// 将标题栏文本框的内容设置为当前选中游戏窗口的标题
void FxMainWindow::changeWindowTitle()
{
    int window_index = combo_windows->currentIndex();

    if (window_index == -1)
    {
        return;
    }

    QString text = line_title->text();

    if (!text.isEmpty())
    {
        SetWindowTextW(gameWindows[window_index], text.toStdWString().c_str());
    }
}

// 判断某个按键是否满足间隔条件，满足（或 force 为真）时真正发送按键
void FxMainWindow::tryPressKey(HWND window, int key_index, bool force)
{
    auto nowTimePoint = std::chrono::steady_clock::now();

    // 距该按键上次触发的时间
    std::chrono::milliseconds differFromSelf =
        std::chrono::duration_cast<std::chrono::milliseconds>(nowTimePoint - lastPressedTimePoint[key_index]);
    // 距任意按键上次触发的时间（用于全局排队间隔）
    std::chrono::milliseconds differFromAny =
        std::chrono::duration_cast<std::chrono::milliseconds>(nowTimePoint - lastAnyPressedTimePoint);
    // 该按键自身的间隔要求
    std::chrono::milliseconds selfInterval(static_cast<long long>(key_intervals[key_index]->value() * 1000));
    // 全局间隔要求
    std::chrono::milliseconds anyInterval(static_cast<long long>(spin_global_interval->value() * 1000));

    if (force || (differFromSelf >= selfInterval && differFromAny >= anyInterval))
    {
        lastPressedTimePoint[key_index] = nowTimePoint;
        lastAnyPressedTimePoint         = nowTimePoint;

        pressKey(window, VK_F1 + key_index);
    }
}

// 向指定窗口投递一次按键（按下+抬起），使用 PostMessage 不阻塞本进程
void FxMainWindow::pressKey(HWND window, UINT code)
{
    PostMessageA(window, WM_KEYDOWN, code, 0);
    PostMessageA(window, WM_KEYUP, code, 0);
}

// 对窗口指定区域截图，返回 RGB888 格式的 QImage（失败返回空图片）
QImage FxMainWindow::getGamePicture(HWND window, QRect rect)
{
    std::vector<uchar> pixelBuffer;
    QImage             result;

    BITMAPINFO b;

    // 窗口无效或已最小化时无法截图
    if ((IsWindow(window) == FALSE) || (IsIconic(window) == TRUE))
        return QImage();

    // 描述目标位图：24 位色、自下而上的 BI_RGB
    b.bmiHeader.biSize          = sizeof(BITMAPINFOHEADER);
    b.bmiHeader.biWidth         = rect.width();
    b.bmiHeader.biHeight        = rect.height();
    b.bmiHeader.biPlanes        = 1;
    b.bmiHeader.biBitCount      = 3 * 8;
    b.bmiHeader.biCompression   = BI_RGB;
    b.bmiHeader.biSizeImage     = 0;
    b.bmiHeader.biXPelsPerMeter = 0;
    b.bmiHeader.biYPelsPerMeter = 0;
    b.bmiHeader.biClrUsed       = 0;
    b.bmiHeader.biClrImportant  = 0;
    b.bmiColors[0].rgbBlue      = 8;
    b.bmiColors[0].rgbGreen     = 8;
    b.bmiColors[0].rgbRed       = 8;
    b.bmiColors[0].rgbReserved  = 0;

    // 抓取窗口客户区画面到内存 DC
    HDC dc  = GetDC(window);
    HDC cdc = CreateCompatibleDC(dc);

    HBITMAP hBitmap = CreateCompatibleBitmap(dc, rect.width(), rect.height());
    SelectObject(cdc, hBitmap);

    BitBlt(cdc, 0, 0, rect.width(), rect.height(), dc, rect.left(), rect.top(), SRCCOPY);
    pixelBuffer.resize(rect.width() * rect.height() * 4);
    GetDIBits(cdc, hBitmap, 0, rect.height(), pixelBuffer.data(), &b, DIB_RGB_COLORS);
    DeleteObject(hBitmap);

    DeleteDC(cdc);
    ReleaseDC(window, dc);

    // DIB 为 BGR 且自下而上，这里交换红蓝通道并镜像翻转，得到正常的 RGB888 图片
    return QImage(pixelBuffer.data(), rect.width(), rect.height(), (rect.width() * 3 + 3) & (~3), QImage::Format_RGB888)
        .rgbSwapped()
        .mirrored();
}

// 计算配置文件路径：exe所在目录/config/<exe文件名>.json
QString FxMainWindow::getConfigPath()
{
    // exe目录/config/exe文件名.json
    auto dirp = QCoreApplication::applicationDirPath();
    auto exep = QCoreApplication::applicationFilePath();

    return (dirp + "/config/%1.json").arg(exep.mid(dirp.length() + 1, exep.length() - dirp.length() - 5));
}

// 从 JSON 文件读取配置，任何失败情况都回退到默认配置
SConfigData FxMainWindow::readConfig(const QString &filename)
{
    QFile         file;
    QJsonDocument doc;
    QJsonObject   root;

    file.setFileName(filename);
    if (!file.open(QIODevice::Text | QIODevice::ReadOnly))
    {
        return SConfigData();
    }

    doc = QJsonDocument::fromJson(file.readAll());
    if (doc.isNull())
    {
        return SConfigData();
    }

    return jsonToConfig(doc.object());
}

// 将配置以缩进格式的 JSON 写入文件
void FxMainWindow::writeConfig(const QString &filename, const SConfigData &config)
{
    QFile         file;
    QJsonObject   root;
    QJsonDocument doc;

    file.setFileName(filename);
    if (!file.open(QIODevice::Text | QIODevice::WriteOnly | QIODevice::Truncate))
    {
        return;
    }

    root = configToJson(config);
    doc.setObject(root);
    file.write(doc.toJson(QJsonDocument::Indented));
}

// 加载配置文件并应用到界面
void FxMainWindow::loadConfig()
{
    applyConfigToUI(readConfig(getConfigPath()));
}

// 将界面当前状态保存到配置文件
void FxMainWindow::autoWriteConfig()
{
    writeConfig(getConfigPath(), makeConfigFromUI());
}

// 从界面控件收集数据，构造配置结构
SConfigData FxMainWindow::makeConfigFromUI()
{
    SConfigData result;

    for (int index = 0; index < 10; ++index)
    {
        result.fxSwitch[index] = key_checks[index]->isChecked();
        result.fxCD[index]     = key_intervals[index]->value();
    }

    result.globalInterval = spin_global_interval->value();
    result.defaultKey     = currentDefaultKey;

    result.hash  = currentHash;
    result.title = line_title->text();

    // 记录窗口当前位置，下次启动时恢复
    auto rect = geometry();

    result.x = rect.x();
    result.y = rect.y();

    return result;
}

// 将配置数据填充到界面控件
void FxMainWindow::applyConfigToUI(const SConfigData &config)
{
    for (int index = 0; index < 10; ++index)
    {
        key_checks[index]->setChecked(config.fxSwitch[index]);
        key_intervals[index]->setValue(config.fxCD[index]);
    }

    spin_global_interval->setValue(config.globalInterval);

    // 缺省技能：仅勾选对应的那一个
    currentDefaultKey = config.defaultKey;
    for (int index = 0; index < 10; ++index)
    {
        key_defaults[index]->setChecked(index == config.defaultKey);
    }

    currentHash = config.hash;
    line_title->setText(config.title);

    // 恢复窗口位置（仅在配置中有有效坐标时）
    auto rect = geometry();

    if (config.x != -1 && config.y != -1)
    {
        setGeometry(config.x, config.y, rect.width(), rect.height());
    }
}

// 将默认配置应用到界面（首次运行或配置损坏时使用）
void FxMainWindow::applyDefaultConfigToUI()
{
    applyConfigToUI(SConfigData());
}

// 将配置结构序列化为 JSON 对象
QJsonObject FxMainWindow::configToJson(const SConfigData &config)
{
    QJsonObject result;
    QJsonArray  pressArray;
    QJsonObject supplyObject;

    // AutoPress：10 个技能各自的启用状态与间隔
    for (int index = 0; index < 10; index++)
    {
        QJsonObject keyObject;
        keyObject[QStringLiteral("Enabled")]  = config.fxSwitch[index];
        keyObject[QStringLiteral("Interval")] = config.fxCD[index];
        pressArray.append(keyObject);
    }
    result[QStringLiteral("AutoPress")] = pressArray;

    result["Interval"]   = config.globalInterval;
    result["DefaultKey"] = config.defaultKey;

    result["X"] = config.x;
    result["Y"] = config.y;

    result["Title"] = config.title;
    result["Hash"]  = QString::fromUtf8(config.hash);

    return result;
}

// 将 JSON 对象反序列化为配置结构（缺失字段使用默认值）
SConfigData FxMainWindow::jsonToConfig(QJsonObject json)
{
    SConfigData result;
    QJsonArray  pressArray;
    QJsonObject supplyObject;

    pressArray = json.take(QStringLiteral("AutoPress")).toArray();

    // 数组长度必须为 10 才认为有效
    if (pressArray.size() == 10)
    {
        for (int index = 0; index < 10; index++)
        {
            QJsonObject keyObject  = pressArray[index].toObject();
            result.fxSwitch[index] = keyObject.take(QStringLiteral("Enabled")).toBool(false);
            result.fxCD[index]     = keyObject.take(QStringLiteral("Interval")).toDouble(1.0);
        }
    }

    result.globalInterval = json.take("Interval").toDouble(0.8);
    result.defaultKey     = json.take("DefaultKey").toInt(-1);

    result.x = json.take("X").toInt(-1);
    result.y = json.take("Y").toInt(-1);

    result.title = json.take("Title").toString("");
    result.hash  = json.take("Hash").toString("").toUtf8();

    return result;
}

// 计算角色名图片的特征值：将 QImage 经 QDataStream 序列化后取 MD5 并转为 Base64
QByteArray FxMainWindow::imageHash(QImage image)
{
    if (image.isNull() || image.format() != QImage::Format_RGB888)
        return QByteArray();

    QByteArray  imageBytes;
    QDataStream stream(&imageBytes, QIODevice::WriteOnly);

    stream << image;

    return QCryptographicHash::hash(imageBytes, QCryptographicHash::Md5).toBase64();
}

// 构建整个主界面：扫描按钮、窗口下拉框、标题栏、全局开关与 10 个技能设置
void FxMainWindow::setupUI()
{
    // 生成一条水平分隔线的辅助 lambda
    auto get_h_line = []() {
        auto line = new QFrame;

        line->setFrameShape(QFrame::HLine);
        line->setFrameShadow(QFrame::Sunken);
        line->setLineWidth(1);

        return line;
    };

    // 全局开关使用的加粗大字体
    QFont switch_font;
    switch_font.setFamily(QStringLiteral("微软雅黑"));
    switch_font.setPointSize(20);
    switch_font.setBold(true);

    QStringList supply_keys;

    for (int index = 0; index < 10; ++index)
    {
        supply_keys << QString("F%1").arg(index + 1);
    }

    auto main_widget  = new QWidget;
    auto vlayout_main = new QVBoxLayout;

    // ---- 扫描按钮：重新扫描并尝试自动选中上次的角色 ----
    btn_scan = new QPushButton(QStringLiteral("扫描游戏窗口"));
    connect(btn_scan, &QPushButton::clicked, [this]() {
        scanGameWindows();

        if (!gameWindows.isEmpty())
            autoSelectAndRenameGameWindow(currentHash);
    });
    vlayout_main->addWidget(btn_scan);

    // ---- 窗口下拉框：每项显示该角色名的截图 ----
    combo_windows = new QComboBox;
    combo_windows->setIconSize(playerNameRect.size());
    combo_windows->setItemDelegate(new CharacterBoxDelegate);
    connect(combo_windows, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), [this](int index) {
        // 切换角色时关闭全局开关，避免在新角色上误按
        check_global_switch->setChecked(false);

        if (index != -1)
        {
            currentHash = playerNameHashes[index];
        }
    });
    vlayout_main->addWidget(combo_windows);

    // ---- 窗口标题输入与修改按钮 ----
    line_title         = new QLineEdit;
    auto hlayout_title = new QHBoxLayout;
    hlayout_title->addWidget(new QLabel(QStringLiteral("窗口标题")));
    hlayout_title->addWidget(line_title, 1);
    vlayout_main->addLayout(hlayout_title);

    btn_change_title = new QPushButton(QStringLiteral("修改窗口标题"));
    connect(btn_change_title, &QPushButton::clicked, this, &FxMainWindow::changeWindowTitle);
    vlayout_main->addWidget(btn_change_title);

    // ---- 切换到游戏窗口：把游戏窗口置前，方便手动操作 ----
    btn_switch_to_window = new QPushButton(QStringLiteral("切换到游戏窗口"));
    connect(btn_switch_to_window, &QPushButton::clicked, [this]() {
        int window_index = combo_windows->currentIndex();

        if (window_index == -1)
        {
            return;
        }

        SetForegroundWindow(gameWindows[window_index]);
    });
    vlayout_main->addWidget(btn_switch_to_window);

    vlayout_main->addWidget(get_h_line());

    // ---- 全局开关：开启后立即重置计时，使缺省技能马上触发一次 ----
    check_global_switch = new QCheckBox(QStringLiteral("全局开关"));
    check_global_switch->setFont(switch_font);
    connect(check_global_switch, &QCheckBox::toggled, [this](bool checked) {
        if (checked)
        {
            defaultKeyTriggered = false;
            resetAllTimeStamps();
        }
    });
    auto hlayout_switch = new QHBoxLayout;
    hlayout_switch->addStretch();
    hlayout_switch->addWidget(check_global_switch);
    hlayout_switch->addStretch();
    vlayout_main->addLayout(hlayout_switch);

    // ---- 全局间隔：任意两次按键之间的最小时间差 ----
    spin_global_interval = new QDoubleSpinBox;
    spin_global_interval->setSuffix(" s");
    spin_global_interval->setDecimals(2);
    spin_global_interval->setMinimum(0.1);
    spin_global_interval->setMaximum(365.0);
    spin_global_interval->setSingleStep(0.01);
    spin_global_interval->setValue(0.8);
    auto hlayout_press_interval = new QHBoxLayout;
    hlayout_press_interval->addStretch();
    hlayout_press_interval->addWidget(new QLabel(QStringLiteral("全局间隔")));
    hlayout_press_interval->addWidget(spin_global_interval);
    hlayout_press_interval->addStretch();
    vlayout_main->addLayout(hlayout_press_interval);
    vlayout_main->addWidget(get_h_line());

    // ---- 技能表格：每行一个技能（F1~F10），列为 启用/间隔/缺省 ----
    auto gridlayout_keys = new QGridLayout;

    // gridlayout_keys尽可能紧凑
    gridlayout_keys->setSpacing(0);

    gridlayout_keys->addWidget(new QLabel(QStringLiteral("启用")), 0, 0);
    gridlayout_keys->addWidget(new QLabel(QStringLiteral("间隔")), 0, 1);
    gridlayout_keys->addWidget(new QLabel(QStringLiteral("缺省")), 0, 2);

    for (int index = 0; index < 10; ++index)
    {
        auto check_key         = new QCheckBox(QString("F%1").arg(index + 1));
        auto spin_key_interval = new QDoubleSpinBox;
        auto check_default     = new QCheckBox;

        spin_key_interval->setSuffix(" s");
        spin_key_interval->setDecimals(1);
        spin_key_interval->setMinimum(0.1);
        spin_key_interval->setMaximum(365.0);
        spin_key_interval->setSingleStep(0.1);
        spin_key_interval->setValue(1.0);
        key_checks[index]    = check_key;
        key_intervals[index] = spin_key_interval;
        key_defaults[index]  = check_default;

        // 启用开关：勾选时才可编辑间隔；切换后重置该键计时
        connect(check_key, &QCheckBox::toggled, [this, index](bool checked) {
            key_intervals[index]->setEnabled(!checked);
            resetTimeStamp(index);
        });

        // 缺省技能为单选：勾选一个会取消其它；允许全部不选
        connect(check_default, &QCheckBox::toggled, [this, index](bool checked) {
            // 模拟QButtonGroup互斥，并能够全部取消选择
            if (checked)
            {
                currentDefaultKey = index;

                for (int key_index = 0; key_index < 10; ++key_index)
                {
                    if (key_index != index)
                        key_defaults[key_index]->setChecked(false);
                }
            }
            else
            {
                currentDefaultKey = -1;
            }
        });

        gridlayout_keys->addWidget(check_key, index + 1, 0);
        gridlayout_keys->addWidget(spin_key_interval, index + 1, 1);
        gridlayout_keys->addWidget(check_default, index + 1, 2);
    }

    vlayout_main->addLayout(gridlayout_keys);

    // 固定窗口尺寸，避免用户拖动改变布局
    main_widget->setLayout(vlayout_main);
    this->setCentralWidget(main_widget);
    this->setFixedSize(minimumSize());
}
