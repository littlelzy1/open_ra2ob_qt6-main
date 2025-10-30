#include "./ob.h"
#include "./Ra2ob/Ra2ob/src/Game.hpp"
#include "./Ra2ob/Ra2ob/src/Datatypes.hpp" // 添加Datatypes.hpp头文件
#include "./playerstatustracker.h"
#include "./mainwindow.h"
#include "./globalsetting.h"
#include "./membermanager.h"
#include "./buildingdetector.h" // 添加BuildingDetector头文件
#include "./theftalertmanager.h" // 添加TheftAlertManager头文件

#include <QDebug>
#include <QScreen>
#include <QTimer>
#include <QCoreApplication>
#include <QPixmapCache>
#include <QPainter>
#include <QDir>
#include <QFile>
#include <QLabel>
#include <QDate>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QEventLoop>
#include <QDateTime>
#include <QMouseEvent>
#include <QApplication>
#include <algorithm>
#ifdef Q_OS_WIN
    #include <windows.h>
    #include <tlhelp32.h>
#endif
#include <QFontDatabase>

// 初始化静态变量
bool Ob::remoteAvatarLoaded1 = false;
bool Ob::remoteAvatarLoaded2 = false;

Ob::Ob(QWidget *parent) : QWidget(parent) {
    // 初始化必要的成员变量
    g = &Ra2ob::Game::getInstance();
    p1_index = -1;
    p2_index = -1;
    boNumber = 5;
    showPmbImage = true;
    lastGameValid = false;
    pmbOffsetX = 0;
    pmbOffsetY = 0;
    
    // 设置窗口属性 - 关键添加
    this->setWindowFlags(Qt::WindowMinMaxButtonsHint | Qt::FramelessWindowHint |
                         Qt::WindowStaysOnTopHint | Qt::Tool);
    this->setAttribute(Qt::WA_TranslucentBackground);
    
    #ifdef Q_OS_WIN
        HWND hwnd = (HWND)this->winId();
        LONG_PTR exStyle = GetWindowLongPtr(hwnd, GWL_EXSTYLE);
        SetWindowLongPtr(hwnd, GWL_EXSTYLE, exStyle | WS_EX_NOACTIVATE);
    #endif
    
    // 设置窗口大小为全屏
    QScreen *screen = QGuiApplication::primaryScreen();
    this->setGeometry(screen->geometry());
    
    // 初始化全局设置
    setLayoutByScreen();
    
    // 初始化玩家状态跟踪器
    statusTracker = new PlayerStatusTracker(this, this);
    
    // 初始化建筑检测器
    buildingDetector = new BuildingDetector(this);
    connect(buildingDetector, &BuildingDetector::buildingTheftDetected,
            this, &Ob::onBuildingTheftDetected);
    connect(buildingDetector, &BuildingDetector::minerLossDetected,
            this, &Ob::onMinerLossDetected);
    buildingDetector->startDetection(500); // 每0.5秒检测一次
    
    // 如果父窗口存在，连接到MainWindow
    MainWindow* mainWindow = dynamic_cast<MainWindow*>(parent);
    if (mainWindow) {
        connect(statusTracker, &PlayerStatusTracker::playerScoreChanged, 
                mainWindow, &MainWindow::onPlayerScoreChanged);
    }
    
    // 连接PlayerStatusTracker信号到Ob类本身
    connect(statusTracker, &PlayerStatusTracker::playerScoreChanged,
            this, &Ob::onPlayerScoreChanged);

    // 创建内存清理计时器
    memoryCleanupTimer = new QTimer(this);
    connect(memoryCleanupTimer, &QTimer::timeout, this, &Ob::forceMemoryCleanup);
    memoryCleanupTimer->setInterval(60000); // 每60秒执行一次内存回收
    memoryCleanupTimer->start();
    
    // 初始化建造栏区域检测定时器
    producingBlocksTimer = new QTimer(this);
    connect(producingBlocksTimer, &QTimer::timeout, this, &Ob::checkMousePositionForProducingBlocks);
    producingBlocksTimer->setInterval(100); // 100毫秒检测一次鼠标位置
    producingBlocksTimer->start();
    
    // 创建定时器检测游戏状态
    gameTimer = new QTimer(this);
    connect(gameTimer, &QTimer::timeout, this, &Ob::onGameTimerTimeout);
    gameTimer->setInterval(500); // 每0.5秒检测一次
    gameTimer->start();
    
    // 创建单位信息刷新定时器
    unitUpdateTimer = new QTimer(this);
    connect(unitUpdateTimer, &QTimer::timeout, this, &Ob::onUnitUpdateTimerTimeout);
    unitUpdateTimer->setInterval(200); // 每0.2秒刷新一次单位信息
    unitUpdateTimer->start();
    
    // 初始化游戏时间和日期标签
    lb_gametime = new QLabel(this);
    lb_gametime->setStyleSheet("color: white;");
    lb_gametime->hide();
    
    // 初始化网络管理器（用于下载头像）
    networkManager = new QNetworkAccessManager(this);
    
    // 初始化头像刷新定时器（50秒刷新一次）
    avatarRefreshTimer = new QTimer(this);
    connect(avatarRefreshTimer, &QTimer::timeout, this, &Ob::resetAvatarLoadStatus);
    avatarRefreshTimer->setInterval(50000); // 设置为50秒
    avatarRefreshTimer->start();
    
    // 初始化滚动文字相关变量
    hideDate = false;
    scrollText = "";
    scrollPosition = 0;
    lb_scrollText = nullptr;
    scrollTimer = nullptr;
    
    // 初始化滚动文字
    initScrollText();
    
    // 从全局设置加载PMB图片显示状态
    showPmbImage = Globalsetting::getInstance().s.show_pmb_image;
    
    // 初始化PMB图片为空
    pmbImage = QPixmap();
    
    // 初始化PMB位置和大小
    pmbOffsetX = 0;
    pmbOffsetY = 0;
    pmbWidth = 137;
    pmbHeight = 220;
    
    // 加载字体
    QString appDir = QCoreApplication::applicationDirPath();
    
    // 加载特效字体
    QString technoGlitchPath = appDir + "/assets/fonts/LLDEtechnoGlitch-Bold0.ttf";
    int technoFontId = QFontDatabase::addApplicationFont(technoGlitchPath);
    if (technoFontId != -1) {
        QStringList fontFamilies = QFontDatabase::applicationFontFamilies(technoFontId);
        if (!fontFamilies.isEmpty()) {
            technoGlitchFontFamily = fontFamilies.first();
        }
    }
    
    // 加载MiSans-Bold字体
    QString miSansBoldPath = appDir + "/assets/fonts/MiSans-Bold.ttf";
    int miSansBoldId = QFontDatabase::addApplicationFont(miSansBoldPath);
    if (miSansBoldId != -1) {
        QStringList families = QFontDatabase::applicationFontFamilies(miSansBoldId);
        if (!families.isEmpty()) {
            miSansBoldFamily = families.first();
        }
    }
    
    // 加载MiSans-Heavy字体
    QString miSansHeavyPath = appDir + "/assets/fonts/MISANS-HEAVY.TTF";
    int miSansHeavyId = QFontDatabase::addApplicationFont(miSansHeavyPath);
    if (miSansHeavyId != -1) {
        QStringList families = QFontDatabase::applicationFontFamilies(miSansHeavyId);
        if (!families.isEmpty()) {
            miSansHeavyFamily = families.first();
        }
    }
    
    // 加载MiSans-Medium字体
    QString miSansMediumPath = appDir + "/assets/fonts/MISANS-MEDIUM.TTF";
    int miSansMediumId = QFontDatabase::addApplicationFont(miSansMediumPath);
    if (miSansMediumId != -1) {
        QStringList families = QFontDatabase::applicationFontFamilies(miSansMediumId);
        if (!families.isEmpty()) {
            miSansMediumFamily = families.first();
        }
    }
    
    // 初始化玩家信息标签
    initPlayerInfoLabels();
    
    // 添加玩家信息更新计时器
    QTimer *playerInfoTimer = new QTimer(this);
    connect(playerInfoTimer, &QTimer::timeout, this, &Ob::updatePlayerInfo);
    playerInfoTimer->setInterval(200);
    playerInfoTimer->start();
    
    // 直接隐藏窗口，由toggleOb方法控制显示
    this->hide();

    // 在构造函数结束前添加:
    // 初始化资金不足指示器
    initIfBar(true);

    // 设置默认玩家颜色
    qs_1 = "FF0000"; // 默认红色
    qs_2 = "0000FF"; // 默认蓝色

    // 添加玩家信息标签初始化时启用鼠标跟踪和事件过滤
    this->installEventFilter(this);
    
    // 初始化地图视频标签
    initMapVideoLabel();
    
    // 初始化OBS配置
    obsWebSocketUrl = "ws://localhost:8063"; // OBS WebSocket默认地址
    obsPassword = ""; // OBS WebSocket密码，如果需要的话可以设置
    gameEndSceneName = "游戏结束"; // 游戏结束时切换到的场景名称
    obsConnected = false; // 初始化连接状态
    obsAuthenticated = false; // 初始化认证状态
    
    // 初始化OBS连接
    initObsConnection();
    
    // 初始化间谍渗透标签
    initSpyInfiltrationLabels();
    
    // 初始化建筑偷取提示管理器
    theftAlertManager = new TheftAlertManager(this);
    theftAlertManager->initTheftAlertSystem();
    theftAlertManager->setBuildingDetector(buildingDetector);
    
}

void Ob::setLayoutByScreen() {
    qreal ratio_screen = this->screen()->devicePixelRatio();
    int gw = g->_gameInfo.debug.setting.screenWidth;
    int gh = g->_gameInfo.debug.setting.screenHeight;
    int sw = this->screen()->size().width();
    int sh = this->screen()->size().height();
    
    // 获取全局设置实例，尝试使用MainWindow启动时获取的实际屏幕分辨率
    Globalsetting &gls = Globalsetting::getInstance();

    qreal ratio;
    // 计算比例 - 优先使用实际屏幕分辨率
    if (gls.currentScreenWidth > 0 && gls.currentScreenHeight > 0) {
        // 使用MainWindow启动时获取的实际屏幕分辨率
        ratio = 1.0 * gls.currentScreenWidth / 1920.0; // 以1920为基准分辨率
    } else if (gw != 0) {
        // 回退到原有逻辑：使用游戏配置文件中的分辨率
        ratio = 1.0 * sw / gw;
    } else {
        // 最后的备选方案：使用设备像素比
        ratio = 1.0 / ratio_screen;
     
    }

    gls.loadLayoutSetting(sw, sh, ratio);
}

Ob::~Ob() {
    // 清理所有分配的资源
    
    // 清理状态跟踪器
    delete statusTracker;
    
    // 清理计时器
    if (memoryCleanupTimer) {
        memoryCleanupTimer->stop();
        delete memoryCleanupTimer;
    }
    
    // 清理游戏状态检测计时器
    if (gameTimer) {
        gameTimer->stop();
        delete gameTimer;
        gameTimer = nullptr;
    }
    
    // 清理单位信息刷新定时器
    if (unitUpdateTimer) {
        unitUpdateTimer->stop();
        delete unitUpdateTimer;
        unitUpdateTimer = nullptr;
    }
    
    // 清理头像刷新定时器
    if (avatarRefreshTimer) {
        avatarRefreshTimer->stop();
        delete avatarRefreshTimer;
        avatarRefreshTimer = nullptr;
    }
    
    // 清理滚动文字定时器
    if (scrollTimer) {
        scrollTimer->stop();
        delete scrollTimer;
        scrollTimer = nullptr;
    }
    
    // 清理滚动文字标签
    if (lb_scrollText) {
        delete lb_scrollText;
        lb_scrollText = nullptr;
    }
    
    // 清理地图视频定时器
    if (mapVideoTimer) {
        mapVideoTimer->stop();
        delete mapVideoTimer;
        mapVideoTimer = nullptr;
    }
    
    // 清理地图视频加载延迟定时器
    if (mapVideoLoadTimer) {
        mapVideoLoadTimer->stop();
        delete mapVideoLoadTimer;
        mapVideoLoadTimer = nullptr;
    }
    
    // 清理OBS延迟定时器
    if (obsDelayTimer) {
        obsDelayTimer->stop();
        delete obsDelayTimer;
        obsDelayTimer = nullptr;
    }
    
    // 清理地图视频标签
    if (lb_mapVideo) {
        delete lb_mapVideo;
        lb_mapVideo = nullptr;
    }
    
    // 清理网络管理器
    if (networkManager) {
        networkManager->deleteLater();
        networkManager = nullptr;
    }
    
    // 清理OBS WebSocket连接
    if (obsWebSocket) {
        obsWebSocket->close();
        obsWebSocket->deleteLater();
        obsWebSocket = nullptr;
    }
    
    // 清理建筑偷取提示管理器
    if (theftAlertManager) {
        delete theftAlertManager;
        theftAlertManager = nullptr;
    }
    

    
    // 清理标签控件
    if (lb_gametime) {
        delete lb_gametime;
        lb_gametime = nullptr;
    }
    
    if (lb_date) {
        delete lb_date;
        lb_date = nullptr;
    }
    
    // 清理玩家信息标签
    delete lb_playerName1;
    delete lb_balance1;
    delete lb_kills1;
    delete lb_alive1;
    delete lb_power1;
    delete lb_score1;
    delete lb_avatar1;
    
    delete lb_playerName2;
    delete lb_balance2;
    delete lb_kills2;
    delete lb_alive2;
    delete lb_power2;
    delete lb_score2;
    delete lb_avatar2;
}

void Ob::paintEvent(QPaintEvent *) {
    if (!this->isVisible()) return;
    
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    
    // 获取全局设置中的比例
    float ratio = Globalsetting::getInstance().l.ratio;
    
    // 只有当游戏有效时才绘制面板
    if (g->_gameInfo.valid) {
        // 绘制顶部面板
        paintTopPanel(&painter);
        
        // 绘制右侧栏，使用ratio进行缩放
        if (!rightBarImage.isNull()) {
            int rightBarWidth = rightBarImage.width();
            int scaledWidth = qRound(rightBarWidth * ratio);
            int rightX = width() - scaledWidth;
            
            // 缩放图片，保持高度与窗口一致
            QPixmap scaledBar = rightBarImage.scaled(scaledWidth, height(), 
                                                  Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
            painter.drawPixmap(rightX, 0, scaledBar);
            
            // 绘制玩家国旗
            paintPlayerFlags(&painter);
            
            // 绘制玩家单位信息
            paintUnits(&painter);
            
            // 绘制玩家建造栏
            paintProducingBlocks(&painter);
            
            // 绘制游戏时间和日期
            if (g->_gameInfo.valid) {
                // 获取游戏时间
                int gameTime;
                if (g->_gameInfo.realGameTime > 0) {
                    gameTime = g->_gameInfo.realGameTime;
                } else {
                    gameTime = g->_gameInfo.currentFrame / Ra2ob::GAMESPEED;
                }
                
                // 格式化游戏时间为时:分:秒
                int hours = gameTime / 3600;
                int minutes = (gameTime % 3600) / 60;
                int seconds = gameTime % 60;
                
                QString timeStr = QString("%1:%2:%3")
                    .arg(hours, 2, 10, QChar('0'))
                    .arg(minutes, 2, 10, QChar('0'))
                    .arg(seconds, 2, 10, QChar('0'));
                
                // 设置游戏时间字体 - 使用特效字体
                QFont technoFont(!technoGlitchFontFamily.isEmpty() ? 
                               technoGlitchFontFamily : "LLDEtechnoGlitch-Bold0", 
                               qRound(20 * ratio), QFont::Bold);
                
                // 计算文本位置
                QFontMetrics fm(technoFont);
                int textWidth = fm.horizontalAdvance(timeStr);
                int textHeight = fm.height();
                
                // 计算右侧面板中心点
                int rightPanelCenter = rightX + scaledWidth / 2;
                int textX = rightPanelCenter - textWidth / 2;
                int textY = 10 * ratio; // 顶部边距
                
                // 更新游戏时间标签
                if (lb_gametime) {
                    lb_gametime->setText(timeStr);
                    lb_gametime->setFont(technoFont);
                    lb_gametime->adjustSize();
                    lb_gametime->setGeometry(textX, textY, textWidth, textHeight);
                    lb_gametime->show();
                }
                
                // 根据hideDate标志决定显示日期还是滚动文字
                if (!hideDate) {
                    // 显示当前日期
                    QDate currentDate = QDate::currentDate();
                    QString dateStr = currentDate.toString("yyyy/MM/dd");
                    
                    // 设置日期字体 - 使用特效字体
                    QFont dateFont(!technoGlitchFontFamily.isEmpty() ? 
                                 technoGlitchFontFamily : "LLDEtechnoGlitch-Bold0", 
                                 qRound(20 * ratio), QFont::Bold);
                    
                    // 计算日期文本位置
                    QFontMetrics dateFm(dateFont);
                    int dateWidth = dateFm.horizontalAdvance(dateStr);
                    int dateHeight = dateFm.height();
                    
                    // 将日期放在右侧栏底部
                    int dateX = rightX + scaledWidth - dateWidth - 40 * ratio; // 右边距
                    int dateY = height() - dateHeight - 1 * ratio; // 底部边距
                    
                    // 创建或更新日期标签
                    if (!lb_date) {
                        lb_date = new QLabel(this);
                        lb_date->setStyleSheet("color: white;");
                    }
                    
                    lb_date->setText(dateStr);
                    lb_date->setFont(dateFont);
                    lb_date->adjustSize();
                    lb_date->setGeometry(dateX, dateY, dateWidth, dateHeight);
                    lb_date->show();
                    
                    // 隐藏滚动文字标签
                    if (lb_scrollText) {
                        lb_scrollText->hide();
                    }
                } else {
                    // 隐藏日期标签
                    if (lb_date) {
                        lb_date->hide();
                    }
                    
                    // 显示滚动文字标签
                    if (lb_scrollText && !scrollText.isEmpty()) {
                        lb_scrollText->show();
                        // 确保滚动定时器正在运行
                        if (scrollTimer && !scrollTimer->isActive()) {
                            startScrollText();
                        }
                    }
                }
            } else {
                // 游戏无效时隐藏标签
                if (lb_gametime) lb_gametime->hide();
                if (lb_date) lb_date->hide();
            }
            
            // 绘制PMB图片
            if (pmbImage.isNull() && showPmbImage) {
                // 如果PMB图片为空但需要显示，则尝试加载
                QString appDir = QCoreApplication::applicationDirPath();
                QString pmbPath = appDir + "/assets/panels/ra2.png";
                pmbImage.load(pmbPath);
            }
            
            if (!pmbImage.isNull() && showPmbImage) {
                // 计算图片位置：在右侧面板的中部偏下位置
                int scaledPmbWidth = qRound(pmbWidth * ratio); // 使用设置的图片大小
                int scaledPmbHeight = qRound(pmbHeight * ratio);
                
                // 获取右侧栏位置信息
                int rightPanelX = width() - scaledWidth;
                int rightPanelWidth = scaledWidth;
                
                // 计算PMB图片位置，应用自定义偏移量
                int pmbX = rightPanelX + (rightPanelWidth - scaledPmbWidth) / 2 + pmbOffsetX;
                int pmbY = qRound(810 * ratio) + pmbOffsetY; // 向下偏移900像素（经过缩放）加上自定义Y偏移
                
                // 绘制PMB图片
                QPixmap scaledPmb = pmbImage.scaled(scaledPmbWidth, scaledPmbHeight, Qt::KeepAspectRatio, Qt::SmoothTransformation);
                painter.drawPixmap(pmbX, pmbY, scaledPmb);
            }
            

        }
        
        // 添加绘制底部经济条
        paintBottomPanel(&painter);
    } else {
        // 游戏无效时隐藏标签
        if (lb_gametime) lb_gametime->hide();
        if (lb_date) lb_date->hide();
    }
}

// 添加实现paintBottomPanel方法
void Ob::paintBottomPanel(QPainter *painter) {
    // 如果设置为不显示底部面板则直接返回
    if (!Globalsetting::getInstance().s.show_bottom_panel) {
        if (credit_1) credit_1->hide();
        if (credit_2) credit_2->hide();
        if (lb_mapname) lb_mapname->hide();
        return;
    }

    // 设置颜色
    QColor f_bg(0x2f, 0x37, 0x3f); // 背景色
    QColor f_g(QColor(0x22, 0xac, 0x38)); // 绿色(资金充足)
    QColor f_r(QColor("red")); // 红色(资金不足)
    
    // 固定宽度为5像素
    int w = 5;  
    int sx = 60;
    int sx_1 = sx;
    int sx_2 = sx;

    // 获取玩家颜色
    QString qs_l = QString::fromStdString("#" + qs_1);
    QString qs_r = QString::fromStdString("#" + qs_2);
    QColor lColor = QColor(qs_l);
    QColor rColor = QColor(qs_r);

    // 绘制底部面板背景
    painter->fillRect(QRect(0, Globalsetting::getInstance().l.bottom_y, 
                          Globalsetting::getInstance().l.right_x, 
                          Globalsetting::getInstance().l.bottom_h), f_bg);

    // 绘制玩家颜色条
    painter->fillRect(QRect(0, Globalsetting::getInstance().l.bottom_y1, sx, 
                          Globalsetting::getInstance().l.bottom_credit_h), lColor);
    painter->fillRect(QRect(0, Globalsetting::getInstance().l.bottom_y2, sx, 
                          Globalsetting::getInstance().l.bottom_credit_h), rColor);

    // 绘制边框
    QPen blackBorder(Qt::black);
    blackBorder.setWidth(2);
    painter->setPen(blackBorder);
    painter->drawRect(QRect(0, Globalsetting::getInstance().l.bottom_y1, 
                          Globalsetting::getInstance().l.right_x - 1, 
                          Globalsetting::getInstance().l.bottom_credit_h));
    painter->drawRect(QRect(0, Globalsetting::getInstance().l.bottom_y2, 
                          Globalsetting::getInstance().l.right_x - 1, 
                          Globalsetting::getInstance().l.bottom_credit_h));

    // 绘制资金状态历史条(玩家1)
    for (int ifund : insufficient_fund_bar_1) {
        if (ifund == 0) {
            painter->fillRect(QRect(sx_1, Globalsetting::getInstance().l.bottom_fill_y1, 
                                  w, Globalsetting::getInstance().l.bottom_fill_h), f_r);
        } else {
            painter->fillRect(QRect(sx_1, Globalsetting::getInstance().l.bottom_fill_y1, 
                                  w, Globalsetting::getInstance().l.bottom_fill_h), f_g);
        }
        sx_1 += w;
    }
    
    // 绘制资金状态历史条(玩家2)
    for (int ifund : insufficient_fund_bar_2) {
        if (ifund == 0) {
            painter->fillRect(QRect(sx_2, Globalsetting::getInstance().l.bottom_fill_y2, 
                                  w, Globalsetting::getInstance().l.bottom_fill_h), f_r);
        } else {
            painter->fillRect(QRect(sx_2, Globalsetting::getInstance().l.bottom_fill_y2, 
                                  w, Globalsetting::getInstance().l.bottom_fill_h), f_g);
        }
        sx_2 += w;
    }

    // 显示玩家消费总额
    if (p1_index >= 0 && p2_index >= 0 && g->_gameInfo.valid) {
        Ra2ob::tagGameInfo &gi = g->_gameInfo;

        // 玩家1消费额显示
        int c_1 = gi.players[p1_index].panel.creditSpent;
        int ck_1 = c_1 / 1000;
        int ch_1 = c_1 % 1000 / 100;
        QString res_1 = QString::number(ck_1) + "." + QString::number(ch_1) + "k";

        // 玩家2消费额显示
        int c_2 = gi.players[p2_index].panel.creditSpent;
        int ck_2 = c_2 / 1000;
        int ch_2 = c_2 % 1000 / 100;
        QString res_2 = QString::number(ck_2) + "." + QString::number(ch_2) + "k";

        // 更新标签文本
        credit_1->setText(res_1);
        credit_2->setText(res_2);

        // 使用特效字体
        QFont creditFont(!technoGlitchFontFamily.isEmpty() ? 
                       technoGlitchFontFamily : "LLDEtechnoGlitch-Bold0", 
                       qRound(15 * Globalsetting::getInstance().l.ratio));

        credit_1->setFont(creditFont);
        credit_2->setFont(creditFont);

        // 设置位置
        credit_1->setGeometry(Globalsetting::getInstance().l.bottom_credit_x, 
                            Globalsetting::getInstance().l.bottom_credit_y1, 
                            Globalsetting::getInstance().l.bottom_credit_w,
                            Globalsetting::getInstance().l.bottom_credit_h);
        credit_2->setGeometry(Globalsetting::getInstance().l.bottom_credit_x, 
                            Globalsetting::getInstance().l.bottom_credit_y2, 
                            Globalsetting::getInstance().l.bottom_credit_w,
                            Globalsetting::getInstance().l.bottom_credit_h);
        
        credit_1->show();
        credit_2->show();
    }

    // 绘制地图名称
    if (lb_mapname == nullptr) {
        lb_mapname = new QLabel(this);
        
        // 使用预加载的字体
        QFont mapNameFont(!miSansMediumFamily.isEmpty() ? 
                        miSansMediumFamily : "MiSans-Medium", 
                        qRound(12 * Globalsetting::getInstance().l.ratio));
        
        lb_mapname->setFont(mapNameFont);
        lb_mapname->setStyleSheet("color: white");
    } else {
        // 确保更新字体大小
        QFont mapNameFont(!miSansMediumFamily.isEmpty() ? 
                        miSansMediumFamily : "MiSans-Medium", 
                        qRound(12 * Globalsetting::getInstance().l.ratio));
        lb_mapname->setFont(mapNameFont);
    }

    QString mapName = QString::fromUtf8(g->_gameInfo.mapNameUtf);
    if (mapName.length() > 1) {
        lb_mapname->setText(mapName);
        lb_mapname->adjustSize();

        int fw = lb_mapname->width();
        int fh = lb_mapname->height();
        int px = 0;
        int py = Globalsetting::getInstance().l.bottom_y - fh - 2;

        lb_mapname->setGeometry(px + 5, py - 2, fw, fh);
        lb_mapname->show();
    }
}

// 添加初始化资金不足指示器函数
void Ob::initIfBar(bool clean) {
    if (clean) {
        insufficient_fund_bar_1.clear();
        insufficient_fund_bar_2.clear();
    }

    if (credit_1 == nullptr) {
        credit_1 = new QLabel(this);
        credit_1->setStyleSheet("color: white");
    }

    if (credit_2 == nullptr) {
        credit_2 = new QLabel(this);
        credit_2->setStyleSheet("color: white");
    }

    // 使用预加载的字体
    QFont creditFont(!technoGlitchFontFamily.isEmpty() ? 
                   technoGlitchFontFamily : "LLDEtechnoGlitch-Bold0", 
                   qRound(15 * Globalsetting::getInstance().l.ratio));

    credit_1->setFont(creditFont);
    credit_1->setText("0.0k");
    credit_1->setGeometry(Globalsetting::getInstance().l.bottom_credit_x, 
                        Globalsetting::getInstance().l.bottom_credit_y1, 
                        Globalsetting::getInstance().l.bottom_credit_w,
                        Globalsetting::getInstance().l.bottom_credit_h);

    credit_2->setFont(creditFont);
    credit_2->setText("0.0k");
    credit_2->setGeometry(Globalsetting::getInstance().l.bottom_credit_x, 
                        Globalsetting::getInstance().l.bottom_credit_y2, 
                        Globalsetting::getInstance().l.bottom_credit_w,
                        Globalsetting::getInstance().l.bottom_credit_h);
}

// 添加更新玩家颜色的函数
void Ob::setPlayerColor() {
    Ra2ob::tagGameInfo &gi = g->_gameInfo;

    if (p1_index < 0 || p1_index >= Ra2ob::MAXPLAYER || !g->_gameInfo.players[p1_index].valid) {
        qs_1 = "FF0000"; // 默认红色
    } else {
        qs_1 = gi.players[p1_index].panel.color;
        if (qs_1 == "0") {
            qs_1 = "FF0000"; // 默认红色
        }
    }

    if (p2_index < 0 || p2_index >= Ra2ob::MAXPLAYER || !g->_gameInfo.players[p2_index].valid) {
        qs_2 = "FF0000"; // 默认红色
    } else {
        qs_2 = gi.players[p2_index].panel.color;
        if (qs_2 == "0") {
            qs_2 = "FF0000"; // 默认红色
        }
    }
}

// 在detectGame函数中添加资金状态更新
// 需要在现有的refreshPanel方法中添加更新资金状态的代码
void Ob::refreshPanel() {
    if (p1_index < 0 || p2_index < 0 || p1_index >= Ra2ob::MAXPLAYER || p2_index >= Ra2ob::MAXPLAYER ||
        !g->_gameInfo.players[p1_index].valid || !g->_gameInfo.players[p2_index].valid) {
        return;
    }

    // 更新玩家信息和状态
    updatePlayerInfo();
    
    // 获取资金不足状态
    int p_if_1 = getInsufficientFund(p1_index);
    int p_if_2 = getInsufficientFund(p2_index);

   
    const int w = 5; // 条宽度
    const int sx = 60; // 起始x坐标
    
    if ((Globalsetting::getInstance().l.right_x - sx) / (insufficient_fund_bar_1.size() + 1) < w) {
    
        insufficient_fund_bar_1.clear();
        insufficient_fund_bar_2.clear();
        
     
    }

    // 添加最新的资金状态
    insufficient_fund_bar_1.push_back(p_if_1);
    insufficient_fund_bar_2.push_back(p_if_2);
    
    // 检查是否需要进行红色块汇集统计
    checkAndProcessRedBlocks();
}

// 新增：检查并处理红色块汇集统计
void Ob::checkAndProcessRedBlocks() {
    // 统计玩家1的红色块总数和最长连续红色块
    int totalRed1 = 0;
    int maxConsecutiveRed1 = 0;
    int currentConsecutiveRed1 = 0;
    
    for (int fund : insufficient_fund_bar_1) {
        if (fund == 0) { // 红色块（资金不足）
            totalRed1++;
            currentConsecutiveRed1++;
            maxConsecutiveRed1 = std::max(maxConsecutiveRed1, currentConsecutiveRed1);
        } else {
            currentConsecutiveRed1 = 0;
        }
    }
    
    // 统计玩家2的红色块总数和最长连续红色块
    int totalRed2 = 0;
    int maxConsecutiveRed2 = 0;
    int currentConsecutiveRed2 = 0;
    
    for (int fund : insufficient_fund_bar_2) {
        if (fund == 0) { // 红色块（资金不足）
            totalRed2++;
            currentConsecutiveRed2++;
            maxConsecutiveRed2 = std::max(maxConsecutiveRed2, currentConsecutiveRed2);
        } else {
            currentConsecutiveRed2 = 0;
        }
    }
    
    // 显示卡钱时间对比
    QString comparisonText = QString("卡钱对比 - P1总计:%1次 最长:%2次 | P2总计:%3次 最长:%4次")
                            .arg(totalRed1).arg(maxConsecutiveRed1).arg(totalRed2).arg(maxConsecutiveRed2);
    
    // 如果经济条已满或达到特定条件，重新加载经济条
    const int w = 5; // 条宽度
    const int sx = 60; // 起始x坐标
    int maxBlocks = (Globalsetting::getInstance().l.right_x - sx) / w;
    
    // 当经济条接近满载时，基于最长红色块重新加载
    if (insufficient_fund_bar_1.size() >= maxBlocks * 0.9 || insufficient_fund_bar_2.size() >= maxBlocks * 0.9) {
        reloadEconomyBarsFromLongestRed(maxConsecutiveRed1, maxConsecutiveRed2);
    }
}

// 新增：基于最长红色块重新加载经济条
void Ob::reloadEconomyBarsFromLongestRed(int maxRed1, int maxRed2) {
    qDebug() << "重新加载经济条 - 基于最长红色块 玩家1:" << maxRed1 << "玩家2:" << maxRed2;
    
    // 清空现有经济条
    insufficient_fund_bar_1.clear();
    insufficient_fund_bar_2.clear();
    
    // 找到最长的红色块长度作为统一起点
    int maxRedLength = std::max(maxRed1, maxRed2);
    
    // 两位玩家都从同一起点开始，以最长红色块的长度作为起点
    // 短的一方用绿色补足到相同长度
    for (int i = 0; i < maxRedLength; i++) {
        if (i < maxRed1) {
            insufficient_fund_bar_1.push_back(0); // 红色块
        } else {
            insufficient_fund_bar_1.push_back(1); // 绿色补足
        }
        
        if (i < maxRed2) {
            insufficient_fund_bar_2.push_back(0); // 红色块
        } else {
            insufficient_fund_bar_2.push_back(1); // 绿色补足
        }
    }
    
    qDebug() << "经济条重新加载完成 - 两位玩家从同一起点开始，长度:" << maxRedLength;
}

// 添加获取资金不足状态的函数
int Ob::getInsufficientFund(int playerIndex) {
    if (playerIndex < 0 || playerIndex >= Ra2ob::MAXPLAYER || !g->_gameInfo.valid || 
        !g->_gameInfo.players[playerIndex].valid) {
        return 1; // 默认为资金充足
    }
    
    // 获取玩家当前金钱
    int currentMoney = g->_gameInfo.players[playerIndex].panel.balance;
    
    // 当余额低于30时，判定为资金不足
    return (currentMoney < 30) ? 0 : 1;
}

// 添加paintTopPanel方法实现
void Ob::paintTopPanel(QPainter *painter) {
    // 获取全局设置中的比例
    float ratio = Globalsetting::getInstance().l.ratio;
    int wCenter = Globalsetting::getInstance().l.top_m;
    
    // 计算缩放后的面板尺寸
    int pWidth = qRound(1073 * ratio);  // 面板宽度
    int pHeight = qRound(96 * ratio);   // 面板高度
    int colorBgHeight = qRound(76 * ratio); // 颜色背景高度
    
    // 1. 绘制底层背景图(bg.png)
    if (!topBgImage.isNull()) {
        QPixmap scaledBg = topBgImage.scaled(pWidth, pHeight, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
        painter->drawPixmap(wCenter - pWidth / 2, 0, scaledBg);
    }
    
    // 2. 绘制玩家颜色背景图(中间层)
    if (p1_index >= 0 && p2_index >= 0) {
        // 计算面板半宽度
        int panelHalfWidth = pWidth / 2;
        
        // 直接加载玩家1颜色背景图
        QString appDir = QCoreApplication::applicationDirPath();
        QString p1ColorPath;
        QString p2ColorPath;
        
        // 获取玩家颜色
        std::string p1Color = g->_gameInfo.players[p1_index].panel.color;
        std::string p2Color = g->_gameInfo.players[p2_index].panel.color;
        
        // 玩家1颜色图片路径
        if (p1Color == "e0d838") { // 黄色
            p1ColorPath = appDir + "/assets/colors/Yellow-left.png";
        } else if (p1Color == "f84c48") { // 红色
            p1ColorPath = appDir + "/assets/colors/Red-left.png";
        } else if (p1Color == "58cc50") { // 绿色
            p1ColorPath = appDir + "/assets/colors/Green-left.png";
        } else if (p1Color == "487cc8") { // 蓝色
            p1ColorPath = appDir + "/assets/colors/Blue-left.png";
        } else if (p1Color == "58d4e0") { // 天蓝色
            p1ColorPath = appDir + "/assets/colors/skyblue-left.png";
        } else if (p1Color == "9848b8") { // 紫色
            p1ColorPath = appDir + "/assets/colors/Purple-left.png";
        } else if (p1Color == "f8ace8") { // 粉色
            p1ColorPath = appDir + "/assets/colors/Pink-left.png";
        } else if (p1Color == "f8b048") { // 橙色
            p1ColorPath = appDir + "/assets/colors/Orange-left.png";
        } else {
            // 对于未知的颜色，尝试基于RGB值选择最接近的颜色
            bool ok;
            unsigned int colorHex = QString::fromStdString(p1Color).toUInt(&ok, 16);
            
            if (ok) {
                // 解析RGB分量
                int r = (colorHex >> 16) & 0xFF;
                int g = (colorHex >> 8) & 0xFF;
                int b = colorHex & 0xFF;
                
                // 根据RGB分量的相对强度选择最匹配的颜色
                if (r > 200 && r > g*1.5 && r > b*1.5) {
                    p1ColorPath = appDir + "/assets/colors/Red-left.png";
                } else if (b > 150 && b > r*1.5 && b > g*1.5) {
                    p1ColorPath = appDir + "/assets/colors/Blue-left.png";
                } else if (g > 150 && g > r*1.2 && g > b*1.5) {
                    p1ColorPath = appDir + "/assets/colors/Green-left.png";
                } else if (r > 180 && g > 180 && b < 100) {
                    p1ColorPath = appDir + "/assets/colors/Yellow-left.png";
                } else if (r > 200 && g > 100 && g < 180 && b < 100) {
                    p1ColorPath = appDir + "/assets/colors/Orange-left.png";
                } else if (r > 200 && b > 150 && g < r*0.8) {
                    p1ColorPath = appDir + "/assets/colors/Pink-left.png";
                } else if (r > 100 && b > 100 && g < r*0.7 && g < b*0.7) {
                    p1ColorPath = appDir + "/assets/colors/Purple-left.png";
                } else if (b > 150 && g > 150 && r < b*0.8) {
                    p1ColorPath = appDir + "/assets/colors/skyblue-left.png";
                } else {
                    p1ColorPath = appDir + "/assets/colors/Red-left.png"; // 默认红色
                }
            } else {
                p1ColorPath = appDir + "/assets/colors/Red-left.png"; // 默认红色
            }
        }
        
        // 玩家2颜色图片路径
        if (p2Color == "e0d838") { // 黄色
            p2ColorPath = appDir + "/assets/colors/Yellow-left.png";
        } else if (p2Color == "f84c48") { // 红色
            p2ColorPath = appDir + "/assets/colors/Red-left.png";
        } else if (p2Color == "58cc50") { // 绿色
            p2ColorPath = appDir + "/assets/colors/Green-left.png";
        } else if (p2Color == "487cc8") { // 蓝色
            p2ColorPath = appDir + "/assets/colors/Blue-left.png";
        } else if (p2Color == "58d4e0") { // 天蓝色
            p2ColorPath = appDir + "/assets/colors/skyblue-left.png";
        } else if (p2Color == "9848b8") { // 紫色
            p2ColorPath = appDir + "/assets/colors/Purple-left.png";
        } else if (p2Color == "f8ace8") { // 粉色
            p2ColorPath = appDir + "/assets/colors/Pink-left.png";
        } else if (p2Color == "f8b048") { // 橙色
            p2ColorPath = appDir + "/assets/colors/Orange-left.png";
        } else {
            // 对于未知的颜色，尝试基于RGB值选择最接近的颜色
            bool ok;
            unsigned int colorHex = QString::fromStdString(p2Color).toUInt(&ok, 16);
            
            if (ok) {
                // 解析RGB分量
                int r = (colorHex >> 16) & 0xFF;
                int g = (colorHex >> 8) & 0xFF;
                int b = colorHex & 0xFF;
                
                // 根据RGB分量的相对强度选择最匹配的颜色
                if (r > 200 && r > g*1.5 && r > b*1.5) {
                    p2ColorPath = appDir + "/assets/colors/Red-left.png";
                } else if (b > 150 && b > r*1.5 && b > g*1.5) {
                    p2ColorPath = appDir + "/assets/colors/Blue-left.png";
                } else if (g > 150 && g > r*1.2 && g > b*1.5) {
                    p2ColorPath = appDir + "/assets/colors/Green-left.png";
                } else if (r > 180 && g > 180 && b < 100) {
                    p2ColorPath = appDir + "/assets/colors/Yellow-left.png";
                } else if (r > 200 && g > 100 && g < 180 && b < 100) {
                    p2ColorPath = appDir + "/assets/colors/Orange-left.png";
                } else if (r > 200 && b > 150 && g < r*0.8) {
                    p2ColorPath = appDir + "/assets/colors/Pink-left.png";
                } else if (r > 100 && b > 100 && g < r*0.7 && g < b*0.7) {
                    p2ColorPath = appDir + "/assets/colors/Purple-left.png";
                } else if (b > 150 && g > 150 && r < b*0.8) {
                    p2ColorPath = appDir + "/assets/colors/skyblue-left.png";
                } else {
                    p2ColorPath = appDir + "/assets/colors/Red-left.png"; // 默认红色
                }
            } else {
                p2ColorPath = appDir + "/assets/colors/Red-left.png"; // 默认红色
            }
        }
        
        // 加载玩家1颜色背景
        QPixmap p1ColorBg(p1ColorPath);
        if (!p1ColorBg.isNull()) {
            QPixmap scaledP1Bg = p1ColorBg.scaled(panelHalfWidth, colorBgHeight, 
                                                Qt::IgnoreAspectRatio, 
                                                Qt::SmoothTransformation);
            painter->drawPixmap(wCenter - pWidth / 2, 0, scaledP1Bg);
        }
        
        // 加载玩家2颜色背景并水平翻转
        QPixmap p2ColorBg(p2ColorPath);
        if (!p2ColorBg.isNull()) {
            // 水平翻转图片(scale(-1,1)表示X轴翻转，Y轴不变)
            QPixmap mirroredP2Bg = p2ColorBg.transformed(QTransform().scale(-1, 1));
            
            // 缩放处理后的图片
            QPixmap scaledP2Bg = mirroredP2Bg.scaled(panelHalfWidth, colorBgHeight,
                                                   Qt::IgnoreAspectRatio,
                                                   Qt::SmoothTransformation);
            
            // 绘制在右半边
            painter->drawPixmap(wCenter, 0, scaledP2Bg);
        }
    }
    
    // 3. 绘制上层前景图(bg1.png)
    if (!topBg1Image.isNull()) {
        QPixmap scaledBg1 = topBg1Image.scaled(pWidth, pHeight, 
                                        Qt::IgnoreAspectRatio, 
                                        Qt::SmoothTransformation);
        painter->drawPixmap(wCenter - pWidth / 2, 0, scaledBg1);
    }
    
    // 4. 绘制BO数显示在顶部面板底部中央
    QString boText = QString("BO %1").arg(boNumber);
    
    // 使用预加载的MiSans-Bold字体，大小12
    QFont boFont(!miSansBoldFamily.isEmpty() ? 
               miSansBoldFamily : "MiSans-Bold", 
               qRound(12 * ratio), QFont::Bold);
    
    painter->setFont(boFont);
    
    // 计算文本尺寸
    QFontMetrics boFm(boFont);
    int boTextWidth = boFm.horizontalAdvance(boText);
    int boTextHeight = boFm.height();
    
    // 计算文本位置 - 顶部面板底部中央，往下移动一点
    int boTextX = wCenter - boTextWidth / 2;
    int boTextY = pHeight - boTextHeight / 2 + qRound(7 * ratio); // 向下偏移10个单位（会根据比例缩放）
    
    // 直接绘制白色文本，不添加描边
    painter->setPen(Qt::white);
    painter->drawText(boTextX, boTextY, boText);
}

// getPlayerColorBackground方法已删除，直接在paintTopPanel中实现

bool Ob::setPmbImage(const QString &filePath) {
    if (filePath.isEmpty() || !QFile::exists(filePath)) {
        return false;
    }
    
    QPixmap newImage(filePath);
    if (newImage.isNull()) {
        return false;
    }
    
    // 确保图片文件夹存在
    QString appDir = QCoreApplication::applicationDirPath();
    QDir panelsDir(appDir + "/assets/panels");
    if (!panelsDir.exists()) {
        QDir().mkpath(panelsDir.path());
    }
    
    // 保存新图片到assets/panels目录下
    QString targetPath = appDir + "/assets/panels/ra2.png";
    
    // 如果是不同的文件，先备份原始文件（如果存在且尚未备份）
    QString backupPath = appDir + "/assets/panels/ra2.png.backup";
    if (!QFile::exists(backupPath) && QFile::exists(targetPath) && filePath != targetPath) {
        QFile::copy(targetPath, backupPath);
    }
    
    // 如果是外部文件，复制到目标位置
    if (filePath != targetPath) {
        // 如果目标文件已存在，先删除
        if (QFile::exists(targetPath)) {
            QFile::remove(targetPath);
        }
        
        if (!QFile::copy(filePath, targetPath)) {
            return false;
        }
    }
    
    // 重新加载图片
    pmbImage = QPixmap();  // 先清空，强制重新加载
    pmbImage = newImage;
    showPmbImage = true;   // 设置为显示
    
    // 更新显示
    this->update();
    return true;
}

void Ob::resetPmbImage() {
    // 还原备份的原始图片（如果存在）
    QString appDir = QCoreApplication::applicationDirPath();
    QString backupPath = appDir + "/assets/panels/ra2.png.backup";
    QString targetPath = appDir + "/assets/panels/ra2.png";
    
    if (QFile::exists(backupPath)) {
        // 删除当前图片
        if (QFile::exists(targetPath)) {
            QFile::remove(targetPath);
        }
        
        // 恢复备份
        QFile::copy(backupPath, targetPath);
    }
    
    // 重新加载图片
    pmbImage = QPixmap();  // 清空，强制下次绘制时重新加载
    showPmbImage = true;   // 设置为显示
    
    // 更新显示
    this->update();
}

void Ob::setShowPmbImage(bool show) {
    if (showPmbImage != show) {
        showPmbImage = show;
        
        // 保存设置到全局配置
        Globalsetting::getInstance().s.show_pmb_image = show;
        
        // 更新显示
        this->update();
    }
}

void Ob::updatePlayerAvatars() {
    // 重置远程头像加载状态，强制重新加载
    remoteAvatarLoaded1 = false;
    remoteAvatarLoaded2 = false;
    
    // 更新玩家1头像
    if (p1_index >= 0 && p1_index < Ra2ob::MAXPLAYER && g->_gameInfo.valid && g->_gameInfo.players[p1_index].valid) {
        setAvatar(p1_index);
    }
    
    // 更新玩家2头像
    if (p2_index >= 0 && p2_index < Ra2ob::MAXPLAYER && g->_gameInfo.valid && g->_gameInfo.players[p2_index].valid) {
        setAvatar(p2_index);
    }
}

void Ob::updateScores() {
    // 更新得分
}

void Ob::setBONumber(int number) {
    boNumber = number;
}

int Ob::getBoNumber() const {
    return boNumber;
}

void Ob::detectGame() {
    // 检测游戏进程是否存在
    static bool lastProcessRunning = false;
    bool processRunning = false;
    
    // 直接检测gamemd-spawn.exe进程
    HANDLE hProcessSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hProcessSnap != INVALID_HANDLE_VALUE) {
        PROCESSENTRY32 processInfo{};
        processInfo.dwSize = sizeof(PROCESSENTRY32);
        
        for (BOOL success = Process32First(hProcessSnap, &processInfo); success;
             success = Process32Next(hProcessSnap, &processInfo)) {
            std::wstring processName = processInfo.szExeFile;
            if (processName == L"gamemd-spawn.exe") {
                processRunning = true;
                break;
            }
        }
        CloseHandle(hProcessSnap);
    }
    
    // 检测进程从无到有的转变
    if (processRunning && !lastProcessRunning) {
        // 进程刚启动，启动2秒延迟定时器后显示"游戏中"源并隐藏BGM源和转场源
        if (!obsDelayTimer) {
            obsDelayTimer = new QTimer(this);
            obsDelayTimer->setSingleShot(true);
            connect(obsDelayTimer, &QTimer::timeout, [this]() {
                showObsSource("游戏中");
                switchObsScene("BGM");  // 隐藏BGM源
                switchObsScene("转场");  // 隐藏转场源
                qDebug() << "延迟2秒后显示OBS源: 游戏中，并隐藏BGM源和转场源";
            });
        }
        obsDelayTimer->start(2000); // 2秒延迟
        qDebug() << "检测到游戏进程启动，将在2秒后显示OBS源: 游戏中，并隐藏BGM源和转场源";
        
        // 启动4秒延迟定时器隐藏"游戏结束"源 - 已注释：取消隐藏游戏结束源的逻辑
        /*
        QTimer *gameEndHideTimer = new QTimer(this);
        gameEndHideTimer->setSingleShot(true);
        connect(gameEndHideTimer, &QTimer::timeout, [this, gameEndHideTimer]() {
            switchObsScene("游戏结束");  // 隐藏游戏结束源
            qDebug() << "延迟4秒后隐藏OBS源: 游戏结束";
            gameEndHideTimer->deleteLater();
        });
        gameEndHideTimer->start(4000); // 4秒延迟
        qDebug() << "检测到游戏进程启动，将在4秒后隐藏OBS源: 游戏结束";
        */
    }
    
    // 检测进程从有到无的转变
    if (!processRunning && lastProcessRunning) {
        // 进程刚结束，隐藏"游戏中"源并显示"游戏结束"源
        switchObsScene("游戏中");
        qDebug() << "检测到游戏进程结束，隐藏OBS源: 游戏中";
        
        // 停止并清理地图视频
        stopMapVideo();
        
        // 停止玩家状态跟踪器
        if (statusTracker) {
            statusTracker->stopTracking();
            qDebug() << "游戏结束，停止玩家状态跟踪器";
        }
        
        // 强制内存回收
        forceMemoryCleanup();
        
        // Qt清理PixmapCache
        QPixmapCache::clear();
        
     
    }
    
    // 记住上次进程状态
    lastProcessRunning = processRunning;
    
    // 检测游戏是否刚刚开始
    bool gameJustStarted = g->_gameInfo.valid && !lastGameValid;
    bool gameJustEnded = !g->_gameInfo.valid && lastGameValid;

    // 记住上次游戏状态
    lastGameValid = g->_gameInfo.valid;

    // 游戏刚开始时加载面板图片
    if (gameJustStarted) {
        // 获取应用程序目录路径
        QString appDir = QCoreApplication::applicationDirPath();
        
        // 加载右侧栏背景图
        rightBarImage.load(appDir + "/assets/panels/Rightbar.png");
        
        // 加载顶部背景图
        topBgImage.load(appDir + "/assets/panels/bg.png");
        topBg1Image.load(appDir + "/assets/panels/bg1.png");
        
        // 清理单位显示，确保新游戏开始时重新加载单位信息
        clearUnitDisplay();
        
        // 更新屏幕布局
        setLayoutByScreen();

        // 初始化经济条数据
        initIfBar(true);
        
        // 启动玩家状态跟踪器
        if (statusTracker) {
            statusTracker->startTracking();
            qDebug() << "游戏开始，启动玩家状态跟踪器";
        }
        
        // 清理间谍渗透提示状态
        if (lb_spyAlert1) {
            lb_spyAlert1->hide();
        }
        if (lb_spyAlert2) {
            lb_spyAlert2->hide();
        }
        if (spyAlertTimer1) {
            spyAlertTimer1->stop();
        }
        if (spyAlertTimer2) {
            spyAlertTimer2->stop();
        }
        
        // 重置间谍渗透状态跟踪变量
        resetSpyInfiltrationStatus();
        
        qDebug() << "游戏开始，清理间谍渗透提示状态并重置状态跟踪变量";
        
        // 初始化地图视频定时器（如果还未创建）
        if (!mapVideoTimer) {
            mapVideoTimer = new QTimer(this);
            connect(mapVideoTimer, &QTimer::timeout, [this]() {
                if (!mapVideoFrames.isEmpty() && this->isVisible()) {
                    currentMapVideoFrame++;
                    
                    // 特殊处理：对于尤复天梯地图，处理MAP1_00013.png和htw.png
                     QString mapNameUtf = QString::fromStdString(g->_gameInfo.mapNameUtf);
                     if (mapNameUtf == "巴乔尔LE-尤复天梯" || mapNameUtf == "不列兹然湖LE-尤复天梯" || 
                         mapNameUtf == "魔法一号之魂LE-尤复天梯" || mapNameUtf == "沙摩西岛-尤复天梯" ||
                         mapNameUtf == "首要港口LE-尤复天梯" || mapNameUtf == "斯坦利公园-尤复天梯" ||
                         mapNameUtf == "尤伦湖LE-尤复天梯" || mapNameUtf == "再见宣言LE-尤复天梯" ||
                         mapNameUtf == "正式锦标赛D-LE-尤复天梯" || mapNameUtf == "变量S-尤复天梯" ||
                         mapNameUtf == "猪猡湾LE-尤复天梯" || mapNameUtf == "咫尺天涯LE—尤复天梯") {
                         
                         QString appDir = QCoreApplication::applicationDirPath();
                         QString baseImagePath;
                         
                         // 前25帧：显示MAP1_00013.png与地图图片的合成图片
                         if (currentMapVideoFrame < 25) {
                             baseImagePath = appDir + "/assets/1v1png/MAP1_00013.png";
                             QString mapImagePath = appDir + "/assets/map/" + mapNameUtf + ".png";
                             
                             QPixmap baseImage(baseImagePath);
                             QPixmap mapImage(mapImagePath);
                             
                             if (!baseImage.isNull() && !mapImage.isNull()) {
                                 // 缩放地图图片到550x265像素
                                  QPixmap scaledMapImage = mapImage.scaled(550, 265, Qt::KeepAspectRatio, Qt::SmoothTransformation);
                                 
                                 // 创建合成图片
                                  QPixmap combinedImage = baseImage.copy();
                                  QPainter painter(&combinedImage);
                                  
                                  // 将地图图片绘制在合适位置（可以根据需要调整位置）
                                  int mapX = (combinedImage.width() - scaledMapImage.width()) / 2;
                                  int mapY = (combinedImage.height() - scaledMapImage.height()) / 2;
                                  painter.drawPixmap(mapX, mapY, scaledMapImage);
                                  
                                  // 设置字体和颜色
                                  QFont font(miSansBoldFamily, 40);
                                  font.setBold(true);
                                  painter.setFont(font);
                                  painter.setPen(Qt::white);
                                  
                                  // 在地图正中间上方绘制"当前地图"
                                  int centerX = combinedImage.width() / 2;
                                  int textY1 = mapY - 25; // 地图上方
                                  QFontMetrics fm1(font);
                                  int titleWidth = fm1.horizontalAdvance("当前地图");
                                  painter.drawText(centerX - titleWidth/2, textY1, "当前地图");
                                  
                                  // 在地图正中间下方绘制地图名
                                  int textY2 = mapY + scaledMapImage.height() + 60; // 地图下方
                                  QFontMetrics fm(font);
                                  int textWidth = fm.horizontalAdvance(mapNameUtf);
                                  painter.drawText(centerX - textWidth/2, textY2, mapNameUtf);
                                  
                                  painter.end();
                                 
                                 // 更新当前帧
                                 mapVideoFrames[currentMapVideoFrame] = combinedImage;
                                 qDebug() << "合成显示基础图片和地图图片:" << baseImagePath << "+" << mapImagePath;
                             } else {
                                 qDebug() << "警告: 无法加载基础图片或地图图片";
                             }
                         }
                         // 后35帧：只显示htw.png原图，不进行合成
                         else if (currentMapVideoFrame < 60) {
                             baseImagePath = appDir + "/assets/map/htw.png";
                             QPixmap baseImage(baseImagePath);
                             if (!baseImage.isNull()) {
                                 // 直接使用htw.png原图，不进行任何合成处理
                                 mapVideoFrames[currentMapVideoFrame] = baseImage;
                                 qDebug() << "显示htw.png原图:" << baseImagePath;
                             } else {
                                 qDebug() << "警告: 无法加载htw.png图片";
                             }
                         }
                     }
                    
                    if (currentMapVideoFrame >= mapVideoFrames.size()) {
                        // 播放完成，停止定时器
                        mapVideoTimer->stop();
                        mapVideoPlaying = false;
                        mapVideoCompleted = true;
                        currentMapVideoFrame = -1; // 设置为无效帧，不再显示
                    }
                    updateMapVideoDisplay();
                } else if (!this->isVisible()) {
                    // 界面不可见时暂停播放
                    mapVideoTimer->stop();
                    mapVideoPlaying = false;
                }
            });
        }
        
        // 重置地图视频状态
        currentMapVideoFrame = 0;
        mapVideoPlaying = false;
        mapVideoCompleted = false;
        
        // 延迟加载地图视频帧（用于中间显示），等待地图名读取完成
        if (!mapVideoLoaded) {
            // 创建延迟定时器来等待地图名读取完成
            if (!mapVideoLoadTimer) {
                mapVideoLoadTimer = new QTimer(this);
                mapVideoLoadTimer->setSingleShot(true);
                connect(mapVideoLoadTimer, &QTimer::timeout, [this]() {
                    // 检查游戏是否仍然有效
                    if (!g->_gameInfo.valid) {
                        qDebug() << "游戏已无效，取消地图视频加载";
                        return;
                    }
                    
                    QString mapNameUtf = QString::fromStdString(g->_gameInfo.mapNameUtf);
                    qDebug() << "延迟后准备加载地图视频，地图名:" << mapNameUtf;
                    if (!mapNameUtf.isEmpty()) {
                        loadMapVideoFramesByMapName(mapNameUtf);
                    } else {
                        qDebug() << "警告: 延迟后地图名仍为空，将再次尝试";
                        // 如果地图名仍为空，再延迟1秒重试
                        mapVideoLoadTimer->start(1000);
                    }
                });
            }
            // 延迟1.5秒后加载地图视频，给地图名读取留出时间
            mapVideoLoadTimer->start(1500);
            qDebug() << "游戏开始，将在1.5秒后加载地图视频";
        } else {
            qDebug() << "游戏开始，地图视频已加载，帧数:" << mapVideoFrames.size();
        }
        
        // 更新界面
        update();
    }
    
    // 游戏刚结束时，释放面板图片和内存
    if (gameJustEnded) {
        // 停止玩家状态跟踪器
        if (statusTracker) {
            statusTracker->stopTracking();
            qDebug() << "游戏结束，停止玩家状态跟踪器";
        }
        
        // 释放图片资源
        rightBarImage = QPixmap();
        topBgImage = QPixmap();
        topBg1Image = QPixmap();
        pmbImage = QPixmap(); // 释放PMB图片资源
        
        // 清理各种图片缓存
        flagCache.clear();
        flagsLoaded = false;
        unitIconCache.clear();    // 清理单位图标缓存
        
        // 清理头像缓存和相关内存资源
        remoteAvatarLoaded1 = false;
        remoteAvatarLoaded2 = false;
        
        // 释放头像QPixmap内存
        playerAvatar1 = QPixmap();
        playerAvatar2 = QPixmap();
        
        // 清理头像昵称字符串
        currentNickname1.clear();
        currentNickname2.clear();
        
        // 清理头像标签显示
        if (lb_avatar1) {
            lb_avatar1->clear();
            lb_avatar1->setPixmap(QPixmap());
        }
        if (lb_avatar2) {
            lb_avatar2->clear();
            lb_avatar2->setPixmap(QPixmap());
        }
        
        // 中断正在进行的网络请求
        if (networkManager) {
            // 中断所有正在进行的网络请求
            networkManager->clearConnectionCache();
            networkManager->clearAccessCache();
        }
        
        // 清理单位显示
        clearUnitDisplay();
        
        // 清理地图视频资源
        stopMapVideo();
        mapVideoFrames.clear();
        mapVideoLoaded = false;
        mapVideoCompleted = false;
        
        // 停止地图视频加载延迟定时器
        if (mapVideoLoadTimer && mapVideoLoadTimer->isActive()) {
            mapVideoLoadTimer->stop();
            qDebug() << "游戏结束，停止地图视频加载定时器";
        }
        
        // 隐藏时间和日期标签
        if (lb_gametime) lb_gametime->hide();
        if (lb_date) lb_date->hide();
        
        // 强制系统回收内存
        #ifdef Q_OS_WIN
            SetProcessWorkingSetSize(GetCurrentProcess(), (SIZE_T)-1, (SIZE_T)-1);
        #endif
        
        // 强制Qt清理PixmapCache
        QPixmapCache::clear();
        
        // 更新界面以清除图片显示
        update();
    }
    
    // 如果游戏有效，更新玩家信息和经济条
    if (g->_gameInfo.valid && this->isVisible()) {
        // Debug: 输出游戏基本信息
        qDebug() << QString("[DEBUG] 游戏状态: 有效=%1, 当前帧=%2, 游戏时间=%3秒")
            .arg(g->_gameInfo.valid ? "是" : "否")
            .arg(g->_gameInfo.currentFrame)
            .arg(g->_gameInfo.realGameTime);
        
        // 更新屏幕布局
        setLayoutByScreen();
        
        // 寻找两个主要玩家
        p1_index = -1;
        p2_index = -1;
        
        qDebug() << "[DEBUG] 开始扫描玩家...";
        
        for (int i = 0; i < Ra2ob::MAXPLAYER; i++) {
            if (g->_gameInfo.players[i].valid) {
                if (p1_index == -1) {
                    p1_index = i;
                } else if (p2_index == -1) {
                    p2_index = i;
                    break;
                }
            }
        }
        
        // 设置玩家颜色
        setPlayerColor();
        
        // 更新玩家面板和经济条
        refreshPanel();
        
        // 需要重绘界面
        update();
    }
}

void Ob::toggleOb() {
    bool shouldShow = false;
    QString hideReason = "";
    
    // 首先检查全局设置中的show_all_panel标志
    // 如果设置为false，则不显示OB界面
    if (!Globalsetting::getInstance().s.show_all_panel) {
        hideReason = "全局面板显示设置为隐藏";
    }
    // 仅当全局设置允许显示时，才检查其他条件
    else if (g->_gameInfo.valid && 
        !g->_gameInfo.isGamePaused && 
        !isGameReallyOver() && 
        g->_gameInfo.currentFrame >= 5) {
        
        // 计算有效玩家数量
        int validPlayerCount = 0;
        for (int i = 0; i < Ra2ob::MAXPLAYER; i++) {
            if (g->_gameInfo.players[i].valid) {
                validPlayerCount++;
            }
        }
        
        if (validPlayerCount >= 2) {
            shouldShow = true;
        }
    }
    
    // 更新窗口显示状态
    if (shouldShow != this->isVisible()) {
        if (shouldShow) {
            this->show();
            
            // 启动地图视频播放（用于中间显示）
            if (!mapVideoFrames.isEmpty() && !mapVideoPlaying && !mapVideoCompleted && mapVideoTimer) {
                mapVideoTimer->start(40); // 每40毫秒一帧，25FPS
                mapVideoPlaying = true;
                qDebug() << "地图视频开始播放，帧数:" << mapVideoFrames.size();
            } else {
                qDebug() << "地图视频未启动 - 帧数:" << mapVideoFrames.size() << ", 播放中:" << mapVideoPlaying << ", 已完成:" << mapVideoCompleted << ", 定时器:" << (mapVideoTimer != nullptr);
            }
            
            // 确保在Windows平台上设置WS_EX_NOACTIVATE，使窗口不获取焦点
            #ifdef Q_OS_WIN
                HWND hwnd = (HWND)this->winId();
                LONG_PTR exStyle = GetWindowLongPtr(hwnd, GWL_EXSTYLE);
                SetWindowLongPtr(hwnd, GWL_EXSTYLE, exStyle | WS_EX_NOACTIVATE);
            #endif
        } else {
            this->hide();
            
            // 停止地图视频播放
            stopMapVideo();
        }
    }
}

void Ob::switchScreen() {
    // 存根实现
}

Ra2ob::Game* Ob::getGamePtr() const {
    return g;
}

bool Ob::isGameReallyOver() {
    if (!g->_gameInfo.valid) {
        return true;
    }
    
    if (g->_gameInfo.isGameOver) {
        int validPlayers = 0;
        for (int i = 0; i < Ra2ob::MAXPLAYER; i++) {
            if (g->_gameInfo.players[i].valid) {
                validPlayers++;
            }
        }
        
        if (validPlayers == 0) {
            return true;
        } else {
            return false;
        }
    }
    
    if (g->_gameInfo.currentFrame <= 2) {
        int validPlayers = 0;
        for (int i = 0; i < Ra2ob::MAXPLAYER; i++) {
            if (g->_gameInfo.players[i].valid) {
                validPlayers++;
            }
        }
        
        if (validPlayers == 0) {
            return true;
        }
    }
    
    return false;
}

void Ob::onGameTimerTimeout() {
    // 检测游戏状态
    detectGame();
    
    // 控制窗口显示/隐藏
    toggleOb();
}

// 实现单位信息更新定时器的槽函数
void Ob::onUnitUpdateTimerTimeout() {
    // 只在游戏有效且窗口可见时更新单位信息
    if (g->_gameInfo.valid && this->isVisible()) {
        // 添加对刚实现的单位刷新函数的调用
        refreshUnits();
        
        // 更新间谍渗透显示
        updateSpyInfiltrationDisplay();
        
        // 触发界面重绘以更新单位显示
        update();
    }
}

// 添加单位过滤函数 - 合并类似单位（如Yuri和Yuri Clone）
void Ob::filterDuplicateUnits(Ra2ob::tagUnitsInfo &uInfo) {
    // 过滤Yuri和Yuri Clone，保留Yuri但使用最大的数量
    auto yuriIt = std::find_if(uInfo.units.begin(), uInfo.units.end(), 
        [](const Ra2ob::tagUnitSingle &unit) { return unit.unitName == "Yuri"; });
    
    auto yuriCloneIt = std::find_if(uInfo.units.begin(), uInfo.units.end(), 
        [](const Ra2ob::tagUnitSingle &unit) { return unit.unitName == "Yuri Clone"; });

    if (yuriIt != uInfo.units.end() && yuriCloneIt != uInfo.units.end()) {
        // 隐藏Yuri Clone
        yuriCloneIt->show = false;
        
        // 使用最大数量
        if (yuriCloneIt->num > yuriIt->num) {
            yuriIt->num = yuriCloneIt->num;
        }
    }
    
    // 过滤Yuri Prime和Yuri X，保留Yuri Prime但使用最大的数量
    auto yuriPrimeIt = std::find_if(uInfo.units.begin(), uInfo.units.end(), 
        [](const Ra2ob::tagUnitSingle &unit) { return unit.unitName == "Yuri Prime"; });
    
    auto yuriXIt = std::find_if(uInfo.units.begin(), uInfo.units.end(), 
        [](const Ra2ob::tagUnitSingle &unit) { return unit.unitName == "Yuri X"; });
    
    if (yuriPrimeIt != uInfo.units.end() && yuriXIt != uInfo.units.end()) {
        // 隐藏Yuri X
        yuriXIt->show = false;
        
        // 使用最大数量
        if (yuriXIt->num > yuriPrimeIt->num) {
            yuriPrimeIt->num = yuriXIt->num;
        }
    }
    
    // 可以在这里添加更多的单位合并规则
}

// 添加单位排序函数 - 优先显示共有单位，再显示特有单位
void Ob::sortUnitsForDisplay(Ra2ob::tagUnitsInfo &uInfo1, Ra2ob::tagUnitsInfo &uInfo2, 
                           std::vector<Ra2ob::tagUnitSingle> &sortedUnits1, 
                           std::vector<Ra2ob::tagUnitSingle> &sortedUnits2) {
    // 清空输出数组
    sortedUnits1.clear();
    sortedUnits2.clear();
    
    // 筛选有效单位(显示且数量>0)
    std::vector<Ra2ob::tagUnitSingle> validUnits1;
    std::vector<Ra2ob::tagUnitSingle> validUnits2;
    
    for (const auto &unit : uInfo1.units) {
        if (unit.show && unit.num > 0) {
            validUnits1.push_back(unit);
        }
    }
    
    for (const auto &unit : uInfo2.units) {
        if (unit.show && unit.num > 0) {
            validUnits2.push_back(unit);
        }
    }
    
    // 分离共有单位和特有单位
    std::vector<Ra2ob::tagUnitSingle> commonUnits1;
    std::vector<Ra2ob::tagUnitSingle> commonUnits2;
    
    // 查找共有单位
    for (auto it1 = validUnits1.begin(); it1 != validUnits1.end(); ) {
        bool found = false;
        for (auto it2 = validUnits2.begin(); it2 != validUnits2.end(); ) {
            // 检查是否为相同单位
            if (it1->unitName == it2->unitName) {
                // 添加到共有单位列表
                commonUnits1.push_back(*it1);
                commonUnits2.push_back(*it2);
                
                // 从原始列表中移除
                it1 = validUnits1.erase(it1);
                it2 = validUnits2.erase(it2);
                found = true;
                break;
            } else {
                ++it2;
            }
        }
        
        if (!found) {
            ++it1;
        }
    }
    
    // 按照显示优先级组合结果：共有单位优先，特有单位次之
    sortedUnits1.insert(sortedUnits1.end(), commonUnits1.begin(), commonUnits1.end());
    sortedUnits1.insert(sortedUnits1.end(), validUnits1.begin(), validUnits1.end());
    
    sortedUnits2.insert(sortedUnits2.end(), commonUnits2.begin(), commonUnits2.end());
    sortedUnits2.insert(sortedUnits2.end(), validUnits2.begin(), validUnits2.end());
}

// 添加单位显示刷新函数
void Ob::refreshUnits() {
    if (!g->_gameInfo.valid || p1_index < 0 || p2_index < 0) {
        return;
    }

    // 创建玩家单位信息的副本，避免修改原始数据
    Ra2ob::tagUnitsInfo uInfo1 = g->_gameInfo.players[p1_index].units;
    Ra2ob::tagUnitsInfo uInfo2 = g->_gameInfo.players[p2_index].units;
    
    // 过滤重复单位（在副本上操作）
    filterDuplicateUnits(uInfo1);
    filterDuplicateUnits(uInfo2);
    
    // 进行单位排序
    std::vector<Ra2ob::tagUnitSingle> sortedUnits1;
    std::vector<Ra2ob::tagUnitSingle> sortedUnits2;
    sortUnitsForDisplay(uInfo1, uInfo2, sortedUnits1, sortedUnits2);
    
    // 更新单位显示（这里只保留基本框架，实际显示逻辑需要根据你的UI组件实现）
    updateUnitDisplay(sortedUnits1, sortedUnits2);
}

// 更新单位显示
void Ob::updateUnitDisplay(const std::vector<Ra2ob::tagUnitSingle> &units1, 
                         const std::vector<Ra2ob::tagUnitSingle> &units2) {
    // 存储排序后的单位数据到成员变量
    sortedUnits1 = units1;
    sortedUnits2 = units2;
    
    // 触发重绘
    update();
}

// 实现获取国家国旗的方法
QPixmap Ob::getCountryFlag(const QString& country) {
    QString countryName = country;
    if (!country.isEmpty()) {
        countryName = country.at(0).toUpper() + country.mid(1).toLower();
    } else {
        countryName = "Americans"; // 默认使用美国国旗
    }

    QString appDir = QCoreApplication::applicationDirPath();
    QString flagPath = appDir + "/assets/countries/" + countryName + ".png";
    QPixmap flag(flagPath);
    
    if (flag.isNull()) {
        // 如果找不到指定国家的国旗，使用默认国旗
        flagPath = appDir + "/assets/countries/Americans.png";
        flag = QPixmap(flagPath);
        
        if (flag.isNull()) {
            // 如果默认国旗也找不到，创建一个空的透明图像
            flag = QPixmap(25, 15);
            flag.fill(Qt::transparent);
            qDebug() << "无法加载国旗图片: " << countryName;
        }
    }
    
    return flag;
}

// 添加绘制单个玩家国旗的方法
void Ob::drawPlayerFlag(QPainter *painter, int playerIndex, int flagX, int colorBarY, int colorBarWidth, int colorBarHeight, int flagY, int scaledFlagWidth, int scaledFlagHeight) {
    QString country = QString::fromStdString(g->_gameInfo.players[playerIndex].panel.country);
    QColor playerColor = QColor(QString::fromStdString("#" + g->_gameInfo.players[playerIndex].panel.color));
    
    // 获取国旗图片(使用缓存)
    QPixmap flag;
    if (flagCache.contains(country)) {
        flag = flagCache[country];
    } else {
        flag = getCountryFlag(country);
        if (!flag.isNull()) {
            flagCache.insert(country, flag);
        }
    }
    
    // 绘制颜色条和国旗
    if (!flag.isNull()) {
        // 绘制颜色条
        painter->fillRect(QRect(flagX, colorBarY, colorBarWidth, colorBarHeight), playerColor);
        
        // 缩放并绘制国旗
        QPixmap scaledFlag = flag.scaled(scaledFlagWidth, scaledFlagHeight, 
                                       Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
        painter->drawPixmap(flagX, flagY, scaledFlag);
    }
}

// 添加绘制玩家国旗的方法
void Ob::paintPlayerFlags(QPainter *painter) {
    // 获取全局设置中的比例
    float ratio = Globalsetting::getInstance().l.ratio;
    
    // 缩放后的国旗尺寸
    int scaledFlagWidth = qRound(flagWidth * ratio);
    int scaledFlagHeight = qRound(flagHeight * ratio);
    
    // 右侧面板位置
    int rightPanelX = width() - qRound(rightBarImage.width() * ratio);
    
    // 国旗位置 - 放在地图区域下方
    int flagY = Globalsetting::getInstance().l.map_y + Globalsetting::getInstance().l.map_h + 23 * ratio;
    
    // 水平位置 - 居中放置在右侧面板中
    int totalWidth = qRound(flagWidth * 2 * ratio);
    int panelCenter = rightPanelX + Globalsetting::getInstance().l.right_w / 2;
    int flag1X = panelCenter - totalWidth / 2 + 2;
    int flag2X = flag1X + scaledFlagWidth - 1;
    
    // 颜色条高度和位置
    int colorBarHeight = 5 * ratio;
    int colorBarY = flagY - colorBarHeight;
    int colorBarWidth = 84 * ratio;
    
    // 寻找两个主要玩家
    int player1 = -1;
    int player2 = -1;
    
    // 简单方法：选择前两个有效玩家
    for (int i = 0; i < Ra2ob::MAXPLAYER; i++) {
        if (g->_gameInfo.players[i].valid) {
            if (player1 == -1) {
                player1 = i;
            } else if (player2 == -1) {
                player2 = i;
                break;
            }
        }
    }
    
    // 绘制第一个玩家的国旗
    if (player1 != -1) {
        drawPlayerFlag(painter, player1, flag1X, colorBarY, colorBarWidth, colorBarHeight, flagY, scaledFlagWidth, scaledFlagHeight);
    }
    
    // 绘制第二个玩家的国旗
    if (player2 != -1) {
        drawPlayerFlag(painter, player2, flag2X, colorBarY, colorBarWidth, colorBarHeight, flagY, scaledFlagWidth, scaledFlagHeight);
    }
    
    // 标记国旗已加载
    flagsLoaded = true;
}

QPixmap Ob::getUnitIcon(const QString& unitName) {
    // 先检查缓存中是否有该单位图标
    if (unitIconCache.contains(unitName)) {
        return unitIconCache[unitName];
    }
    
    // 如果没有，则从文件加载
    QString appDir = QCoreApplication::applicationDirPath();
    QString iconPath = appDir + "/assets/obicons/" + unitName + ".png";
    
    // 加载图片
    QPixmap unitIcon(iconPath);
    
    if (unitIcon.isNull()) {
        qDebug() << "无法加载单位图标: " << iconPath;
        // 返回空图片
        QPixmap emptyPixmap(unitIconWidth, unitIconHeight);
        emptyPixmap.fill(Qt::transparent);
        return emptyPixmap;
    }
    
    // 缓存并返回图标
    unitIconCache.insert(unitName, unitIcon);
    return unitIcon;
}

QPixmap Ob::getRadiusPixmap(const QPixmap& src, int radius) {
    // 获取源图像的宽高
    int w = src.width();
    int h = src.height();
    
    // 创建一个透明的目标图像
    QPixmap dest(w, h);
    dest.fill(Qt::transparent);
    
    // 创建画家
    QPainter painter(&dest);
    painter.setRenderHint(QPainter::Antialiasing);
    
    // 创建圆角路径
    QPainterPath path;
    path.addRoundedRect(QRect(0, 0, w, h), radius, radius);
    
    // 设置裁剪路径
    painter.setClipPath(path);
    
    // 绘制源图像
    painter.drawPixmap(0, 0, w, h, src);
    
    // 结束绘制
    painter.end();
    
    return dest;
}

void Ob::clearUnitDisplay() {
    // 清空单位缓存
    unitIconCache.clear();
    unitIconsLoaded = false;
    
    // 强制清空所有单位状态，确保下次重新加载
    for (int i = 0; i < Ra2ob::MAXPLAYER; i++) {
        if (g->_gameInfo.players[i].valid) {
            // 清空单位列表，确保不会显示旧单位
            g->_gameInfo.players[i].units.units.clear();
        }
    }
    
    qDebug() << "清空单位显示，清理缓存和状态";
}

// 添加一个新方法用于绘制单个玩家的单位
void Ob::drawPlayerUnits(QPainter *painter, int playerIndex, int startX, int startY, float ratio) {
    // 获取玩家颜色
    QColor playerColor = QColor(QString::fromStdString("#" + g->_gameInfo.players[playerIndex].panel.color));
    
    // 单位图标尺寸和位置
    int scaledUnitWidth = qRound(unitIconWidth * ratio);
    int scaledUnitHeight = qRound(unitIconHeight * ratio);
    int scaledUnitSpacing = qRound(unitSpacing * ratio);
    
    int unitX = startX;
    int unitY = startY;
    
    // 遍历玩家的单位，不限制显示数量
    int displayedUnits = 0;
    
    // 选择要使用的单位数据源
    const std::vector<Ra2ob::tagUnitSingle>* unitsToUse = nullptr;
    
    // 寻找两个主要玩家
    int player1 = -1;
    int player2 = -1;
    
    for (int i = 0; i < Ra2ob::MAXPLAYER; i++) {
        if (g->_gameInfo.players[i].valid) {
            if (player1 == -1) {
                player1 = i;
            } else if (player2 == -1) {
                player2 = i;
                break;
            }
        }
    }
    
    // 优先使用排序后的单位数据
    if (playerIndex == player1 && !sortedUnits1.empty()) {
        unitsToUse = &sortedUnits1;
    } else if (playerIndex == player2 && !sortedUnits2.empty()) {
        unitsToUse = &sortedUnits2;
    } else {
        // 回退到原始单位数据
        unitsToUse = &g->_gameInfo.players[playerIndex].units.units;
    }
    
    // 遍历选定的单位数据
    for (const auto& unit : *unitsToUse) {
        if (unit.num <= 0 || !unit.show) {
            continue; // 跳过数量为0的单位
        }
        
        QString unitName = QString::fromStdString(unit.unitName);
        int unitCount = unit.num;
        
        // 获取单位图标
        QPixmap unitIcon = getUnitIcon(unitName);
        
        if (!unitIcon.isNull()) {
            // 缩放单位图标
            QPixmap scaledIcon = unitIcon.scaled(scaledUnitWidth, scaledUnitHeight, 
                                               Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
            
            // 应用圆角效果
            QPixmap radiusIcon = getRadiusPixmap(scaledIcon, qRound(8 * ratio));
            
            // 绘制单位图标
            painter->drawPixmap(unitX, unitY, radiusIcon);
            
            // 绘制数量背景
            int bgWidth = scaledUnitWidth;
            int bgHeight = qRound(13 * ratio);
            int bgY = unitY + scaledUnitHeight - bgHeight;
            
            // 创建带圆角的数量背景路径
            QPainterPath path;
            path.addRoundedRect(QRectF(unitX, bgY, bgWidth, bgHeight), 
                                qRound(4 * ratio), qRound(4 * ratio));
            
            painter->save();
            painter->setRenderHint(QPainter::Antialiasing);
            painter->fillPath(path, playerColor);
            painter->restore();
            
            // 绘制单位数量 - 使用特效字体
            painter->save();
            
            // 使用LLDEtechnoGlitch特效字体
            QFont technoFont(!technoGlitchFontFamily.isEmpty() ? technoGlitchFontFamily : "LLDEtechnoGlitch-Bold0", qRound(14 * ratio), QFont::Bold);
            painter->setFont(technoFont);
            
            QString countText = QString::number(unitCount);
            QFontMetrics technoFm(technoFont);
            int textWidth = technoFm.horizontalAdvance(countText);
            int textX = unitX + (bgWidth - textWidth) / 2;
            int textY = bgY + bgHeight - qRound(2 * ratio);
            
            // 先绘制黑色描边
            painter->setPen(Qt::black);
            for (int dx = -1; dx <= 1; ++dx) {
                for (int dy = -1; dy <= 1; ++dy) {
                    if (dx != 0 || dy != 0) {
                        painter->drawText(textX + dx, textY + dy, countText);
                    }
                }
            }
            
            // 再绘制白色文本
            painter->setPen(Qt::white);
            painter->drawText(textX, textY, countText);
            painter->restore();
            
            // 更新Y坐标，为下一个单位做准备
            unitY += scaledUnitHeight + scaledUnitSpacing;
            displayedUnits++;
        }
    }
}

void Ob::paintUnits(QPainter *painter) {
    // 确保只在游戏有效时绘制单位
    if (!g->_gameInfo.valid) {
        return; // 游戏无效，不绘制单位
    }

    // 获取全局设置中的比例
    float ratio = Globalsetting::getInstance().l.ratio;
    
    // 获取国旗尺寸和位置（与paintPlayerFlags中相同，保持一致性）
    int flagWidth = qRound(this->flagWidth * ratio);
    int flagHeight = qRound(this->flagHeight * ratio);
    
    // 右侧面板位置
    int rightPanelX = width() - qRound(rightBarImage.width() * ratio);
    
    // 国旗位置 - 放在地图区域下方
    int flagY = Globalsetting::getInstance().l.map_y + Globalsetting::getInstance().l.map_h + 23 * ratio;
    
    // 水平位置 - 居中放置在右侧面板中
    int totalWidth = qRound(this->flagWidth * 2 * ratio);
    int panelCenter = rightPanelX + Globalsetting::getInstance().l.right_w / 2;
    int flag1X = panelCenter - totalWidth / 2 + 2;
    int flag2X = flag1X + flagWidth - 1;
    
    // 单位区域起始Y坐标（国旗下方）
    int unitsStartY = flagY + flagHeight + qRound(unitDisplayOffsetY * ratio);
    
    // 寻找两个主要玩家
    int player1 = -1;
    int player2 = -1;
    
    // 简单方法：选择前两个有效玩家
    for (int i = 0; i < Ra2ob::MAXPLAYER; i++) {
        if (g->_gameInfo.players[i].valid) {
            if (player1 == -1) {
                player1 = i;
            } else if (player2 == -1) {
                player2 = i;
                break;
            }
        }
    }
    
    // 绘制第一个玩家的单位
    if (player1 != -1) {
        // 计算单位起始位置 - 中心对齐在国旗下方
        int flagCenterX = flag1X + flagWidth/2;
        int unitX = flagCenterX - qRound(unitIconWidth * ratio)/2;
        drawPlayerUnits(painter, player1, unitX, unitsStartY, ratio);
    }
    
    // 绘制第二个玩家的单位
    if (player2 != -1) {
        // 计算单位起始位置 - 中心对齐在国旗下方
        int flagCenterX = flag2X + flagWidth/2;
        int unitX = flagCenterX - qRound(unitIconWidth * ratio)/2;
        drawPlayerUnits(painter, player2, unitX, unitsStartY, ratio);
    }
    
    // 标记单位图标已加载
    unitIconsLoaded = true;
}

// 绘制玩家建造栏
void Ob::paintProducingBlocks(QPainter *painter) {
    // 确保只在游戏有效时绘制建造栏
    if (!g->_gameInfo.valid || !showProducingBlocks) {
        return; // 如果不显示建造栏，直接返回
    }
    
    // 获取全局设置中的比例
    float ratio = Globalsetting::getInstance().l.ratio;
    
    // 左侧建造栏区域起始坐标
    int startX = qRound(10 * ratio); // 左侧边距
    int startY1 = qRound(120 * ratio); // 第一个玩家的建造栏Y坐标
    int startY2 = qRound(205 * ratio); // 第二个玩家的建造栏Y坐标
    
    // 建造项目之间的水平间距
    int itemSpacing = qRound(3 * ratio);
    
    // 寻找两个主要玩家
    int player1 = -1;
    int player2 = -1;
    
    // 简单方法：选择前两个有效玩家
    for (int i = 0; i < Ra2ob::MAXPLAYER; i++) {
        if (g->_gameInfo.players[i].valid) {
            if (player1 == -1) {
                player1 = i;
            } else if (player2 == -1) {
                player2 = i;
                break;
            }
        }
    }
    
    // 绘制第一个玩家的建造栏
    if (player1 != -1) {
        // 获取玩家颜色
        QColor playerColor = QColor(QString::fromStdString("#" + g->_gameInfo.players[player1].panel.color));
        
        // 获取玩家建造信息
        const auto& buildingInfo = g->_gameInfo.players[player1].building;
        
        // 显示玩家建造信息（从左到右排列）
        int displayedBuildings = 0;
        int currentX = startX;
        
        for (const auto& building : buildingInfo.list) {
            if (displayedBuildings >= 5) {
                break; // 最多显示5个建造栏
            }
            
            if (building.status != 2) { // 跳过已暂停的建造项
                drawProducingBlock(painter, building, currentX, startY1, playerColor, ratio);
                currentX += qRound(producingBlockWidth * ratio) + itemSpacing; // 向右移动
                displayedBuildings++;
            }
        }
    }
    
    // 绘制第二个玩家的建造栏
    if (player2 != -1) {
        // 获取玩家颜色
        QColor playerColor = QColor(QString::fromStdString("#" + g->_gameInfo.players[player2].panel.color));
        
        // 获取玩家建造信息
        const auto& buildingInfo = g->_gameInfo.players[player2].building;
        
        // 显示玩家建造信息（从左到右排列）
        int displayedBuildings = 0;
        int currentX = startX;
        
        for (const auto& building : buildingInfo.list) {
            if (displayedBuildings >= 5) {
                break; // 最多显示5个建造栏
            }
            
            if (building.status != 2) { // 跳过已暂停的建造项
                drawProducingBlock(painter, building, currentX, startY2, playerColor, ratio);
                currentX += qRound(producingBlockWidth * ratio) + itemSpacing; // 向右移动
                displayedBuildings++;
            }
        }
    }
}

// 绘制单个建造块
void Ob::drawProducingBlock(QPainter *painter, const Ra2ob::tagBuildingNode& building,
                          int x, int y, const QColor& color, float ratio) {
    // 根据比例缩放尺寸
    int scaledWidth = qRound(producingBlockWidth * ratio);
    int scaledHeight = qRound(producingBlockHeight * ratio);
    
    // 绘制背景矩形
    QRect blockRect(x, y, scaledWidth, scaledHeight);
    
    // 创建圆角路径
    QPainterPath path;
    path.addRoundedRect(blockRect, qRound(8 * ratio), qRound(8 * ratio));
    
    // 使用玩家颜色的半透明版本作为背景
    QColor bgColor = color;
    bgColor.setAlpha(180); // 设置透明度
    
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);
    painter->fillPath(path, bgColor);
    painter->restore();
    
    // 加载并绘制单位图标
    QString unitName = QString::fromStdString(building.name);
    QPixmap unitIcon = getUnitIcon(unitName);
    
    if (!unitIcon.isNull()) {
        // 缩放图标大小 - 使用与obteam相同的尺寸
        int iconWidth = qRound(60 * ratio);
        int iconHeight = qRound(60 * ratio);
        QPixmap scaledIcon = unitIcon.scaled(iconWidth, iconHeight, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        
        // 应用圆角效果
        QPixmap radiusIcon = getRadiusPixmap(scaledIcon, qRound(6 * ratio));
        
        // 计算图标位置（居中）
        int iconX = x + (scaledWidth - iconWidth) / 2;
        int iconY = y + qRound(5 * ratio);
        
        // 绘制图标
        painter->drawPixmap(iconX, iconY, radiusIcon);
    }
    
    // 绘制进度条
    int progressBarHeight = qRound(6 * ratio);
    int progressBarWidth = scaledWidth - qRound(10 * ratio);
    int progressBarX = x + qRound(5 * ratio);
    int progressBarY = y + scaledHeight - progressBarHeight - qRound(20 * ratio);
    
    // 进度条背景
    QColor progressBgColor(50, 50, 50, 250);
    painter->fillRect(QRect(progressBarX, progressBarY, progressBarWidth, progressBarHeight), progressBgColor);
    
    // 计算进度
    int maxProgress = 54; // 根据tagBuildingNode的注释，最大进度为54
    float progressRatio = static_cast<float>(building.progress) / maxProgress;
    int filledWidth = qRound(progressBarWidth * progressRatio);
    
    // 绘制已完成进度
    painter->fillRect(QRect(progressBarX, progressBarY, filledWidth, progressBarHeight), color);
    
    // 使用MiSans-Bold字体，加粗并设置字间距
    QFont statusFont("MiSans-Bold", qRound(9 * ratio), QFont::Bold);
    statusFont.setLetterSpacing(QFont::AbsoluteSpacing, qRound(1.2 * ratio)); // 增加字符间距
    painter->setFont(statusFont);
    
    // 绘制状态文本
    QString statusText;
    float progressPercentage = (static_cast<float>(building.progress) / maxProgress) * 100;
    
    if (building.status == 1 && progressPercentage < 100) {
        statusText = "暂 停 中";  // 当status为1或进度停止时显示暂停中
    } else if (progressPercentage >= 100) {
        statusText = "已 完 成";  // 当进度达到100%时显示已完成
    } else {
        statusText = "建 造 中";  // 不显示百分比
    }
    
    QFontMetrics fm(statusFont);
    int textWidth = fm.horizontalAdvance(statusText);
    int textX = x + (scaledWidth - textWidth) / 2;
    int textY = y + scaledHeight - qRound(8 * ratio);
    
    // 使用白色绘制状态文本，不添加描边
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);
    painter->setPen(Qt::white);
    painter->drawText(textX, textY, statusText);
    painter->restore();
    
    // 如果数量大于1，绘制数量
    if (building.number > 1) {
        // 设置数量文本的字体 - 使用LLDEtechnoGlitch特效字体
        QFont technoFont(!technoGlitchFontFamily.isEmpty() ? 
                       technoGlitchFontFamily : "LLDEtechnoGlitch-Bold0", 
                       qRound(15 * ratio), QFont::Bold);
        
        painter->setFont(technoFont);
        
        // 绘制数量文本
        QString numberText = "x" + QString::number(building.number);
        QFontMetrics technoFm(technoFont);
        int numberWidth = technoFm.horizontalAdvance(numberText);
        int numberX = x + scaledWidth - numberWidth - qRound(5 * ratio);
        int numberY = y + qRound(15 * ratio);
        
        // 绘制带有描边的数量文本
        painter->save();
        painter->setRenderHint(QPainter::Antialiasing);
        
        // 先绘制黑色描边
        painter->setPen(Qt::black);
        for (int dx = -1; dx <= 1; ++dx) {
            for (int dy = -1; dy <= 1; ++dy) {
                if (dx != 0 || dy != 0) {
                    painter->drawText(numberX + dx, numberY + dy, numberText);
                }
            }
        }
        
        // 再绘制白色文本
        painter->setPen(Qt::white);
        painter->drawText(numberX, numberY, numberText);
        painter->restore();
    }
}

// 计算建造栏区域
void Ob::calculateProducingBlocksRegions() {
    // 获取全局设置中的比例
    float ratio = Globalsetting::getInstance().l.ratio;
    
    // 左侧建造栏区域起始坐标
    int startX = qRound(10 * ratio); // 左侧边距
    int startY1 = qRound(120 * ratio); // 第一个玩家的建造栏Y坐标
    int startY2 = qRound(205 * ratio); // 第二个玩家的建造栏Y坐标
    
    // 根据比例缩放尺寸
    int scaledWidth = qRound(producingBlockWidth * ratio);
    int scaledHeight = qRound(producingBlockHeight * ratio);
    int itemSpacing = qRound(producingItemSpacing * ratio);
    
    // 最大检测的建造项目数量（限制为2个）
    const int maxDetectingBuildings = 2;
    
    // 计算区域宽度 - 最多2个建造项目
    int areaWidth = scaledWidth * maxDetectingBuildings + itemSpacing * (maxDetectingBuildings - 1);
    
    // 添加一些内边距便于检测
    int padding = qRound(10 * ratio);
    
    // 计算玩家1建造栏区域
    player1ProducingRect = QRect(
        startX - padding,
        startY1 - padding,
        areaWidth + padding * 2,
        scaledHeight + padding * 2
    );
    
    // 计算玩家2建造栏区域
    player2ProducingRect = QRect(
        startX - padding,
        startY2 - padding,
        areaWidth + padding * 2,
        scaledHeight + padding * 2
    );
}

// 检测鼠标位置是否在建造栏区域
void Ob::checkMousePositionForProducingBlocks() {
    // 只有当游戏有效且界面可见时才检测
    if (!g->_gameInfo.valid || !this->isVisible()) {
        return;
    }
    
    // 计算当前建造栏区域（因为可能随窗口调整而变化）
    calculateProducingBlocksRegions();
    
    // 获取全局鼠标位置
    QPoint globalMousePos = QCursor::pos();
    
    // 将全局坐标转换为窗口坐标
    QPoint localMousePos = this->mapFromGlobal(globalMousePos);
    
    // 检查鼠标是否在任一建造栏区域内
    bool mouseInProducingArea = player1ProducingRect.contains(localMousePos) || 
                                player2ProducingRect.contains(localMousePos);
    
    // 根据鼠标位置决定是否显示建造栏
    bool shouldShow = !mouseInProducingArea;
    
    // 如果状态需要变化，则更新并触发重绘
    if (showProducingBlocks != shouldShow) {
        showProducingBlocks = shouldShow;   
        this->update(); // 触发重绘
    }
}

// 添加初始化玩家信息标签方法
void Ob::initPlayerInfoLabels() {
    // 获取缩放比例
    float ratio = Globalsetting::getInstance().l.ratio;
    
    // 初始化字体
    technoFont = QFont(technoGlitchFontFamily, qRound(14 * ratio));
    technoFont.setStyleStrategy(QFont::PreferAntialias);
    
    technoFontLarge = QFont(technoGlitchFontFamily, qRound(16 * ratio));
    technoFontLarge.setStyleStrategy(QFont::PreferAntialias);
    
    miSansBold = QFont(miSansBoldFamily, qRound(22 * ratio));
    miSansBold.setStyleStrategy(QFont::PreferAntialias);
    miSansBold.setWeight(QFont::Bold);
    
    miSansMedium = QFont(miSansHeavyFamily, qRound(25 * ratio));
    miSansMedium.setStyleStrategy(QFont::PreferAntialias);
    miSansMedium.setWeight(QFont::Black);
    
    // 创建玩家1标签
    lb_playerName1 = new QLabel(this);
    lb_playerName1->setStyleSheet("color: #ffffff;");
    lb_playerName1->setFont(miSansBold);
    lb_playerName1->setAlignment(Qt::AlignCenter | Qt::AlignVCenter);
    
    lb_balance1 = new QLabel(this);
    lb_balance1->setStyleSheet("color: #ffffff;");
    lb_balance1->setFont(technoFontLarge);
    
    lb_kills1 = new QLabel(this);
    lb_kills1->setStyleSheet("color: #ffffff;");
    lb_kills1->setFont(technoFont);
    lb_kills1->setAlignment(Qt::AlignCenter | Qt::AlignVCenter);
    
    lb_alive1 = new QLabel(this);
    lb_alive1->setStyleSheet("color: #ffffff;");
    lb_alive1->setFont(technoFont);
    lb_alive1->setAlignment(Qt::AlignCenter | Qt::AlignVCenter);
    
    lb_power1 = new QLabel(this);
    int powerSize = qRound(16 * ratio);
    int borderRadius = powerSize / 2;
    lb_power1->setStyleSheet(QString("background-color: grey; border-radius: %1px;").arg(borderRadius));
    lb_power1->setFixedSize(powerSize, powerSize);
    
    lb_score1 = new QLabel(this);
    lb_score1->setStyleSheet("color: #ffffff;");
    lb_score1->setFont(miSansMedium);
    lb_score1->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    // 添加鼠标跟踪和事件过滤
    lb_score1->setMouseTracking(true);
    lb_score1->installEventFilter(this);
    
    // 创建玩家1头像标签
    lb_avatar1 = new QLabel(this);
    int avatarSize = qRound(48 * ratio);
    lb_avatar1->setFixedSize(avatarSize, avatarSize);
    lb_avatar1->setStyleSheet(QString("border-radius: %1px;").arg(avatarSize / 2));
    lb_avatar1->setScaledContents(true);
    
    // 创建玩家2标签（镜像布局）
    lb_playerName2 = new QLabel(this);
    lb_playerName2->setStyleSheet("color: #ffffff;");
    lb_playerName2->setFont(miSansBold);
    lb_playerName2->setAlignment(Qt::AlignCenter | Qt::AlignVCenter);
    
    lb_balance2 = new QLabel(this);
    lb_balance2->setStyleSheet("color: #ffffff;");
    lb_balance2->setFont(technoFontLarge);
    
    lb_kills2 = new QLabel(this);
    lb_kills2->setStyleSheet("color: #ffffff;");
    lb_kills2->setFont(technoFont);
    lb_kills2->setAlignment(Qt::AlignCenter | Qt::AlignVCenter);
    
    lb_alive2 = new QLabel(this);
    lb_alive2->setStyleSheet("color: #ffffff;");
    lb_alive2->setFont(technoFont);
    lb_alive2->setAlignment(Qt::AlignCenter | Qt::AlignVCenter);
    
    lb_power2 = new QLabel(this);
    lb_power2->setStyleSheet(QString("background-color: grey; border-radius: %1px;").arg(borderRadius));
    lb_power2->setFixedSize(powerSize, powerSize);
    
    lb_score2 = new QLabel(this);
    lb_score2->setStyleSheet("color: #ffffff;");
    lb_score2->setFont(miSansMedium);
    lb_score2->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    // 添加鼠标跟踪和事件过滤
    lb_score2->setMouseTracking(true);
    lb_score2->installEventFilter(this);
    
    // 创建玩家2头像标签
    lb_avatar2 = new QLabel(this);
    lb_avatar2->setFixedSize(avatarSize, avatarSize);
    lb_avatar2->setStyleSheet(QString("border-radius: %1px;").arg(avatarSize / 2));
    lb_avatar2->setScaledContents(true);
    
    // 默认隐藏所有标签
    lb_playerName1->hide();
    lb_balance1->hide();
    lb_kills1->hide();
    lb_alive1->hide();
    lb_power1->hide();
    lb_score1->hide();
    lb_avatar1->hide();
    
    lb_playerName2->hide();
    lb_balance2->hide();
    lb_kills2->hide();
    lb_alive2->hide();
    lb_power2->hide();
    lb_score2->hide();
    lb_avatar2->hide();
}

// 添加更新玩家信息方法
void Ob::updatePlayerInfo() {
    // 仅在游戏有效且窗口可见时更新
    if (!g->_gameInfo.valid || !this->isVisible()) {
        return;
    }
    
    // 获取全局设置实例
    Globalsetting &gls = Globalsetting::getInstance();
    
    // 获取缩放比例
    float ratio = gls.l.ratio;
    
    // 找到两个主要玩家
    int player1 = -1;
    int player2 = -1;
    
    for (int i = 0; i < Ra2ob::MAXPLAYER; i++) {
        if (g->_gameInfo.players[i].valid) {
            if (player1 == -1) {
                player1 = i;
            } else if (player2 == -1) {
                player2 = i;
                break;
            }
        }
    }
    
    // 保存玩家索引，供paintTopPanel使用
    p1_index = player1;
    p2_index = player2;
    
    // 标记是否需要触发玩家名称更新信号
    bool nicknameChanged = false;
    
    // 获取顶部面板位置
    int wCenter = gls.l.top_m;
    int pWidth = qRound(1073 * ratio); // 面板基础宽度
    int panelHalfWidth = pWidth / 2;
    int startX = wCenter - pWidth / 2;
    
    // 设置标签位置和文本
    if (player1 != -1) {
        // 玩家1信息
        std::string name = g->_gameInfo.players[player1].panel.playerNameUtf;
        QString nickname = QString::fromUtf8(name);
        QString playername = gls.findNameByNickname(nickname);
        lb_playerName1->setText(playername.isEmpty() ? nickname : playername);
        
        // 检查昵称是否变化，如果变化则更新头像并标记需要更新
        if (nickname != currentNickname1) {
            currentNickname1 = nickname;
            setAvatar(player1);
            nicknameChanged = true; // 标记昵称已变化
            
            // 保存到全局设置
            gls.sb.p1_nickname = nickname;
            gls.sb.p1_nickname_cache = nickname;
            gls.sb.p1_playername_cache = playername;
        }
        
        int balance = g->_gameInfo.players[player1].panel.balance;
        lb_balance1->setText("$ " + QString::number(balance));
        
        int kills = g->_gameInfo.players[player1].score.kills;
        lb_kills1->setText(QString::number(kills));
        
        int alive = g->_gameInfo.players[player1].score.alive;
        lb_alive1->setText(QString::number(alive));
        
        int powerDrain = g->_gameInfo.players[player1].panel.powerDrain;
        int powerOutput = g->_gameInfo.players[player1].panel.powerOutput;
        setPowerState(lb_power1, powerDrain, powerOutput);
        
        // 获取玩家分数（从全局设置）
        int score = 0;
        if (gls.playerScores.contains(nickname)) {
            score = gls.playerScores[nickname];
        }
        lb_score1->setText(QString::number(score));
        
        // 玩家1位置设置
        int nameX = startX + qRound(73 * ratio);
        int nameY = qRound(25 * ratio);
        int nameWidth = qRound(153 * ratio);
        int nameHeight = qRound(25 * ratio);
        lb_playerName1->setGeometry(nameX, nameY, nameWidth, nameHeight);
        
        int scoreX = startX + qRound(11 * ratio);
        int scoreY = qRound(25 * ratio);
        int scoreWidth = qRound(40 * ratio);
        int scoreHeight = qRound(30 * ratio);
        lb_score1->setGeometry(scoreX, scoreY, scoreWidth, scoreHeight);
        
        int balanceX = startX + qRound(460 * ratio);
        int balanceY = qRound(28 * ratio);
        int balanceWidth = qRound(60 * ratio);
        int balanceHeight = qRound(18 * ratio);
        lb_balance1->setGeometry(balanceX, balanceY, balanceWidth, balanceHeight);
        
        int killsX = startX + qRound(346 * ratio);
        int killsY = qRound(45 * ratio);
        int killsWidth = qRound(60 * ratio);
        int killsHeight = qRound(18 * ratio);
        lb_kills1->setGeometry(killsX, killsY, killsWidth, killsHeight);
        
        int aliveX = startX + qRound(310 * ratio);
        int aliveY = qRound(45 * ratio);
        int aliveWidth = qRound(60 * ratio);
        int aliveHeight = qRound(18 * ratio);
        lb_alive1->setGeometry(aliveX, aliveY, aliveWidth, aliveHeight);
        
        int powerX = startX + qRound(405 * ratio);
        int powerY = qRound(47 * ratio);
        lb_power1->setGeometry(powerX, powerY, lb_power1->width(), lb_power1->height());
        
        // 设置头像位置
        int avatarSize = qRound(49 * ratio);
        int avatarX = startX + qRound(255 * ratio);
        int avatarY = qRound(12 * ratio);
        lb_avatar1->setGeometry(avatarX, avatarY, avatarSize, avatarSize);
        
        // 显示玩家1标签
        lb_playerName1->show();
        lb_balance1->show();
        lb_kills1->show();
        lb_alive1->show();
        lb_power1->show();
        lb_score1->show();
        lb_avatar1->show();
    } else {
        // 隐藏玩家1标签
        lb_playerName1->hide();
        lb_balance1->hide();
        lb_kills1->hide();
        lb_alive1->hide();
        lb_power1->hide();
        lb_score1->hide();
        lb_avatar1->hide();
    }
    
    if (player2 != -1) {
        // 玩家2信息
        std::string name = g->_gameInfo.players[player2].panel.playerNameUtf;
        QString nickname = QString::fromUtf8(name);
        QString playername = gls.findNameByNickname(nickname);
        lb_playerName2->setText(playername.isEmpty() ? nickname : playername);
        
        // 检查昵称是否变化，如果变化则更新头像并标记需要更新
        if (nickname != currentNickname2) {
            currentNickname2 = nickname;
            setAvatar(player2);
            nicknameChanged = true; // 标记昵称已变化
            
            // 保存到全局设置
            gls.sb.p2_nickname = nickname;
            gls.sb.p2_nickname_cache = nickname;
            gls.sb.p2_playername_cache = playername;
        }
        
        int balance = g->_gameInfo.players[player2].panel.balance;
        lb_balance2->setText("$ " + QString::number(balance));
        
        int kills = g->_gameInfo.players[player2].score.kills;
        lb_kills2->setText(QString::number(kills));
        
        int alive = g->_gameInfo.players[player2].score.alive;
        lb_alive2->setText(QString::number(alive));
        
        int powerDrain = g->_gameInfo.players[player2].panel.powerDrain;
        int powerOutput = g->_gameInfo.players[player2].panel.powerOutput;
        setPowerState(lb_power2, powerDrain, powerOutput);
        
        // 获取玩家分数（从全局设置）
        int score = 0;
        if (gls.playerScores.contains(nickname)) {
            score = gls.playerScores[nickname];
        }
        lb_score2->setText(QString::number(score));
        
        // 玩家2位置设置（正确的镜像）
        // 计算面板总宽度和中心线
        int totalPanelWidth = pWidth;
        
        // 镜像计算方法：相对于面板中心线对称
        int nameWidth = qRound(153 * ratio);
        int nameHeight = qRound(25 * ratio);
        int nameX = startX + panelHalfWidth + (panelHalfWidth - (qRound(73 * ratio) + nameWidth));
        int nameY = qRound(25 * ratio);
        lb_playerName2->setGeometry(nameX, nameY, nameWidth, nameHeight);
        
        int scoreWidth = qRound(40 * ratio);
        int scoreHeight = qRound(30 * ratio);
        int scoreX = startX + panelHalfWidth + (panelHalfWidth - (qRound(11 * ratio) + scoreWidth));
        int scoreY = qRound(25 * ratio);
        lb_score2->setGeometry(scoreX, scoreY, scoreWidth, scoreHeight);
        
        int balanceWidth = qRound(60 * ratio);
        int balanceHeight = qRound(18 * ratio);
        int balanceX = startX + panelHalfWidth + (panelHalfWidth - (qRound(443 * ratio) + balanceWidth));
        int balanceY = qRound(28 * ratio);
        lb_balance2->setGeometry(balanceX, balanceY, balanceWidth, balanceHeight);
        
        int killsWidth = qRound(60 * ratio);
        int killsHeight = qRound(18 * ratio);
        int killsX = startX + panelHalfWidth + (panelHalfWidth - (qRound(346 * ratio) + killsWidth));
        int killsY = qRound(45 * ratio);
        lb_kills2->setGeometry(killsX, killsY, killsWidth, killsHeight);
        
        int aliveWidth = qRound(60 * ratio);
        int aliveHeight = qRound(18 * ratio);
        int aliveX = startX + panelHalfWidth + (panelHalfWidth - (qRound(310 * ratio) + aliveWidth));
        int aliveY = qRound(45 * ratio);
        lb_alive2->setGeometry(aliveX, aliveY, aliveWidth, aliveHeight);
        
        int powerSize = qRound(16 * ratio);
        int powerX = startX + panelHalfWidth + (panelHalfWidth - (qRound(405 * ratio) + powerSize));
        int powerY = qRound(47 * ratio);
        lb_power2->setGeometry(powerX, powerY, powerSize, powerSize);
        
        // 设置头像位置（镜像）
        int avatarSize = qRound(49 * ratio);
        int avatarX = startX + panelHalfWidth + (panelHalfWidth - (qRound(253 * ratio) + avatarSize));
        int avatarY = qRound(12 * ratio);
        lb_avatar2->setGeometry(avatarX, avatarY, avatarSize, avatarSize);
        
        // 显示玩家2标签
        lb_playerName2->show(); 
        lb_balance2->show();
        lb_kills2->show();
        lb_alive2->show();
        lb_power2->show();
        lb_score2->show();
        lb_avatar2->show();
    } else {
        // 隐藏玩家2标签
        lb_playerName2->hide();
        lb_balance2->hide();
        lb_kills2->hide();
        lb_alive2->hide();
        lb_power2->hide();
        lb_score2->hide();
        lb_avatar2->hide();
    }
    
    // 如果任一玩家的昵称发生变化，触发playernameNeedsUpdate信号
    if (nicknameChanged) {
      
        emit playernameNeedsUpdate();
    }
}

// 添加设置电力状态方法
void Ob::setPowerState(QLabel *label, int powerDrain, int powerOutput) {
    QString color;
    
    if (powerDrain == 0 && powerOutput == 0) {
        // 初始状态
        color = "grey";
    } else if (powerDrain > 0 && powerOutput == 0) {
        // 完全断电
        color = "qradialgradient(cx:0.5, cy:0.5, radius:0.8, fx:0.5, fy:0.5, stop:0 #ff3333, stop:1 #ff0000)";
    } else if (powerOutput > powerDrain) {
        if (powerOutput * 0.85 < powerDrain) {
            // 电力紧张
            color = "qradialgradient(cx:0.5, cy:0.5, radius:0.8, fx:0.5, fy:0.5, stop:0 #ffff77, stop:1 #dddd00)";
        } else {
            // 电力充足
            color = "qradialgradient(cx:0.5, cy:0.5, radius:0.8, fx:0.5, fy:0.5, stop:0 #33ff33, stop:1 #00cc00)";
        }
    } else {
        // 电力不足
        color = "qradialgradient(cx:0.5, cy:0.5, radius:0.8, fx:0.5, fy:0.5, stop:0 #ff3333, stop:1 #ff0000)";
    }
    
    // 设置电力指示器样式
    float ratio = Globalsetting::getInstance().l.ratio;
    int powerSize = qRound(16 * ratio);
    int borderRadius = powerSize / 2;
    
    label->setStyleSheet(QString("background-color: %1; border-radius: %2px;").arg(color).arg(borderRadius));
}

// 设置玩家头像
void Ob::setAvatar(int playerIndex) {
    // 确保玩家索引有效
    if (playerIndex < 0 || playerIndex >= Ra2ob::MAXPLAYER || !g->_gameInfo.valid || !g->_gameInfo.players[playerIndex].valid) {
        return;
    }
    
    // 获取玩家昵称
    std::string name = g->_gameInfo.players[playerIndex].panel.playerNameUtf;
    QString nickname = QString::fromUtf8(name);
    QString playername = Globalsetting::getInstance().findNameByNickname(nickname);
    
    // 确定是玩家1还是玩家2
    bool isPlayer1 = (playerIndex == p1_index);
    bool& remoteLoaded = isPlayer1 ? remoteAvatarLoaded1 : remoteAvatarLoaded2;
    QString& currentNick = isPlayer1 ? currentNickname1 : currentNickname2;
    QPixmap& playerAvatar = isPlayer1 ? playerAvatar1 : playerAvatar2;
    
    // 如果已经成功加载过远程头像，且昵称没变，则不需要重新加载
    if (remoteLoaded && nickname == currentNick && !playerAvatar.isNull()) {
        return;
    }
    
    // 首先尝试加载本地头像
    if (!nickname.isEmpty()) {
        // 1. 首先尝试使用游戏名称查找本地头像
        if (!playername.isEmpty()) {
            QString localGameNamePath = QCoreApplication::applicationDirPath() + "/avatars/" + playername + ".png";
            if (QFile::exists(localGameNamePath)) {
                QPixmap localAvatar(localGameNamePath);
                if (!localAvatar.isNull()) {
                    playerAvatar = localAvatar;
                    // 标记为已成功加载头像，避免重复尝试
                    remoteLoaded = true;
                    displayAvatar(playerIndex);
                    return;
                }
            }
        }
        
        // 2. 如果游戏名称头像不存在，尝试使用昵称查找本地头像
        QString localAvatarPath = QCoreApplication::applicationDirPath() + "/avatars/" + nickname + ".png";
        if (QFile::exists(localAvatarPath)) {
            QPixmap localAvatar(localAvatarPath);
            if (!localAvatar.isNull()) {
                playerAvatar = localAvatar;
                // 标记为已成功加载头像，避免重复尝试
                remoteLoaded = true;
                displayAvatar(playerIndex);
                return;
            }
        }
        
        // 本地头像都不存在，尝试从远程服务器加载
        // 生成时间戳，用于避免缓存
        QString timestamp = QString::number(QDateTime::currentMSecsSinceEpoch());
        
        // 3. 优先使用游戏名称查找远程头像
        if (!playername.isEmpty()) {
            // TODO: 配置头像服务器地址
        QString remoteGameNameUrl = QString("https://your-server.com/avatars/%1.png?t=%2").arg(playername).arg(timestamp);
            if (checkRemoteFileExists(remoteGameNameUrl)) {
                // 远程头像存在，下载并显示
                downloadAvatar(remoteGameNameUrl, playerIndex);
                return;
            }
        }
        
        // 4. 如果游戏名称不存在或找不到对应头像，尝试使用昵称查找远程头像
        // TODO: 配置头像服务器地址
        QString remoteNicknameUrl = QString("https://your-server.com/avatars/%1.png?t=%2").arg(nickname).arg(timestamp);
        if (checkRemoteFileExists(remoteNicknameUrl)) {
            // 远程头像存在，下载并显示
            downloadAvatar(remoteNicknameUrl, playerIndex);
            return;
        }
    }
    
    // 如果远程头像都不存在，标记为已完成远程加载
    remoteLoaded = true;
    
    // 使用默认的阵营头像
    useDefaultAvatar(playerIndex);
}

// 检查远程文件是否存在
bool Ob::checkRemoteFileExists(const QString &url) {
    QNetworkRequest request(url);
    request.setRawHeader("User-Agent", "Ra2ob Qt Client");
    
    // 添加防缓存头
    request.setAttribute(QNetworkRequest::CacheLoadControlAttribute, QNetworkRequest::AlwaysNetwork);
    request.setRawHeader("Cache-Control", "no-cache");
    request.setRawHeader("Pragma", "no-cache");
    
    // 使用HEAD请求检查文件是否存在，减少数据传输量
    QNetworkReply *reply = networkManager->head(request);
    
    // 使用事件循环等待响应完成，避免异步问题
    QEventLoop loop;
    connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();
    
    bool exists = (reply->error() == QNetworkReply::NoError);
    
    reply->deleteLater();
    return exists;
}

// 下载头像
void Ob::downloadAvatar(const QString &url, int playerIndex) {
    QNetworkRequest request(url);
    request.setRawHeader("User-Agent", "Ra2ob Qt Client");
    
    // 添加防缓存头
    request.setAttribute(QNetworkRequest::CacheLoadControlAttribute, QNetworkRequest::AlwaysNetwork);
    request.setRawHeader("Cache-Control", "no-cache");
    request.setRawHeader("Pragma", "no-cache");
    
    QNetworkReply *reply = networkManager->get(request);
    
    // 使用事件循环等待下载完成，避免异步问题
    QEventLoop loop;
    connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();
    
    // 确定是玩家1还是玩家2
    bool isPlayer1 = (playerIndex == p1_index);
    bool& remoteLoaded = isPlayer1 ? remoteAvatarLoaded1 : remoteAvatarLoaded2;
    QPixmap& playerAvatar = isPlayer1 ? playerAvatar1 : playerAvatar2;
    
    if (reply->error() == QNetworkReply::NoError) {
        QByteArray imageData = reply->readAll();
        
        // 将图像数据加载到QPixmap
        QPixmap remoteAvatar;
        if (remoteAvatar.loadFromData(imageData)) {
            // 设置头像
            playerAvatar = remoteAvatar;
            
            // 标记为已成功加载远程头像
            remoteLoaded = true;
            
            displayAvatar(playerIndex);
        } else {
            remoteLoaded = true; // 即使加载失败也标记为已尝试
            useDefaultAvatar(playerIndex);
        }
    } else {
        remoteLoaded = true; // 即使下载失败也标记为已尝试
        useDefaultAvatar(playerIndex);
    }
    
    reply->deleteLater();
}

// 使用默认阵营头像
void Ob::useDefaultAvatar(int playerIndex) {
    // 确保玩家索引有效
    if (playerIndex < 0 || playerIndex >= Ra2ob::MAXPLAYER || !g->_gameInfo.valid || !g->_gameInfo.players[playerIndex].valid) {
        return;
    }
    
    // 确定是玩家1还是玩家2
    bool isPlayer1 = (playerIndex == p1_index);
    QPixmap& playerAvatar = isPlayer1 ? playerAvatar1 : playerAvatar2;
    
    QString defaultAvatarPath;
    
    // 获取玩家国家
    std::string country = g->_gameInfo.players[playerIndex].panel.country;
    QString countryStr = QString::fromStdString(country);
    
    // 根据国家选择头像 - 正确区分阵营
    if (countryStr == "Russians" || countryStr == "Cuba" || 
        countryStr == "Iraq" || countryStr == "Libya") {
        // 苏联阵营使用logo2.png
        defaultAvatarPath = QCoreApplication::applicationDirPath() + "/assets/panels/logo2.png";
    } else if (countryStr == "Americans" || countryStr == "British" || 
               countryStr == "France" || countryStr == "Germans" || 
               countryStr == "Korea") {
        // 盟军阵营使用logo.png
        defaultAvatarPath = QCoreApplication::applicationDirPath() + "/assets/panels/logo.png";
    } else if (countryStr == "Yuri") {
        // 尤里阵营可以使用专门的头像
        defaultAvatarPath = QCoreApplication::applicationDirPath() + "/assets/panels/logo3.png";
    }
    
    // 如果没有确定特定国家头像或文件不存在，使用默认头像
    if (defaultAvatarPath.isEmpty() || !QFile::exists(defaultAvatarPath)) {
        defaultAvatarPath = QCoreApplication::applicationDirPath() + "/avatars/default.png";
        
        // 如果默认头像也不存在，使用程序图标
        if (!QFile::exists(defaultAvatarPath)) {
            playerAvatar.load(":/icon/assets/icons/icon_64.png");
            displayAvatar(playerIndex);
            return;
        }
    }
    
    playerAvatar.load(defaultAvatarPath);
    displayAvatar(playerIndex);
}

// 显示当前加载的头像
void Ob::displayAvatar(int playerIndex) {
    // 确保玩家索引有效
    if (playerIndex < 0 || playerIndex >= Ra2ob::MAXPLAYER || !g->_gameInfo.valid || !g->_gameInfo.players[playerIndex].valid) {
        return;
    }
    
    // 确定是玩家1还是玩家2
    bool isPlayer1 = (playerIndex == p1_index);
    QPixmap& playerAvatar = isPlayer1 ? playerAvatar1 : playerAvatar2;
    QLabel* avatarLabel = isPlayer1 ? lb_avatar1 : lb_avatar2;
    
    if (playerAvatar.isNull()) {
        return;
    }
    
    // 计算适当的头像大小（考虑分辨率）
    float ratio = Globalsetting::getInstance().l.ratio;
    int avatarSize = qRound(48 * ratio);
    
    // 将头像设置为圆形
    QPixmap scaledAvatar = playerAvatar.scaled(avatarSize, avatarSize, 
                                            Qt::KeepAspectRatio, 
                                            Qt::SmoothTransformation);
    QPixmap roundedAvatar(scaledAvatar.size());
    roundedAvatar.fill(Qt::transparent);
    
    QPainter painter(&roundedAvatar);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    
    QPainterPath path;
    path.addEllipse(roundedAvatar.rect());
    painter.setClipPath(path);
    painter.drawPixmap(0, 0, scaledAvatar);
    
    // 设置到标签
    avatarLabel->setPixmap(roundedAvatar);
    avatarLabel->setFixedSize(avatarSize, avatarSize);
    
    // 确保圆角样式与头像大小匹配
    int borderRadius = avatarSize / 2;
    avatarLabel->setStyleSheet(QString("border-radius: %1px;").arg(borderRadius));
}

// 重置头像加载状态
void Ob::resetAvatarLoadStatus() {
    // 重置远程头像加载状态标志，这将使头像重新加载
    remoteAvatarLoaded1 = false;
    remoteAvatarLoaded2 = false;
    
    // 如果有当前玩家，重新加载头像
    if (p1_index >= 0 && p1_index < Ra2ob::MAXPLAYER && g->_gameInfo.valid && g->_gameInfo.players[p1_index].valid) {
        setAvatar(p1_index);
    }
    
    if (p2_index >= 0 && p2_index < Ra2ob::MAXPLAYER && g->_gameInfo.valid && g->_gameInfo.players[p2_index].valid) {
        setAvatar(p2_index);
    }
}

// 添加事件过滤器实现
bool Ob::eventFilter(QObject *watched, QEvent *event) {
    // 检查是否是分数标签上的鼠标事件
    if ((watched == lb_score1 || watched == lb_score2) && event->type() == QEvent::MouseButtonPress) {
        QMouseEvent *mouseEvent = static_cast<QMouseEvent*>(event);
        
        // 检查Ctrl键是否按下
        if (QApplication::keyboardModifiers() & Qt::ControlModifier) {
            // 确定是哪个玩家的分数标签
            bool isPlayer1 = (watched == lb_score1);
            QString nickname = isPlayer1 ? currentNickname1 : currentNickname2;
            QLabel* scoreLabel = isPlayer1 ? lb_score1 : lb_score2;
            
            // 如果昵称为空，不执行任何操作
            if (nickname.isEmpty()) {
                return true;
            }
            
            // 获取当前分数
            int currentScore = scoreLabel->text().toInt();
            
            // 左键点击增加分数
            if (mouseEvent->button() == Qt::LeftButton) {
                currentScore++;
                scoreLabel->setText(QString::number(currentScore));
                
                // 更新全局分数记录
                Globalsetting::getInstance().playerScores[nickname] = currentScore;
                
                
                // 发送分数变化信号
                emit scoreChanged(nickname, currentScore);
                
                return true;
            }
            // 右键点击减少分数
            else if (mouseEvent->button() == Qt::RightButton) {
                if (currentScore > 0) {
                    currentScore--;
                    scoreLabel->setText(QString::number(currentScore));
                    
                    // 更新全局分数记录
                    Globalsetting::getInstance().playerScores[nickname] = currentScore;
                   
                    
                    // 发送分数变化信号
                    emit scoreChanged(nickname, currentScore);
                }
                return true;
            }
        }
    }
    
    // 其他事件传递给基类处理
    return QWidget::eventFilter(watched, event);
}

// 添加设置PMB位置的方法
void Ob::setPmbPosition(int offsetX, int offsetY) {
    pmbOffsetX = offsetX;
    pmbOffsetY = offsetY;
    
    // 更新界面显示
    this->update();
}

// 添加设置PMB大小的方法
void Ob::setPmbSize(int width, int height) {
    pmbWidth = width;
    pmbHeight = height;
    
    // 更新界面显示
    this->update();
}

// 地图视频相关方法实现
void Ob::initMapVideoLabel() {
    lb_mapVideo = new QLabel(this);
    lb_mapVideo->setScaledContents(true);
    lb_mapVideo->setAttribute(Qt::WA_TransparentForMouseEvents);
    lb_mapVideo->hide(); // 初始隐藏
    mapVideoLoaded = false;
}

void Ob::loadMapVideoFramesByMapName(const QString& mapName) {
    mapVideoFrames.clear();
    currentMapVideoFrame = 0;
    QString videoDir = QCoreApplication::applicationDirPath() + "/assets/1v1png/";
    
    qDebug() << "当前地图名称:" << mapName;
    
    // 根据地图名称确定要加载的动画帧序列
    QStringList frameSequence;
    
    if (mapName == "2025冰天王-星夜冰天") {
        // 加载MAP3_00000.png-MAP3_00087.png-MAP2_00088.png-MAP2_00149.png
        for (int i = 0; i <= 87; ++i) {
            frameSequence.append(QString("MAP3_%1.png").arg(i, 5, 10, QChar('0')));
        }
        for (int i = 88; i <= 149; ++i) {
            frameSequence.append(QString("MAP2_%1.png").arg(i, 5, 10, QChar('0')));
        }
        qDebug() << "加载星夜冰天动画帧序列";
    }
    else if (mapName == "2025冰天王-星夜黄金冰天") {
        // 加载MAP4_00000.png-MAP4_00087.png-MAP2_00088.png-MAP2_00149.png
        for (int i = 0; i <= 87; ++i) {
            frameSequence.append(QString("MAP4_%1.png").arg(i, 5, 10, QChar('0')));
        }
        for (int i = 88; i <= 149; ++i) {
            frameSequence.append(QString("MAP2_%1.png").arg(i, 5, 10, QChar('0')));
        }
        qDebug() << "加载星夜黄金冰天动画帧序列";
    }
    else if (mapName == "2025冰天王-黄金冰天") {
        // 加载MAP2_00000.png-MAP2_00149.png
        for (int i = 0; i <= 149; ++i) {
            frameSequence.append(QString("MAP2_%1.png").arg(i, 5, 10, QChar('0')));
        }
        qDebug() << "加载黄金冰天动画帧序列";
    }
    else if (mapName == "2025冰天王-宝石冰天") {
        // 加载MAP1_00000.png-MAP1_00087.png-MAP2_00088.png-MAP2_00149.png
        for (int i = 0; i <= 87; ++i) {
            frameSequence.append(QString("MAP1_%1.png").arg(i, 5, 10, QChar('0')));
        }
        for (int i = 88; i <= 149; ++i) {
            frameSequence.append(QString("MAP2_%1.png").arg(i, 5, 10, QChar('0')));
        }
        qDebug() << "加载宝石冰天动画帧序列";
    }
    else if (mapName == "巴乔尔LE-尤复天梯" || mapName == "不列兹然湖LE-尤复天梯" || 
             mapName == "魔法一号之魂LE-尤复天梯" || mapName == "沙摩西岛-尤复天梯" ||
             mapName == "首要港口LE-尤复天梯" || mapName == "斯坦利公园-尤复天梯" ||
             mapName == "尤伦湖LE-尤复天梯" || mapName == "再见宣言LE-尤复天梯" ||
             mapName == "正式锦标赛D-LE-尤复天梯" || mapName == "变量S-尤复天梯" ||
             mapName == "猪猡湾LE-尤复天梯" || mapName == "咫尺天涯LE—尤复天梯") {
        // 新的地图加载逻辑：30帧MAP1_00013.png，然后20帧htw.png
        // 前25帧显示MAP1_00013.png
        for (int i = 0; i < 25; ++i) {
            frameSequence.append("MAP1_00013.png");
        }
        // 接下来35帧显示htw.png
        for (int i = 0; i < 35; ++i) {
            frameSequence.append("htw.png");
        }
        qDebug() << "加载尤复天梯地图动画帧序列:" << mapName;
    }
    else {
        // 其他地图名称不加载动画帧
        qDebug() << "地图名称不匹配，不加载PNG动画帧";
        // 设置加载完成标志，避免重复尝试
        mapVideoLoaded = true;
        return;
    }
    
    // 加载动画帧
    int loadedFrames = 0;
    QString appDir = QCoreApplication::applicationDirPath();
    for (const QString& frameName : frameSequence) {
        QString framePath;
        // 为htw.png使用不同的路径
        if (frameName == "htw.png") {
            framePath = appDir + "/assets/map/" + frameName;
        } else {
            framePath = videoDir + frameName;
        }
        QPixmap frame(framePath);
        if (!frame.isNull()) {
            mapVideoFrames.append(frame);
            loadedFrames++;
        } else {
            qDebug() << "警告: 无法加载动画帧:" << framePath;
        }
    }
    
    qDebug() << "PNG动画帧加载完成，共加载" << loadedFrames << "帧，总计" << frameSequence.size() << "帧";
    
    // 设置加载完成标志
    mapVideoLoaded = true;
}

void Ob::playMapVideo() {
    if (!mapVideoFrames.isEmpty() && mapVideoTimer && !mapVideoPlaying) {
        mapVideoTimer->start(40); // 每40毫秒一帧，25FPS
        mapVideoPlaying = true;
        qDebug() << "地图视频播放开始";
    }
}

void Ob::stopMapVideo() {
    if (mapVideoTimer) {
        mapVideoTimer->stop();
        mapVideoPlaying = false;
        qDebug() << "地图视频播放停止";
    }
}

QPixmap Ob::getMapVideoImage() {
    if (mapVideoPlaying && !mapVideoFrames.isEmpty() && currentMapVideoFrame < mapVideoFrames.size()) {
        return mapVideoFrames[currentMapVideoFrame];
    }
    return QPixmap();
}

void Ob::setMapVideoImage() {
    if (!lb_mapVideo) return;
    
    QPixmap pixmap = getMapVideoImage();
    if (!pixmap.isNull()) {
        lb_mapVideo->setPixmap(pixmap);
    }
}

void Ob::updateMapVideoDisplay() {
    if (!lb_mapVideo || !this->isVisible()) {
        if (lb_mapVideo) lb_mapVideo->hide();
        return;
    }
    
    if (!mapVideoPlaying || mapVideoFrames.isEmpty() || currentMapVideoFrame < 0 || currentMapVideoFrame >= mapVideoFrames.size()) {
        lb_mapVideo->hide();
        return;
    }
    
    // 设置当前帧
    setMapVideoImage();
    
    // 计算位置和大小（与ob1.cpp保持一致）
    float ratio = Globalsetting::getInstance().l.ratio;
    int frameW = qRound(900 * ratio);
    int frameH = qRound(513 * ratio);
    int x = (width() - frameW) / 2 - qRound(84 * ratio);
    int y = (height() - frameH) / 2;
    
    // 设置位置和大小
    lb_mapVideo->setGeometry(x, y, frameW, frameH);
    lb_mapVideo->show();
}

void Ob::setMapVideoPosition(int x, int y) {
    mapVideoX = x;
    mapVideoY = y;
    
    if (lb_mapVideo) {
        float ratio = Globalsetting::getInstance().l.ratio;
        int scaledX = qRound(mapVideoX * ratio);
        int scaledY = qRound(mapVideoY * ratio);
        lb_mapVideo->move(scaledX, scaledY);
    }
    
    qDebug() << "地图视频位置已更新: (" << mapVideoX << "," << mapVideoY << ")";
}

// OBS WebSocket相关方法实现
void Ob::initObsConnection() {
    // 创建WebSocket连接
    obsWebSocket = new QWebSocket("", QWebSocketProtocol::VersionLatest, this);
    
    // 连接信号
    connect(obsWebSocket, &QWebSocket::connected, this, &Ob::onObsConnected);
    connect(obsWebSocket, &QWebSocket::disconnected, this, &Ob::onObsDisconnected);
    connect(obsWebSocket, &QWebSocket::textMessageReceived, this, &Ob::onObsMessageReceived);
    
    // 尝试连接到OBS
    qDebug() << "正在连接OBS WebSocket:" << obsWebSocketUrl;
    obsWebSocket->open(QUrl(obsWebSocketUrl));
}



void Ob::onObsConnected() {
    obsConnected = true;
    obsAuthenticated = false;
    qDebug() << "OBS WebSocket连接成功，等待Hello消息";
}

void Ob::onObsDisconnected() {
    obsConnected = false;
    obsAuthenticated = false;
    qDebug() << "OBS WebSocket连接断开";
}

void Ob::onObsMessageReceived(const QString &message) {
    qDebug() << "收到OBS消息:" << message;
    
    QJsonDocument doc = QJsonDocument::fromJson(message.toUtf8());
    if (!doc.isObject()) {
        qDebug() << "无效的JSON消息";
        return;
    }
    
    QJsonObject obj = doc.object();
    int op = obj["op"].toInt();
    
    switch (op) {
        case 0: // Hello消息
            handleObsHello(obj);
            break;
        case 2: // Identified消息
            handleObsIdentified(obj);
            break;
        case 7: // RequestResponse消息
            handleObsRequestResponse(obj);
            break;
        default:
            qDebug() << "未处理的OBS消息类型:" << op;
            break;
    }
}

void Ob::handleObsHello(const QJsonObject &message) {
    qDebug() << "收到OBS Hello消息，开始认证";
    
    // 发送Identify消息进行认证
    QJsonObject identify;
    identify["op"] = 1; // Identify操作码
    identify["d"] = QJsonObject{
        {"rpcVersion", 1},
        {"authentication", obsPassword.isEmpty() ? QJsonValue() : obsPassword}
    };
    
    sendObsMessage(identify);
}

void Ob::handleObsIdentified(const QJsonObject &message) {
    qDebug() << "OBS认证成功";
    obsAuthenticated = true;
}



void Ob::switchObsScene(const QString &sourceName) {
    if (sourceName.isEmpty()) {
        qDebug() << "源名称为空，跳过OBS源显示/隐藏";
        return;
    }
    
    if (!obsAuthenticated) {
        qDebug() << "OBS未认证，无法控制源显示/隐藏";
        return;
    }
    
    // 先获取源的ID
    QJsonObject getIdMessage;
    getIdMessage["op"] = 6; // Request操作码
    getIdMessage["d"] = QJsonObject{
        {"requestType", "GetSceneItemId"},
        {"requestId", "get-id-hide-" + QString::number(QDateTime::currentMSecsSinceEpoch())},
        {"requestData", QJsonObject{
            {"sceneName", "红警"}, // 可以设置为具体的场景名称
            {"sourceName", sourceName}
        }}
    };
    
    // 发送获取ID的消息
    sendObsMessage(getIdMessage);
    
    qDebug() << "尝试获取OBS源ID以隐藏:" << sourceName;
}

void Ob::showObsSource(const QString &sourceName) {
    if (sourceName.isEmpty()) {
        qDebug() << "源名称为空，跳过OBS源显示";
        return;
    }
    
    if (!obsAuthenticated) {
        qDebug() << "OBS未认证，无法控制源显示";
        return;
    }
    
    // 先获取源的ID
    QJsonObject getIdMessage;
    getIdMessage["op"] = 6; // Request操作码
    getIdMessage["d"] = QJsonObject{
        {"requestType", "GetSceneItemId"},
        {"requestId", "get-id-show-" + QString::number(QDateTime::currentMSecsSinceEpoch())},
        {"requestData", QJsonObject{
            {"sceneName", "红警"}, // 可以设置为具体的场景名称
            {"sourceName", sourceName}
        }}
    };
    
    // 发送获取ID的消息
    sendObsMessage(getIdMessage);
    
    qDebug() << "尝试获取OBS源ID以显示:" << sourceName;
}

void Ob::sendObsMessage(const QJsonObject &message) {
    if (!obsWebSocket || !obsConnected) {
        qDebug() << "OBS WebSocket未连接，无法发送消息";
        return;
    }
    
    QJsonDocument doc(message);
    QString jsonString = doc.toJson(QJsonDocument::Compact);
    obsWebSocket->sendTextMessage(jsonString);
    qDebug() << "发送OBS消息:" << jsonString;
}

void Ob::refreshBrowserSource(const QString &sourceName) {
    if (sourceName.isEmpty()) {
        qDebug() << "源名称为空，跳过浏览器源刷新";
        return;
    }
    
    if (!obsAuthenticated) {
        qDebug() << "OBS未认证，无法刷新浏览器源";
        return;
    }
    
    // 使用PressInputPropertiesButton请求来刷新浏览器源缓存
    QJsonObject refreshMessage;
    refreshMessage["op"] = 6; // Request操作码
    refreshMessage["d"] = QJsonObject{
        {"requestType", "PressInputPropertiesButton"},
        {"requestId", "refresh-browser-" + QString::number(QDateTime::currentMSecsSinceEpoch())},
        {"requestData", QJsonObject{
            {"inputName", sourceName},
            {"propertyName", "refreshnocache"}
        }}
    };
    
    sendObsMessage(refreshMessage);
    qDebug() << "已发送刷新浏览器源缓存请求:" << sourceName;
}

void Ob::handleObsRequestResponse(const QJsonObject &message) {
    QJsonObject data = message["d"].toObject();
    QString requestType = data["requestType"].toString();
    QString requestId = data["requestId"].toString();
    bool requestStatus = data["requestStatus"].toObject()["result"].toBool();
    
    if (requestType == "GetSceneItemId") {
        if (requestStatus) {
            int sceneItemId = data["responseData"].toObject()["sceneItemId"].toInt();
            
            if (requestId.startsWith("get-id-hide-")) {
                // 隐藏源
                QJsonObject hideMessage;
                hideMessage["op"] = 6;
                hideMessage["d"] = QJsonObject{
                    {"requestType", "SetSceneItemEnabled"},
                    {"requestId", "hide-source-" + QString::number(QDateTime::currentMSecsSinceEpoch())},
                    {"requestData", QJsonObject{
                        {"sceneName", "红警"},
                        {"sceneItemId", sceneItemId},
                        {"sceneItemEnabled", false}
                    }}
                };
                sendObsMessage(hideMessage);
            } else if (requestId.startsWith("get-id-show-")) {
                // 显示源
                QJsonObject showMessage;
                showMessage["op"] = 6;
                showMessage["d"] = QJsonObject{
                    {"requestType", "SetSceneItemEnabled"},
                    {"requestId", "show-source-" + QString::number(QDateTime::currentMSecsSinceEpoch())},
                    {"requestData", QJsonObject{
                        {"sceneName", "红警"},
                        {"sceneItemId", sceneItemId},
                        {"sceneItemEnabled", true}
                    }}
                };
                sendObsMessage(showMessage);
            }
        } else {
            qDebug() << "获取OBS源ID失败:" << data["requestStatus"].toObject()["comment"].toString();
        }
    } else if (requestType == "SetSceneItemEnabled") {
        if (requestStatus) {
            if (requestId.startsWith("hide-source-")) {
                qDebug() << "OBS源隐藏控制成功";
            } else if (requestId.startsWith("show-source-")) {
                qDebug() << "OBS源显示控制成功";
            } else {
                qDebug() << "OBS源显示/隐藏控制成功";
            }
        } else {
            qDebug() << "OBS源显示/隐藏控制失败:" << data["requestStatus"].toObject()["comment"].toString();
        }
    } else if (requestType == "PressInputPropertiesButton") {
        if (requestStatus) {
            qDebug() << "OBS浏览器源缓存刷新成功";
        } else {
            qDebug() << "OBS浏览器源缓存刷新失败:" << data["requestStatus"].toObject()["comment"].toString();
        }
    }
}

// 处理玩家分数变化的槽函数
void Ob::onPlayerScoreChanged(const QString& playerName, int newScore) {
    qDebug() << "Ob: 玩家分数已更新:" << playerName << "->" << newScore;
    
    // 加分完毕后，先显示转场源和BGM源
    showObsSource("转场");      // 显示转场源
    showObsSource("BGM");       // 显示BGM源
    qDebug() << "Ob: 已显示转场源和BGM源";
    
    // 1700毫秒后隐藏游戏中源
    QTimer::singleShot(1700, [this]() {
        switchObsScene("游戏中");  // 隐藏"游戏中"源
        qDebug() << "Ob: 延迟1700ms后已隐藏游戏中源";
    });
}

// 实现强制内存回收方法
void Ob::forceMemoryCleanup() {
    qDebug() << "Ob：执行定期内存回收";
    
    // 强制系统回收内存
    #ifdef Q_OS_WIN
        SetProcessWorkingSetSize(GetCurrentProcess(), (SIZE_T)-1, (SIZE_T)-1);
    #endif
    
    // 清理图像缓存，但仅在游戏无效时清理所有缓存
    if (!g->_gameInfo.valid) {
        // 游戏无效，清理所有缓存
        flagCache.clear();
        unitIconCache.clear();
        QPixmapCache::clear();
        
        // 重置加载状态
        unitIconsLoaded = false;
        flagsLoaded = false;
        
        qDebug() << "Ob：游戏无效，清理所有缓存";
    } else {
        // 游戏有效，只清理部分缓存以避免影响性能
        QPixmapCache::clear();
        qDebug() << "Ob：游戏有效，只清理PixmapCache";
    }
}

// 滚动文字相关方法实现
void Ob::initScrollText() {
    if (!lb_scrollText) {
        lb_scrollText = new QLabel(this);
        lb_scrollText->setStyleSheet("color: white; background: transparent;");
        lb_scrollText->hide();
    }
    
    if (!scrollTimer) {
        scrollTimer = new QTimer(this);
        connect(scrollTimer, &QTimer::timeout, this, &Ob::updateScrollText);
    }
    
    // 设置默认滚动文字
    if (scrollText.isEmpty()) {
        scrollText = "2025冰天王赞助商:  神存富贵: 10000元  大西洋: 10000元  零叉: 5500元  战网对战平台: 5000元  肉肉: 5000元  红警V神: 1000元  祖国国宝: 1000元  脑子强: 1000元  陆陆顺: 500元  魔方疯才: 500元  Jackie: 500元  黑色幽默日: 500元  古道照颜色: 300元  汪汪: 200元";
    }
}

void Ob::updateScrollText() {
    if (!lb_scrollText || scrollText.isEmpty()) {
        return;
    }
    
    // 获取当前缩放比例
    float ratio = static_cast<float>(height()) / 768.0f;
    
    // 设置滚动文字字体
    QFont scrollFont(!miSansBoldFamily.isEmpty() ? 
                    miSansBoldFamily : "Microsoft YaHei", 
                    qRound(10 * ratio), QFont::Normal);
    
    lb_scrollText->setFont(scrollFont);
    
    // 计算文本尺寸
    QFontMetrics fm(scrollFont);
    int textWidth = fm.horizontalAdvance(scrollText);
    int textHeight = fm.height();
    
    // 固定滚动区域宽度为200px（按比例缩放）
    int scrollAreaWidth = qRound(130 * ratio);
    
    // 计算滚动容器位置（右下角区域）
    int containerX = width() - qRound(160 * ratio) + 40 * ratio;
    int containerY = height() - textHeight - 2 * ratio;
    
    // 设置滚动容器的位置和大小（固定200px宽度）
    lb_scrollText->setGeometry(containerX, containerY, scrollAreaWidth, textHeight);
    
    // 设置剪切区域，只显示容器内的内容
    lb_scrollText->setStyleSheet(QString("QLabel { "
                                        "background: transparent; "
                                        "color: white; "
                                        "padding: 0px; "
                                        "margin: 0px; "
                                        "}"));
    
    // 获取所有现有的滚动文字子标签
    QList<QLabel*> scrollLabels = lb_scrollText->findChildren<QLabel*>();
    
    // 更新现有标签的位置
    for (QLabel* label : scrollLabels) {
        QRect geometry = label->geometry();
        geometry.moveLeft(geometry.x() - 2); // 每次向左移动2像素
        label->setGeometry(geometry);
        
        // 如果标签完全移出左边界，删除它
        if (geometry.right() < 0) {
            label->deleteLater();
        }
    }
    
    // 检查是否需要创建新的滚动文字
    bool needNewLabel = true;
    if (!scrollLabels.isEmpty()) {
        // 找到最右边的标签
        int rightmostX = -1000;
        for (QLabel* label : scrollLabels) {
            if (label->geometry().x() > rightmostX) {
                rightmostX = label->geometry().x();
            }
        }
        // 只有当最右边的标签移动到足够左边时才创建新标签
        if (rightmostX > scrollAreaWidth - textWidth - 40) {
            needNewLabel = false;
        }
    }
    
    // 如果需要，创建新的滚动文字标签
    if (needNewLabel) {
        QLabel* newLabel = new QLabel(lb_scrollText);
        newLabel->setFont(scrollFont);
        newLabel->setText(scrollText);
        newLabel->setStyleSheet("background: transparent; color: white;");
        newLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        newLabel->setGeometry(scrollAreaWidth, 0, textWidth, textHeight);
        
        // 创建带描边效果的文字图像
        QPixmap textPixmap(textWidth, textHeight);
        textPixmap.fill(Qt::transparent);
        
        QPainter painter(&textPixmap);
        painter.setRenderHint(QPainter::Antialiasing);
        
        // 设置描边
        QPen outlinePen(QColor("#60D1E0"));
        outlinePen.setWidthF(0.3);
        painter.setPen(outlinePen);
        painter.setFont(scrollFont);
        
        // 绘制描边文字
        QPainterPath textPath;
        textPath.addText(0, fm.ascent(), scrollFont, scrollText);
        painter.strokePath(textPath, outlinePen);
        
        // 填充文字
        painter.fillPath(textPath, QBrush(Qt::white));
        
        painter.end();
        
        // 将图像设置到标签
        newLabel->setPixmap(textPixmap);
        newLabel->setText(""); // 清空文本，使用图像显示
        newLabel->show();
    }
    
    // 清空父标签的文本
    lb_scrollText->setText("");
}

void Ob::startScrollText() {
    if (scrollTimer && !scrollText.isEmpty()) {
        // 获取当前缩放比例
        float ratio = static_cast<float>(height()) / 768.0f;
        int scaledWidth = qRound(256 * ratio);
        int scrollAreaWidth = scaledWidth - 80 * ratio;
        
        scrollPosition = scrollAreaWidth; // 从滚动区域右边开始
        scrollTimer->start(50); // 每50毫秒更新一次
    }
}

void Ob::stopScrollText() {
    if (scrollTimer) {
        scrollTimer->stop();
    }
    if (lb_scrollText) {
        lb_scrollText->hide();
    }
}

void Ob::setHideDate(bool hide) {
    hideDate = hide;
    if (hide) {
        startScrollText();
    } else {
        stopScrollText();
    }
    update(); // 触发重绘
}

void Ob::setScrollText(const QString &text) {
    scrollText = text;
    if (hideDate && !text.isEmpty()) {
        startScrollText();
    } else if (text.isEmpty()) {
        stopScrollText();
    }
}

void Ob::initSpyInfiltrationLabels() {
    // 获取全局设置中的比例
    float ratio = Globalsetting::getInstance().l.ratio;
    
    // 初始化玩家1间谍渗透提示标签
    lb_spyAlert1 = new QLabel(this);
    lb_spyAlert1->setStyleSheet("background: transparent;");
    lb_spyAlert1->resize(spyAlertWidth * ratio, spyAlertHeight * ratio);
    lb_spyAlert1->setScaledContents(true);
    lb_spyAlert1->hide();
    lb_spyAlert1->raise(); // 确保在最上层显示
    
    // 初始化玩家2间谍渗透提示标签
    lb_spyAlert2 = new QLabel(this);
    lb_spyAlert2->setStyleSheet("background: transparent;");
    lb_spyAlert2->resize(spyAlertWidth * ratio, spyAlertHeight * ratio);
    lb_spyAlert2->setScaledContents(true);
    lb_spyAlert2->hide();
    lb_spyAlert2->raise(); // 确保在最上层显示
    
    // 初始化玩家1间谍提示定时器
    spyAlertTimer1 = new QTimer(this);
    spyAlertTimer1->setSingleShot(true);
    connect(spyAlertTimer1, &QTimer::timeout, [this]() {
        if (lb_spyAlert1) {
            lb_spyAlert1->hide();
        }
        qDebug() << "玩家1间谍渗透提示已隐藏";
    });
    
    // 初始化玩家2间谍提示定时器
    spyAlertTimer2 = new QTimer(this);
    spyAlertTimer2->setSingleShot(true);
    connect(spyAlertTimer2, &QTimer::timeout, [this]() {
        if (lb_spyAlert2) {
            lb_spyAlert2->hide();
        }
        qDebug() << "玩家2间谍渗透提示已隐藏";
    });
}

// 动态间谍渗透状态跟踪（支持0-7索引的所有玩家）
static std::map<int, bool> g_lastPlayerTechInfiltrated;
static std::map<int, bool> g_lastPlayerBarracksInfiltrated;
static std::map<int, bool> g_lastPlayerWarFactoryInfiltrated;

void Ob::resetSpyInfiltrationStatus() {
    // 清空动态间谍渗透状态跟踪
    g_lastPlayerTechInfiltrated.clear();
    g_lastPlayerBarracksInfiltrated.clear();
    g_lastPlayerWarFactoryInfiltrated.clear();

    qDebug() << "重置所有玩家间谍渗透状态跟踪变量";
}

void Ob::updateSpyInfiltrationDisplay() {
    if (!g->_gameInfo.valid) return;
    
    // 检查配置开关和会员权限
    if (!Globalsetting::getInstance().s.show_spy_infiltration || 
        !MemberManager::getInstance().hasFeatureAccess(MemberManager::FeatureType::SpyInfiltration)) {
        return; // 配置关闭或非会员用户不显示间谍渗透功能
    }
    
    // 获取当前游戏时间（秒）
    int gameTime;
    if (g->_gameInfo.realGameTime > 0) {
        gameTime = g->_gameInfo.realGameTime;
    } else {
        gameTime = g->_gameInfo.currentFrame / Ra2ob::GAMESPEED;
    }
    
    // 只在游戏运行30秒后才开始检测渗透状态
    if (gameTime < 30) return;
    
    int validPlayerCount = 0; // 跟踪有效玩家的相对顺序
    
    // 遍历所有可能的玩家索引（0-7）
    for (int i = 0; i < Ra2ob::MAXPLAYER; i++) {
        // 检查玩家是否有效
        if (!g->_gameInfo.players[i].valid) continue;
        
        // 获取当前玩家基址
        uintptr_t playerBaseAddr = g->_playerBases[i];
        if (playerBaseAddr == 0) continue;
        
        // 检查当前玩家的间谍渗透状态
        bool techInfiltrated = g->r.getBool(playerBaseAddr + Ra2ob::SIDE0_TECH_INFILTRATED_OFFSET) ||
                               g->r.getBool(playerBaseAddr + Ra2ob::SIDE1_TECH_INFILTRATED_OFFSET) ||
                               g->r.getBool(playerBaseAddr + Ra2ob::SIDE2_TECH_INFILTRATED_OFFSET);
        bool barracksInfiltrated = g->r.getBool(playerBaseAddr + Ra2ob::BARRACKS_INFILTRATED_OFFSET);
        bool warFactoryInfiltrated = g->r.getBool(playerBaseAddr + Ra2ob::WARFACTORY_INFILTRATED_OFFSET);
        
        // 获取上次状态
        bool lastTech = g_lastPlayerTechInfiltrated[i];
        bool lastBarracks = g_lastPlayerBarracksInfiltrated[i];
        bool lastWarFactory = g_lastPlayerWarFactoryInfiltrated[i];
        
        // 只在状态从false变为true时显示提示（新的渗透事件）
        // 只为前两个有效玩家显示UI（由于UI限制）
        if (validPlayerCount < 2) {
            if (techInfiltrated && !lastTech) {
                showSpyAlert(validPlayerCount, "spy3");  // 传入相对顺序而非实际索引
            }
            if (barracksInfiltrated && !lastBarracks) {
                showSpyAlert(validPlayerCount, "spy2");  // 传入相对顺序而非实际索引
            }
            if (warFactoryInfiltrated && !lastWarFactory) {
                showSpyAlert(validPlayerCount, "spy1");  // 传入相对顺序而非实际索引
            }
        } else {
            // 对于其他玩家，只记录日志但不显示UI
            if (techInfiltrated && !lastTech) {
                qDebug() << QString("玩家%1高科被渗透（无UI显示）").arg(i);
            }
            if (barracksInfiltrated && !lastBarracks) {
                qDebug() << QString("玩家%1兵营被渗透（无UI显示）").arg(i);
            }
            if (warFactoryInfiltrated && !lastWarFactory) {
                qDebug() << QString("玩家%1重工被渗透（无UI显示）").arg(i);
            }
        }
        
        // 更新状态
        g_lastPlayerTechInfiltrated[i] = techInfiltrated;
        g_lastPlayerBarracksInfiltrated[i] = barracksInfiltrated;
        g_lastPlayerWarFactoryInfiltrated[i] = warFactoryInfiltrated;
        
        validPlayerCount++; // 增加有效玩家计数
    }
}

void Ob::showSpyAlert(int playerIndex, const QString& spyType) {
    // 基于相对顺序分配UI元素
    if (playerIndex == 0) {
        // 第一个有效玩家使用左侧位置
        QLabel* alertLabel = lb_spyAlert1;
        QTimer* alertTimer = spyAlertTimer1;
        
        if (!alertLabel || !alertTimer) return;
        
        // 构建图片路径 - 注意文件名首字母大写
        QString imagePath = QString("./assets/panels/%1.png").arg(spyType.left(1).toUpper() + spyType.mid(1));
        
        // 检查文件是否存在
        if (!QFile::exists(imagePath)) {
            qDebug() << "间谍渗透图片不存在:" << imagePath;
            return;
        }
        
        // 加载原始图片
        QPixmap originalPixmap(imagePath);
        if (originalPixmap.isNull()) {
            qDebug() << "无法加载间谍渗透图片:" << imagePath;
            return;
        }
        
        // 获取比例和目标尺寸
        float ratio = Globalsetting::getInstance().l.ratio;
        int targetWidth = spyAlertWidth * ratio;
        int targetHeight = spyAlertHeight * ratio;
        
        // 缩放图片
        QPixmap scaledPixmap = originalPixmap.scaled(targetWidth, targetHeight, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        alertLabel->setPixmap(scaledPixmap);
        
        // 设置位置 - 在玩家名称下方
        int alertX = 620 * ratio;  // 根据实际玩家0名称位置调整
        int alertY = 78 * ratio; // 根据实际玩家0名称位置调整
        
        alertLabel->move(alertX, alertY);
        alertLabel->show();
        alertLabel->raise();
        
        // 停止之前的定时器并启动新的5秒定时器
        alertTimer->stop();
        alertTimer->start(5000);
        
        qDebug() << QString("显示第一个有效玩家间谍渗透提示: %1").arg(spyType);
    } else if (playerIndex == 1) {
        // 第二个有效玩家使用右侧位置
        QLabel* alertLabel = lb_spyAlert2;
        QTimer* alertTimer = spyAlertTimer2;
        
        if (!alertLabel || !alertTimer) return;
        
        // 构建图片路径 - 注意文件名首字母大写
        QString imagePath = QString("./assets/panels/%1.png").arg(spyType.left(1).toUpper() + spyType.mid(1));
        
        // 检查文件是否存在
        if (!QFile::exists(imagePath)) {
            qDebug() << "间谍渗透图片不存在:" << imagePath;
            return;
        }
        
        // 加载原始图片
        QPixmap originalPixmap(imagePath);
        if (originalPixmap.isNull()) {
            qDebug() << "无法加载间谍渗透图片:" << imagePath;
            return;
        }
        
        // 获取比例和目标尺寸
        float ratio = Globalsetting::getInstance().l.ratio;
        int targetWidth = spyAlertWidth * ratio;
        int targetHeight = spyAlertHeight * ratio;
        
        // 缩放图片
        QPixmap scaledPixmap = originalPixmap.scaled(targetWidth, targetHeight, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        alertLabel->setPixmap(scaledPixmap);
        
        // 设置位置 - 在玩家名称下方
        int alertX = 1120 * ratio; // 根据实际玩家1名称位置调整
        int alertY = 78 * ratio;  // 根据实际玩家1名称位置调整
        
        alertLabel->move(alertX, alertY);
        alertLabel->show();
        alertLabel->raise();
        
        // 停止之前的定时器并启动新的5秒定时器
        alertTimer->stop();
        alertTimer->start(5000);
        
        qDebug() << QString("显示第二个有效玩家间谍渗透提示: %1").arg(spyType);
    } else {
        // 超出UI显示范围的玩家，记录日志但不显示
        qDebug() << QString("玩家索引%1超出UI显示范围，间谍类型：%2").arg(playerIndex).arg(spyType);
    }
}

// 初始化建筑偷取事件标签
// 初始化建筑偷取事件系统
void Ob::onBuildingTheftDetected(const QString& thiefName, const QString& victimName,
                                const QString& buildingName, int count) {
    qDebug() << "onBuildingTheftDetected called:" << thiefName << "stole" << buildingName << "from" << victimName;
    if (theftAlertManager) {
        theftAlertManager->showTheftAlert(thiefName, victimName, buildingName, count);
    }
}

void Ob::onMinerLossDetected(const QString& playerName, const QString& minerType, int count) {
    qDebug() << "onMinerLossDetected called:" << playerName << "lost" << count << minerType;
    if (theftAlertManager) {
        theftAlertManager->showMinerLossAlert(playerName, minerType, count);
    }
}

