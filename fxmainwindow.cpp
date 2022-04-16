#include "fxmainwindow.h"
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QMessageBox>
#include <QStyledItemDelegate>
#include <QPainter>
#include <QCryptographicHash>
#include <QApplication>
#include <QButtonGroup>
#include <Psapi.h>
#pragma comment(lib, "psapi.lib")

//角色名取样区域
static const QRect playerNameRect{ 80,22,90,14 };

class CharacterBoxDelegate : public QStyledItemDelegate
{
public:
    CharacterBoxDelegate(QObject* parent = nullptr)
        : QStyledItemDelegate(parent) {}

    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override
    {
        auto o = option;
        initStyleOption(&o, index);
        o.decorationSize.setWidth(o.rect.width());
        auto style = o.widget ? o.widget->style() : QApplication::style();
        style->drawControl(QStyle::CE_ItemViewItem, &o, painter, o.widget);
    }
};

FxMainWindow::FxMainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    setupUI();

    connect(&pressTimer, &QTimer::timeout, this, &FxMainWindow::pressProc);

    QDir dir = QCoreApplication::applicationDirPath();
    dir.mkdir(QStringLiteral("config"));

    //读取参数
    loadConfig();

    //扫描游戏窗口
    scanGameWindows();

    //首次自动选择游戏窗口
    autoSelectAndRenameGameWindow(currentHash);

    pressTimer.setTimerType(Qt::PreciseTimer);
    pressTimer.start(50);
}

FxMainWindow::~FxMainWindow()
{
    pressTimer.stop();

    autoWriteConfig();
}

void FxMainWindow::autoSelectAndRenameGameWindow(const QByteArray& hash)
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

    //找到窗口之后自动更改窗口标题
    if (index != -1)
    {
        changeWindowTitle();
    }
}

void FxMainWindow::pressProc()
{
    if (!check_global_switch->isChecked())
    {
        return;
    }

    int window_index = combo_windows->currentIndex();

    if (window_index == -1)
    {
        return;
    }

    if (currentDefaultKey != -1 && !defaultKeyTriggered && key_checks[currentDefaultKey]->isChecked())
    {
        tryPressKey(gameWindows[window_index], currentDefaultKey, true);
        defaultKeyTriggered = true;
    }

    for (int key_index = 0; key_index < 10; ++key_index)
    {
        if (key_index == currentDefaultKey || !key_checks[key_index]->isChecked())
        {
            continue;
        }

        tryPressKey(gameWindows[window_index], key_index, false);
    }
}

void FxMainWindow::resetTimeStamp(int index)
{
    lastPressedTimePoint[index] = std::chrono::steady_clock::now();
}

void FxMainWindow::resetAllTimeStamps()
{
    //为了实现点击全局开关时自动触发一次，此处将每个按键的上次时间设为0
    lastPressedTimePoint.fill(std::chrono::steady_clock::time_point());
    lastAnyPressedTimePoint = std::chrono::steady_clock::time_point();
}

void FxMainWindow::scanGameWindows()
{
    wchar_t c_string[512];

    int found = 0, invalid = 0;

    gameWindows.clear();
    playerNameImages.clear();
    playerNameHashes.clear();
    combo_windows->clear();
    check_global_switch->setChecked(false);

    HWND hWindow = FindWindowW(L"QQSwordWinClass", nullptr); //暂不知道是不是FO/FFO独有类名

    combo_windows->blockSignals(true);

    while (hWindow != nullptr)
    {
        DWORD pid;
        GetWindowThreadProcessId(hWindow, &pid);
        HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, pid);
        GetProcessImageFileNameW(hProcess, c_string, 512);
        CloseHandle(hProcess);

        if (QString::fromWCharArray(c_string).endsWith(QStringLiteral("\\qqffo.exe"))) //此处要跟FO区分，不区分则两游戏通用。
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
                ++invalid;
            }
        }

        hWindow = FindWindowExW(nullptr, hWindow, L"QQSwordWinClass", nullptr);
    }

    combo_windows->blockSignals(false);
}

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

void FxMainWindow::tryPressKey(HWND window, int key_index, bool force)
{
    auto nowTimePoint = std::chrono::steady_clock::now();

    std::chrono::milliseconds differFromSelf = std::chrono::duration_cast<std::chrono::milliseconds>(nowTimePoint - lastPressedTimePoint[key_index]);
    std::chrono::milliseconds differFromAny = std::chrono::duration_cast<std::chrono::milliseconds>(nowTimePoint - lastAnyPressedTimePoint);
    std::chrono::milliseconds selfInterval(static_cast<long long>(key_intervals[key_index]->value() * 1000));
    std::chrono::milliseconds anyInterval(static_cast<long long>(spin_global_interval->value() * 1000));

    if (force || (differFromSelf >= selfInterval && differFromAny >= anyInterval))
    {
        lastPressedTimePoint[key_index] = nowTimePoint;
        lastAnyPressedTimePoint = nowTimePoint;

        pressKey(window, VK_F1 + key_index);
    }
}

void FxMainWindow::pressKey(HWND window, UINT code)
{
    PostMessageA(window, WM_KEYDOWN, code, 0);
    PostMessageA(window, WM_KEYUP, code, 0);
}

QImage FxMainWindow::getGamePicture(HWND window, QRect rect)
{
    std::vector<uchar> pixelBuffer;
    QImage result;

    BITMAPINFO b;

    if ((IsWindow(window) == FALSE) || (IsIconic(window) == TRUE))
        return QImage();

    b.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    b.bmiHeader.biWidth = rect.width();
    b.bmiHeader.biHeight = rect.height();
    b.bmiHeader.biPlanes = 1;
    b.bmiHeader.biBitCount = 3 * 8;
    b.bmiHeader.biCompression = BI_RGB;
    b.bmiHeader.biSizeImage = 0;
    b.bmiHeader.biXPelsPerMeter = 0;
    b.bmiHeader.biYPelsPerMeter = 0;
    b.bmiHeader.biClrUsed = 0;
    b.bmiHeader.biClrImportant = 0;
    b.bmiColors[0].rgbBlue = 8;
    b.bmiColors[0].rgbGreen = 8;
    b.bmiColors[0].rgbRed = 8;
    b.bmiColors[0].rgbReserved = 0;

    HDC dc = GetDC(window);
    HDC cdc = CreateCompatibleDC(dc);

    HBITMAP hBitmap = CreateCompatibleBitmap(dc, rect.width(), rect.height());
    SelectObject(cdc, hBitmap);

    BitBlt(cdc, 0, 0, rect.width(), rect.height(), dc, rect.left(), rect.top(), SRCCOPY);
    pixelBuffer.resize(rect.width() * rect.height() * 4);
    GetDIBits(cdc, hBitmap, 0, rect.height(), pixelBuffer.data(), &b, DIB_RGB_COLORS);
    DeleteObject(hBitmap);

    DeleteDC(cdc);
    ReleaseDC(window, dc);

    return QImage(pixelBuffer.data(), rect.width(), rect.height(), (rect.width() * 3 + 3) & (~3), QImage::Format_RGB888).rgbSwapped().mirrored();
}

QString FxMainWindow::getConfigPath()
{
    //exe目录/config/exe文件名.json
    auto dirp = QCoreApplication::applicationDirPath();
    auto exep = QCoreApplication::applicationFilePath();

    return (dirp + "/config/%1.json").arg(exep.mid(dirp.length() + 1, exep.length() - dirp.length() - 5));
}

SConfigData FxMainWindow::readConfig(const QString& filename)
{
    QFile file;
    QJsonDocument doc;
    QJsonObject root;

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

void FxMainWindow::writeConfig(const QString& filename, const SConfigData& config)
{
    QFile file;
    QJsonObject root;
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

void FxMainWindow::loadConfig()
{
    applyConfigToUI(readConfig(getConfigPath()));
}

void FxMainWindow::autoWriteConfig()
{
    writeConfig(getConfigPath(), makeConfigFromUI());
}

SConfigData FxMainWindow::makeConfigFromUI()
{
    SConfigData result;

    for (int index = 0; index < 10; ++index)
    {
        result.fxSwitch[index] = key_checks[index]->isChecked();
        result.fxCD[index] = key_intervals[index]->value();
    }

    result.globalInterval = spin_global_interval->value();
    result.defaultKey = currentDefaultKey;

    result.hash = currentHash;
    result.title = line_title->text();

    auto rect = geometry();

    result.x = rect.x();
    result.y = rect.y();

    return result;
}

void FxMainWindow::applyConfigToUI(const SConfigData& config)
{
    for (int index = 0; index < 10; ++index)
    {
        key_checks[index]->setChecked(config.fxSwitch[index]);
        key_intervals[index]->setValue(config.fxCD[index]);
    }

    spin_global_interval->setValue(config.globalInterval);

    currentDefaultKey = config.defaultKey;
    for (int index = 0; index < 10; ++index)
    {
        key_defaults[index]->setChecked(index == config.defaultKey);
    }

    currentHash = config.hash;
    line_title->setText(config.title);

    auto rect = geometry();

    if (config.x != -1 && config.y != -1)
    {
        setGeometry(config.x, config.y, rect.width(), rect.height());
    }
}

void FxMainWindow::applyDefaultConfigToUI()
{
    applyConfigToUI(SConfigData());
}

QJsonObject FxMainWindow::configToJson(const SConfigData& config)
{
    QJsonObject result;
    QJsonArray pressArray;
    QJsonObject supplyObject;

    for (int index = 0; index < 10; index++)
    {
        QJsonObject keyObject;
        keyObject[QStringLiteral("Enabled")] = config.fxSwitch[index];
        keyObject[QStringLiteral("Interval")] = config.fxCD[index];
        pressArray.append(keyObject);
    }
    result[QStringLiteral("AutoPress")] = pressArray;

    result["Interval"] = config.globalInterval;
    result["DefaultKey"] = config.defaultKey;

    result["X"] = config.x;
    result["Y"] = config.y;

    result["Title"] = config.title;
    result["Hash"] = QString::fromUtf8(config.hash);

    return result;
}

SConfigData FxMainWindow::jsonToConfig(QJsonObject json)
{
    SConfigData result;
    QJsonArray pressArray;
    QJsonObject supplyObject;

    pressArray = json.take(QStringLiteral("AutoPress")).toArray();

    if (pressArray.size() == 10)
    {
        for (int index = 0; index < 10; index++)
        {
            QJsonObject keyObject = pressArray[index].toObject();
            result.fxSwitch[index] = keyObject.take(QStringLiteral("Enabled")).toBool(false);
            result.fxCD[index] = keyObject.take(QStringLiteral("Interval")).toDouble(1.0);
        }
    }

    result.globalInterval = json.take("Interval").toDouble(0.8);
    result.defaultKey = json.take("DefaultKey").toInt(-1);

    result.x = json.take("X").toInt(-1);
    result.y = json.take("Y").toInt(-1);

    result.title = json.take("Title").toString("");
    result.hash = json.take("Hash").toString("").toUtf8();

    return result;
}

QByteArray FxMainWindow::imageHash(QImage image)
{
    if (image.isNull() || image.format() != QImage::Format_RGB888)
        return QByteArray();

    QByteArray imageBytes;
    QDataStream stream(&imageBytes, QIODevice::WriteOnly);

    stream << image;

    return QCryptographicHash::hash(imageBytes, QCryptographicHash::Md5).toBase64();
}

void FxMainWindow::setupUI()
{
    auto get_h_line = []() {
        auto line = new QFrame;

        line->setFrameShape(QFrame::HLine);
        line->setFrameShadow(QFrame::Sunken);
        line->setLineWidth(1);

        return line;
    };

    QFont switch_font;
    switch_font.setFamily(QStringLiteral("微软雅黑"));
    switch_font.setPointSize(20);
    switch_font.setBold(true);

    QStringList supply_keys;

    for (int index = 0; index < 10; ++index)
    {
        supply_keys << QString("F%1").arg(index + 1);
    }

    auto main_widget = new QWidget;
    auto vlayout_main = new QVBoxLayout;

    btn_scan = new QPushButton(QStringLiteral("扫描游戏窗口"));
    connect(btn_scan, &QPushButton::clicked, [this]()
        {
            scanGameWindows();

            if (!gameWindows.isEmpty())
                autoSelectAndRenameGameWindow(currentHash);
        });
    vlayout_main->addWidget(btn_scan);

    combo_windows = new QComboBox;
    combo_windows->setIconSize(playerNameRect.size());
    combo_windows->setItemDelegate(new CharacterBoxDelegate);
    connect(combo_windows, static_cast<void(QComboBox::*)(int)>(&QComboBox::currentIndexChanged), [this](int index)
        {
            check_global_switch->setChecked(false);

            if (index != -1)
            {
                currentHash = playerNameHashes[index];
            }
        });
    vlayout_main->addWidget(combo_windows);

    line_title = new QLineEdit;
    auto hlayout_title = new QHBoxLayout;
    hlayout_title->addWidget(new QLabel(QStringLiteral("窗口标题")));
    hlayout_title->addWidget(line_title, 1);
    vlayout_main->addLayout(hlayout_title);

    btn_change_title = new QPushButton(QStringLiteral("修改窗口标题"));
    connect(btn_change_title, &QPushButton::clicked, this, &FxMainWindow::changeWindowTitle);
    vlayout_main->addWidget(btn_change_title);

    btn_switch_to_window = new QPushButton(QStringLiteral("切换到游戏窗口"));
    connect(btn_switch_to_window, &QPushButton::clicked, [this]()
        {
            int window_index = combo_windows->currentIndex();

            if (window_index == -1)
            {
                return;
            }

            SetForegroundWindow(gameWindows[window_index]);
        });
    vlayout_main->addWidget(btn_switch_to_window);

    vlayout_main->addWidget(get_h_line());

    check_global_switch = new QCheckBox(QStringLiteral("全局开关"));
    check_global_switch->setFont(switch_font);
    connect(check_global_switch, &QCheckBox::toggled, [this](bool checked)
        {
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

    auto gridlayout_keys = new QGridLayout;
    gridlayout_keys->addWidget(new QLabel(QStringLiteral("启用")), 0, 0);
    gridlayout_keys->addWidget(new QLabel(QStringLiteral("间隔")), 0, 1);
    gridlayout_keys->addWidget(new QLabel(QStringLiteral("是缺省技能")), 0, 2);

    for (int index = 0; index < 10; ++index)
    {
        auto check_key = new QCheckBox(QString("F%1").arg(index + 1));
        auto spin_key_interval = new QDoubleSpinBox;
        auto check_default = new QCheckBox;

        spin_key_interval->setSuffix(" s");
        spin_key_interval->setDecimals(1);
        spin_key_interval->setMinimum(0.1);
        spin_key_interval->setMaximum(365.0);
        spin_key_interval->setSingleStep(0.1);
        spin_key_interval->setValue(1.0);
        key_checks[index] = check_key;
        key_intervals[index] = spin_key_interval;
        key_defaults[index] = check_default;

        connect(check_key, &QCheckBox::toggled,
            [this, index](bool checked) {
                key_intervals[index]->setEnabled(!checked);
                resetTimeStamp(index);
            });

        connect(check_default, &QCheckBox::toggled,
            [this, index](bool checked) {
                //模拟QButtonGroup互斥，并能够全部取消选择
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

    main_widget->setLayout(vlayout_main);
    this->setCentralWidget(main_widget);
    this->setFixedSize(minimumSize());
}
