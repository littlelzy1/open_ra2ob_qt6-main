#include "obteam.h"
#include "playerstatustracker.h"
#include "buildingdetector.h"
#include "theftalertmanager.h"
#include <QPainter>
#include <QScreen>
#include <QCoreApplication>
#include <QDebug>
#include <QFont>
#include <QFontMetrics>
#include <QDir>
#include <QFile>
#include <QFontDatabase>
#include <QMouseEvent>
#include <QApplication>
#include <QLabel>
#include <QPixmapCache>
#include <QCursor>
#include <QProcess>
#include "membermanager.h"

#ifdef Q_OS_WIN
    #include <windows.h>
#endif

// 动态间谍渗透状态跟踪（支持0-7索引的所有玩家）
static std::map<int, bool> g_lastPlayerTechInfiltrated;
static std::map<int, bool> g_lastPlayerBarracksInfiltrated;
static std::map<int, bool> g_lastPlayerWarFactoryInfiltrated;

ObTeam::ObTeam(QWidget *parent) : QWidget(parent), g(Ra2ob::Game::getInstance()) {
    // 初始化变量
    lastGameValid = false;
    teamsDetected = false;
    enoughPlayersDetected = false;  // 初始化为false
    
    // 初始化建造栏显示控制
    showProducingBlocks = true;  // 默认显示建造栏
    team1DisplayedBuildings = 0;  // 初始化为0
    team2DisplayedBuildings = 0;  // 初始化为0
    
    // 初始化网络管理器用于远程头像获取
    networkManager = new QNetworkAccessManager(this);
    
    this->setWindowFlags(Qt::WindowMinMaxButtonsHint | Qt::FramelessWindowHint |
                         Qt::WindowStaysOnTopHint | Qt::Tool);
    this->setAttribute(Qt::WA_TranslucentBackground);
    
    #ifdef Q_OS_WIN
        HWND hwnd = (HWND)this->winId();
        LONG_PTR exStyle = GetWindowLongPtr(hwnd, GWL_EXSTYLE);
        SetWindowLongPtr(hwnd, GWL_EXSTYLE, exStyle | WS_EX_NOACTIVATE);
    #endif

    QScreen *screen = QGuiApplication::primaryScreen();
    this->setGeometry(screen->geometry());

    gls = &Globalsetting::getInstance();
    
    // 初始化字体设置
    playerNameFont = QFont("MiSans-Bold", 14, QFont::Bold);
    economyFont = QFont("MiSans-Bold", 12, QFont::Normal);
    unitCountFont = QFont("MiSans-Bold", 10, QFont::Bold); // 初始化单位数量字体
    producingStatusFont = QFont("MiSans-Bold", 9, QFont::Bold); // 初始化建造状态字体
    mapNameFont = QFont("MiSans-Bold", 12, QFont::Bold); // 添加地图名称字体
    
    // 初始化位置偏移量 - 为两位玩家单独设置
    player1NameOffsetX = 119;   // 第一位玩家名称水平偏移量
    player1NameOffsetY = 50;   // 第一位玩家名称垂直偏移量
    player1BalanceOffsetX = 218; // 第一位玩家余额水平偏移量
    player1BalanceOffsetY = 57; // 第一位玩家余额垂直偏移量
    player1SpentOffsetX = 218; // 第一位玩家总花费水平偏移量
    player1SpentOffsetY = 35; // 第一位玩家总花费垂直偏移量
    player1KillsOffsetX = 206; // 第一位玩家摧毁数水平偏移量
    player1KillsOffsetY = 57; // 第一位玩家摧毁数垂直偏移量
    player1PowerOffsetX = 177; // 第一位玩家电力指示灯X偏移
    player1PowerOffsetY = 45;  // 第一位玩家电力指示灯Y偏移
    
    player2NameOffsetX = 340;    // 第二位玩家名称水平偏移量
    player2NameOffsetY = 50;    // 第二位玩家名称垂直偏移量
    player2BalanceOffsetX = 440;  // 第二位玩家余额水平偏移量
    player2BalanceOffsetY = 57  ;  // 第二位玩家余额垂直偏移量
    player2SpentOffsetX = 440;   // 第二位玩家总花费水平偏移量
    player2SpentOffsetY = 35;   // 第二位玩家总花费垂直偏移量
    player2KillsOffsetX = 425;   // 第二位玩家摧毁数水平偏移量
    player2KillsOffsetY = 57;    // 第二位玩家摧毁数垂直偏移量
    player2PowerOffsetX = 396;   // 第二位玩家电力指示灯X偏移
    player2PowerOffsetY = 45;    // 第二位玩家电力指示灯Y偏移
    
    // 颜色背景图片尺寸和位置
    colorBgWidth = 219;        // 颜色背景图片宽度
    colorBgHeight = 75;        // 颜色背景图片高度
    player1ColorBgX = 65;      // 第一位玩家颜色背景X偏移
    player1ColorBgY = 0;       // 第一位玩家颜色背景Y偏移
    player2ColorBgX = 284;     // 第二位玩家颜色背景X偏移
    player2ColorBgY = 0;       // 第二位玩家颜色背景Y偏移
    
    // 通用偏移量（用于其他玩家）
    playerNameOffsetX = 15;
    playerNameOffsetY = 40;
    economyOffsetX = 15;
    economyOffsetY = 60;
    powerIndicatorSize = 13;   // 电力指示灯大小
    
    // 初始化加载状态标志
    flagsLoaded = false;
    unitIconsLoaded = false; // 初始化单位图标加载标志
    producingIconsLoaded = false; // 初始化建造图标加载标志
    colorBgsLoaded = false; // 初始化颜色背景图片加载标志
    
    // 初始化电力闪烁定时器和状态
    powerBlinkTimer = new QTimer(this);
    powerBlinkState = false;
    connect(powerBlinkTimer, &QTimer::timeout, [this]() {
        powerBlinkState = !powerBlinkState; // 切换闪烁状态
        this->update(); // 刷新界面
    });
    powerBlinkTimer->start(300); // 300毫秒切换一次状态
    
    // 初始化建造栏区域检测定时器
    producingBlocksTimer = new QTimer(this);
    connect(producingBlocksTimer, &QTimer::timeout, this, &ObTeam::checkMousePositionForProducingBlocks);
    producingBlocksTimer->setInterval(100); // 100毫秒检测一次鼠标位置
    producingBlocksTimer->start();
    
    // 获取应用程序目录路径
    QString appDir = QCoreApplication::applicationDirPath();
    
    // 加载MiSans-Bold字体
    QString fontPath = appDir + "/assets/fonts/MiSans-Bold.ttf";
    if (QFile::exists(fontPath)) {
        int fontId = QFontDatabase::addApplicationFont(fontPath);
        if (fontId != -1) {
            QStringList fontFamilies = QFontDatabase::applicationFontFamilies(fontId);
            if (!fontFamilies.isEmpty()) {
                QString miSansFontFamily = fontFamilies.first();
                qDebug() << "成功加载MiSans-Bold字体，字体族:" << miSansFontFamily;
                playerNameFont = QFont(miSansFontFamily, 22, QFont::Bold);
                playerNameFont.setStyleHint(QFont::SansSerif, QFont::PreferAntialias);
            } else {
                qDebug() << "成功加载MiSans-Bold字体";
            }
        } else {
            qDebug() << "无法加载MiSans-Bold字体:" << fontPath;
        }
    } else {
        qDebug() << "MiSans-Bold字体文件不存在:" << fontPath;
    }
    
    // 加载特效字体LLDEtechnoGlitch-Bold0，确保正确加载
    QString technoGlitchPath = appDir + "/assets/fonts/LLDEtechnoGlitch-Bold0.ttf";
    if (QFile::exists(technoGlitchPath)) {
        int technoFontId = QFontDatabase::addApplicationFont(technoGlitchPath);
        if (technoFontId != -1) {
            QStringList technoFontFamilies = QFontDatabase::applicationFontFamilies(technoFontId);
            if (!technoFontFamilies.isEmpty()) {
                technoGlitchFontFamily = technoFontFamilies.first();
                qDebug() << "成功加载LLDEtechnoGlitch-Bold0字体，字体族:" << technoGlitchFontFamily;
                
                // 测试字体是否可用
                QFont testFont(technoGlitchFontFamily);
                if (testFont.exactMatch()) {
                    qDebug() << "特效字体精确匹配成功";
                } else {
                    qDebug() << "特效字体精确匹配失败，将使用回退字体";
                }
            } else {
                qDebug() << "LLDEtechnoGlitch-Bold0字体加载成功但无法获取字体族";
            }
        } else {
            qDebug() << "无法加载LLDEtechnoGlitch-Bold0字体:" << technoGlitchPath;
        }
    } else {
        qDebug() << "LLDEtechnoGlitch-Bold0字体文件不存在:" << technoGlitchPath;
    }
    
    // 确保2v2颜色图片目录存在
    QDir colorDir(appDir + "/assets/2v2colors");
    if (!colorDir.exists()) {
        QDir().mkpath(colorDir.path());
        qDebug() << "创建2v2colors目录:" << colorDir.path();
    }
    
    // 初始化BO数标签
    lb_bo_number = new QOutlineLabel(this);
    lb_bo_number->setStyleSheet("color: white;");
    
    // 使用预加载的字体并应用ratio缩放
    QFont boFont("MiSans-Bold", qRound(12 * gls->l.ratio));
    boFont.setBold(true);
    lb_bo_number->setFont(boFont);
    lb_bo_number->setText("BO" + QString::number(boNumber));
    // 初始隐藏BO数标签，只在游戏进行时显示
    lb_bo_number->hide();
    
    setLayoutByScreen();
    
    // 创建定时器检测游戏状态
    gameTimer = new QTimer(this);
    connect(gameTimer, &QTimer::timeout, this, &ObTeam::onGameTimerTimeout);
    gameTimer->setInterval(500); // 每秒检测一次
    gameTimer->start();
    
    // 初始化团队状态跟踪计时器
    teamStatusTimer = new QTimer(this);
    connect(teamStatusTimer, &QTimer::timeout, this, &ObTeam::checkTeamStatus);
    teamStatusTimer->setInterval(500); // 每秒检测一次
    
    // 初始化内存清理定时器
    memoryCleanupTimer = new QTimer(this);
    connect(memoryCleanupTimer, &QTimer::timeout, this, &ObTeam::forceMemoryCleanup);
    memoryCleanupTimer->setInterval(60000); // 每60秒执行一次内存回收
    memoryCleanupTimer->start();
    
    // 初始化头像刷新定时器
    avatarRefreshTimer = new QTimer(this);
    connect(avatarRefreshTimer, &QTimer::timeout, this, &ObTeam::resetAvatarLoadStatus);
    avatarRefreshTimer->setInterval(60000); // 设置为60秒刷新一次
    avatarRefreshTimer->start();
    

    
    // 直接显示窗口
    this->show();
    qDebug() << "2V2模式: 显示观察者界面";
    
    // 初始化比分UI
    initScoreUI();
    
    // 初始化地图名称标签
    lb_mapname = nullptr;
    
    // 初始化窗口为隐藏状态，将由外部代码控制显示
    this->hide();
    qDebug() << "2V2模式: 观察者界面已创建，等待显示";
    
    // 初始化PlayerStatusTracker
    statusTracker = new PlayerStatusTracker(this);
    
    // 初始化建筑偷取检测器
    buildingDetector = new BuildingDetector(this);
    theftAlertManager = new TheftAlertManager(this);
    
    // 连接建筑偷取检测信号
    connect(buildingDetector, &BuildingDetector::buildingTheftDetected,
            this, &ObTeam::onBuildingTheftDetected);
    
    // 连接矿车丢失检测信号
    connect(buildingDetector, &BuildingDetector::minerLossDetected,
            this, &ObTeam::onMinerLossDetected);
    
    // 启动建筑偷取检测
    buildingDetector->startDetection(500);
    
    // 设置建筑检测器引用到偷取提示管理器
    theftAlertManager->setBuildingDetector(buildingDetector);
    
    // 初始化动画系统
    initAnimationSystem();
}

ObTeam::~ObTeam() {
    if (gameTimer) {
        gameTimer->stop();
        delete gameTimer;
        gameTimer = nullptr;
    }
    
    // 清理网络管理器
    if (networkManager) {
        delete networkManager;
        networkManager = nullptr;
    }
    
    if (powerBlinkTimer) {
        powerBlinkTimer->stop();
        delete powerBlinkTimer;
        powerBlinkTimer = nullptr;
    }
    
    if (teamStatusTimer) {
        teamStatusTimer->stop();
        delete teamStatusTimer;
        teamStatusTimer = nullptr;
    }
    
    if (memoryCleanupTimer) {
        memoryCleanupTimer->stop();
        delete memoryCleanupTimer;
        memoryCleanupTimer = nullptr;
    }
    
    // 清理建造栏检测定时器
    if (producingBlocksTimer) {
        producingBlocksTimer->stop();
        delete producingBlocksTimer;
        producingBlocksTimer = nullptr;
    }
    
    if (lb_bo_number) {
        delete lb_bo_number;
        lb_bo_number = nullptr;
    }
    
    if (lb_mapname) {
        delete lb_mapname;
        lb_mapname = nullptr;
    }
    
    // 清理PlayerStatusTracker
    if (statusTracker) {
        statusTracker->stopTracking();
        delete statusTracker;
        statusTracker = nullptr;
    }
    
    // 清理建筑偷取检测器
    if (buildingDetector) {
        buildingDetector->stopDetection();
        delete buildingDetector;
        buildingDetector = nullptr;
    }
    
    // 清理偷取提示管理器
    if (theftAlertManager) {
        delete theftAlertManager;
        theftAlertManager = nullptr;
    }
    
    // 清理动画资源
    if (mainAnimGroup) {
        mainAnimGroup->stop();
        delete mainAnimGroup; // 会自动删除包含的动画对象
        mainAnimGroup = nullptr;
    }
    
    if (avatarRefreshTimer) {
        avatarRefreshTimer->stop();
        delete avatarRefreshTimer;
        avatarRefreshTimer = nullptr;
    }
}

// 添加设置BO数的方法
void ObTeam::setBONumber(int boNumber) {
    this->boNumber = boNumber;
    
    if (lb_bo_number) {
        // 更新字体大小以应用当前ratio
        QFont boFont = lb_bo_number->font();
        boFont.setPointSize(qRound(12 * gls->l.ratio));
        lb_bo_number->setFont(boFont);
        
        lb_bo_number->setText("BO " + QString::number(boNumber));
        lb_bo_number->adjustSize();
        
        // 更新BO数标签位置
        float ratio = gls->l.ratio;
        int wCenter = gls->l.top_m;
        int centerY = qRound(82 * ratio);
        
        lb_bo_number->setGeometry(
            wCenter - lb_bo_number->width() / 2,
            centerY - lb_bo_number->height() / 2,
            lb_bo_number->width(),
            lb_bo_number->height()
        );
    }
}

void ObTeam::paintEvent(QPaintEvent *) {
    // 界面隐藏时不执行绘制操作
    if (!this->isVisible()) return;
    
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    
    // 只有当游戏有效时才绘制面板
    if (g._gameInfo.valid) {
        float ratio = gls->l.ratio;
        int wCenter = gls->l.top_m;
        
        // 面板动画 - 从上方滑入
        if (animationActive || panelAnimProgress < 1.0) {
            int baseHeight = 98;
            int pHeight = qRound(baseHeight * ratio);
            
            // 计算Y轴偏移
            float yOffset = -pHeight * (1.0f - panelAnimProgress);
            
            // 应用变换
            painter.save();
            painter.translate(0, yOffset);
            
            // 绘制顶部面板底层
            paintTopPanel(&painter);
            
            // 恢复画家状态
            painter.restore();
        } else {
            // 正常绘制顶部面板
            paintTopPanel(&painter);
        }
        
        // 绘制右侧面板
        paintRightPanel(&painter);
        
        // 处理发光效果
        if ((animationActive || glowAnimProgress < 1.0) && glowAnimProgress > 0.0) {
            painter.save();
            
            // 创建径向渐变作为发光效果
            float glowRadius = qRound(200 * ratio) * glowAnimProgress;
            float glowOpacity = 0.7f * (1.0f - glowAnimProgress);
            
            QRadialGradient gradient(wCenter, qRound(40 * ratio), glowRadius);
            gradient.setColorAt(0, QColor(255, 255, 200, qRound(255 * glowOpacity)));
            gradient.setColorAt(1, QColor(255, 255, 200, 0));
            
            painter.setCompositionMode(QPainter::CompositionMode_Plus);
            painter.fillRect(QRect(wCenter - glowRadius, qRound(40 * ratio) - glowRadius, 
                                  glowRadius * 2, glowRadius * 2), gradient);
            painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
            
            painter.restore();
        }
        
        // 绘制玩家信息元素 - 使用淡入效果
        if (animationActive || elementsAnimProgress < 1.0) {
            painter.save();
            painter.setOpacity(elementsAnimProgress);
            
            // 玩家颜色背景
            paintPlayerColorBgs(&painter);
            
            // 玩家名称
            paintPlayerNames(&painter);
            
            // 国旗 - 使用透明度但不使用缩放
            paintCountryFlags(&painter);
            
            painter.restore();
        } else {
            // 正常绘制
            paintPlayerColorBgs(&painter);
            paintPlayerNames(&painter);
            paintCountryFlags(&painter);
        }
        
        // 绘制其他元素
        paintUnits(&painter);
        paintProducingBlocks(&painter);
        paintPowerStatus(&painter);
        paintTeamEconomyBars(&painter);
        paintMapName(&painter);
        
        // 最后绘制顶部面板上层(2v2bg3.png)
        int baseWidth = 1167;
        int bg3Height = 76;
        
        // 设置BO数标签位置
        if (lb_bo_number) {
            int centerY = qRound(82 * ratio);
            
            // 确保BO数字体大小随屏幕比例缩放
            QFont boFont = lb_bo_number->font();
            boFont.setPointSize(qRound(12 * ratio));
            lb_bo_number->setFont(boFont);
            
            lb_bo_number->adjustSize();
            lb_bo_number->setGeometry(
                wCenter - lb_bo_number->width() / 2,
                centerY - lb_bo_number->height() / 2,
                lb_bo_number->width(),
                lb_bo_number->height()
            );
            
            // 给BO数字添加淡入效果
            if (animationActive || elementsAnimProgress < 1.0) {
                lb_bo_number->setGraphicsEffect(createFadeEffect(elementsAnimProgress));
            } else {
                lb_bo_number->setGraphicsEffect(nullptr);
            }
            // 不需要在这里调用show，位置更新即可，显示控制由toggleOb方法管理
        }
        
        // 处理比分标签动画
        if (team1ScoreLabel && team2ScoreLabel) {
            // 计算面板区域
            int panelLeft = wCenter - qRound(1167 * ratio) / 2;
            int panelRight = wCenter + qRound(1167 * ratio) / 2;
            
            // 保存原始位置 - 这些是最终位置
            int team1X = panelLeft + qRound(player1NameOffsetX * ratio) - team1ScoreLabel->width() / 2 - qRound(85 * ratio);
            int team1Y = qRound(11 * ratio);
            int team2X = panelRight - qRound(player1NameOffsetX * ratio) - team2ScoreLabel->width() / 2 - qRound(-85 * ratio);
            int team2Y = qRound(11 * ratio);
            
            // 在动画期间应用效果
            if (animationActive || elementsAnimProgress < 1.0) {
                // 计算动画中的位置 (从上往下滑入)
                int verticalOffset = qRound(50 * ratio * (1.0 - elementsAnimProgress));
                
                // 应用位置和透明度
                team1ScoreLabel->move(team1X, team1Y - verticalOffset);
                team2ScoreLabel->move(team2X, team2Y - verticalOffset);
                
                // 添加淡入效果
                team1ScoreLabel->setGraphicsEffect(createFadeEffect(elementsAnimProgress));
                team2ScoreLabel->setGraphicsEffect(createFadeEffect(elementsAnimProgress));
            } else {
                // 动画结束后确保正确位置和无特效
                team1ScoreLabel->move(team1X, team1Y);
                team2ScoreLabel->move(team2X, team2Y);
                team1ScoreLabel->setGraphicsEffect(nullptr);
                team2ScoreLabel->setGraphicsEffect(nullptr);
            }
        }
        
        int pWidth = qRound(baseWidth * ratio);
        int pHeight3 = qRound(bg3Height * ratio);
        
        if (!topBg3Image.isNull()) {
            QPixmap scaledBg = topBg3Image.scaled(pWidth, pHeight3, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
            
            // 对上层面板应用滑入动画
            if (animationActive || panelAnimProgress < 1.0) {
                float yOffset = -pHeight3 * (1.0f - panelAnimProgress);
                painter.drawPixmap(wCenter - pWidth / 2, yOffset, scaledBg);
            } else {
                painter.drawPixmap(wCenter - pWidth / 2, 0, scaledBg);
            }
        }
    }
}

void ObTeam::paintTopPanel(QPainter *painter) {
    float ratio = gls->l.ratio;
    int wCenter = gls->l.top_m;

    // 基础图片尺寸
    int baseWidth = 1167;
    int baseHeight = 98;
    
    // 根据屏幕比例缩放
    int pWidth = qRound(baseWidth * ratio);
    int pHeight = qRound(baseHeight * ratio);
    
    // 只绘制底层图片(2v2bg1.png)
    if (!topBg1Image.isNull()) {
        QPixmap scaledBg = topBg1Image.scaled(pWidth, pHeight, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
        painter->drawPixmap(wCenter - pWidth / 2, 0, scaledBg);
    }
    
    // 移除对2v2bg3.png的绘制，它将在paintEvent的最后绘制
}

void ObTeam::paintRightPanel(QPainter *painter) {
    // 使用已加载的右侧栏背景
    if (!rightBarImage.isNull()) {
        QPixmap bgToUse = rightBarImage.scaled(gls->l.right_w, this->height(), 
                                            Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
        
        painter->save();
        
        // 创建剪切路径以排除地图区域
        QPainterPath clipPath;
        clipPath.addRect(gls->l.right_x, 0, gls->l.right_w, this->height()); // 整个侧边栏
        clipPath.addRect(gls->l.map_x, gls->l.map_y, gls->l.map_w, gls->l.map_h); // 地图区域
        
        // 应用剪切路径
        painter->setClipPath(clipPath);
        painter->setClipRegion(QRegion(gls->l.right_x, 0, gls->l.right_w, this->height()) - 
                               QRegion(gls->l.map_x, gls->l.map_y, gls->l.map_w, gls->l.map_h),
                               Qt::ReplaceClip);

        painter->drawPixmap(gls->l.right_x, 0, bgToUse);
        
        painter->restore();
    }
    
    // 游戏时间和日期显示 - 与ob.cpp实现方式相同
    if (g._gameInfo.valid) {
        // 计算游戏时间
        int gameTime;
        if (g._gameInfo.realGameTime > 0) {
            gameTime = g._gameInfo.realGameTime;
        } else {
            gameTime = g._gameInfo.currentFrame / Ra2ob::GAMESPEED;
        }
        
        // 转换为时:分:秒格式
        int hours = gameTime / 3600;
        int minutes = (gameTime % 3600) / 60;
        int seconds = gameTime % 60;
        
        QString timeStr = QString("%1:%2:%3")
            .arg(hours, 2, 10, QChar('0'))
            .arg(minutes, 2, 10, QChar('0'))
            .arg(seconds, 2, 10, QChar('0'));
        
        // 使用特效字体
        QFont scaledTimeFont;
        if (!technoGlitchFontFamily.isEmpty()) {
            scaledTimeFont = QFont(technoGlitchFontFamily, qRound(20 * gls->l.ratio), QFont::Bold);
        } else {
            scaledTimeFont = QFont("LLDEtechnoGlitch-Bold0", qRound(20 * gls->l.ratio), QFont::Bold);
        }
        
        // 计算文本尺寸和位置
        QFontMetrics fm(scaledTimeFont);
        int textWidth = fm.horizontalAdvance(timeStr);
        int textHeight = fm.height();
        int wCenter = (gls->l.w - gls->l.right_x) / 2 + gls->l.right_x;
        int textX = wCenter - textWidth / 2;
        int textY = gls->l.right_header_h / 2 - textHeight / 2 + textHeight; 
        
        // 绘制游戏时间
        painter->save();
        painter->setFont(scaledTimeFont);
        painter->setPen(Qt::white);
        painter->drawText(textX, textY, timeStr);
        painter->restore();
        
        // 日期显示
        QDate currentDate = QDate::currentDate();
        QString dateStr = currentDate.toString("yyyy/MM/dd");
        
        // 使用相同的特效字体
        QFont scaledDateFont;
        if (!technoGlitchFontFamily.isEmpty()) {
            scaledDateFont = QFont(technoGlitchFontFamily, qRound(20 * gls->l.ratio), QFont::Bold);
        } else {
            scaledDateFont = QFont("LLDEtechnoGlitch-Bold0", qRound(20 * gls->l.ratio), QFont::Bold);
        }
        
        QFontMetrics dateFm(scaledDateFont);
        int dateWidth = dateFm.horizontalAdvance(dateStr);
        int dateX = gls->l.right_x + gls->l.right_w - dateWidth - qRound(40 * gls->l.ratio);
        int dateY = gls->l.h - qRound(10 * gls->l.ratio);
        
        // 绘制日期
        painter->save();
        painter->setFont(scaledDateFont);
        painter->setPen(Qt::white);
        painter->drawText(dateX, dateY, dateStr);
        painter->restore();
    }
}

void ObTeam::paintPlayerNames(QPainter *painter) {
    // 只有当有队伍和玩家信息时才绘制
    if (uniqueTeams.size() < 2) {
        return;
    }
    
    float ratio = gls->l.ratio;
    int wCenter = gls->l.top_m;
    int pWidth = qRound(1167 * ratio); // 与顶部面板宽度一致
    
    // 设置字体 - 使用可自定义的字体并根据屏幕比例缩放
    QFont scaledNameFont = playerNameFont;
    scaledNameFont.setPointSizeF(playerNameFont.pointSizeF() * ratio);
    scaledNameFont.setBold(true); // 确保加粗
    painter->setFont(scaledNameFont);
    
    // 设置文本颜色
    painter->setPen(Qt::white);
    
    // 计算面板区域
    int panelLeft = wCenter - pWidth / 2;
    int panelRight = wCenter + pWidth / 2;
    int middleX = wCenter; // 中心位置
    
    // 创建经济信息字体 - 使用特效字体
    QFont technoFont;
    if (!technoGlitchFontFamily.isEmpty()) {
        technoFont = QFont(technoGlitchFontFamily, qRound(18 * ratio), QFont::Bold);
    } else {
        technoFont = QFont("LLDEtechnoGlitch-Bold0", qRound(12 * ratio), QFont::Bold);
    }
    
    // 创建摧毁数量专用字体 - 使用相同的特效字体但大小可单独设置
    QFont killsFont;
    if (!technoGlitchFontFamily.isEmpty()) {
        killsFont = QFont(technoGlitchFontFamily, qRound(killsFontSize * ratio), QFont::Bold);
    } else {
        killsFont = QFont("LLDEtechnoGlitch-Bold0", qRound(killsFontSize * ratio), QFont::Bold);
    }
    
    QFontMetrics nameFm(scaledNameFont);
    QFontMetrics technoFm(technoFont);
    QFontMetrics killsFm(killsFont);
    
    // 设置玩家名称最大宽度（根据屏幕比例缩放）
    int maxNameWidth = qRound(100 * ratio); // 调回更小的值，确保名称有适当限制
    
    // 第一队玩家（左侧）
    if (!team1Players.isEmpty()) {
        int playerCount = team1Players.size();
        
        for (int i = 0; i < playerCount && i < 4; ++i) { // 最多显示4个玩家
            int playerIdx = team1Players[i];
            if (playerIdx >= 0 && playerIdx < Ra2ob::MAXPLAYER && g._gameInfo.players[playerIdx].valid) {
                // 获取玩家昵称
                QString playerNickname = QString::fromUtf8(g._gameInfo.players[playerIdx].panel.playerNameUtf);
                
                // 使用gls->findNameByNickname获取映射的玩家实际名称
                QString playerName = gls->findNameByNickname(playerNickname);
                
                // 如果没有找到映射，则使用原始昵称
                QString displayName = playerName.isEmpty() ? playerNickname : playerName;
                
                // 限制玩家名称宽度，直接截断过长名称而不使用省略号
                int nameLen = displayName.length();
                while (nameLen > 0 && nameFm.horizontalAdvance(displayName) > maxNameWidth) {
                    displayName = displayName.left(--nameLen);
                }
                
                // 获取玩家经济信息
                int balance = g._gameInfo.players[playerIdx].panel.balance;
                int creditSpent = g._gameInfo.players[playerIdx].panel.creditSpent;
                
                // 格式化经济信息：分开符号和数值
                QString balanceSymbol = "$";
                QString balanceValue = QString("%1").arg(balance);
                QString spentSymbol = "T";
                QString spentValue = QString("%1k").arg(creditSpent / 1000.0, 0, 'f', 1);
                
                // 根据玩家索引选择对应的偏移量
                int nameOffsetX, nameOffsetY, balanceOffsetX, balanceOffsetY, spentOffsetX, spentOffsetY;
                if (i == 0) { // 第一位玩家
                    nameOffsetX = player1NameOffsetX;
                    nameOffsetY = player1NameOffsetY;
                    balanceOffsetX = player1BalanceOffsetX;
                    balanceOffsetY = player1BalanceOffsetY;
                    spentOffsetX = player1SpentOffsetX;
                    spentOffsetY = player1SpentOffsetY;
                } else if (i == 1) { // 第二位玩家
                    nameOffsetX = player2NameOffsetX;
                    nameOffsetY = player2NameOffsetY;
                    balanceOffsetX = player2BalanceOffsetX;
                    balanceOffsetY = player2BalanceOffsetY;
                    spentOffsetX = player2SpentOffsetX;
                    spentOffsetY = player2SpentOffsetY;
                } else { // 其他玩家使用通用偏移量
                    nameOffsetX = playerNameOffsetX;
                    nameOffsetY = playerNameOffsetY;
                    balanceOffsetX = economyOffsetX;
                    balanceOffsetY = economyOffsetY;
                    spentOffsetX = economyOffsetX + 70;
                    spentOffsetY = economyOffsetY;
                }
                
                // 计算这个玩家名称的位置 - 使用居中显示
                int nameWidth = nameFm.horizontalAdvance(displayName); // 计算名称宽度用于居中
                int playerX = panelLeft + qRound(nameOffsetX * ratio) - nameWidth / 2; // 居中显示名称
                int nameY = qRound(nameOffsetY * ratio);
                
                // 计算余额符号和数值的位置
                int balanceSymbolX = panelLeft + qRound(balanceOffsetX * ratio);
                int balanceValueX = balanceSymbolX + qRound(10 * ratio); // 符号后15像素显示数值
                int balanceY = qRound(balanceOffsetY * ratio);
                
                // 计算总花费符号和数值的位置
                int spentSymbolX = panelLeft + qRound(spentOffsetX * ratio);
                int spentValueX = spentSymbolX + qRound(10 * ratio); // 符号后15像素显示数值
                int spentY = qRound(spentOffsetY * ratio);
                
                // 获取玩家颜色代码 - 颜色背景功能已移除
                std::string playerColor = g._gameInfo.players[playerIdx].panel.color;
                
                // 绘制玩家名称
                painter->setFont(scaledNameFont);
                painter->drawText(playerX, nameY, displayName); // 使用截断后的名称
                
                // 绘制余额符号和数值 - 使用特效字体
                painter->setFont(technoFont);
                painter->setPen(Qt::white);
                painter->drawText(balanceSymbolX, balanceY, balanceSymbol);
                painter->drawText(balanceValueX, balanceY, balanceValue);
                painter->drawText(spentSymbolX, spentY, spentSymbol);
                painter->drawText(spentValueX, spentY, spentValue);
                
                // 计算并绘制摧毁数量
                if (i < 2) { // 只为前两位玩家显示摧毁数
                    int kills = g._gameInfo.players[playerIdx].score.kills;
                    QString killsValue = QString::number(kills);
                    
                    int killsOffsetX = (i == 0) ? player1KillsOffsetX : player2KillsOffsetX;
                    int killsOffsetY = (i == 0) ? player1KillsOffsetY : player2KillsOffsetY;
                    
                    // 使用专用字体绘制摧毁数量
                    painter->setFont(killsFont);
                    
                    // 计算文本宽度用于居中显示
                    int killsTextWidth = killsFm.horizontalAdvance(killsValue);
                    
                    // 设置居中位置：基础位置X + 偏移量 - 文本宽度的一半
                    int killsX = panelLeft + qRound(killsOffsetX * ratio) - killsTextWidth / 2;
                    int killsY = qRound(killsOffsetY * ratio);
                    
                    painter->drawText(killsX, killsY, killsValue);
                }
            }
        }
    }
    
    // 第二队玩家（右侧）
    if (!team2Players.isEmpty()) {
        int playerCount = team2Players.size();
        
        for (int i = 0; i < playerCount && i < 4; ++i) { // 最多显示4个玩家
            int playerIdx = team2Players[i];
            if (playerIdx >= 0 && playerIdx < Ra2ob::MAXPLAYER && g._gameInfo.players[playerIdx].valid) {
                // 获取玩家昵称
                QString playerNickname = QString::fromUtf8(g._gameInfo.players[playerIdx].panel.playerNameUtf);
                
                // 使用gls->findNameByNickname获取映射的玩家实际名称
                QString playerName = gls->findNameByNickname(playerNickname);
                
                // 如果没有找到映射，则使用原始昵称
                QString displayName = playerName.isEmpty() ? playerNickname : playerName;
                
                // 限制玩家名称宽度，直接截断过长名称而不使用省略号
                int nameLen = displayName.length();
                while (nameLen > 0 && nameFm.horizontalAdvance(displayName) > maxNameWidth) {
                    displayName = displayName.left(--nameLen);
                }
                
                // 获取玩家经济信息
                int balance = g._gameInfo.players[playerIdx].panel.balance;
                int creditSpent = g._gameInfo.players[playerIdx].panel.creditSpent;
                
                // 格式化经济信息：分开符号和数值
                QString balanceSymbol = "$";
                QString balanceValue = QString("%1").arg(balance);
                QString spentSymbol = "T";
                QString spentValue = QString("%1k").arg(creditSpent / 1000.0, 0, 'f', 1);
                
                // 根据玩家索引选择对应的偏移量
                int nameOffsetX, nameOffsetY, balanceOffsetX, balanceOffsetY, spentOffsetX, spentOffsetY;
                if (i == 0) { // 第一位玩家
                    nameOffsetX = player1NameOffsetX;
                    nameOffsetY = player1NameOffsetY;
                    balanceOffsetX = player1BalanceOffsetX;
                    balanceOffsetY = player1BalanceOffsetY;
                    spentOffsetX = player1SpentOffsetX;
                    spentOffsetY = player1SpentOffsetY;
                } else if (i == 1) { // 第二位玩家
                    nameOffsetX = player2NameOffsetX;
                    nameOffsetY = player2NameOffsetY;
                    balanceOffsetX = player2BalanceOffsetX;
                    balanceOffsetY = player2BalanceOffsetY;
                    spentOffsetX = player2SpentOffsetX;
                    spentOffsetY = player2SpentOffsetY;
                } else { // 其他玩家使用通用偏移量
                    nameOffsetX = playerNameOffsetX;
                    nameOffsetY = playerNameOffsetY;
                    balanceOffsetX = economyOffsetX;
                    balanceOffsetY = economyOffsetY;
                    spentOffsetX = economyOffsetX + 70;
                    spentOffsetY = economyOffsetY;
                }
                
                // 计算文本宽度以便居中显示和右对齐
                int nameWidth = nameFm.horizontalAdvance(displayName); // 使用截断后的名称宽度
                int balanceValueWidth = technoFm.horizontalAdvance(balanceValue);
                int balanceSymbolWidth = technoFm.horizontalAdvance(balanceSymbol);
                int spentValueWidth = technoFm.horizontalAdvance(spentValue);
                int spentSymbolWidth = technoFm.horizontalAdvance(spentSymbol);
                
                // 计算这个玩家名称的位置 - 居中显示
                // 对于第二队，从右侧开始计算居中位置
                int playerX = panelRight - qRound(nameOffsetX * ratio) - nameWidth / 2;
                int nameY = qRound(nameOffsetY * ratio);
                
                // 计算余额符号和数值的位置 - 保持符号在左，数值在右的一致格式，并向左偏移100px
                int balanceSymbolX = panelRight - qRound(balanceOffsetX * ratio) - qRound(50 * ratio);
                int balanceValueX = balanceSymbolX + qRound(10 * ratio); // 符号右侧15像素显示数值
                int balanceY = qRound(balanceOffsetY * ratio);
                
                // 计算总花费符号和数值的位置 - 保持符号在左，数值在右的一致格式，并向左偏移100px
                int spentSymbolX = panelRight - qRound(spentOffsetX * ratio) - qRound(50 * ratio);
                int spentValueX = spentSymbolX + qRound(10 * ratio); // 符号右侧15像素显示数值
                int spentY = qRound(spentOffsetY * ratio);
                
                // 获取玩家颜色代码 - 颜色背景功能已移除
                std::string playerColor = g._gameInfo.players[playerIdx].panel.color;
                
                // 绘制玩家名称
                painter->setFont(scaledNameFont);
                painter->drawText(playerX, nameY, displayName); // 使用截断后的名称
                
                // 绘制余额符号和数值 - 使用特效字体
                painter->setFont(technoFont);
                painter->setPen(Qt::white);
                painter->drawText(balanceSymbolX, balanceY, balanceSymbol);
                painter->drawText(balanceValueX, balanceY, balanceValue);
                painter->drawText(spentSymbolX, spentY, spentSymbol);
                painter->drawText(spentValueX, spentY, spentValue);
                
                // 计算并绘制摧毁数量 - 第二队玩家
                if (i < 2) { // 只为前两位玩家显示摧毁数
                    int kills = g._gameInfo.players[playerIdx].score.kills;
                    QString killsValue = QString::number(kills);
                    
                    int killsOffsetX = (i == 0) ? player1KillsOffsetX : player2KillsOffsetX;
                    int killsOffsetY = (i == 0) ? player1KillsOffsetY : player2KillsOffsetY;
                    
                    // 使用专用字体绘制摧毁数量
                    painter->setFont(killsFont);
                    
                    // 计算文本宽度用于居中显示
                    int killsTextWidth = killsFm.horizontalAdvance(killsValue);
                    
                    // 设置居中位置：基础位置X(从右侧计算) - 文本宽度的一半
                    int killsX = panelRight - qRound(killsOffsetX * ratio) - killsTextWidth / 2;
                    int killsY = qRound(killsOffsetY * ratio);
                    
                    painter->drawText(killsX, killsY, killsValue);
                }
            }
        }
    }
}

void ObTeam::paintCountryFlags(QPainter *painter) {
    if (uniqueTeams.size() < 2) {
        return;
    }
    
    float ratio = gls->l.ratio;
    int wCenter = gls->l.top_m;
    int pWidth = qRound(1167 * ratio); // 与顶部面板宽度一致
    
    // 计算每个队伍的绘制区域
    int panelLeft = wCenter - pWidth / 2;
    int panelRight = wCenter + pWidth / 2;
    int middleX = wCenter; // 中心位置
    
    // 国旗尺寸和位置
    int flagWidth = qRound(85 * ratio);
    int flagHeight = qRound(55 * ratio);
    int flagSpacing = qRound(0 * ratio); // 国旗之间的间距
    
    // 第一队玩家国旗位置（右侧面板左半部分）
    int team1FlagsX = gls->l.right_x + qRound(1 * ratio);
    int team1FlagsY = qRound(170 * ratio); // 在右侧面板顶部下方100px处
    
    // 第二队玩家国旗位置（右侧面板左半部分，但在第一队下方500px处）
    int team2FlagsX = team1FlagsX;
    int team2FlagsY = team1FlagsY + qRound(445 * ratio); // 在第一队国旗下方500px处
    
    // 绘制第一队玩家国旗
    if (!team1Players.isEmpty()) {
        int colorBarHeight = qRound(7 * ratio); // 颜色条高度
        
        for (int i = 0; i < team1Players.size() && i < 2; ++i) { // 最多显示2个玩家
            int playerIdx = team1Players[i];
            if (playerIdx >= 0 && playerIdx < Ra2ob::MAXPLAYER && g._gameInfo.players[playerIdx].valid) {
                QString country = QString::fromStdString(g._gameInfo.players[playerIdx].panel.country);
                QColor playerColor = QColor(QString::fromStdString("#" + g._gameInfo.players[playerIdx].panel.color));
                
                // 使用缓存的国旗图片
                QPixmap flag;
                if (flagCache.contains(country)) {
                    flag = flagCache[country];
                } else if (!flagsLoaded) { // 只有在国旗未加载时才尝试加载
                    flag = getCountryFlag(country);
                    if (!flag.isNull()) {
                        flagCache.insert(country, flag);
                    }
                }
                
                // 计算这个国旗的位置
                int flagX = team1FlagsX + i * (flagWidth + flagSpacing);
                int colorBarY = team1FlagsY - colorBarHeight;
                
                // 绘制颜色条
                painter->fillRect(QRect(flagX, colorBarY, flagWidth, colorBarHeight), playerColor);
                
                // 绘制国旗
                if (!flag.isNull()) {
                    painter->drawPixmap(flagX, team1FlagsY, flagWidth, flagHeight, flag);
                }
            }
        }
    }
    
    // 绘制第二队玩家国旗
    if (!team2Players.isEmpty()) {
        int colorBarHeight = qRound(7 * ratio); // 颜色条高度
        
        for (int i = 0; i < team2Players.size() && i < 2; ++i) { // 最多显示2个玩家
            int playerIdx = team2Players[i];
            if (playerIdx >= 0 && playerIdx < Ra2ob::MAXPLAYER && g._gameInfo.players[playerIdx].valid) {
                QString country = QString::fromStdString(g._gameInfo.players[playerIdx].panel.country);
                QColor playerColor = QColor(QString::fromStdString("#" + g._gameInfo.players[playerIdx].panel.color));
                
                // 使用缓存的国旗图片
                QPixmap flag;
                if (flagCache.contains(country)) {
                    flag = flagCache[country];
                } else if (!flagsLoaded) { // 只有在国旗未加载时才尝试加载
                    flag = getCountryFlag(country);
                    if (!flag.isNull()) {
                        flagCache.insert(country, flag);
                    }
                }
                
                // 计算这个国旗的位置
                int flagX = team2FlagsX + i * (flagWidth + flagSpacing);
                int colorBarY = team2FlagsY - colorBarHeight;
                
                // 绘制颜色条
                painter->fillRect(QRect(flagX, colorBarY, flagWidth, colorBarHeight), playerColor);
                
                // 绘制国旗
                if (!flag.isNull()) {
                    painter->drawPixmap(flagX, team2FlagsY, flagWidth, flagHeight, flag);
                }
            }
        }
    }
    
    // 标记国旗已加载
    flagsLoaded = true;
}

void ObTeam::paintUnits(QPainter *painter) {
    // 确保只在游戏有效时绘制单位
    if (!g._gameInfo.valid) {
        return; // 游戏无效，不绘制单位
    }

    if (uniqueTeams.size() < 2) {
        return;
    }
    
    float ratio = gls->l.ratio;
    
    // 国旗尺寸和位置（从paintCountryFlags中获取相同的值以保持一致性）
    int flagWidth = qRound(85 * ratio);
    int flagHeight = qRound(55 * ratio);
    
    // 单位图标尺寸和位置
    int scaledUnitWidth = qRound(unitIconWidth * ratio);
    int scaledUnitHeight = qRound(unitIconHeight * ratio);
    int scaledUnitSpacing = qRound(unitSpacing * ratio);
    
    // 使用与paintCountryFlags相同的国旗位置值，保持一致性
    int team1FlagsX = gls->l.right_x + qRound(8 * ratio);
    int team1FlagsY = qRound(110 * ratio);
    
    // 第二队玩家国旗位置（右侧面板左半部分，但在第一队下方500px处）
    int team2FlagsX = team1FlagsX;
    int team2FlagsY = team1FlagsY + qRound(445 * ratio);
    
    // 单位区域起始Y坐标（国旗下方）
    int team1UnitsStartY = team1FlagsY + flagHeight + qRound(unitDisplayOffsetY * ratio);
    int team2UnitsStartY = team2FlagsY + flagHeight + qRound(unitDisplayOffsetY * ratio);
    
    // 设置单位数量字体
    QFont scaledUnitFont = unitCountFont;
    scaledUnitFont.setPointSizeF(unitCountFont.pointSizeF() * ratio);
    QFontMetrics unitFm(scaledUnitFont);
    
    // 绘制第一队玩家单位
    if (!team1Players.isEmpty()) {
        for (int i = 0; i < team1Players.size() && i < 2; ++i) { // 最多显示2个玩家
            int playerIdx = team1Players[i];
            if (playerIdx >= 0 && playerIdx < Ra2ob::MAXPLAYER && g._gameInfo.players[playerIdx].valid) {
                // 计算这个玩家单位的起始位置
                int unitColumnX = team1FlagsX + i * (flagWidth + qRound(1 * ratio));
                int unitY = team1UnitsStartY;
                
                // 获取玩家颜色
                QColor playerColor = QColor(QString::fromStdString("#" + g._gameInfo.players[playerIdx].panel.color));
                
                // 遍历玩家的单位，最多显示8个
                int displayedUnits = 0;
                
                // 获取玩家的单位信息
                for (const auto& unit : g._gameInfo.players[playerIdx].units.units) {
                    if (displayedUnits >= 7 || unit.num <= 0 || !unit.show) {
                        continue; // 最多显示8个单位或跳过数量为0的单位
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
                        painter->drawPixmap(unitColumnX, unitY, radiusIcon);
                        
                        // 绘制数量背景
                        int bgWidth = scaledUnitWidth;
                        int bgHeight = qRound(10 * ratio);
                        int bgY = unitY + scaledUnitHeight - bgHeight;
                        
                        // 创建带圆角的数量背景路径
                        QPainterPath path;
                        path.addRoundedRect(QRectF(unitColumnX, bgY, bgWidth, bgHeight), 
                                           qRound(2 * ratio), qRound(2 * ratio));
                        
                        painter->save();
                        painter->setRenderHint(QPainter::Antialiasing);
                        painter->fillPath(path, playerColor);
                        painter->restore();
                        
                        // 绘制单位数量 - 使用特效字体
                        painter->save();
                        
                        // 使用LLDEtechnoGlitch特效字体
                        QFont technoFont;
                        if (!technoGlitchFontFamily.isEmpty()) {
                            technoFont = QFont(technoGlitchFontFamily, qRound(13 * ratio), QFont::Bold);
                        } else {
                            technoFont = QFont("LLDEtechnoGlitch-Bold0", qRound(10 * ratio), QFont::Bold);
                        }
                        
                        painter->setFont(technoFont);
                        
                        QString countText = QString::number(unitCount);
                        QFontMetrics technoFm(technoFont);
                        int textWidth = technoFm.horizontalAdvance(countText);
                        int textX = unitColumnX + (bgWidth - textWidth) / 2;
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
        }
    }
    
    // 绘制第二队玩家单位（类似于第一队的逻辑）
    if (!team2Players.isEmpty()) {
        for (int i = 0; i < team2Players.size() && i < 2; ++i) { // 最多显示2个玩家
            int playerIdx = team2Players[i];
            if (playerIdx >= 0 && playerIdx < Ra2ob::MAXPLAYER && g._gameInfo.players[playerIdx].valid) {
                // 计算这个玩家单位的起始位置
                int unitColumnX = team2FlagsX + i * (flagWidth + qRound(1 * ratio));
                int unitY = team2UnitsStartY;
                
                // 获取玩家颜色
                QColor playerColor = QColor(QString::fromStdString("#" + g._gameInfo.players[playerIdx].panel.color));
                
                // 遍历玩家的单位，最多显示6个
                int displayedUnits = 0;
                
                // 获取玩家的单位信息
                for (const auto& unit : g._gameInfo.players[playerIdx].units.units) {
                    if (displayedUnits >= 7 || unit.num <= 0 || !unit.show) {
                        continue; // 最多显示8个单位或跳过数量为0的单位
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
                        painter->drawPixmap(unitColumnX, unitY, radiusIcon);
                        
                        // 绘制数量背景
                        int bgWidth = scaledUnitWidth;
                        int bgHeight = qRound(10 * ratio);
                        int bgY = unitY + scaledUnitHeight - bgHeight;
                        
                        // 创建带圆角的数量背景路径
                        QPainterPath path;
                        path.addRoundedRect(QRectF(unitColumnX, bgY, bgWidth, bgHeight), 
                                           qRound(2 * ratio), qRound(2 * ratio));
                        
                        painter->save();
                        painter->setRenderHint(QPainter::Antialiasing);
                        painter->fillPath(path, playerColor);
                        painter->restore();
                        
                        // 绘制单位数量 - 使用特效字体
                        painter->save();
                        
                        // 使用LLDEtechnoGlitch特效字体
                        QFont technoFont;
                        if (!technoGlitchFontFamily.isEmpty()) {
                            technoFont = QFont(technoGlitchFontFamily, qRound(13 * ratio), QFont::Bold);
                        } else {
                            technoFont = QFont("LLDEtechnoGlitch-Bold0", qRound(10 * ratio), QFont::Bold);
                        }
                        
                        painter->setFont(technoFont);
                        
                        QString countText = QString::number(unitCount);
                        QFontMetrics technoFm(technoFont);
                        int textWidth = technoFm.horizontalAdvance(countText);
                        int textX = unitColumnX + (bgWidth - textWidth) / 2;
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
        }
    }
    
    // 标记单位图标已加载
    unitIconsLoaded = true;
}

void ObTeam::paintProducingBlocks(QPainter *painter) {
    // 检查是否应该显示建造栏
    if (!showProducingBlocks) {
        return; // 如果不显示建造栏，直接返回
    }
    
    if (uniqueTeams.size() < 2) {
        return;
    }
    
    float ratio = gls->l.ratio;
    
    // 从屏幕左上角开始计算XY坐标
    int team1ProducingX = qRound(10 * ratio); // 从屏幕左边开始10px
    int team1ProducingY = qRound(80 * ratio); // 从屏幕顶部开始50px
    
    // 镜像时从屏幕右边只减去右侧面板宽度和建造栏宽度
    int screenWidth = this->width();
    int rightPanelWidth = qRound(170 * ratio);
    int team2ProducingX = screenWidth - rightPanelWidth - qRound(producingBlockWidth * ratio);
    int team2ProducingY = team1ProducingY; // 与第一队相同的Y坐标
    
    // 队友之间的垂直间距
    int teamMateSpacing = qRound(5 * ratio);
    // 建造项目之间的水平间距
    int producingItemSpacing = qRound(2 * ratio);
    
    // 每行最多显示的建造栏数量
    const int maxProducingPerRow = 4;
    // 最大检测的建造项目数量(限制为2个)
    const int maxDetectingBuildings = 2;
    
    // 重置实际显示的建造项目数量
    team1DisplayedBuildings = 0;
    team2DisplayedBuildings = 0;
    
    // 绘制第一队玩家建造栏（左侧）
    if (!team1Players.isEmpty()) {
        for (int i = 0; i < team1Players.size() && i < 2; ++i) { // 最多显示2个玩家
            int playerIdx = team1Players[i];
            if (playerIdx >= 0 && playerIdx < Ra2ob::MAXPLAYER && g._gameInfo.players[playerIdx].valid) {
                // 获取玩家颜色
                QColor playerColor = QColor(QString::fromStdString("#" + g._gameInfo.players[playerIdx].panel.color));
                
                // 获取玩家建造信息
                const auto& buildingInfo = g._gameInfo.players[playerIdx].building;
                
                // 计算这个玩家建造栏的起始位置（上下排列）
                int playerProducingY = team1ProducingY + i * (producingBlockHeight + teamMateSpacing) * ratio;
                int producingX = team1ProducingX;
                
                // 显示玩家建造信息（从左到右排列）
                int displayedBuildings = 0;
                for (const auto& building : buildingInfo.list) {
                    if (displayedBuildings >= maxProducingPerRow) {
                        break; // 最多显示指定数量的建造栏
                    }
                    
                    drawProducingBlock(painter, building, producingX, playerProducingY, playerColor, ratio);
                    
                    // 更新X坐标，为下一个建造栏做准备（从左到右）
                    producingX += qRound((producingBlockWidth + producingItemSpacing) * ratio);
                    displayedBuildings++;
                    // 更新团队1显示的建造项目总数
                    team1DisplayedBuildings = qMax(team1DisplayedBuildings, displayedBuildings);
                }
            }
        }
    }
    
    // 绘制第二队玩家建造栏（右侧）
    if (!team2Players.isEmpty()) {
        for (int i = 0; i < team2Players.size() && i < 2; ++i) { // 最多显示2个玩家
            int playerIdx = team2Players[i];
            if (playerIdx >= 0 && playerIdx < Ra2ob::MAXPLAYER && g._gameInfo.players[playerIdx].valid) {
                // 获取玩家颜色
                QColor playerColor = QColor(QString::fromStdString("#" + g._gameInfo.players[playerIdx].panel.color));
                
                // 获取玩家建造信息
                const auto& buildingInfo = g._gameInfo.players[playerIdx].building;
                
                // 计算这个玩家建造栏的起始位置（上下排列）
                int playerProducingY = team2ProducingY + i * (producingBlockHeight + teamMateSpacing) * ratio;
                
                // 收集要显示的建造项目
                QVector<const Ra2ob::tagBuildingNode*> buildingsToShow;
                for (const auto& building : buildingInfo.list) {
                    if (buildingsToShow.size() < maxProducingPerRow) {
                        buildingsToShow.append(&building);
                    } else {
                        break;
                    }
                }
                
                // 从右往左显示建造项目
                for (int j = 0; j < buildingsToShow.size(); j++) {
                    // 计算X坐标（从右往左）
                    int producingX = team2ProducingX - j * qRound((producingBlockWidth + producingItemSpacing) * ratio);
                    
                    drawProducingBlock(painter, *buildingsToShow[j], producingX, playerProducingY, playerColor, ratio);
                }
                
                // 更新团队2显示的建造项目总数
                team2DisplayedBuildings = qMax(team2DisplayedBuildings, buildingsToShow.size());
            }
        }
    }
    
    // 标记建造图标已加载
    producingIconsLoaded = true;
    
    // 不再调用calculateProducingBlocksRegions，改为直接在checkMousePositionForProducingBlocks中检测
}

void ObTeam::drawProducingBlock(QPainter *painter, const Ra2ob::tagBuildingNode& building,
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
        // 缩放图标大小
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
    int progressBarY = y + scaledHeight - progressBarHeight - qRound(5 * ratio);
    
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
    QFont miSansFont("MiSans-Bold", qRound(9 * ratio), QFont::Bold);
    miSansFont.setLetterSpacing(QFont::AbsoluteSpacing, qRound(1.2 * ratio)); // 增加字符间距
    painter->setFont(miSansFont);
    
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
    
    QFontMetrics fm(miSansFont);
    int textWidth = fm.horizontalAdvance(statusText);
    int textX = x + (scaledWidth - textWidth) / 2;
    int textY = y + scaledHeight - qRound(15 * ratio);
    
    // 使用白色绘制状态文本，不添加描边
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);
    painter->setPen(Qt::white);
    painter->drawText(textX, textY, statusText);
    painter->restore();
    
    // 如果数量大于1，绘制数量
    if (building.number > 1) {
        // 设置数量文本的字体 - 使用LLDEtechnoGlitch特效字体
        QFont technoFont;
        
        // 使用保存的实际字体族名称，如果没有则尝试直接使用字体文件名
        if (!technoGlitchFontFamily.isEmpty()) {
            technoFont = QFont(technoGlitchFontFamily, qRound(14 * ratio), QFont::Bold);
        } else {
            // 如果没有保存的字体族名称，使用默认的LLDEtechnoGlitch-Bold0
            technoFont = QFont("LLDEtechnoGlitch-Bold0", qRound(15 * ratio), QFont::Bold);
        }
        
        painter->setFont(technoFont);
        
        // 绘制数量文本
        QString numberText = "x" + QString::number(building.number);
        QFontMetrics technoFm(technoFont);
        int numberWidth = technoFm.horizontalAdvance(numberText);
        int numberX = x + scaledWidth - numberWidth - qRound(5 * ratio);
        int numberY = y + qRound(18 * ratio);
        
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

bool ObTeam::detectTeams() {
    // 清空之前的队伍数据
    teamPlayers.clear();
    uniqueTeams.clear();
    team1Players.clear();
    team2Players.clear();
    team1Index = -1;
    team2Index = -1;
    
    // 返回值，标记是否成功检测到队伍
    
    // 统计有效玩家数
    int validPlayerCount = 0;
    
    // 遍历所有玩家
    for (int i = 0; i < Ra2ob::MAXPLAYER; i++) {
        // 如果玩家有效
        if (g._gameInfo.players[i].valid) {
            validPlayerCount++;
            // 获取玩家的队伍号
            int teamNumber = g._gameInfo.players[i].status.teamNumber;
            
            // 如果这个队伍号是新的，记录下来
            if (!teamPlayers.contains(teamNumber)) {
                uniqueTeams.append(teamNumber);
            }
            
            // 将玩家添加到对应队伍的列表中
            teamPlayers[teamNumber].append(i);
        }
    }
    
    // 检查是否已达到足够的玩家数量（4位或以上）
    if (validPlayerCount >= 4) {
        enoughPlayersDetected = true;
        qDebug() << "2V2模式: 检测到足够的玩家数量(" << validPlayerCount << ")，停止持续检测";
    }
    
    // 如果有两个或更多队伍，按队伍分配
    if (uniqueTeams.size() >= 2) {
        team1Index = uniqueTeams[0];
        team2Index = uniqueTeams[1];
        
        // 分配玩家到各自队伍
        if (teamPlayers.contains(team1Index)) {
            team1Players = teamPlayers[team1Index];
        }
        
        if (teamPlayers.contains(team2Index)) {
            team2Players = teamPlayers[team2Index];
        }
        
        // 输出检测到的队伍信息
        QString team1PlayersStr, team2PlayersStr;
        for (int idx : team1Players) {
            team1PlayersStr += QString::number(idx) + " ";
        }
        for (int idx : team2Players) {
            team2PlayersStr += QString::number(idx) + " ";
        }
        
        qDebug() << "2V2模式: 检测到队伍关系 - 队伍1:" << team1PlayersStr
                 << "队伍2:" << team2PlayersStr;
                 
        // 成功检测到两个队伍，返回true
        return true;
    }
    
    // 如果队伍少于2个，返回false表示未完全检测到队伍
    return uniqueTeams.size() >= 2;
}

void ObTeam::setLayoutByScreen() {
    qreal ratio_screen = this->screen()->devicePixelRatio();
    int gw = g._gameInfo.debug.setting.screenWidth;
    int gh = g._gameInfo.debug.setting.screenHeight;
    int sw = this->screen()->size().width();
    int sh = this->screen()->size().height();
    
    // 获取全局设置实例，尝试使用MainWindow启动时获取的实际屏幕分辨率
    Globalsetting &gls_instance = Globalsetting::getInstance();

    qreal ratio;
    // 计算比例 - 优先使用实际屏幕分辨率
    if (gls_instance.currentScreenWidth > 0 && gls_instance.currentScreenHeight > 0) {
        // 使用MainWindow启动时获取的实际屏幕分辨率
        ratio = 1.0 * gls_instance.currentScreenWidth / 1920.0; // 以1920为基准分辨率
        qDebug() << "ObTeam使用实际屏幕分辨率计算ratio:" << gls_instance.currentScreenWidth << "x" << gls_instance.currentScreenHeight << ", ratio:" << ratio;
    } else if (gw != 0) {
        // 回退到原有逻辑：使用游戏配置文件中的分辨率
        ratio = 1.0 * sw / gw;
        qDebug() << "ObTeam使用游戏配置分辨率计算ratio:" << gw << "x" << gh << ", ratio:" << ratio;
    } else {
        // 最后的备选方案：使用设备像素比
        ratio = 1.0 / ratio_screen;
        qDebug() << "ObTeam使用设备像素比计算ratio:" << ratio_screen << ", ratio:" << ratio;
    }

    gls->loadLayoutSetting(sw, sh, ratio);
}

bool ObTeam::isGameReallyOver() {
    if (!g._gameInfo.valid) {
        return true;
    }
    
    if (g._gameInfo.isGameOver) {
        int validPlayers = 0;
        for (int i = 0; i < Ra2ob::MAXPLAYER; i++) {
            if (g._gameInfo.players[i].valid) {
                validPlayers++;
            }
        }
        
        if (validPlayers == 0) {
            return true;
        } else {
            return false;
        }
    }
    
    if (g._gameInfo.currentFrame <= 2) {
        int validPlayers = 0;
        for (int i = 0; i < Ra2ob::MAXPLAYER; i++) {
            if (g._gameInfo.players[i].valid) {
                validPlayers++;
            }
        }
        
        if (validPlayers == 0) {
            return true;
        }
    }
    
    return false;
}

void ObTeam::detectGame() {
    // 检测游戏是否刚刚开始
    bool gameJustStarted = g._gameInfo.valid && !lastGameValid;
    bool gameJustEnded = !g._gameInfo.valid && lastGameValid;

    // 记住上次游戏状态
    lastGameValid = g._gameInfo.valid;

    // 游戏刚开始时加载面板图片
    if (gameJustStarted) {
        qDebug() << "2V2模式：游戏有效，加载面板图片";
        
        // 重置动画播放状态，为新游戏做准备
        animationPlayedForCurrentGame = false;
        
        // 清理上一局可能留下的单位显示
        clearUnitDisplay();
        
        QString appDir = QCoreApplication::applicationDirPath();
        // 加载2V2顶部背景图
        topBg1Image.load(appDir + "/assets/panels/2v2bg1.png");
        topBg3Image.load(appDir + "/assets/panels/2v2bg3.png");
        // 加载右侧栏背景图
        rightBarImage.load(appDir + "/assets/panels/Rightbar.png");
        
        // 确保比分标签已创建并准备好进行动画
        if (!team1ScoreLabel || !team2ScoreLabel) {
            initScoreUI();
        }
        
        // 更新比分显示
        updateTeamScores();
        
        // 确保比分标签已显示，后续动画才能生效
        if (team1ScoreLabel && team2ScoreLabel) {
            team1ScoreLabel->show();
            team2ScoreLabel->show();
        }
        
        // 不要立即启动动画，将在shouldShowOb()条件满足时启动
        
        // 重置颜色背景加载标志，允许在新游戏开始时加载一次
        flagsLoaded = false;
        unitIconsLoaded = false; // 重置单位图标加载标志
        producingIconsLoaded = false; // 重置建造图标加载标志
        colorBgsLoaded = false; // 重置颜色背景图片加载标志
        flagCache.clear();
        unitIconCache.clear(); // 清理单位图标缓存
        colorBgCache.clear(); // 清理颜色背景图片缓存
        
        // 重置团队状态跟踪
        gameFinished = false;
        teamStatusTimer->start();
        teamStatusTracking = true;
        qDebug() << "2V2模式: 团队状态跟踪已启动";
        
        // 初始化间谍渗透标签
        initSpyInfiltrationLabels();
        
        // 重置间谍渗透状态
        resetSpyInfiltrationStatus();
        qDebug() << "2V2模式: 游戏开始，初始化间谍渗透系统";
        
        // 重置队伍检测标志
        teamsDetected = false;
        enoughPlayersDetected = false; // 重置玩家数量检测标志
        
        // 重置间谍显示状态
        
        // 启动PlayerStatusTracker
        if (statusTracker) {
            statusTracker->startTracking();
        }
        
        if (topBg1Image.isNull()) {
            qDebug() << "无法加载2V2背景图1: " << appDir + "/assets/panels/2v2bg1.png";
        } else {
            qDebug() << "成功加载2V2背景图1";
        }
        
        if (topBg3Image.isNull()) {
            qDebug() << "无法加载2V2背景图3: " << appDir + "/assets/panels/2v2bg3.png";
        } else {
            qDebug() << "成功加载2V2背景图3";
        }
        
        if (rightBarImage.isNull()) {
            qDebug() << "无法加载右侧栏背景图: " << appDir + "/assets/panels/Rightbar.png";
        } else {
            qDebug() << "成功加载右侧栏背景图";
        }
        
        // 确保2v2颜色图片目录存在
        QDir colorDir(appDir + "/assets/2v2colors");
        if (!colorDir.exists()) {
            QDir().mkpath(colorDir.path());
            qDebug() << "创建2v2colors目录:" << colorDir.path();
        }
        
        // 检测队伍关系
        bool teamDetectResult = detectTeams();
        // 只有成功检测到队伍后，才设置标志位为true
        teamsDetected = teamDetectResult;
        
        // 更新屏幕布局
        setLayoutByScreen();
        
        // 更新界面
        this->update();
    }
    
    // 如果游戏有效且队伍关系尚未检测，则检测一次队伍关系
    else if (g._gameInfo.valid && !teamsDetected) {
        // 检测队伍关系
        bool teamDetectResult = detectTeams();
        // 只有成功检测到队伍后，才设置标志位为true
        teamsDetected = teamDetectResult;
        if (teamDetectResult) {
            qDebug() << "2V2模式: 检测到队伍关系，不再重复检测";
        } else {
            qDebug() << "2V2模式: 队伍关系检测不完整，下次游戏会重新检测";
        }
    }
    // 如果队伍已检测但玩家数量不足，只输出日志但不再重新检测队伍关系
    else if (g._gameInfo.valid && teamsDetected && !enoughPlayersDetected) {
        qDebug() << "2V2模式: 持续检测玩家数量...";
    }

    // 检查是否应该显示界面并启动动画
    checkForAnimationStart();

    // 游戏刚结束时，释放面板图片和内存
    if (gameJustEnded) {
        qDebug() << "2V2模式：游戏无效，释放面板图片和内存";
        
        // 清理单位显示
        clearUnitDisplay();
        
        // 释放图片资源 - 使用clear()方法确保真正释放内存
        topBg1Image = QPixmap();
        topBg3Image = QPixmap();
        rightBarImage = QPixmap(); // 释放右侧栏背景图资源
        
        // 使用clear方法确保真正释放缓存中的内存
        flagCache.clear();         // 清理国旗图片缓存
        unitIconCache.clear();     // 清理单位图标缓存
        colorBgCache.clear();      // 清理颜色背景图片缓存
        avatarCache.clear();       // 清理玩家头像缓存
        
        // 重置标志
        flagsLoaded = false;       // 重置国旗加载标志
        unitIconsLoaded = false;   // 重置单位图标加载标志
        producingIconsLoaded = false; // 重置建造图标加载标志
        colorBgsLoaded = false;    // 重置颜色背景图片加载标志
        avatarsLoaded = false;     // 重置头像加载标志
        teamsDetected = false;     // 重置队伍检测标志
        enoughPlayersDetected = false; // 重置玩家数量检测标志
        
        // 停止团队状态跟踪
        if (teamStatusTracking) {
            teamStatusTimer->stop();
            teamStatusTracking = false;
            qDebug() << "2V2模式: 团队状态跟踪已停止";
        }
        
        // 停止PlayerStatusTracker
        if (statusTracker) {
            statusTracker->stopTracking();
        }
        
        // 清空间谍渗透显示记录
        
        // 清空队伍数据
        teamPlayers.clear();
        uniqueTeams.clear();
        team1Players.clear();
        team2Players.clear();
        team1Index = -1;
        team2Index = -1;
        
        // 强制系统回收内存 - 参考ob.cpp的实现
        #ifdef Q_OS_WIN
            SetProcessWorkingSetSize(GetCurrentProcess(), (SIZE_T)-1, (SIZE_T)-1);
        #endif
        
        // 强制Qt清理PixmapCache
        QPixmapCache::clear();
        
        qDebug() << "2V2模式：内存回收完成";
        
        // 更新界面以清除图片显示
        this->update();
    }
    
    // 如果界面是隐藏状态，且游戏无效，尝试进一步回收内存
    if (!this->isVisible() && !g._gameInfo.valid) {
        static int cleanupCounter = 0;
        if (++cleanupCounter >= 60) { // 每10次检查执行一次额外清理
            cleanupCounter = 0;
            
            // 强制系统回收内存
            #ifdef Q_OS_WIN
                SetProcessWorkingSetSize(GetCurrentProcess(), (SIZE_T)-1, (SIZE_T)-1);
            #endif
            
            qDebug() << "2V2模式：执行额外内存清理";
        }
    }
}

// 添加一个新方法，用于检查是否应该启动动画
void ObTeam::checkForAnimationStart() {
    // 检查是否应该显示界面并且动画尚未开始
    bool shouldShow = false;
    
    // 首先检查全局面板显示设置
    if (!gls->s.show_all_panel) {
        return; // 全局设置为隐藏，不显示界面也不启动动画
    }
    
    // 计算有效玩家数量
    int validPlayerCount = 0;
    for (int i = 0; i < Ra2ob::MAXPLAYER; i++) {
        if (g._gameInfo.players[i].valid) {
            validPlayerCount++;
        }
    }
    
    // 检查是否应该显示界面的条件
    if (g._gameInfo.valid && 
        !g._gameInfo.isGamePaused && 
        !isGameReallyOver() && 
        g._gameInfo.currentFrame >= 5 && 
        validPlayerCount >= 2) {
        shouldShow = true;
    }
    
    // 如果应该显示界面并且当前动画未激活，且主动画组存在，并且当前游戏尚未播放过动画，则启动动画
    if (shouldShow && !animationActive && mainAnimGroup && !animationPlayedForCurrentGame) {
        // 只有当界面真正可见时才启动动画
        if (this->isVisible()) {
            // 重置动画参数
            panelAnimProgress = 0.0;
            elementsAnimProgress = 0.0;
            glowAnimProgress = 0.0;
            animationActive = true;
            
            // 停止并重新启动动画
            mainAnimGroup->stop();
            mainAnimGroup->start();
            
            // 标记当前游戏已播放动画，直到下次游戏开始才会再次播放
            animationPlayedForCurrentGame = true;
            
            qDebug() << "2V2模式：启动动画（该游戏首次显示）";
        }
    }
}

void ObTeam::toggleOb() {
    bool shouldShow = false;
    QString hideReason = "";
    static bool gameHasStarted = false;
    static int maxPlayersDetected = 0;
    
    // 首先检查全局面板显示设置
    if (!gls->s.show_all_panel) {
        hideReason = "全局面板显示设置为隐藏";
    }
    else {
        // 计算有效玩家数量
        int validPlayerCount = 0;
        for (int i = 0; i < Ra2ob::MAXPLAYER; i++) {
            if (g._gameInfo.players[i].valid) {
                validPlayerCount++;
            }
        }
        
        // 记录最大玩家数量，用于判断游戏是否已经开始
        if (validPlayerCount > maxPlayersDetected) {
            maxPlayersDetected = validPlayerCount;
            if (maxPlayersDetected >= 2) {
                gameHasStarted = true;
            }
        }
        
        // 检查游戏是否真的结束
        bool gameReallyOver = isGameReallyOver();
        
        // 检查各种不应该显示界面的情况
        if (g._gameInfo.isGamePaused) {
            hideReason = "游戏已暂停";
        }
        else if (gameReallyOver) {
            hideReason = "游戏确实已经结束";
            gameHasStarted = false;
            maxPlayersDetected = 0;
        }
        else if (g._gameInfo.currentFrame < 5) {
            hideReason = "游戏帧数过低，尚未开始";
        }
        else if (validPlayerCount >= 2) {
            shouldShow = true;
        } else {
            hideReason = QString("没有足够的有效玩家(当前玩家数=%1)").arg(validPlayerCount);
        }
    }
    
    // 控制BO数标签和比分标签的显示/隐藏
    // 只在游戏进行时显示这些元素
    if (shouldShow) {
        // 游戏进行中，显示BO数和比分标签
        if (lb_bo_number) {
            lb_bo_number->show();
        }
        if (team1ScoreLabel) {
            team1ScoreLabel->show();
        }
        if (team2ScoreLabel) {
            team2ScoreLabel->show();
        }
    } else {
        // 游戏未进行且没有动画正在播放时，隐藏BO数和比分标签
        if (!animationActive) {
            if (lb_bo_number) {
                lb_bo_number->hide();
            }
            if (team1ScoreLabel) {
                team1ScoreLabel->hide();
            }
            if (team2ScoreLabel) {
                team2ScoreLabel->hide();
            }
        }
    }
    
    // 如果当前界面可见状态与应该显示的状态不一致，则更新
    if (shouldShow != this->isVisible()) {
        if (shouldShow) {
            this->show();
            
            // 确保在Windows平台上设置WS_EX_NOACTIVATE，使窗口不获取焦点
            #ifdef Q_OS_WIN
                HWND hwnd = (HWND)this->winId();
                LONG_PTR exStyle = GetWindowLongPtr(hwnd, GWL_EXSTYLE);
                SetWindowLongPtr(hwnd, GWL_EXSTYLE, exStyle | WS_EX_NOACTIVATE);
            #endif
            
            qDebug() << "2V2模式: 显示观察者界面";
        } else {
            this->hide();
            qDebug() << "2V2模式: 隐藏观察者界面，原因:" << hideReason;
        }
    }
}

void ObTeam::onGameTimerTimeout() {
    // 检测游戏状态
    detectGame();
    
    // 更新间谍渗透显示（仅在游戏有效且窗口可见时）
    if (g._gameInfo.valid && this->isVisible()) {
        updateSpyInfiltrationDisplay();
    }
    
    // 控制窗口显示/隐藏
    toggleOb();
}

QPixmap ObTeam::getPlayerColorBackground(const std::string& color) {
    // 返回空图像，不再使用颜色背景图片
    return QPixmap();
}

QPixmap ObTeam::getCountryFlag(const QString& country)
{
    QString countryName = country;
    if (!country.isEmpty()) {
        countryName = country.at(0).toUpper() + country.mid(1).toLower();
    } else {
        countryName = "Americans";
    }

    QString appDir = QCoreApplication::applicationDirPath();
    QString flagPath = appDir + "/assets/countries/" + countryName + ".png";
    QPixmap flag(flagPath);
    
    if (flag.isNull()) {
        flagPath = appDir + "/assets/countries/Americans.png";
        flag = QPixmap(flagPath);
        
        if (flag.isNull()) {
            flag = QPixmap(25, 15);
            flag.fill(Qt::transparent);
            qDebug() << "无法加载国旗图片: " << countryName;
        }
    }
    
    return flag;
}

QPixmap ObTeam::getUnitIcon(const QString& unitName) {
    // 先检查缓存中是否有该单位图标
    if (unitIconCache.contains(unitName)) {
        return unitIconCache[unitName];
    }
    
    // 如果没有，则从文件加载
    QString appDir = QCoreApplication::applicationDirPath();
    QString iconPath = appDir + "/assets/obicons/" + unitName + ".png";
    
    // 检查文件是否存在
    QFile iconFile(iconPath);
    if (!iconFile.exists()) {
        // 如果单位图标不存在，使用占位图
        iconPath = appDir + "/assets/obicons/unit_placeholder_trans.png";
        QFile placeholderFile(iconPath);
        
        if (!placeholderFile.exists()) {
            qDebug() << "警告：单位图标占位图不存在: " << iconPath;
            // 返回空图片
            QPixmap emptyPixmap(unitIconWidth, unitIconHeight);
            emptyPixmap.fill(Qt::transparent);
            return emptyPixmap;
        }
    }
    
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

QPixmap ObTeam::getRadiusPixmap(const QPixmap& src, int radius) {
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

// 添加新方法: 获取玩家电力状态
PowerState ObTeam::getPlayerPowerState(int playerIndex) {
    if (playerIndex < 0 || playerIndex >= Ra2ob::MAXPLAYER || 
        !g._gameInfo.valid || !g._gameInfo.players[playerIndex].valid) {
        return PowerState::Normal; // 默认正常状态
    }
    
    int powerDrain = g._gameInfo.players[playerIndex].panel.powerDrain;
    int powerOutput = g._gameInfo.players[playerIndex].panel.powerOutput;
    
    // 根据电力情况判断状态
    if (powerDrain == 0 && powerOutput == 0) {
        // 初始状态 - 灰色
        return PowerState::Normal;
    } else if (powerDrain > 0 && powerOutput == 0) {
        // 完全断电 - 红色
        return PowerState::Critical;
    } else if (powerOutput > powerDrain) {
        if (powerOutput * 0.85 < powerDrain) {
            // 电力紧张 - 黄色
            return PowerState::Warning;
        } else {
            // 电力充足 - 绿色
            return PowerState::Normal;
        }
    } else {
        // 电力不足 - 红色
        return PowerState::Critical;
    }
}

// 添加新方法: 绘制电力状态
void ObTeam::paintPowerStatus(QPainter *painter) {
    if (uniqueTeams.size() < 2) {
        return;
    }
    
    float ratio = gls->l.ratio;
    int wCenter = gls->l.top_m;
    int pWidth = qRound(1167 * ratio); // 与顶部面板宽度一致
    
    // 计算面板区域
    int panelLeft = wCenter - pWidth / 2;
    int panelRight = wCenter + pWidth / 2;
    
    // 电力指示灯大小
    int powerSize = qRound(powerIndicatorSize * ratio);
    int borderRadius = powerSize / 2; // 圆形，所以半径是尺寸的一半
    
    // 使用动画进度计算垂直偏移
    int verticalOffset = 0;
    float opacity = 1.0;
    
    if (animationActive || elementsAnimProgress < 1.0) {
        verticalOffset = qRound(50 * ratio * (1.0 - elementsAnimProgress));
        opacity = elementsAnimProgress;
    }
    
    // 应用动画效果
    painter->save();
    if (opacity < 1.0) {
        painter->setOpacity(opacity);
    }

    // 第一队玩家（左侧）
    if (!team1Players.isEmpty()) {
        int playerCount = team1Players.size();
        
        for (int i = 0; i < playerCount && i < 2; ++i) { // 最多显示2个玩家
            int playerIdx = team1Players[i];
            if (playerIdx >= 0 && playerIdx < Ra2ob::MAXPLAYER && g._gameInfo.players[playerIdx].valid) {
                // 根据玩家索引选择对应的偏移量
                int powerOffsetX, powerOffsetY;
                if (i == 0) { // 第一位玩家
                    powerOffsetX = player1PowerOffsetX;
                    powerOffsetY = player1PowerOffsetY;
                } else if (i == 1) { // 第二位玩家
                    powerOffsetX = player2PowerOffsetX;
                    powerOffsetY = player2PowerOffsetY;
                } else { // 其他玩家使用通用偏移量
                    powerOffsetX = economyOffsetX + 100; // 在经济信息右侧
                    powerOffsetY = economyOffsetY;
                }
                
                // 计算电力指示灯位置，应用垂直动画偏移
                int powerX = panelLeft + qRound(powerOffsetX * ratio);
                int powerY = qRound(powerOffsetY * ratio) - verticalOffset;
                
                // 获取玩家电力状态
                PowerState powerState = getPlayerPowerState(playerIdx);
                
                // 根据电力状态设置颜色和样式
                QColor powerColor;
                bool shouldBlink = false;
                
                switch (powerState) {
                    case PowerState::Critical:
                        powerColor = QColor("#ff0000"); // 红色
                        shouldBlink = true; // 红色时闪烁
                        break;
                    case PowerState::Warning:
                        powerColor = QColor("#ffdd00"); // 黄色
                        break;
                    case PowerState::Normal:
                    default:
                        powerColor = QColor("#00cc00"); // 绿色
                        break;
                }
                
                // 如果需要闪烁且当前是关闭状态，降低透明度
                if (shouldBlink && !powerBlinkState) {
                    powerColor.setAlpha(80); // 降低透明度
                }
                
                // 绘制电力指示灯
                painter->save();
                painter->setRenderHint(QPainter::Antialiasing);
                
                // 创建径向渐变
                QRadialGradient gradient(
                    QPointF(powerX + powerSize/2, powerY + powerSize/2), // 中心点
                    powerSize/2 // 半径
                );
                
                // 设置渐变颜色
                QColor centerColor = powerColor.lighter(150);
                QColor edgeColor = powerColor;
                gradient.setColorAt(0, centerColor);
                gradient.setColorAt(0.7, powerColor);
                gradient.setColorAt(1, edgeColor.darker(120));
                
                // 填充圆形
                QPainterPath path;
                path.addEllipse(powerX, powerY, powerSize, powerSize);
                painter->fillPath(path, gradient);
                
                // 添加白色边框
                painter->setPen(QPen(Qt::white, qRound(1.5 * ratio), Qt::SolidLine));
                painter->drawEllipse(powerX, powerY, powerSize, powerSize);
                
                // 添加高光效果
                QPainterPath highlightPath;
                int highlightSize = powerSize / 2;
                highlightPath.addEllipse(
                    powerX + powerSize/4, 
                    powerY + powerSize/4, 
                    highlightSize, highlightSize
                );
                QColor highlightColor = centerColor;
                highlightColor.setAlpha(100);
                painter->fillPath(highlightPath, highlightColor);
                
                painter->restore();
            }
        }
    }
    
    // 第二队玩家（右侧）
    if (!team2Players.isEmpty()) {
        int playerCount = team2Players.size();
        
        for (int i = 0; i < playerCount && i < 2; ++i) { // 最多显示2个玩家
            int playerIdx = team2Players[i];
            if (playerIdx >= 0 && playerIdx < Ra2ob::MAXPLAYER && g._gameInfo.players[playerIdx].valid) {
                // 根据玩家索引选择对应的偏移量
                int powerOffsetX, powerOffsetY;
                if (i == 0) { // 第一位玩家
                    powerOffsetX = player1PowerOffsetX;
                    powerOffsetY = player1PowerOffsetY;
                } else if (i == 1) { // 第二位玩家
                    powerOffsetX = player2PowerOffsetX;
                    powerOffsetY = player2PowerOffsetY;
                } else { // 其他玩家使用通用偏移量
                    powerOffsetX = economyOffsetX + 100; // 在经济信息右侧
                    powerOffsetY = economyOffsetY;
                }
                
                // 计算电力指示灯位置 - 右侧玩家的指示灯位置需要镜像，并应用垂直动画偏移
                int powerX = panelRight - qRound(powerOffsetX * ratio) - powerSize;
                int powerY = qRound(powerOffsetY * ratio) - verticalOffset;
                
                // 获取玩家电力状态
                PowerState powerState = getPlayerPowerState(playerIdx);
                
                // 根据电力状态设置颜色和样式
                QColor powerColor;
                bool shouldBlink = false;
                
                switch (powerState) {
                    case PowerState::Critical:
                        powerColor = QColor("#ff0000"); // 红色
                        shouldBlink = true; // 红色时闪烁
                        break;
                    case PowerState::Warning:
                        powerColor = QColor("#ffdd00"); // 黄色
                        break;
                    case PowerState::Normal:
                    default:
                        powerColor = QColor("#00cc00"); // 绿色
                        break;
                }
                
                // 如果需要闪烁且当前是关闭状态，降低透明度
                if (shouldBlink && !powerBlinkState) {
                    powerColor.setAlpha(80); // 降低透明度
                }
                
                // 绘制电力指示灯
                painter->save();
                painter->setRenderHint(QPainter::Antialiasing);
                
                // 创建径向渐变
                QRadialGradient gradient(
                    QPointF(powerX + powerSize/2, powerY + powerSize/2), // 中心点
                    powerSize/2 // 半径
                );
                
                // 设置渐变颜色
                QColor centerColor = powerColor.lighter(150);
                QColor edgeColor = powerColor;
                gradient.setColorAt(0, centerColor);
                gradient.setColorAt(0.7, powerColor);
                gradient.setColorAt(1, edgeColor.darker(120));
                
                // 填充圆形
                QPainterPath path;
                path.addEllipse(powerX, powerY, powerSize, powerSize);
                painter->fillPath(path, gradient);
                
                // 添加白色边框
                painter->setPen(QPen(Qt::white, qRound(1.5 * ratio), Qt::SolidLine));
                painter->drawEllipse(powerX, powerY, powerSize, powerSize);
                
                // 添加高光效果
                QPainterPath highlightPath;
                int highlightSize = powerSize / 2;
                highlightPath.addEllipse(
                    powerX + powerSize/4, 
                    powerY + powerSize/4, 
                    highlightSize, highlightSize
                );
                QColor highlightColor = centerColor;
                highlightColor.setAlpha(100);
                painter->fillPath(highlightPath, highlightColor);
                
                painter->restore();
            }
        }
    }
    
    // 恢复画笔状态
    painter->restore();
}

// 添加新方法: 绘制队伍经济长条
void ObTeam::paintTeamEconomyBars(QPainter *painter) {
    // 检查是否应该显示底部经济条
    if (!gls->s.show_bottom_panel) {
        return; // 如果设置为不显示，直接返回
    }
    
    if (uniqueTeams.size() < 2) {
        return;
    }
    
    float ratio = gls->l.ratio;
    
    // 计算每个队伍的经济总和
    int team1TotalEconomy = 0;
    int team2TotalEconomy = 0;
    
    // 存储每队玩家的颜色和索引
    QVector<QColor> team1Colors;
    QVector<QColor> team2Colors;
    QVector<int> team1PlayerIndices;
    QVector<int> team2PlayerIndices;
    
    // 计算第一队总经济并获取玩家颜色
    if (!team1Players.isEmpty()) {
        for (int i = 0; i < team1Players.size() && i < 2; ++i) { // 最多取两个玩家
            int playerIdx = team1Players[i];
            if (playerIdx >= 0 && playerIdx < Ra2ob::MAXPLAYER && g._gameInfo.players[playerIdx].valid) {
                team1TotalEconomy += g._gameInfo.players[playerIdx].panel.creditSpent;
                // 获取玩家颜色
                QColor playerColor = QColor(QString::fromStdString("#" + g._gameInfo.players[playerIdx].panel.color));
                team1Colors.append(playerColor);
                team1PlayerIndices.append(playerIdx);
            }
        }
        // 如果只有一个玩家，添加一个默认颜色
        if (team1Colors.size() < 2) {
            team1Colors.append(QColor("#58cc50")); // 默认绿色
            team1PlayerIndices.append(-1); // 无效的玩家索引
        }
    } else {
        // 添加默认颜色
        team1Colors.append(QColor("#58cc50")); // 默认绿色
        team1Colors.append(QColor("#58cc50")); // 默认绿色
        team1PlayerIndices.append(-1);
        team1PlayerIndices.append(-1);
    }
    
    // 计算第二队总经济并获取玩家颜色
    if (!team2Players.isEmpty()) {
        for (int i = 0; i < team2Players.size() && i < 2; ++i) { // 最多取两个玩家
            int playerIdx = team2Players[i];
            if (playerIdx >= 0 && playerIdx < Ra2ob::MAXPLAYER && g._gameInfo.players[playerIdx].valid) {
                team2TotalEconomy += g._gameInfo.players[playerIdx].panel.creditSpent;
                // 获取玩家颜色
                QColor playerColor = QColor(QString::fromStdString("#" + g._gameInfo.players[playerIdx].panel.color));
                team2Colors.append(playerColor);
                team2PlayerIndices.append(playerIdx);
            }
        }
        // 如果只有一个玩家，添加一个默认颜色
        if (team2Colors.size() < 2) {
            team2Colors.append(QColor("#487cc8")); // 默认蓝色
            team2PlayerIndices.append(-1); // 无效的玩家索引
        }
    } else {
        // 添加默认颜色
        team2Colors.append(QColor("#487cc8")); // 默认蓝色
        team2Colors.append(QColor("#487cc8")); // 默认蓝色
        team2PlayerIndices.append(-1);
        team2PlayerIndices.append(-1);
    }
    
    // 将总经济转换为K单位的浮点数
    float team1EconomyK = team1TotalEconomy / 1000.0f;
    float team2EconomyK = team2TotalEconomy / 1000.0f;
    
    // 底部面板尺寸和位置（简化风格）
    QColor f_bg(0x2f, 0x37, 0x3f); // 深灰色背景
    QColor f_g(QColor(0x22, 0xac, 0x38)); // 绿色（使用ob.cpp中相同的绿色）
    int panelWidth = gls->l.right_x; // 面板宽度
    int barHeight = qRound(14 * ratio); // 条高度
    int bottomPanelHeight = qRound(32 * ratio); // 底部面板高度
    
    // 计算底部面板位置
    int bottomY = this->height() - bottomPanelHeight;
    int bottomY1 = bottomY; // 第一队Y位置
    int bottomY2 = bottomY + qRound(16 * ratio); // 第二队Y位置
    
    // 绘制底部面板背景
    painter->fillRect(QRect(0, bottomY, panelWidth, bottomPanelHeight), f_bg);
    
    // 左侧颜色标识宽度
    int leftColorWidth = qRound(60 * ratio);
    int halfColorWidth = leftColorWidth / 2;
    
    // 绘制队伍玩家颜色标识（左侧显示两个玩家的颜色，各占一半）
    // 第一队的两个玩家颜色
    painter->fillRect(QRect(0, bottomY1, halfColorWidth, barHeight), team1Colors[0]);
    painter->fillRect(QRect(halfColorWidth, bottomY1, halfColorWidth, barHeight), team1Colors[1]);
    
    // 第二队的两个玩家颜色
    painter->fillRect(QRect(0, bottomY2, halfColorWidth, barHeight), team2Colors[0]);
    painter->fillRect(QRect(halfColorWidth, bottomY2, halfColorWidth, barHeight), team2Colors[1]);
    
    // 绘制经济条边框
    QPen blackBorder(Qt::black);
    blackBorder.setWidth(1);
    painter->setPen(blackBorder);
    painter->drawRect(QRect(0, bottomY1, panelWidth - 1, barHeight));
    painter->drawRect(QRect(0, bottomY2, panelWidth - 1, barHeight));
    
    // 创建用于显示经济值的字体 - 使用已加载的特效字体
    QFont economyFont;
    if (!technoGlitchFontFamily.isEmpty()) {
        economyFont = QFont(technoGlitchFontFamily, qRound(10 * ratio), QFont::Bold);
        economyFont.setStyleStrategy(QFont::PreferAntialias);
    } else {
        economyFont = QFont("Arial", qRound(10 * ratio), QFont::Bold);
    }
    
    // 计算每个队伍的经济进度块
    const int progressSegmentWidth = qRound(5 * ratio); // 每段宽度5像素
    int maxSegments = (panelWidth - leftColorWidth) / progressSegmentWidth - 10; // 最大段数（减去文本显示空间）
    
    // 使用240K作为最大值
    const float maxEconomy = 240.0f; // 240K为满进度
    const float kPerSegment = maxEconomy / maxSegments; // 每段代表的经济值
    int team1Segments = qMin(static_cast<int>(team1EconomyK / kPerSegment), maxSegments);
    int team2Segments = qMin(static_cast<int>(team2EconomyK / kPerSegment), maxSegments);
    
    // 缩放头像尺寸
    int scaledAvatarSize = qRound(avatarSize * ratio);
    
    // 绘制第一队的经济进度 - 统一使用绿色
    int sx_1 = leftColorWidth;
    for (int i = 0; i < maxSegments; i++) {
        if (i < team1Segments) {
            // 使用绿色绘制已达到的进度
            painter->fillRect(QRect(sx_1, bottomY1 + 1, progressSegmentWidth, barHeight - 2), f_g);
        }
        sx_1 += progressSegmentWidth;
    }
    
    // 绘制第二队的经济进度 - 统一使用绿色
    int sx_2 = leftColorWidth;
    for (int i = 0; i < maxSegments; i++) {
        if (i < team2Segments) {
            // 使用绿色绘制已达到的进度
            painter->fillRect(QRect(sx_2, bottomY2 + 1, progressSegmentWidth, barHeight - 2), f_g);
        }
        sx_2 += progressSegmentWidth;
    }
    
    // 绘制经济数值文本
    painter->setFont(economyFont);
    
    // 第一队经济值 - 向右移动一点
    QString team1Text = QString("%1k").arg(team1EconomyK, 0, 'f', 1);
    int team1TextX = qRound(17 * ratio); // 向右移动
    int team1TextY = bottomY1 + qRound(13 * ratio);
    
    // 第二队经济值 - 向右移动一点
    QString team2Text = QString("%1k").arg(team2EconomyK, 0, 'f', 1);
    int team2TextX = qRound(17 * ratio); // 向右移动
    int team2TextY = bottomY2 + qRound(13 * ratio);
    
    // 先绘制黑色描边
    painter->setPen(Qt::black);
    for (int dx = -1; dx <= 1; ++dx) {
        for (int dy = -1; dy <= 1; ++dy) {
            if (dx != 0 || dy != 0) {
                painter->drawText(team1TextX + dx, team1TextY + dy, team1Text);
                painter->drawText(team2TextX + dx, team2TextY + dy, team2Text);
            }
        }
    }
    
    // 再绘制白色文本
    painter->setPen(Qt::white);
    painter->drawText(team1TextX, team1TextY, team1Text);
    painter->drawText(team2TextX, team2TextY, team2Text);
    
    // 绘制刻度标记 - 绘制80K和160K
    painter->setPen(QPen(Qt::white, 1, Qt::DotLine));
    for (int i = 1; i <= 2; i++) {
        int segmentsForThisMark = i * 80 / kPerSegment;
        if (segmentsForThisMark < maxSegments) {
            int markX = leftColorWidth + segmentsForThisMark * progressSegmentWidth;
            painter->drawLine(markX, bottomY1, markX, bottomY1 + barHeight);
            painter->drawLine(markX, bottomY2, markX, bottomY2 + barHeight);
            
            // 刻度标记值
            QString markText = QString("%1k").arg(i * 80);
            painter->drawText(markX - qRound(10 * ratio), bottomY1 - qRound(2 * ratio), markText);
        }
    }
    
    // 绘制玩家头像跟随经济条
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->setRenderHint(QPainter::SmoothPixmapTransform, true);
    
    // 计算第一队头像位置
    if (team1PlayerIndices.size() >= 2) {
        // 计算两个玩家头像的基础位置 - 紧跟着进度条末端，稍微左移
        int baseX = leftColorWidth + (team1Segments * progressSegmentWidth) - qRound(50 * ratio);
        int baseY = bottomY1 - scaledAvatarSize / 2 + qRound(3 * ratio);  // 将头像向下稍微偏移
        
        // 绘制第一个玩家头像
        if (team1PlayerIndices[0] >= 0) {
            // 第一个头像位置 - 放在经济条的右侧
            int avatar1X = baseX + qRound(2 * ratio);
            int avatar1Y = baseY;
            
            // 获取并绘制头像
            QPixmap avatar = getPlayerAvatar(team1PlayerIndices[0]);
            if (!avatar.isNull()) {
                // 缩放头像
                QPixmap scaledAvatar = avatar.scaled(scaledAvatarSize, scaledAvatarSize, 
                                                  Qt::KeepAspectRatio, Qt::SmoothTransformation);
                
                // 创建圆形头像
                QPixmap roundedAvatar = getRadiusPixmap(scaledAvatar, scaledAvatarSize / 2);
                
                // 添加发光效果 - 使用玩家颜色
                QColor glowColor = team1Colors[0];
                glowColor.setAlpha(180);  // 半透明
                
                int glowSize = qRound(2 * ratio);
                
                // 绘制发光效果
                for (int g = glowSize; g > 0; g--) {
                    QColor currentColor = glowColor;
                    currentColor.setAlpha(180 - g * 30);  // 透明度渐变
                    painter->setPen(QPen(currentColor, g));
                    painter->drawEllipse(avatar1X - g/2, avatar1Y - g/2, 
                                       scaledAvatarSize + g, scaledAvatarSize + g);
                }
                
                // 绘制玩家头像
                painter->drawPixmap(avatar1X, avatar1Y, roundedAvatar);
                
                // 添加白色边框
                painter->setPen(QPen(Qt::white, qRound(1.5 * ratio)));
                painter->drawEllipse(avatar1X, avatar1Y, scaledAvatarSize, scaledAvatarSize);
            }
        }
        
        // 绘制第二个玩家头像 (如果存在)
        if (team1PlayerIndices.size() > 1 && team1PlayerIndices[1] >= 0) {
            // 第二个玩家头像位置 - 放在第一个头像右侧，平行排列
            int avatar2X = baseX + scaledAvatarSize + qRound(4 * ratio); // 在第一个头像右侧留出间距(减小间距)
            int avatar2Y = baseY; // 与第一个头像同一高度
            
            // 获取并绘制头像
            QPixmap avatar = getPlayerAvatar(team1PlayerIndices[1]);
            if (!avatar.isNull()) {
                // 缩放头像
                QPixmap scaledAvatar = avatar.scaled(scaledAvatarSize, scaledAvatarSize, 
                                                  Qt::KeepAspectRatio, Qt::SmoothTransformation);
                
                // 创建圆形头像
                QPixmap roundedAvatar = getRadiusPixmap(scaledAvatar, scaledAvatarSize / 2);
                
                // 添加发光效果 - 使用玩家颜色
                QColor glowColor = team1Colors[1];
                glowColor.setAlpha(180);  // 半透明
                
                int glowSize = qRound(2 * ratio);
                
                // 绘制发光效果
                for (int g = glowSize; g > 0; g--) {
                    QColor currentColor = glowColor;
                    currentColor.setAlpha(180 - g * 30);  // 透明度渐变
                    painter->setPen(QPen(currentColor, g));
                    painter->drawEllipse(avatar2X - g/2, avatar2Y - g/2, 
                                       scaledAvatarSize + g, scaledAvatarSize + g);
                }
                
                // 绘制玩家头像
                painter->drawPixmap(avatar2X, avatar2Y, roundedAvatar);
                
                // 添加白色边框
                painter->setPen(QPen(Qt::white, qRound(1.5 * ratio)));
                painter->drawEllipse(avatar2X, avatar2Y, scaledAvatarSize, scaledAvatarSize);
            }
        }
    }
    
    // 计算第二队头像位置
    if (team2PlayerIndices.size() >= 2) {
        // 计算两个玩家头像的基础位置 - 紧跟着进度条末端，稍微左移
        int baseX = leftColorWidth + (team2Segments * progressSegmentWidth) - qRound(50 * ratio);
        int baseY = bottomY2 - scaledAvatarSize / 2 + qRound(3 * ratio);  // 将头像向下稍微偏移
        
        // 绘制第一个玩家头像
        if (team2PlayerIndices[0] >= 0) {
            // 第一个头像位置 - 放在经济条的右侧
            int avatar1X = baseX + qRound(2 * ratio);
            int avatar1Y = baseY;
            
            // 获取并绘制头像
            QPixmap avatar = getPlayerAvatar(team2PlayerIndices[0]);
            if (!avatar.isNull()) {
                // 缩放头像
                QPixmap scaledAvatar = avatar.scaled(scaledAvatarSize, scaledAvatarSize, 
                                                  Qt::KeepAspectRatio, Qt::SmoothTransformation);
                
                // 创建圆形头像
                QPixmap roundedAvatar = getRadiusPixmap(scaledAvatar, scaledAvatarSize / 2);
                
                // 添加发光效果 - 使用玩家颜色
                QColor glowColor = team2Colors[0];
                glowColor.setAlpha(180);  // 半透明
                
                int glowSize = qRound(2 * ratio);
                
                // 绘制发光效果
                for (int g = glowSize; g > 0; g--) {
                    QColor currentColor = glowColor;
                    currentColor.setAlpha(180 - g * 30);  // 透明度渐变
                    painter->setPen(QPen(currentColor, g));
                    painter->drawEllipse(avatar1X - g/2, avatar1Y - g/2, 
                                       scaledAvatarSize + g, scaledAvatarSize + g);
                }
                
                // 绘制玩家头像
                painter->drawPixmap(avatar1X, avatar1Y, roundedAvatar);
                
                // 添加白色边框
                painter->setPen(QPen(Qt::white, qRound(1.5 * ratio)));
                painter->drawEllipse(avatar1X, avatar1Y, scaledAvatarSize, scaledAvatarSize);
            }
        }
        
        // 绘制第二个玩家头像 (如果存在)
        if (team2PlayerIndices.size() > 1 && team2PlayerIndices[1] >= 0) {
            // 第二个玩家头像位置 - 放在第一个头像右侧，平行排列
            int avatar2X = baseX + scaledAvatarSize + qRound(4 * ratio); // 在第一个头像右侧留出间距(减小间距)
            int avatar2Y = baseY; // 与第一个头像同一高度
            
            // 获取并绘制头像
            QPixmap avatar = getPlayerAvatar(team2PlayerIndices[1]);
            if (!avatar.isNull()) {
                // 缩放头像
                QPixmap scaledAvatar = avatar.scaled(scaledAvatarSize, scaledAvatarSize, 
                                                  Qt::KeepAspectRatio, Qt::SmoothTransformation);
                
                // 创建圆形头像
                QPixmap roundedAvatar = getRadiusPixmap(scaledAvatar, scaledAvatarSize / 2);
                
                // 添加发光效果 - 使用玩家颜色
                QColor glowColor = team2Colors[1];
                glowColor.setAlpha(180);  // 半透明
                
                int glowSize = qRound(2 * ratio);
                
                // 绘制发光效果
                for (int g = glowSize; g > 0; g--) {
                    QColor currentColor = glowColor;
                    currentColor.setAlpha(180 - g * 30);  // 透明度渐变
                    painter->setPen(QPen(currentColor, g));
                    painter->drawEllipse(avatar2X - g/2, avatar2Y - g/2, 
                                       scaledAvatarSize + g, scaledAvatarSize + g);
                }
                
                // 绘制玩家头像
                painter->drawPixmap(avatar2X, avatar2Y, roundedAvatar);
                
                // 添加白色边框
                painter->setPen(QPen(Qt::white, qRound(1.5 * ratio)));
                painter->drawEllipse(avatar2X, avatar2Y, scaledAvatarSize, scaledAvatarSize);
            }
        }
    }
    
    painter->restore();
}

// 添加新方法：绘制玩家颜色背景图片
void ObTeam::paintPlayerColorBgs(QPainter *painter) {
    if (uniqueTeams.size() < 2) {
        return;
    }
    
    float ratio = gls->l.ratio;
    int wCenter = gls->l.top_m;
    int pWidth = qRound(1167 * ratio); // 与顶部面板宽度一致
    
    // 计算每个队伍的绘制区域
    int panelLeft = wCenter - pWidth / 2;
    int panelRight = wCenter + pWidth / 2;
    
    // 颜色背景图片尺寸
    int scaledColorBgWidth = qRound(colorBgWidth * ratio);
    int scaledColorBgHeight = qRound(colorBgHeight * ratio);
    
    // 左侧队伍(team1)玩家颜色背景图片
    if (!team1Players.isEmpty()) {
        for (int i = 0; i < team1Players.size() && i < 2; ++i) { // 最多显示2个玩家
            int playerIdx = team1Players[i];
            if (playerIdx >= 0 && playerIdx < Ra2ob::MAXPLAYER && g._gameInfo.players[playerIdx].valid) {
                std::string playerColor = g._gameInfo.players[playerIdx].panel.color;
                
                // 计算位置
                int colorBgX = panelLeft + qRound((i == 0 ? player1ColorBgX : player2ColorBgX) * ratio);
                int colorBgY = qRound((i == 0 ? player1ColorBgY : player2ColorBgY) * ratio);
                
                // 获取颜色背景图片
                QPixmap colorBg;
                QString colorKey = QString::fromStdString(playerColor);
                
                if (colorBgCache.contains(colorKey)) {
                    colorBg = colorBgCache[colorKey];
                } else {
                    colorBg = getPlayerColorImage(playerColor);
                    if (!colorBg.isNull()) {
                        colorBgCache.insert(colorKey, colorBg);
                    }
                }
                
                // 绘制颜色背景图片
                if (!colorBg.isNull()) {
                    QPixmap scaledColorBg = colorBg.scaled(scaledColorBgWidth, scaledColorBgHeight, 
                                                      Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
                    painter->drawPixmap(colorBgX, colorBgY, scaledColorBg);
                }
            }
        }
    }
    
    // 右侧队伍(team2)玩家颜色背景图片
    if (!team2Players.isEmpty()) {
        for (int i = 0; i < team2Players.size() && i < 2; ++i) { // 最多显示2个玩家
            int playerIdx = team2Players[i];
            if (playerIdx >= 0 && playerIdx < Ra2ob::MAXPLAYER && g._gameInfo.players[playerIdx].valid) {
                std::string playerColor = g._gameInfo.players[playerIdx].panel.color;
                
                // 计算位置 - 右侧队伍镜像显示
                int colorBgX = panelRight - qRound((i == 0 ? player1ColorBgX : player2ColorBgX) * ratio) - scaledColorBgWidth;
                int colorBgY = qRound((i == 0 ? player1ColorBgY : player2ColorBgY) * ratio);
                
                // 获取颜色背景图片
                QPixmap colorBg;
                QString colorKey = QString::fromStdString(playerColor);
                
                if (colorBgCache.contains(colorKey)) {
                    colorBg = colorBgCache[colorKey];
                } else {
                    colorBg = getPlayerColorImage(playerColor);
                    if (!colorBg.isNull()) {
                        colorBgCache.insert(colorKey, colorBg);
                    }
                }
                
                // 绘制颜色背景图片 - 镜像处理
                if (!colorBg.isNull()) {
                    QPixmap mirroredColorBg = colorBg.transformed(QTransform().scale(-1, 1));
                    QPixmap scaledColorBg = mirroredColorBg.scaled(scaledColorBgWidth, scaledColorBgHeight, 
                                                             Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
                    painter->drawPixmap(colorBgX, colorBgY, scaledColorBg);
                }
            }
        }
    }
    
    // 标记颜色背景已加载
    colorBgsLoaded = true;
}

QPixmap ObTeam::getPlayerColorImage(const std::string& color) {
    QString appDir = QCoreApplication::applicationDirPath();
    QString imagePath;
    
    // 根据颜色代码选择对应的图片
    if (color == "e0d838") {
        imagePath = appDir + "/assets/2v2colors/Yellow.png";
    } else if (color == "f84c48") {
        imagePath = appDir + "/assets/2v2colors/Red.png";
    } else if (color == "58cc50") {
        imagePath = appDir + "/assets/2v2colors/Green.png";
    } else if (color == "487cc8") {
        imagePath = appDir + "/assets/2v2colors/Blue.png";
    } else if (color == "58d4e0") {
        imagePath = appDir + "/assets/2v2colors/skyblue.png";
    } else if (color == "9848b8") {
        imagePath = appDir + "/assets/2v2colors/Purple.png";
    } else if (color == "f8ace8") {
        imagePath = appDir + "/assets/2v2colors/Pink.png";
    } else if (color == "f8b048") {
        imagePath = appDir + "/assets/2v2colors/Orange.png";
    } else {
        // 对于未知的颜色，尝试基于RGB值选择最接近的颜色
        bool ok;
        unsigned int colorHex = QString::fromStdString(color).toUInt(&ok, 16);
        
        if (!ok) {
            return QPixmap(); // 如果颜色代码无效，返回空图片
        }
        
        // 解析RGB分量
        int r = (colorHex >> 16) & 0xFF;
        int g = (colorHex >> 8) & 0xFF;
        int b = colorHex & 0xFF;
        
        // 根据RGB分量的相对强度选择最匹配的颜色
        if (r > 200 && r > g*1.5 && r > b*1.5) {
            imagePath = appDir + "/assets/2v2colors/Red.png";
        } else if (b > 150 && b > r*1.5 && b > g*1.5) {
            imagePath = appDir + "/assets/2v2colors/Blue.png";
        } else if (g > 150 && g > r*1.2 && g > b*1.5) {
            imagePath = appDir + "/assets/2v2colors/Green.png";
        } else if (r > 180 && g > 180 && b < 100) {
            imagePath = appDir + "/assets/2v2colors/Yellow.png";
        } else if (r > 200 && g > 100 && g < 180 && b < 100) {
            imagePath = appDir + "/assets/2v2colors/Orange.png";
        } else if (r > 200 && b > 150 && g < r*0.8) {
            imagePath = appDir + "/assets/2v2colors/Pink.png";
        } else if (r > 100 && b > 100 && g < r*0.7 && g < b*0.7) {
            imagePath = appDir + "/assets/2v2colors/Purple.png";
        } else if (b > 150 && g > 150 && r < b*0.8) {
            imagePath = appDir + "/assets/2v2colors/skyblue.png";
        } else {
            return QPixmap(); // 如果无法匹配任何颜色，返回空图片
        }
    }
    
    // 加载并返回图片
    QPixmap colorImage(imagePath);
    
    if (colorImage.isNull()) {
        qDebug() << "无法加载颜色背景图片:" << imagePath;
    }
    
    return colorImage;
}

// 初始化比分UI元素
void ObTeam::initScoreUI() {
    // 获取显示比例
    float ratio = gls->l.ratio;
    
    // 加载MISANS-HEAVY字体
    QFont scoreFont;
    QString appDir = QCoreApplication::applicationDirPath();
    QString fontPath = appDir + "/assets/fonts/MISANS-HEAVY.TTF";
    
    // 尝试加载字体
    int fontId = QFontDatabase::addApplicationFont(fontPath);
    if (fontId != -1) {
        QStringList fontFamilies = QFontDatabase::applicationFontFamilies(fontId);
        if (!fontFamilies.isEmpty()) {
            scoreFont.setFamily(fontFamilies.first());
            qDebug() << "成功加载队伍比分字体:" << fontFamilies.first();
        } else {
            // 如果加载失败，使用系统字体
            scoreFont.setFamily("Arial");
            qDebug() << "无法获取字体族，使用Arial字体";
        }
    } else {
        // 如果加载失败，使用系统字体
        scoreFont.setFamily("Arial");
        qDebug() << "无法加载字体文件，使用Arial字体";
    }
    
    // 设置字体大小和粗细
    scoreFont.setPointSize(qRound(28 * ratio));
    scoreFont.setWeight(QFont::Black);
    
    // 创建队伍1比分标签
    if (!team1ScoreLabel) {
        team1ScoreLabel = new QLabel(this);
        team1ScoreLabel->setFont(scoreFont);
        team1ScoreLabel->setStyleSheet("color: white;"); // 只设置颜色，移除背景
        team1ScoreLabel->setAlignment(Qt::AlignCenter);
        team1ScoreLabel->setFixedSize(qRound(80 * ratio), qRound(60 * ratio));
        
        // 位置：使用与第一个玩家名称相同的水平位置
        int panelLeft = gls->l.top_m - qRound(1167 * ratio) / 2;
        team1ScoreLabel->move(panelLeft + qRound(player1NameOffsetX * ratio) - team1ScoreLabel->width() / 2 - qRound(30 * ratio), qRound(11 * ratio));
        team1ScoreLabel->setText("0");
        // 初始时隐藏，只在游戏有效时显示
        team1ScoreLabel->hide();
        
        // 安装事件过滤器
        team1ScoreLabel->installEventFilter(this);
    }
    
    // 创建队伍2比分标签
    if (!team2ScoreLabel) {
        team2ScoreLabel = new QLabel(this);
        team2ScoreLabel->setFont(scoreFont);
        team2ScoreLabel->setStyleSheet("color: white;"); // 只设置颜色，移除背景
        team2ScoreLabel->setAlignment(Qt::AlignCenter);
        team2ScoreLabel->setFixedSize(qRound(80 * ratio), qRound(60 * ratio));
        
        // 位置：使用与第二队第一个玩家名称相同的水平位置
        int panelRight = gls->l.top_m + qRound(1167 * ratio) / 2;
        team2ScoreLabel->move(panelRight - qRound(player1NameOffsetX * ratio) - team2ScoreLabel->width() / 2 - qRound(30 * ratio), qRound(11 * ratio));
        team2ScoreLabel->setText("0");
        // 初始时隐藏，只在游戏有效时显示
        team2ScoreLabel->hide();
        
        // 安装事件过滤器
        team2ScoreLabel->installEventFilter(this);
    }
}

// 更新队伍比分显示
void ObTeam::updateTeamScores() {
    // 如果比分标签尚未创建，先初始化UI
    if (!team1ScoreLabel || !team2ScoreLabel) {
        initScoreUI();
    } else {
        // 更新字体大小以应用当前ratio
        float ratio = gls->l.ratio;
        QFont scoreFont = team1ScoreLabel->font();
        scoreFont.setPointSize(qRound(28 * ratio));
        team1ScoreLabel->setFont(scoreFont);
        team2ScoreLabel->setFont(scoreFont);
        
        // 更新大小
        team1ScoreLabel->setFixedSize(qRound(80 * ratio), qRound(60 * ratio));
        team2ScoreLabel->setFixedSize(qRound(80 * ratio), qRound(60 * ratio));
        
        // 只有在非动画状态下才直接更新位置
        // 如果动画正在进行，paintEvent会处理位置
        if (!animationActive && elementsAnimProgress >= 1.0) {
            // 使用与玩家名称相同的水平位置
            int panelLeft = gls->l.top_m - qRound(1167 * ratio) / 2;
            int panelRight = gls->l.top_m + qRound(1167 * ratio) / 2;
            
            team1ScoreLabel->move(panelLeft + qRound(player1NameOffsetX * ratio) - team1ScoreLabel->width() / 2 - qRound(30 * ratio), qRound(11 * ratio));
            team2ScoreLabel->move(panelRight - qRound(player1NameOffsetX * ratio) - team2ScoreLabel->width() / 2 - qRound(30 * ratio), qRound(11 * ratio));
        }
    }
    
    // 更新比分显示
    team1ScoreLabel->setText(QString::number(team1Score));
    team2ScoreLabel->setText(QString::number(team2Score));
}

// 设置队伍信息
void ObTeam::setTeamInfo(const QString &team1Members, const QString &team2Members) {
    // 设置队伍ID
    team1Id = team1Members;
    team2Id = team2Members;
    
    // 从全局设置获取比分
    if (gls && !team1Id.isEmpty()) {
        if (gls->teamScores.contains(team1Id)) {
            team1Score = gls->teamScores[team1Id];
        } else {
            team1Score = 0; // 默认为0
        }
    }
    
    if (gls && !team2Id.isEmpty()) {
        if (gls->teamScores.contains(team2Id)) {
            team2Score = gls->teamScores[team2Id];
        } else {
            team2Score = 0; // 默认为0
        }
    }
    
    // 更新比分显示
    updateTeamScores();
}

// 计算队伍ID (排序玩家名称)
QString ObTeam::calculateTeamId(const QString &player1, const QString &player2) {
    QStringList players;
    if (!player1.isEmpty()) players << player1;
    if (!player2.isEmpty()) players << player2;
    players.sort(); // 排序确保顺序一致性
    return players.join("+");
}

// 实现事件过滤器以处理鼠标点击事件
bool ObTeam::eventFilter(QObject *watched, QEvent *event) {
    // 检查是否是比分标签上的鼠标事件
    if ((watched == team1ScoreLabel || watched == team2ScoreLabel) && 
        event->type() == QEvent::MouseButtonPress) {
        QMouseEvent *mouseEvent = static_cast<QMouseEvent*>(event);
        
        // 检查Ctrl键是否按下
        if (QApplication::keyboardModifiers() & Qt::ControlModifier) {
            // 处理队伍1比分
            if (watched == team1ScoreLabel) {
                // 左键点击增加分数
                if (mouseEvent->button() == Qt::LeftButton) {
                    team1Score++;
                    team1ScoreLabel->setText(QString::number(team1Score));
                    
                    // 更新全局比分记录
                    if (gls && !team1Id.isEmpty()) {
                        gls->teamScores[team1Id] = team1Score;
                    }
                    
                    return true;
                }
                // 右键点击减少分数
                else if (mouseEvent->button() == Qt::RightButton && team1Score > 0) {
                    team1Score--;
                    team1ScoreLabel->setText(QString::number(team1Score));
                    
                    // 更新全局比分记录
                    if (gls && !team1Id.isEmpty()) {
                        gls->teamScores[team1Id] = team1Score;
                    }
                    
                    return true;
                }
            }
            // 处理队伍2比分
            else if (watched == team2ScoreLabel) {
                // 左键点击增加分数
                if (mouseEvent->button() == Qt::LeftButton) {
                    team2Score++;
                    team2ScoreLabel->setText(QString::number(team2Score));
                    
                    // 更新全局比分记录
                    if (gls && !team2Id.isEmpty()) {
                        gls->teamScores[team2Id] = team2Score;
                    }
                    
                    return true;
                }
                // 右键点击减少分数
                else if (mouseEvent->button() == Qt::RightButton && team2Score > 0) {
                    team2Score--;
                    team2ScoreLabel->setText(QString::number(team2Score));
                    
                    // 更新全局比分记录
                    if (gls && !team2Id.isEmpty()) {
                        gls->teamScores[team2Id] = team2Score;
                    }
                    
                    return true;
                }
            }
        }
    }
    
    // 调用父类的事件过滤器
    return QWidget::eventFilter(watched, event);
}

void ObTeam::checkTeamStatus() {
    // 游戏无效或已处理过分数，则不检测
    if (!g._gameInfo.valid || gameFinished) {
        return;
    }
    
    // 检查游戏时间，太短的游戏不计分
    int gameTime;
    if (g._gameInfo.realGameTime > 0) {
        gameTime = g._gameInfo.realGameTime;
    } else {
        gameTime = g._gameInfo.currentFrame / Ra2ob::GAMESPEED;
    }
    
    // 游戏时间少于70秒不计分
    if (gameTime < 70) {
        return;
    }
    
    // 确保已检测到两个队伍
    if (team1Players.isEmpty() || team2Players.isEmpty()) {
        return;
    }
    
    // 检查第一队的失败状态
    bool team1AllDefeated = !team1Players.isEmpty();
    for (int i = 0; i < team1Players.size(); ++i) {
        int playerIdx = team1Players[i];
        if (playerIdx >= 0 && playerIdx < Ra2ob::MAXPLAYER && g._gameInfo.players[playerIdx].valid) {
            if (!g._playerDefeatFlag[playerIdx]) {
                team1AllDefeated = false;
                break;
            }
        }
    }
    
    // 检查第二队的失败状态
    bool team2AllDefeated = !team2Players.isEmpty();
    for (int i = 0; i < team2Players.size(); ++i) {
        int playerIdx = team2Players[i];
        if (playerIdx >= 0 && playerIdx < Ra2ob::MAXPLAYER && g._gameInfo.players[playerIdx].valid) {
            if (!g._playerDefeatFlag[playerIdx]) {
                team2AllDefeated = false;
                break;
            }
        }
    }
    
    // 如果任一队伍全部失败，更新比分
    if (team1AllDefeated || team2AllDefeated) {
        gameFinished = true; // 标记游戏已处理
        updateTeamScoreFromGameResult();
    }
}

void ObTeam::updateTeamScoreFromGameResult() {
    // 检查第一队的失败状态
    bool team1AllDefeated = !team1Players.isEmpty();
    for (int i = 0; i < team1Players.size(); ++i) {
        int playerIdx = team1Players[i];
        if (playerIdx >= 0 && playerIdx < Ra2ob::MAXPLAYER && g._gameInfo.players[playerIdx].valid) {
            if (!g._playerDefeatFlag[playerIdx]) {
                team1AllDefeated = false;
                break;
            }
        }
    }
    
    // 检查第二队的失败状态
    bool team2AllDefeated = !team2Players.isEmpty();
    for (int i = 0; i < team2Players.size(); ++i) {
        int playerIdx = team2Players[i];
        if (playerIdx >= 0 && playerIdx < Ra2ob::MAXPLAYER && g._gameInfo.players[playerIdx].valid) {
            if (!g._playerDefeatFlag[playerIdx]) {
                team2AllDefeated = false;
                break;
            }
        }
    }
    
    // 提取队伍成员的名字
    QStringList team1Names, team2Names;
    for (int i = 0; i < team1Players.size(); ++i) {
        int playerIdx = team1Players[i];
        if (playerIdx >= 0 && playerIdx < Ra2ob::MAXPLAYER && g._gameInfo.players[playerIdx].valid) {
            QString playerNickname = QString::fromStdString(g._gameInfo.players[playerIdx].panel.playerNameUtf);
            QString playerName = gls->findNameByNickname(playerNickname);
            if (playerName.isEmpty()) playerName = playerNickname;
            team1Names.append(playerName);
        }
    }
    
    for (int i = 0; i < team2Players.size(); ++i) {
        int playerIdx = team2Players[i];
        if (playerIdx >= 0 && playerIdx < Ra2ob::MAXPLAYER && g._gameInfo.players[playerIdx].valid) {
            QString playerNickname = QString::fromStdString(g._gameInfo.players[playerIdx].panel.playerNameUtf);
            QString playerName = gls->findNameByNickname(playerNickname);
            if (playerName.isEmpty()) playerName = playerNickname;
            team2Names.append(playerName);
        }
    }
    
    // 根据失败情况更新分数
    if (team1AllDefeated && !team2AllDefeated) {
        // 第一队全部失败，第二队加分
        team2Score++;
        if (gls && !team2Id.isEmpty()) {
            gls->teamScores[team2Id] = team2Score;
            qDebug() << "2V2模式: 团队比分更新 - 队伍1 (" << team1Names.join("+") 
                     << ") 全部失败，队伍2 (" << team2Names.join("+") 
                     << ") 加1分，当前比分: " << team1Score << ":" << team2Score;
        }
    } 
    else if (team2AllDefeated && !team1AllDefeated) {
        // 第二队全部失败，第一队加分
        team1Score++;
        if (gls && !team1Id.isEmpty()) {
            gls->teamScores[team1Id] = team1Score;
            qDebug() << "2V2模式: 团队比分更新 - 队伍2 (" << team2Names.join("+") 
                     << ") 全部失败，队伍1 (" << team1Names.join("+") 
                     << ") 加1分，当前比分: " << team1Score << ":" << team2Score;
        }
    }
    
    // 更新分数显示
    updateTeamScores();
    
    // 停止团队状态跟踪定时器，避免重复计分
    if (teamStatusTracking) {
        teamStatusTimer->stop();
        teamStatusTracking = false;
    }
    
    // 3秒后强制结束游戏进程 - 已取消
    /*
    #ifdef Q_OS_WIN
        qDebug() << "2V2模式: 玩家被击败并加分完毕，3秒后强制结束游戏进程";
        QTimer::singleShot(3000, []() {
            QProcess::execute("taskkill", QStringList() << "/F" << "/IM" << "gamemd-spawn.exe");
            qDebug() << "已发送强制结束游戏进程命令";
        });
    #endif
    */
}

// 实现强制内存回收方法
void ObTeam::forceMemoryCleanup() {
    // 固定60秒间隔清理
    qDebug() << "2V2模式：执行定期内存回收";
    
    // 强制系统回收内存
    #ifdef Q_OS_WIN
        SetProcessWorkingSetSize(GetCurrentProcess(), (SIZE_T)-1, (SIZE_T)-1);
    #endif
    
    // 清理图像缓存，但仅在游戏无效时清理所有缓存
    if (!g._gameInfo.valid) {
        // 游戏无效，清理所有缓存
        flagCache.clear();
        unitIconCache.clear();
        colorBgCache.clear();
        QPixmapCache::clear();
        qDebug() << "2V2模式：游戏无效，清理所有缓存";
    } else {
        // 游戏有效，只清理部分缓存以避免影响性能
        // 这里可以根据需要选择性清理某些不常用的缓存
    }
}

// 在ObTeam类中添加清理单位显示的方法
void ObTeam::clearUnitDisplay() {
    // 清空单位缓存
    unitIconCache.clear();
    unitIconsLoaded = false;
    
    // 强制清空所有单位状态，确保下次重新加载
    for (int i = 0; i < Ra2ob::MAXPLAYER; i++) {
        if (g._gameInfo.players[i].valid) {
            // 清空单位列表，确保不会显示旧单位
            g._gameInfo.players[i].units.units.clear();
        }
    }
    
    // 强制重新加载单位图标
    QDir iconDir(QCoreApplication::applicationDirPath() + "/assets/obicons");
    if (iconDir.exists()) {
        // 预加载一个默认图标以避免显示错误
        QString defaultIconPath = iconDir.path() + "/unit_placeholder_trans.png";
        if (QFile::exists(defaultIconPath)) {
            QPixmap defaultIcon(defaultIconPath);
            if (!defaultIcon.isNull()) {
                unitIconCache.insert("default", defaultIcon);
            }
        }
    }
    
    qDebug() << "2V2模式：清空单位显示，清理缓存和状态";
}

// 添加绘制地图名称的方法
void ObTeam::paintMapName(QPainter *painter) {
    // 如果底部面板隐藏，也隐藏地图名称
    if (!gls->s.show_bottom_panel) {
        if (lb_mapname) {
            lb_mapname->hide();
        }
        return;
    }
    
    // 如果没有有效游戏，不显示地图名称
    if (!g._gameInfo.valid) {
        return;
    }

    float ratio = gls->l.ratio;
    
    // 初始化地图名称标签
    if (lb_mapname == nullptr) {
        lb_mapname = new QOutlineLabel(this);
        
        // 使用预加载的字体
        QFont scaledMapNameFont = mapNameFont;
        scaledMapNameFont.setPointSizeF(12 * ratio);
        
        lb_mapname->setFont(scaledMapNameFont);
        lb_mapname->setStyleSheet("color: white");
    } else {
        // 确保更新字体大小
        QFont scaledMapNameFont = mapNameFont;
        scaledMapNameFont.setPointSizeF(12 * ratio);
        lb_mapname->setFont(scaledMapNameFont);
    }

    QString mapName = QString::fromUtf8(g._gameInfo.mapNameUtf);
    if (mapName.length() <= 1) {
        return;
    }

    lb_mapname->setText(mapName);
    lb_mapname->adjustSize();

    int fw = lb_mapname->width();
    int fh = lb_mapname->height();
    
    // 计算地图名称在2v2模式下的位置 - 与经济条相似但稍上方
    int bottomY = this->height() - qRound(32 * ratio); // 底部面板位置
    int px = 0;
    int py = bottomY - fh - qRound(2 * ratio); // 位于经济条上方

    lb_mapname->setGeometry(px + qRound(5 * ratio), py - qRound(2 * ratio), fw, fh);
    lb_mapname->show();
}

void ObTeam::initAnimationSystem() {
    // 初始化动画属性
    panelAnimProgress = 1.0;      // 初始设为1.0表示动画已完成
    elementsAnimProgress = 1.0;    // 初始设为1.0表示动画已完成
    glowAnimProgress = 0.0;        // 初始设为0.0表示未开始
    animationActive = false;       // 初始设为false表示动画未活跃
    
    // 创建面板垂直滑入动画
    panelSlideAnim = new QPropertyAnimation(this, "panelAnimProgress");
    panelSlideAnim->setDuration(800);
    panelSlideAnim->setStartValue(0.0);
    panelSlideAnim->setEndValue(1.0);
    panelSlideAnim->setEasingCurve(QEasingCurve::OutCubic);
    
    // 创建元素淡入动画（不使用弹性效果）
    elementsRevealAnim = new QPropertyAnimation(this, "elementsAnimProgress");
    elementsRevealAnim->setDuration(800);
    elementsRevealAnim->setStartValue(0.0);
    elementsRevealAnim->setEndValue(1.0);
    elementsRevealAnim->setEasingCurve(QEasingCurve::OutCubic);
    
    // 创建发光效果动画
    glowEffectAnim = new QPropertyAnimation(this, "glowAnimProgress");
    glowEffectAnim->setDuration(1500);
    glowEffectAnim->setStartValue(0.0);
    glowEffectAnim->setEndValue(1.0);
    glowEffectAnim->setEasingCurve(QEasingCurve::OutQuad);
    
    // 创建延迟效果的暂停动画
    QSequentialAnimationGroup* elementsDelayGroup = new QSequentialAnimationGroup(this);
    QPauseAnimation* elementsDelay = new QPauseAnimation(300);
    elementsDelayGroup->addAnimation(elementsDelay);
    elementsDelayGroup->addAnimation(elementsRevealAnim);
    
    QSequentialAnimationGroup* glowDelayGroup = new QSequentialAnimationGroup(this);
    QPauseAnimation* glowDelay = new QPauseAnimation(400);
    glowDelayGroup->addAnimation(glowDelay);
    glowDelayGroup->addAnimation(glowEffectAnim);
    
    // 创建并行动画组
    mainAnimGroup = new QParallelAnimationGroup(this);
    mainAnimGroup->addAnimation(panelSlideAnim);
    mainAnimGroup->addAnimation(elementsDelayGroup);
    mainAnimGroup->addAnimation(glowDelayGroup);
    
    connect(mainAnimGroup, &QParallelAnimationGroup::finished, [this]() {
        animationActive = false;
        // 动画完成后更新界面
        update();
    });
}

void ObTeam::resetSpyInfiltrationStatus() {
    // 清空动态间谍渗透状态跟踪
    g_lastPlayerTechInfiltrated.clear();
    g_lastPlayerBarracksInfiltrated.clear();
    g_lastPlayerWarFactoryInfiltrated.clear();
    
    qDebug() << "重置所有玩家间谍渗透状态跟踪变量";
}

void ObTeam::showSpyAlert(int playerIndex, const QString& spyType) {
    // 确定玩家属于哪个队伍以及在队伍中的位置
    bool isTeam1 = false;
    int teamPosition = -1; // 在队伍中的位置 (0或1)
    
    // 检查玩家是否在队伍1中
    for (int i = 0; i < team1Players.size(); i++) {
        if (team1Players[i] == playerIndex) { // playerIndex现在是0-based
            isTeam1 = true;
            teamPosition = i;
            break;
        }
    }
    
    // 如果不在队伍1中，检查队伍2
    if (teamPosition == -1) {
        for (int i = 0; i < team2Players.size(); i++) {
            if (team2Players[i] == playerIndex) {
                teamPosition = i;
                break;
            }
        }
    }
    
    // 如果玩家不在任何队伍中，返回
    if (teamPosition == -1) {
        qDebug() << "玩家索引" << playerIndex << "不在任何队伍中";
        return;
    }
    
    // 根据队伍和队伍内位置选择对应的标签和定时器
    QLabel* alertLabel = nullptr;
    QTimer* alertTimer = nullptr;
    
    if (isTeam1) {
        if (teamPosition == 0) {
            alertLabel = lb_spyAlert1;
            alertTimer = spyAlertTimer1;
        } else {
            alertLabel = lb_spyAlert2;
            alertTimer = spyAlertTimer2;
        }
    } else {
        if (teamPosition == 0) {
            alertLabel = lb_spyAlert3;
            alertTimer = spyAlertTimer3;
        } else {
            alertLabel = lb_spyAlert4;
            alertTimer = spyAlertTimer4;
        }
    }
    
    if (!alertLabel || !alertTimer) return;
    
    // 构建图片路径 - 注意文件名首字母大写
    QString imagePath = QString("./assets/panels/%1.png").arg(spyType.left(1).toUpper() + spyType.mid(1));
    
    // 检查文件是否存在
    if (!QFile::exists(imagePath)) {
        qDebug() << "间谍渗透图片不存在:" << imagePath;
        return;
    }
    
    // 使用缓存机制加载图片
    QPixmap scaledPixmap;
    QString cacheKey = QString("%1_%2x%3").arg(spyType).arg(spyAlertWidth).arg(spyAlertHeight);
    
    if (spyInfiltrationCache.contains(cacheKey)) {
        scaledPixmap = spyInfiltrationCache[cacheKey];
    } else {
        // 加载原始图片
        QPixmap originalPixmap(imagePath);
        if (originalPixmap.isNull()) {
            qDebug() << "无法加载间谍渗透图片:" << imagePath;
            return;
        }
        
        // 获取比例和目标尺寸
        float ratio = gls->l.ratio;
        int targetWidth = spyAlertWidth * ratio;
        int targetHeight = spyAlertHeight * ratio;
        
        // 缩放图片
        scaledPixmap = originalPixmap.scaled(targetWidth, targetHeight, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        
        // 缓存缩放后的图片
        spyInfiltrationCache[cacheKey] = scaledPixmap;
    }
    
    alertLabel->setPixmap(scaledPixmap);
    
    // 设置位置 - 根据玩家所属队伍确定左右位置
    int alertX, alertY;
    float ratio = gls->l.ratio;
    
    // 根据队伍和队伍内位置设置坐标
    alertY = 78 * ratio; // 统一高度
    
    if (isTeam1) {
        // 队伍1 - 左侧显示，根据队伍内位置调整X坐标
        if (teamPosition == 0) {
            alertX = 400 * ratio; // 队伍1第一个玩家
        } else {
            alertX = 600 * ratio; // 队伍1第二个玩家
        }
    } else {
        // 队伍2 - 右侧显示，根据队伍内位置调整X坐标
        if (teamPosition == 0) {
            alertX = 1250 * ratio; // 队伍2第一个玩家
        } else {
            alertX = 1050 * ratio; // 队伍2第二个玩家
        }
    }
    
    alertLabel->move(alertX, alertY);
    alertLabel->show();
    alertLabel->raise();
    
    // 停止之前的定时器并启动新的5秒定时器
    alertTimer->stop();
    alertTimer->start(5000);
    
    qDebug() << QString("显示玩家%1间谍渗透提示: %2").arg(playerIndex).arg(spyType);
}

// 创建淡入效果
QGraphicsEffect* ObTeam::createFadeEffect(qreal progress) {
    QGraphicsOpacityEffect* effect = new QGraphicsOpacityEffect(this);
    effect->setOpacity(progress);
    return effect;
}

// 动画属性访问器
qreal ObTeam::getPanelAnimProgress() const { return panelAnimProgress; }
void ObTeam::setPanelAnimProgress(qreal value) { 
    panelAnimProgress = value; 
    update();
}

qreal ObTeam::getElementsAnimProgress() const { return elementsAnimProgress; }
void ObTeam::setElementsAnimProgress(qreal value) { 
    elementsAnimProgress = value; 
    update();
}

qreal ObTeam::getGlowAnimProgress() const { return glowAnimProgress; }
void ObTeam::setGlowAnimProgress(qreal value) { 
    glowAnimProgress = value; 
    update();
}

// Add the new methods after the getPlayerColorImage method

// Get player nickname by index
QString ObTeam::getPlayerNickname(int playerIndex) {
    if (playerIndex < 0 || playerIndex >= Ra2ob::MAXPLAYER || 
        !g._gameInfo.valid || !g._gameInfo.players[playerIndex].valid) {
        return QString();
    }
    
    return QString::fromUtf8(g._gameInfo.players[playerIndex].panel.playerNameUtf);
}

// Get player avatar by index
QPixmap ObTeam::getPlayerAvatar(int playerIndex) {
    if (playerIndex < 0 || playerIndex >= Ra2ob::MAXPLAYER || 
        !g._gameInfo.valid || !g._gameInfo.players[playerIndex].valid) {
        return QPixmap(); // Return empty pixmap for invalid player
    }
    
    // Get player nickname
    QString playerNickname = getPlayerNickname(playerIndex);
    
    // Check cache first - 添加时间戳检查以便于60秒更新一次
    static QMap<QString, qint64> lastAccessTimes;
    qint64 currentTime = QDateTime::currentMSecsSinceEpoch();
    
    // 如果缓存中有头像且60秒内已经访问过，直接使用缓存
    if (avatarCache.contains(playerNickname) && 
        lastAccessTimes.contains(playerNickname) &&
        currentTime - lastAccessTimes[playerNickname] < 60000) {
        return avatarCache[playerNickname];
    }
    
    // 更新上次访问时间
    lastAccessTimes[playerNickname] = currentTime;
    
    // Path to default avatar
    QString appDir = QCoreApplication::applicationDirPath();
    QString defaultAvatarPath = appDir + "/avatars/default.png";
    
    // Try to load avatar from nickname-based path
    QString localNicknamePath = appDir + "/avatars/" + playerNickname + ".png";
    if (QFile::exists(localNicknamePath)) {
        QPixmap avatar(localNicknamePath);
        if (!avatar.isNull()) {
            avatarCache[playerNickname] = avatar;
            return avatar;
        }
    }
    
    // Try to load avatar from normalized game name (similar to playerinfo.cpp)
    QString gameName = gls->findNameByNickname(playerNickname);
    if (!gameName.isEmpty()) {
        QString localGameNamePath = appDir + "/avatars/" + gameName + ".png";
        if (QFile::exists(localGameNamePath)) {
            QPixmap avatar(localGameNamePath);
            if (!avatar.isNull()) {
                avatarCache[playerNickname] = avatar;
                return avatar;
            }
        }
    }
    
    // 尝试从远程服务器加载头像
    loadAvatarFromRemote(playerIndex, playerNickname);
    
    // 远程加载是异步的，此时我们先使用国家阵营头像作为临时显示
    QPixmap countryAvatar = getCountryAvatar(QString::fromStdString(g._gameInfo.players[playerIndex].panel.country));
    if (!countryAvatar.isNull()) {
        // 不缓存国家头像，因为稍后远程头像可能会替换它
        return countryAvatar;
    }
    
    // Use default avatar if others fail
    QPixmap defaultAvatar(defaultAvatarPath);
    if (defaultAvatar.isNull()) {
        // Create a fallback avatar if even default is missing
        QPixmap fallbackAvatar(avatarSize, avatarSize);
        fallbackAvatar.fill(Qt::transparent);
        QPainter painter(&fallbackAvatar);
        painter.setPen(Qt::white);
        painter.setBrush(Qt::gray);
        painter.drawEllipse(0, 0, avatarSize, avatarSize);
        avatarCache[playerNickname] = fallbackAvatar;
        return fallbackAvatar;
    }
    
    // Cache and return the default avatar
    avatarCache[playerNickname] = defaultAvatar;
    return defaultAvatar;
}

// 从远程加载头像
void ObTeam::loadAvatarFromRemote(int playerIndex, const QString &nickname) {
    if (!networkManager || nickname.isEmpty()) {
        return;
    }
    
    // 使用静态变量存储上次加载时间，防止频繁请求
    static QMap<QString, qint64> lastLoadTimes;
    qint64 currentTime = QDateTime::currentMSecsSinceEpoch();
    
    // 检查是否在60秒内已经请求过该头像
    if (lastLoadTimes.contains(nickname)) {
        qint64 lastTime = lastLoadTimes[nickname];
        if (currentTime - lastTime < 60000) { // 60秒内不重复请求
            return;
        }
    }
    
    // 更新上次加载时间
    lastLoadTimes[nickname] = currentTime;
    
    // 获取游戏名称
    QString gameName = gls->findNameByNickname(nickname);
    
    // 生成时间戳，避免缓存
    QString timestamp = QString::number(QDateTime::currentMSecsSinceEpoch());
    
    // 优先使用游戏名称尝试加载远程头像
    QString url;
    if (!gameName.isEmpty()) {
        // TODO: 配置头像服务器地址
            url = QString("https://your-server.com/avatars/%1.png?t=%2").arg(gameName).arg(timestamp);
    } else {
        // 如果没有游戏名称，使用昵称尝试加载
        // TODO: 配置头像服务器地址
        url = QString("https://your-server.com/avatars/%1.png?t=%2").arg(nickname).arg(timestamp);
    }
    
    // 创建网络请求
    QNetworkRequest request(url);
    request.setRawHeader("User-Agent", "Ra2ob Qt Client");
    
    // 添加防缓存头
    request.setAttribute(QNetworkRequest::CacheLoadControlAttribute, QNetworkRequest::AlwaysNetwork);
    request.setRawHeader("Cache-Control", "no-cache");
    request.setRawHeader("Pragma", "no-cache");
    
    // 发送请求
    QNetworkReply *reply = networkManager->get(request);
    
    // 连接完成信号
    connect(reply, &QNetworkReply::finished, [=]() {
        if (reply->error() == QNetworkReply::NoError) {
            QByteArray imageData = reply->readAll();
            
            // 将图像数据加载到QPixmap
            QPixmap avatar;
            if (avatar.loadFromData(imageData)) {
                qDebug() << "成功下载头像:" << url;
                
                // 缓存头像
                avatarCache[nickname] = avatar;
                
                // 同时保存到本地，以便下次可以从本地加载
                QString appDir = QCoreApplication::applicationDirPath();
                QDir avatarsDir(appDir + "/avatars");
                if (!avatarsDir.exists()) {
                    avatarsDir.mkpath(".");
                }
                
                // 保存游戏名版本
                if (!gameName.isEmpty()) {
                    QString localSavePath = appDir + "/avatars/" + gameName + ".png";
                    if (avatar.save(localSavePath, "PNG")) {
                        qDebug() << "远程头像已保存到本地(游戏名):" << localSavePath;
                    }
                }
                
                // 如果昵称和游戏名不同，也保存以昵称命名的版本
                if (nickname != gameName) {
                    QString nicknameLocalPath = appDir + "/avatars/" + nickname + ".png";
                    if (avatar.save(nicknameLocalPath, "PNG")) {
                        qDebug() << "远程头像已保存到本地(昵称):" << nicknameLocalPath;
                    }
                }
                
                // 请求重绘界面，使用新下载的头像
                update();
            }
        } else {
            qDebug() << "下载头像失败:" << url << "，错误:" << reply->errorString();
        }
        
        reply->deleteLater();
    });
}

// 根据国家获取阵营头像
QPixmap ObTeam::getCountryAvatar(const QString &country) {
    QString countryName = country;
    if (countryName.isEmpty()) {
        countryName = "Americans"; // 默认使用美国阵营
    }
    
    QString appDir = QCoreApplication::applicationDirPath();
    QString avatarPath;
    
    // 根据国家选择头像 - 区分阵营
    if (countryName == "Russians" || countryName == "Cuba" || 
        countryName == "Iraq" || countryName == "Libya") {
        // 苏联阵营使用logo2.png
        avatarPath = appDir + "/assets/panels/logo2.png";
    } else if (countryName == "Americans" || countryName == "British" || 
               countryName == "France" || countryName == "Germans" || 
               countryName == "Korea") {
        // 盟军阵营使用logo.png
        avatarPath = appDir + "/assets/panels/logo.png";
    } else if (countryName == "Yuri") {
        // 尤里阵营可以使用专门的头像
        avatarPath = appDir + "/assets/panels/logo3.png";
    } else {
        // 如果是未知国家，尝试直接使用国家图标
        avatarPath = appDir + "/assets/countries/" + countryName + ".png";
    }
    
    // 检查文件是否存在
    if (QFile::exists(avatarPath)) {
        return QPixmap(avatarPath);
    }
    
    // 如果找不到国家头像，使用默认的美国国家图标
    QString defaultCountryPath = appDir + "/assets/countries/Americans.png";
    if (QFile::exists(defaultCountryPath)) {
        return QPixmap(defaultCountryPath);
    }
    
    // 如果都失败了，返回空图像
    return QPixmap();
}

// 实现鼠标移动事件
void ObTeam::mouseMoveEvent(QMouseEvent *event) {
    // 调用父类的实现以确保正常行为
    QWidget::mouseMoveEvent(event);
}

// 删除计算建造栏区域方法，改为直接在checkMousePositionForProducingBlocks中检测

// 实现检测鼠标位置方法
void ObTeam::checkMousePositionForProducingBlocks() {
    // 只有当游戏有效且界面可见时才检测
    if (!g._gameInfo.valid || !this->isVisible()) {
        return;
    }
    
    // 获取全局鼠标位置
    QPoint globalMousePos = QCursor::pos();
    
    // 将全局坐标转换为窗口坐标
    QPoint localMousePos = this->mapFromGlobal(globalMousePos);
    
    float ratio = gls->l.ratio;
    
    // 检测左右两侧各2位玩家的第一个建造栏区域
    int team1ProducingX = qRound(10 * ratio); // 从屏幕左边开始10px
    int team1ProducingY = qRound(80 * ratio); // 从屏幕顶部开始
    int blockWidth = qRound(producingBlockWidth * ratio);
    int blockHeight = qRound(producingBlockHeight * ratio);
    int teamMateSpacing = qRound(5 * ratio); // 队友之间的垂直间距
    
    // 计算右侧第一个建造栏位置
    int screenWidth = this->width();
    int rightPanelWidth = qRound(170 * ratio);
    int team2ProducingX = screenWidth - rightPanelWidth - blockWidth;
    int team2ProducingY = team1ProducingY;
    
    // 创建检测区域
    bool mouseInProducingArea = false;
    
    // 检查左侧2位玩家的第一个建造栏
    for (int i = 0; i < 2; i++) {
        int playerY = team1ProducingY + i * (blockHeight + teamMateSpacing);
        QRect blockRect(team1ProducingX, playerY, blockWidth, blockHeight);
        if (blockRect.contains(localMousePos)) {
            mouseInProducingArea = true;
            break;
        }
    }
    
    // 如果鼠标不在左侧建造栏，检查右侧2位玩家的第一个建造栏
    if (!mouseInProducingArea) {
        for (int i = 0; i < 2; i++) {
            int playerY = team2ProducingY + i * (blockHeight + teamMateSpacing);
            QRect blockRect(team2ProducingX, playerY, blockWidth, blockHeight);
            if (blockRect.contains(localMousePos)) {
                mouseInProducingArea = true;
                break;
            }
        }
    }
    
    // 根据鼠标位置决定是否显示建造栏
    bool shouldShow = !mouseInProducingArea;
    
    // 如果状态需要变化，则更新并触发重绘
    if (showProducingBlocks != shouldShow) {
        showProducingBlocks = shouldShow;
        qDebug() << "建造栏显示状态变更: " << (showProducingBlocks ? "显示" : "隐藏")
                 << ", 鼠标位置: " << localMousePos;
        this->update(); // 触发重绘
    }
}

// 实现resetAvatarLoadStatus方法
void ObTeam::resetAvatarLoadStatus() {
    // 清空头像缓存，强制下次需要时重新加载
    avatarCache.clear();
    avatarsLoaded = false;
    
    // 如果窗口可见，触发重绘以更新头像
    if (this->isVisible()) {
        update();
    }
    
    qDebug() << "头像缓存已清空，下次需要时将重新加载";
}

// 间谍渗透相关函数实现
void ObTeam::updateSpyInfiltrationDisplay() {
    if (!g._gameInfo.valid) return;
    
    // 检查配置开关和会员权限
    if (!Globalsetting::getInstance().s.show_spy_infiltration || 
        !MemberManager::getInstance().hasFeatureAccess(MemberManager::FeatureType::SpyInfiltration)) {
        return; // 配置关闭或非会员用户不显示间谍渗透功能
    }
    
    // 获取当前游戏时间（秒）
    int gameTime;
    if (g._gameInfo.realGameTime > 0) {
        gameTime = g._gameInfo.realGameTime;
    } else {
        gameTime = g._gameInfo.currentFrame / Ra2ob::GAMESPEED;
    }
    
    // 只在游戏运行30秒后才开始检测渗透状态
    if (gameTime < 30) return;
    
    // 遍历所有有效玩家（动态检测，支持0-7索引）
    for (int i = 0; i < Ra2ob::MAXPLAYER; i++) {
        // 检查玩家是否有效
        if (!g._gameInfo.players[i].valid) continue;
        
        uintptr_t playerBaseAddr = g._playerBases[i];
        if (playerBaseAddr == 0) continue;
        
        // 检查当前玩家的间谍渗透状态
        bool techInfiltrated = g.r.getBool(playerBaseAddr + Ra2ob::SIDE0_TECH_INFILTRATED_OFFSET) ||
                              g.r.getBool(playerBaseAddr + Ra2ob::SIDE1_TECH_INFILTRATED_OFFSET) ||
                              g.r.getBool(playerBaseAddr + Ra2ob::SIDE2_TECH_INFILTRATED_OFFSET);
        bool barracksInfiltrated = g.r.getBool(playerBaseAddr + Ra2ob::BARRACKS_INFILTRATED_OFFSET);
        bool warFactoryInfiltrated = g.r.getBool(playerBaseAddr + Ra2ob::WARFACTORY_INFILTRATED_OFFSET);
        
        // 获取上次状态（如果不存在则默认为false）
        bool lastTech = g_lastPlayerTechInfiltrated[i];
        bool lastBarracks = g_lastPlayerBarracksInfiltrated[i];
        bool lastWarFactory = g_lastPlayerWarFactoryInfiltrated[i];
        
        // 只在状态从false变为true时显示提示（新的渗透事件）
        if (techInfiltrated && !lastTech) {
            showSpyAlert(i, "spy3");  // 高科渗透对应spy3，传入实际玩家索引
        }
        if (barracksInfiltrated && !lastBarracks) {
            showSpyAlert(i, "spy2");  // 兵营渗透对应spy2，传入实际玩家索引
        }
        if (warFactoryInfiltrated && !lastWarFactory) {
            showSpyAlert(i, "spy1");  // 重工渗透对应spy1，传入实际玩家索引
        }
        
        // 更新上次状态
        g_lastPlayerTechInfiltrated[i] = techInfiltrated;
        g_lastPlayerBarracksInfiltrated[i] = barracksInfiltrated;
        g_lastPlayerWarFactoryInfiltrated[i] = warFactoryInfiltrated;
    }
}

void ObTeam::initSpyInfiltrationLabels() {
    // 获取全局设置中的比例
    float ratio = gls->l.ratio;
    
    // 初始化4位玩家的间谍渗透提示标签
    lb_spyAlert1 = new QLabel(this);
    lb_spyAlert1->setStyleSheet("background: transparent;");
    lb_spyAlert1->resize(spyAlertWidth * ratio, spyAlertHeight * ratio);
    lb_spyAlert1->setScaledContents(true);
    lb_spyAlert1->hide();
    lb_spyAlert1->raise();
    
    lb_spyAlert2 = new QLabel(this);
    lb_spyAlert2->setStyleSheet("background: transparent;");
    lb_spyAlert2->resize(spyAlertWidth * ratio, spyAlertHeight * ratio);
    lb_spyAlert2->setScaledContents(true);
    lb_spyAlert2->hide();
    lb_spyAlert2->raise();
    
    lb_spyAlert3 = new QLabel(this);
    lb_spyAlert3->setStyleSheet("background: transparent;");
    lb_spyAlert3->resize(spyAlertWidth * ratio, spyAlertHeight * ratio);
    lb_spyAlert3->setScaledContents(true);
    lb_spyAlert3->hide();
    lb_spyAlert3->raise();
    
    lb_spyAlert4 = new QLabel(this);
    lb_spyAlert4->setStyleSheet("background: transparent;");
    lb_spyAlert4->resize(spyAlertWidth * ratio, spyAlertHeight * ratio);
    lb_spyAlert4->setScaledContents(true);
    lb_spyAlert4->hide();
    lb_spyAlert4->raise();
    
    // 初始化4位玩家的间谍提示定时器
    spyAlertTimer1 = new QTimer(this);
    spyAlertTimer1->setSingleShot(true);
    connect(spyAlertTimer1, &QTimer::timeout, [this]() {
        if (lb_spyAlert1) {
            lb_spyAlert1->hide();
        }
        qDebug() << "玩家1间谍渗透提示已隐藏";
    });
    
    spyAlertTimer2 = new QTimer(this);
    spyAlertTimer2->setSingleShot(true);
    connect(spyAlertTimer2, &QTimer::timeout, [this]() {
        if (lb_spyAlert2) {
            lb_spyAlert2->hide();
        }
        qDebug() << "玩家2间谍渗透提示已隐藏";
    });
    
    spyAlertTimer3 = new QTimer(this);
    spyAlertTimer3->setSingleShot(true);
    connect(spyAlertTimer3, &QTimer::timeout, [this]() {
        if (lb_spyAlert3) {
            lb_spyAlert3->hide();
        }
        qDebug() << "玩家3间谍渗透提示已隐藏";
    });
    
    spyAlertTimer4 = new QTimer(this);
    spyAlertTimer4->setSingleShot(true);
    connect(spyAlertTimer4, &QTimer::timeout, [this]() {
        if (lb_spyAlert4) {
            lb_spyAlert4->hide();
        }
        qDebug() << "玩家4间谍渗透提示已隐藏";
    });
}

void ObTeam::onBuildingTheftDetected(const QString& thiefPlayerName, const QString& victimPlayerName, const QString& buildingName, int count) {
    qDebug() << "ObTeam: 检测到建筑偷取事件 -" 
             << "偷取者:" << thiefPlayerName 
             << "受害者:" << victimPlayerName 
             << "建筑:" << buildingName 
             << "数量:" << count;
    
    // 调用偷取提示管理器显示提示
    if (theftAlertManager) {
        theftAlertManager->showTheftAlert(thiefPlayerName, victimPlayerName, buildingName, count);
    }
}

void ObTeam::onMinerLossDetected(const QString& playerName, const QString& minerType, int count) {
    qDebug() << "ObTeam: 检测到矿车丢失事件 -" 
             << "玩家:" << playerName 
             << "矿车类型:" << minerType 
             << "数量:" << count;
    
    // 调用偷取提示管理器显示矿车丢失提示
    if (theftAlertManager) {
        theftAlertManager->showMinerLossAlert(playerName, minerType, count);
    }
}
