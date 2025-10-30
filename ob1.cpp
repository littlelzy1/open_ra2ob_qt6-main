#include "ob1.h"
#include "globalsetting.h"
#include "membermanager.h"
#include "buildingdetector.h"
#include "theftalertmanager.h"
#include "Ra2ob/Ra2ob/src/Constants.hpp"
#include <QCoreApplication>
#include <QDir>
#include <QScreen>
#include <QGuiApplication>
#include <QPixmap>
#include <QLabel>
#include <QFile>
#include <QMovie>
#include <QCoreApplication>
#include <QPainter>
#include <QPainterPath>
#include <QFont>
#include <QFontMetrics>
#include <QPixmapCache>
#include <QFontDatabase>
#include <QApplication>
#include <QEvent>
#include <QMouseEvent>
#include <QtWebSockets/QWebSocket>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDateTime>
#include <QUrl>
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#ifdef Q_OS_WIN
#include <windows.h>
#include <psapi.h>
#include <tlhelp32.h>
#endif

// 全局鼠标钩子静态变量定义
#ifdef Q_OS_WIN
HHOOK Ob1::mouseHook = nullptr;
Ob1* Ob1::instance = nullptr;
#endif

Ob1::Ob1(QWidget *parent) : QWidget(parent) {
    // 游戏实例通过单例模式获取，已在头文件中初始化
    
    // 设置窗口属性
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool);
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_ShowWithoutActivating);
    
    // 设置静态实例指针并安装全局鼠标钩子
#ifdef Q_OS_WIN
    instance = this;
    installGlobalMouseHook();
#endif
    
    // 初始化游戏检测定时器
    gameTimer = new QTimer(this);
    connect(gameTimer, &QTimer::timeout, this, &Ob1::onGameTimerTimeout);
    gameTimer->start(500); // 每0.5秒检测一次游戏状态
    
    // 创建内存清理定时器
    memoryCleanupTimer = new QTimer(this);
    connect(memoryCleanupTimer, &QTimer::timeout, this, &Ob1::forceMemoryCleanup);
    memoryCleanupTimer->setInterval(60000); // 每60秒执行一次内存回收
    memoryCleanupTimer->start();
    
    // 创建单位信息刷新定时器
    unitUpdateTimer = new QTimer(this);
    connect(unitUpdateTimer, &QTimer::timeout, this, &Ob1::onUnitUpdateTimerTimeout);
    unitUpdateTimer->setInterval(200); // 每0.2秒刷新一次单位信息
    unitUpdateTimer->start();
    
    // 初始化头像刷新定时器（50秒刷新一次）
    avatarRefreshTimer = new QTimer(this);
    connect(avatarRefreshTimer, &QTimer::timeout, this, &Ob1::resetAvatarLoadStatus);
    avatarRefreshTimer->setInterval(50000); // 设置为50秒
    avatarRefreshTimer->start();
    
    // 初始化建造栏检测定时器
    producingBlocksTimer = new QTimer(this);
    connect(producingBlocksTimer, &QTimer::timeout, this, &Ob1::checkMousePositionForProducingBlocks);
    producingBlocksTimer->setInterval(100); // 每100毫秒检测一次鼠标位置
    producingBlocksTimer->start();
    
    // 初始化建造栏显示状态
    showProducingBlocks = true;
    
    // 初始化窗口大小
    QScreen *screen = QGuiApplication::primaryScreen();
    if (screen) {
        QRect screenGeometry = screen->geometry();
        setGeometry(0, 0, screenGeometry.width(), screenGeometry.height());
    } else {
        // 默认大小
        resize(1920, 1080);
    }
    
    // 加载MISANS-HEAVY字体
    QString appDir = QCoreApplication::applicationDirPath();
    QString miSansHeavyPath = appDir + "/assets/fonts/MISANS-HEAVY.TTF";
    int miSansHeavyId = QFontDatabase::addApplicationFont(miSansHeavyPath);
    if (miSansHeavyId != -1) {
        QStringList families = QFontDatabase::applicationFontFamilies(miSansHeavyId);
        if (!families.isEmpty()) {
            miSansHeavyFamily = families.first();
            qDebug() << "成功加载MISANS-HEAVY字体，字体族:" << miSansHeavyFamily;
        }
    } else {
        qDebug() << "无法加载MISANS-HEAVY字体:" << miSansHeavyPath;
    }
    
    // 加载LLDEtechnoGlitch-Bold0字体
    QString technoGlitchPath = appDir + "/assets/fonts/LLDEtechnoGlitch-Bold0.ttf";
    int technoGlitchId = QFontDatabase::addApplicationFont(technoGlitchPath);
    if (technoGlitchId != -1) {
        QStringList families = QFontDatabase::applicationFontFamilies(technoGlitchId);
        if (!families.isEmpty()) {
            technoGlitchFamily = families.first();
            qDebug() << "成功加载LLDEtechnoGlitch-Bold0字体，字体族:" << technoGlitchFamily;
        }
    } else {
        qDebug() << "无法加载LLDEtechnoGlitch-Bold0字体:" << technoGlitchPath;
    }
    
    // 初始化小颜色标签
    initSmallColorLabels();
    
    // 初始化颜色GIF标签
    initColorGifLabels();
    
    // 初始化右侧颜色标签
    initRightColorLabels();
    
    // 初始化国旗标签
    initFlagLabels();
    
    // 初始化玩家名称标签
    initPlayerNameLabels();
    
    // 初始化电量条标签
    initPowerBarLabels();
    
    // 初始化击杀数和幸存数标签
    initStatsLabels();
    
    // 初始化余额和总消耗标签
    initEconomyLabels();
    
    // 初始化经济条
    initEconomyBars();
    
    // 初始化游戏时间标签
    initGameTimeLabel();
    
    // 初始化间谍渗透状态标签
    initSpyInfiltrationLabels();
    
    // 初始化分数标签
    initScoreLabels();
    
    // 初始化头像标签
    initAvatarLabels();
    
    // 初始化地图视频标签和帧缓存
    initMapVideoLabel();
    maxCacheSize = 10; // 设置最大缓存帧数
    initFrameCache();
    
    // 初始化滚动文字
    initScrollText();
    
    // 初始化网络管理器
    networkManager = new QNetworkAccessManager(this);
    
    // 初始化OBS配置
    obsWebSocketUrl = "ws://localhost:8063"; // OBS WebSocket默认地址
    obsPassword = ""; // OBS WebSocket密码，如果需要的话可以设置

    obsConnected = false; // 初始化连接状态
    obsAuthenticated = false; // 初始化认证状态
    
    // 初始化OBS连接
    initObsConnection();
    
    // 初始化玩家状态跟踪器
    statusTracker = new PlayerStatusTracker(this, this);
    connect(statusTracker, &PlayerStatusTracker::playerScoreChanged, 
            this, &Ob1::onPlayerScoreChanged);
    
    // 初始化建筑盗窃检测器
    buildingDetector = new BuildingDetector(this);
    theftAlertManager = new TheftAlertManager(this);
    
    // 连接建筑盗窃检测信号
    connect(buildingDetector, &BuildingDetector::buildingTheftDetected,
            this, &Ob1::onBuildingTheftDetected);
    
    // 连接矿车损失检测信号
    connect(buildingDetector, &BuildingDetector::minerLossDetected,
            this, &Ob1::onMinerLossDetected);
    
    // 启动建筑盗窃检测
    buildingDetector->startDetection(500); // 每500毫秒检测一次
    
    // 设置盗窃警报管理器
    theftAlertManager->setBuildingDetector(buildingDetector);
    
    // Shadow.png初始化代码已删除
    
    // 初始化PMB相关变量
    showPmbImage = Globalsetting::getInstance().s.show_pmb_image;
    pmbImage = QPixmap();
    pmbOffsetX = 0;
    pmbOffsetY = 0;
    pmbWidth = 137;
    pmbHeight = 220;
    
    // 默认隐藏窗口
    hide();
}

Ob1::~Ob1() {
    // 停止并清理所有定时器
    if (gameTimer) {
        gameTimer->stop();
        delete gameTimer;
        gameTimer = nullptr;
    }
    
    if (memoryCleanupTimer) {
        memoryCleanupTimer->stop();
        delete memoryCleanupTimer;
        memoryCleanupTimer = nullptr;
    }
    
    if (unitUpdateTimer) {
        unitUpdateTimer->stop();
        delete unitUpdateTimer;
        unitUpdateTimer = nullptr;
    }
    
    if (avatarRefreshTimer) {
        avatarRefreshTimer->stop();
        delete avatarRefreshTimer;
        avatarRefreshTimer = nullptr;
    }
    
    if (powerBarBlinkTimer) {
        powerBarBlinkTimer->stop();
        delete powerBarBlinkTimer;
        powerBarBlinkTimer = nullptr;
    }
    
    if (economyBarTimer) {
        economyBarTimer->stop();
        delete economyBarTimer;
        economyBarTimer = nullptr;
    }
    
    if (mapVideoTimer) {
        mapVideoTimer->stop();
        delete mapVideoTimer;
        mapVideoTimer = nullptr;
    }
    
    if (producingBlocksTimer) {
        producingBlocksTimer->stop();
        delete producingBlocksTimer;
        producingBlocksTimer = nullptr;
    }
    
    if (obsDelayTimer) {
        obsDelayTimer->stop();
        delete obsDelayTimer;
        obsDelayTimer = nullptr;
    }
    
    // 卸载全局鼠标钩子
#ifdef Q_OS_WIN
    uninstallGlobalMouseHook();
    instance = nullptr;
#endif
    
    // 清理玩家状态跟踪器
    if (statusTracker) {
        statusTracker->stopTracking();
        delete statusTracker;
        statusTracker = nullptr;
    }
    
    // 清理建筑盗窃检测器
    if (buildingDetector) {
        buildingDetector->stopDetection();
        delete buildingDetector;
        buildingDetector = nullptr;
    }
    
    // 清理盗窃警报管理器
    if (theftAlertManager) {
        delete theftAlertManager;
        theftAlertManager = nullptr;
    }
    
    // 游戏实例是单例，不需要在这里释放
    
    // 释放图片资源
    bg1v1Image = QPixmap();
    ecobgImage = QPixmap();
    bg1v12Image = QPixmap();
    
    // 清理小颜色图片缓存
    
    // 删除标签
    delete lb_smallColor1;
    delete lb_smallColor2;
    
    // 释放颜色GIF资源
    if (lb_colorGif1) {
        if (lb_colorGif1->movie()) {
            lb_colorGif1->movie()->stop();
            delete lb_colorGif1->movie();
        }
        delete lb_colorGif1;
    }
    if (lb_colorGif2) {
        if (lb_colorGif2->movie()) {
            lb_colorGif2->movie()->stop();
            delete lb_colorGif2->movie();
        }
        delete lb_colorGif2;
    }
    
    // 清理右侧颜色图片缓存
    
    // 删除右侧颜色标签
    delete lb_rightColor1;
    delete lb_rightColor2;
    
    // 清理国旗图片缓存
    flagCache.clear();
    
    // 清理新国旗图片缓存
    newFlagCache.clear();
    
    // 删除国旗标签
    delete lb_flag1;
    delete lb_flag2;
    
    // 删除新国旗组标签
    delete lb_newFlag1;
    delete lb_newFlag2;
    
    // 删除玩家名称标签
    delete lb_playerName1;
    delete lb_playerName2;
    
    // 删除电量条标签
    delete lb_powerBar1;
    delete lb_powerBar2;
    
    // 删除击杀数和幸存数标签
    delete lb_kills1;
    delete lb_kills2;
    delete lb_alive1;
    delete lb_alive2;
    
    // 删除余额和总消耗标签
    delete lb_balance1;
    delete lb_balance2;
    delete lb_spent1;
    delete lb_spent2;
    
    // 删除游戏时间标签
    delete lb_gameTime;
    
    // 删除分数标签
    delete lb_score1;
    delete lb_score2;
    
    // 删除头像标签
    delete lb_avatar1;
    delete lb_avatar2;
    
    // 清理头像缓存
    playerAvatar1 = QPixmap();
    playerAvatar2 = QPixmap();
    
    // 网络管理器会被Qt自动清理
    
    // Shadow.png清理代码已删除
    
    // 清理地图视频相关资源
    clearFrameCache();
    delete lb_mapVideo;
    
    // 清理滚动文字相关资源
    if (scrollTimer) {
        scrollTimer->stop();
        delete scrollTimer;
    }
    
    if (lb_scrollText) {
        // 删除所有子标签
        QList<QLabel*> childLabels = lb_scrollText->findChildren<QLabel*>();
        for (QLabel* label : childLabels) {
            delete label;
        }
        delete lb_scrollText;
    }
}

void Ob1::detectGame() {
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
        
        // 启动4秒延迟定时器隐藏游戏结束源 - 已注释：取消隐藏游戏结束源的逻辑
        /*
        QTimer::singleShot(4000, [this]() {
            switchObsScene("游戏结束");  // 隐藏游戏结束源
            qDebug() << "延迟4秒后隐藏OBS源: 游戏结束";
        });
        */
    }
    
    // 检测进程从有到无的转变
    if (!processRunning && lastProcessRunning) {
        // 进程刚结束，隐藏"游戏中"源并显示"游戏结束"源
        // switchObsScene("游戏中");  // 已注释：取消进程结束时的OBS信号
        // qDebug() << "检测到游戏进程结束，隐藏OBS源: 游戏中";  // 已注释：取消进程结束时的OBS信号
        qDebug() << "检测到游戏进程结束，但已取消OBS信号发送";
        
    
    }
    
    // 记住上次进程状态
    lastProcessRunning = processRunning;
    
    // 检测游戏是否刚刚开始
    bool gameJustStarted = g._gameInfo.valid && !lastGameValid;
    bool gameJustEnded = !g._gameInfo.valid && lastGameValid;

    // 记住上次游戏状态
    lastGameValid = g._gameInfo.valid;

    // 游戏刚开始时加载图片
    if (gameJustStarted) {
        
        // 重置间谍渗透时间跟踪数据
        player1InfiltrationTimes.clear();
    player2InfiltrationTimes.clear();
    player1InfiltrationOrder.clear();
    player2InfiltrationOrder.clear();
    player1TechInfiltrationImage.clear();
    player2TechInfiltrationImage.clear();
    
    // 重置动态玩家索引
    validPlayerIndex1 = -1;
    validPlayerIndex2 = -1;
        qDebug() << "游戏开始，重置间谍渗透时间跟踪数据";
        
        // 重置间谍渗透提示状态
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
        qDebug() << "游戏开始，重置间谍渗透提示状态";
        
        // 获取应用程序目录路径
        QString appDir = QCoreApplication::applicationDirPath();
        
        // 加载背景图
        bg1v1Image.load(appDir + "/assets/panels/bg1v1.png");
        ecobgImage.load(appDir + "/assets/panels/ecobg.png");
        bg1v12Image.load(appDir + "/assets/panels/bg1v12.png");
        
        // 识别玩家
        p1_index = -1;
        p2_index = -1;
        for (int i = 0; i < Ra2ob::MAXPLAYER; i++) {
            if (g._gameInfo.players[i].valid) {
                if (p1_index == -1) {
                    p1_index = i;
                } else if (p2_index == -1) {
                    p2_index = i;
                    break;
                }
            }
        }
        
        // 设置玩家颜色
        if (p1_index != -1) {
            setPlayerColor(p1_index);
        }
        if (p2_index != -1) {
            setPlayerColor(p2_index);
        }
        
        // 更新玩家名称显示
        updatePlayerNameDisplay();
        
        // 更新电量条显示
        updatePowerBars();
        
        // 更新击杀数和幸存数显示
        updateStatsDisplay();
        
        // 更新余额和总消耗显示
        updateEconomyDisplay();
        
        // 更新分数显示
        updateScoreDisplay();
        
        // 重置头像加载状态
        remoteAvatarLoaded1 = false;
        remoteAvatarLoaded2 = false;
        
        // 更新头像显示
        updateAvatarDisplay();
        
        // 重置经济条画布
        economyBarPixmap1.fill(Qt::transparent);
        economyBarPixmap2.fill(Qt::transparent);
        
        // 初始化地图视频定时器（如果还未创建）
        if (!mapVideoTimer) {
            mapVideoTimer = new QTimer(this);
            connect(mapVideoTimer, &QTimer::timeout, [this]() {
                if (totalFrames > 0 && this->isVisible()) {
                    currentMapVideoFrame++;
                    if (currentMapVideoFrame >= totalFrames) {
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
        
        // 初始化地图视频序列（延迟加载）
        if (!mapVideoInitialized) {
            // 检查游戏是否有效
            if (!g._gameInfo.valid) {
                return;
            }
            QString mapNameUtf = QString::fromStdString(g._gameInfo.mapNameUtf);
            qDebug() << "游戏开始，初始化地图视频序列，地图名:" << mapNameUtf;
            if (!mapNameUtf.isEmpty()) {
                initMapVideoSequence(mapNameUtf);
            }
        } else {
            qDebug() << "游戏开始，地图视频序列已初始化，总帧数:" << totalFrames;
        }
        
        // 启动玩家状态跟踪器
        if (statusTracker) {
            statusTracker->startTracking();
        }
        
        // 更新界面
        update();
        
        // 显示窗口
        toggleOb();
    }
    
    // 游戏刚结束时的内存清理代码
    if (gameJustEnded) {
        // 释放图片资源
        bg1v1Image = QPixmap();
        ecobgImage = QPixmap();
        bg1v12Image = QPixmap();
        

        
        // 清理国旗图片缓存
        flagCache.clear();
        
        // 清理新国旗图片缓存
        newFlagCache.clear();
        
        // 清理单位图标缓存
        unitIconCache.clear();
        
        // 清理颜色GIF缓存
        // 先停止所有GIF动画并删除QMovie对象
        for (auto it = colorCache.begin(); it != colorCache.end(); ++it) {
            if (it.value()) {
                it.value()->stop();
                delete it.value();
            }
        }
        colorCache.clear();
        
        // 重置GIF加载状态
        color1Loaded = false;
        color2Loaded = false;
        
        // 清理头像缓存
        playerAvatar1 = QPixmap();
        playerAvatar2 = QPixmap();
        remoteAvatarLoaded1 = false;
        remoteAvatarLoaded2 = false;
        
        // Shadow.png清理代码已删除
        
        // 停止并清理地图视频
        if (mapVideoTimer) {
            mapVideoTimer->stop();
        }
        clearFrameCache();
        currentMapVideoFrame = 0;
        mapVideoPlaying = false;
        mapVideoCompleted = false;
        mapVideoInitialized = false;
        totalFrames = 0;
        frameSequence.clear();
        videoDir.clear();
        
        // 停止玩家状态跟踪器
        if (statusTracker) {
            statusTracker->stopTracking();
        }
        
        // 清理单位显示
        clearUnitDisplay();
        
        // 隐藏间谍渗透标签
        if (lb_spyInfiltration1_tech) lb_spyInfiltration1_tech->hide();
        if (lb_spyInfiltration1_barracks) lb_spyInfiltration1_barracks->hide();
        if (lb_spyInfiltration1_warfactory) lb_spyInfiltration1_warfactory->hide();
        if (lb_spyInfiltration2_tech) lb_spyInfiltration2_tech->hide();
        if (lb_spyInfiltration2_barracks) lb_spyInfiltration2_barracks->hide();
        if (lb_spyInfiltration2_warfactory) lb_spyInfiltration2_warfactory->hide();
        
        // 强制系统回收内存
        #ifdef Q_OS_WIN
            SetProcessWorkingSetSize(GetCurrentProcess(), (SIZE_T)-1, (SIZE_T)-1);
        #endif
        
        // 强制Qt清理PixmapCache
        QPixmapCache::clear();
        
        // 更新界面
        update();
    }
    
    // 如果游戏有效，更新玩家信息
    if (g._gameInfo.valid && this->isVisible()) {
        // 寻找两个主要玩家
        p1_index = -1;
        p2_index = -1;
        
        for (int i = 0; i < Ra2ob::MAXPLAYER; i++) {
            if (g._gameInfo.players[i].valid) {
                if (p1_index == -1) {
                    p1_index = i;
                } else if (p2_index == -1) {
                    p2_index = i;
                    break;
                }
            }
        }
        
        // 设置玩家颜色
        if (p1_index != -1) {
            setPlayerColor(p1_index);
        }
        if (p2_index != -1) {
            setPlayerColor(p2_index);
        }
        
        // 更新玩家名称显示
        updatePlayerNameDisplay();
        
        // 更新电量条显示
        updatePowerBars();
        
        // 更新击杀数和幸存数显示
        updateStatsDisplay();
        
        // 更新余额和总消耗显示
        updateEconomyDisplay();
        
        // 更新分数显示
        updateScoreDisplay();
        
        // 需要重绘界面
        update();
    }
    
    // 检查窗口显示状态（响应快捷键变化）
    toggleOb();
    
    if (g._gameInfo.valid && this->isVisible()) {
        // 更新小颜色显示
        updateSmallColorDisplay();
    }
}

void Ob1::toggleOb() {
    bool shouldShow = false;
    
    // 首先检查全局设置是否允许显示所有面板（快捷键控制）
    if (Globalsetting::getInstance().s.show_all_panel) {
        // 检查游戏是否有效
        if (g._gameInfo.valid && 
            !g._gameInfo.isGamePaused && 
            g._gameInfo.currentFrame >= 5) {
            
            // 计算有效玩家数量
            int validPlayerCount = 0;
            for (int i = 0; i < Ra2ob::MAXPLAYER; i++) {
                if (g._gameInfo.players[i].valid) {
                    validPlayerCount++;
                }
            }
            
            if (validPlayerCount >= 2) {
                shouldShow = true;
            }
        }
    }
    
    // 更新窗口显示状态
    if (shouldShow != this->isVisible()) {
        if (shouldShow) {
            this->show();
            
            // 启动地图视频播放（用于中间显示）
            if (totalFrames > 0 && !mapVideoPlaying && !mapVideoCompleted && mapVideoTimer) {
                mapVideoTimer->start(40); // 每40毫秒一帧，25FPS
                mapVideoPlaying = true;
                qDebug() << "地图视频开始播放，总帧数:" << totalFrames;
            } else {
                qDebug() << "地图视频未启动 - 总帧数:" << totalFrames << ", 播放中:" << mapVideoPlaying << ", 已完成:" << mapVideoCompleted << ", 定时器:" << (mapVideoTimer != nullptr);
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
            if (mapVideoTimer) {
                mapVideoTimer->stop();
                mapVideoPlaying = false;
            }
        }
    }
}

// 初始化小颜色标签
void Ob1::initSmallColorLabels() {
    // 使用ob1.h中定义的位置
    // smallColor1X = 13.5;  // 玩家1小颜色X坐标
    // smallColor1Y = 1030;  // 玩家1小颜色Y坐标
    // smallColor2X = 13.5;  // 玩家2小颜色X坐标
    // smallColor2Y = 1055;  // 玩家2小颜色Y坐标
    
    // 创建小颜色标签
    lb_smallColor1 = new QLabel(this);
    lb_smallColor2 = new QLabel(this);
    
    // 设置标签属性
    lb_smallColor1->setScaledContents(true);
    lb_smallColor2->setScaledContents(true);
    
    // 确保标签在最上层
    lb_smallColor1->raise();
    lb_smallColor2->raise();
}

// 初始化颜色GIF标签
void Ob1::initColorGifLabels() {
  // 创建颜色GIF标签
    lb_colorGif1 = new FlippedLabel(this);
    lb_colorGif2 = new FlippedLabel(this);
    
    // 设置标签属性
    lb_colorGif1->setScaledContents(true);
    lb_colorGif2->setScaledContents(true);
    
    // 确保标签在最上层
    lb_colorGif1->raise();
    lb_colorGif2->raise();
}

// 初始化右侧颜色标签
void Ob1::initRightColorLabels() {
    // 创建右侧颜色标签
    lb_rightColor1 = new QLabel(this);
    lb_rightColor2 = new QLabel(this);
    
    // 设置标签属性
    lb_rightColor1->setScaledContents(true);
    lb_rightColor2->setScaledContents(true);
    
    // 确保标签在最上层
    lb_rightColor1->raise();
    lb_rightColor2->raise();
}

// 初始化国旗标签
void Ob1::initFlagLabels() {
    // 创建国旗标签
    lb_flag1 = new QLabel(this);
    lb_flag2 = new QLabel(this);
    
    // 设置标签属性
    lb_flag1->setScaledContents(true);
    lb_flag2->setScaledContents(true);
    
    // 确保标签在最上层
    lb_flag1->raise();
    lb_flag2->raise();
    
    // 初始化新国旗组标签
    initNewFlagLabels();
}

// 初始化新国旗组标签
void Ob1::initNewFlagLabels() {
    // 创建新国旗组标签
    lb_newFlag1 = new QLabel(this);
    lb_newFlag2 = new QLabel(this);
    
    // 设置标签属性
    lb_newFlag1->setScaledContents(true);
    lb_newFlag2->setScaledContents(true);
    
    // 新国旗组显示在右侧颜色图层的后面，不使用raise()
    // lb_newFlag1->raise();
    // lb_newFlag2->raise();
}

// 获取国旗图片
QPixmap Ob1::getCountryFlag(const QString& country) {
    if (country.isEmpty()) {
        return QPixmap();
    }
    
    // 检查缓存
    if (flagCache.contains(country)) {
        return flagCache[country];
    }
    
    // 标准化国家名称（首字母大写，其余小写）
    QString normalizedName = country.toLower();
    normalizedName[0] = normalizedName[0].toUpper();
    
    // 构建国旗图片路径并直接加载
    QString flagPath = QString("assets/1v1rightcountries/%1.png").arg(normalizedName);
    QPixmap flag(flagPath);
    
    // 缓存结果
    flagCache[country] = flag;
    
    return flag;
}

// 获取新国旗组图片
QPixmap Ob1::getNewCountryFlag(const QString& country) {
    if (country.isEmpty()) {
        return QPixmap();
    }
    
    // 检查新国旗组缓存
    if (newFlagCache.contains(country)) {
        return newFlagCache[country];
    }
    
    // 标准化国家名称（首字母大写，其余小写）
    QString normalizedName = country.toLower();
    normalizedName[0] = normalizedName[0].toUpper();
    
    // 构建新国旗组图片路径并直接加载
    QString flagPath = QString("assets/1v1rightcountries/%1.png").arg(normalizedName);
    QPixmap flag(flagPath);
    
    // 缓存结果
    newFlagCache[country] = flag;
    
    return flag;
}

// 更新国旗显示
void Ob1::updateFlagDisplay() {
    if (!g._gameInfo.valid) {
        // 游戏无效时，隐藏标签
        lb_flag1->hide();
        lb_flag2->hide();
        return;
    }
    
    // 计算国旗标签位置，应用比例
    float ratio = Globalsetting::getInstance().l.ratio;
    
    // 玩家1国旗位置
    int flag1XPos = qRound(flag1X * ratio);
    int flag1YPos = qRound(flag1Y * ratio);
    int flagW = qRound(flagWidth * ratio);
    int flagH = qRound(flagHeight * ratio);
    lb_flag1->setGeometry(flag1XPos, flag1YPos, flagW, flagH);
    
    // 玩家2国旗位置
    int flag2XPos = qRound(flag2X * ratio);
    int flag2YPos = qRound(flag2Y * ratio);
    lb_flag2->setGeometry(flag2XPos, flag2YPos, flagW, flagH);
    
    // 更新玩家1国旗
    if (p1_index >= 0 && p1_index < Ra2ob::MAXPLAYER && g._gameInfo.players[p1_index].valid) {
        std::string countryStr = g._gameInfo.players[p1_index].panel.country;
        QString country = QString::fromStdString(countryStr);
        
        // 获取国旗图片
        QPixmap flagPixmap = getCountryFlag(country);
        
        if (!flagPixmap.isNull()) {
            lb_flag1->setPixmap(flagPixmap.scaled(flagW, flagH, Qt::KeepAspectRatio, Qt::SmoothTransformation));
            lb_flag1->show();
        } else {
            lb_flag1->hide();
        }
    } else {
        lb_flag1->hide();
    }
    
    // 更新玩家2国旗
    if (p2_index >= 0 && p2_index < Ra2ob::MAXPLAYER && g._gameInfo.players[p2_index].valid) {
        std::string countryStr = g._gameInfo.players[p2_index].panel.country;
        QString country = QString::fromStdString(countryStr);
        
        // 获取国旗图片
        QPixmap flagPixmap = getCountryFlag(country);
        
        if (!flagPixmap.isNull()) {
            lb_flag2->setPixmap(flagPixmap.scaled(flagW, flagH, Qt::KeepAspectRatio, Qt::SmoothTransformation));
            lb_flag2->show();
        } else {
            lb_flag2->hide();
        }
    } else {
        lb_flag2->hide();
    }
}

// 更新新国旗组显示
void Ob1::updateNewFlagDisplay() {
    // 右侧国旗现在通过paintEvent直接绘制，不再使用QLabel
    // 隐藏相关的QLabel标签
    lb_newFlag1->hide();
    lb_newFlag2->hide();
}

// 统一颜色映射方法
QString Ob1::getColorName(const QString& colorCode) {
    QString lowerCode = colorCode.toLower();
    
    if (lowerCode == "e0d838") { // 黄色
        return "Yellow";
    } else if (lowerCode == "f84c48" || lowerCode == "ff0000") { // 红色
        return "Red";
    } else if (lowerCode == "58cc50") { // 绿色
        return "Green";
    } else if (lowerCode == "487cc8" || lowerCode == "0000ff") { // 蓝色
        return "Blue";
    } else if (lowerCode == "58d4e0") { // 天蓝色
        return "skyblue";  // 注意：小颜色和GIF使用小写
    } else if (lowerCode == "9848b8") { // 紫色
        return "Purple";
    } else if (lowerCode == "f8ace8") { // 粉色
        return "Pink";
    } else if (lowerCode == "f8b048") { // 橙色
        return "Orange";
    } else {
        // 对于未知的颜色，尝试基于RGB值选择最接近的颜色
        bool ok;
        unsigned int colorHex = colorCode.toUInt(&ok, 16);
        
        if (ok) {
            // 解析RGB分量
            int r = (colorHex >> 16) & 0xFF;
            int g = (colorHex >> 8) & 0xFF;
            int b = colorHex & 0xFF;
            
            // 根据RGB分量的相对强度选择最匹配的颜色
            if (r > 200 && r > g*1.5 && r > b*1.5) {
                return "Red";
            } else if (b > 150 && b > r*1.5 && b > g*1.5) {
                return "Blue";
            } else if (g > 150 && g > r*1.2 && g > b*1.5) {
                return "Green";
            } else if (r > 180 && g > 180 && b < 100) {
                return "Yellow";
            } else if (r > 200 && g > 100 && g < 180 && b < 100) {
                return "Orange";
            } else if (r > 200 && b > 150 && g < r*0.8) {
                return "Pink";
            } else if (r > 100 && b > 100 && g < r*0.7 && g < b*0.7) {
                return "Purple";
            } else if (b > 150 && g > 150 && r < b*0.8) {
                return "skyblue";
            } else {
                return colorCode; // 如果无法匹配任何颜色，返回原始颜色代码
            }
        } else {
            return colorCode; // 如果颜色代码无效，返回原始颜色代码
        }
    }
}

// 获取小颜色图片
QPixmap Ob1::getSmallColorImage(const QString& color) {
    // 使用统一的颜色映射方法
    QString colorName = getColorName(color);
    
    // 构建颜色图片路径并直接加载
    QString colorPath = QString("assets/1v1dicolors/%1.png").arg(colorName);
    QPixmap colorPixmap(colorPath);
    
    return colorPixmap;
}

// 获取右侧颜色图片
QPixmap Ob1::getRightColorImage(const QString& color) {
    // 使用统一的颜色映射方法，但右侧颜色需要首字母大写的Skyblue
    QString colorName = getColorName(color);
    if (colorName == "skyblue") {
        colorName = "Skyblue";  // 右侧颜色目录使用首字母大写
    }
    
    // 构建右侧颜色图片路径并直接加载
    QString colorPath = QString("assets/1v1rightcolors/%1.png").arg(colorName);
    QPixmap colorPixmap(colorPath);
    
    return colorPixmap;
}

// 设置玩家颜色
void Ob1::setPlayerColor(int playerIndex) {
    // 检查玩家索引是否有效
    if (playerIndex < 0 || playerIndex >= Ra2ob::MAXPLAYER) {
        return;
    }
    
    // 获取玩家颜色
    std::string colorStr = g._gameInfo.players[playerIndex].panel.color;
    
    // 如果颜色为空或为"0"，使用默认红色
    if (colorStr.empty() || colorStr == "0") {
        colorStr = "FF0000";
    }
    
    // 根据玩家索引设置颜色
    if (playerIndex == p1_index) {
        qs_1 = colorStr;
    } else if (playerIndex == p2_index) {
        qs_2 = colorStr;
    }
    
    // 更新颜色显示
    updateColorDisplay();
}

// 获取颜色GIF路径


// 更新小颜色显示
void Ob1::updateSmallColorDisplay() {
    // 获取缩放比例
    float ratio = Globalsetting::getInstance().l.ratio;
    
    // 检查游戏是否有效
    if (!g._gameInfo.valid) {
        lb_smallColor1->hide();
        lb_smallColor2->hide();
        // 同时调用新的颜色显示更新
        updateColorDisplay();
        return;
    }
    
    // 更新玩家1的小颜色图片
    if (!qs_1.empty()) {
        QPixmap colorPixmap = getSmallColorImage(QString::fromStdString(qs_1));
        if (!colorPixmap.isNull()) {
            // 计算缩放后的尺寸
            int width = qRound(65 * ratio);
            int height = qRound(25 * ratio);
            
            // 设置标签大小和位置
            lb_smallColor1->setPixmap(colorPixmap.scaled(width, height, Qt::KeepAspectRatio, Qt::SmoothTransformation));
            lb_smallColor1->setGeometry(qRound(smallColor1X * ratio), qRound(smallColor1Y * ratio), width, height);
            lb_smallColor1->show();
        } else {
            lb_smallColor1->hide();
        }
    } else {
        lb_smallColor1->hide();
    }
    
    // 更新玩家2的小颜色图片
    if (!qs_2.empty()) {
        QPixmap colorPixmap = getSmallColorImage(QString::fromStdString(qs_2));
        if (!colorPixmap.isNull()) {
            // 计算缩放后的尺寸
            int width = qRound(65 * ratio);
            int height = qRound(25 * ratio);
            
            // 设置标签大小和位置
            lb_smallColor2->setPixmap(colorPixmap.scaled(width, height, Qt::KeepAspectRatio, Qt::SmoothTransformation));
            lb_smallColor2->setGeometry(qRound(smallColor2X * ratio), qRound(smallColor2Y * ratio), width, height);
            lb_smallColor2->show();
        } else {
            lb_smallColor2->hide();
        }
    } else {
        lb_smallColor2->hide();
    }
    
    // 同时调用新的颜色显示更新
    updateColorDisplay();
    
    // 同时调用右侧颜色显示更新
    updateRightColorDisplay();
    
    // 同时调用国旗显示更新
    updateFlagDisplay();
    
    // 同时调用新国旗组显示更新
    updateNewFlagDisplay();
}

// 更新右侧颜色显示
void Ob1::updateRightColorDisplay() {
    // 右侧颜色图片现在通过paintEvent直接绘制，不再使用QLabel
    // 隐藏QLabel以避免重复显示
    if (lb_rightColor1) lb_rightColor1->hide();
    if (lb_rightColor2) lb_rightColor2->hide();
}

void Ob1::switchScreen() {
    // 暂时不实现
}

void Ob1::paintEvent(QPaintEvent *event) {
    Q_UNUSED(event);
    
    // 创建绘图对象
    QPainter painter(this);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    
    // 获取缩放比例
    float ratio = Globalsetting::getInstance().l.ratio;
    
    // 只有当游戏有效时才绘制图片
    if (g._gameInfo.valid) {
        // 绘制经济背景图（放在最底层）
        if (!ecobgImage.isNull()) {
            // 计算缩放后的尺寸
            int scaledWidth = qRound(350 * ratio);
            int scaledHeight = qRound(45 * ratio);
            
            // 缩放图片
            QPixmap scaledBg = ecobgImage.scaled(scaledWidth, scaledHeight, 
                                              Qt::IgnoreAspectRatio, 
                                              Qt::SmoothTransformation);
            
            // 绘制图片，使用固定的XY坐标位置
            int posX = 76 * ratio; // 左侧留出74*ratio的距离
            int posY = 1032 * ratio; // 顶部留出1032*ratio的距离
            painter.drawPixmap(posX, posY, scaledBg);
        }
        
        // 绘制经济条（在ecobg之后，bg1v1之前）
        paintEconomyBars(&painter);
        
        // 绘制第一张背景图（放在经济背景图上层）
        if (!bg1v1Image.isNull()) {
            // 计算缩放后的尺寸
            int scaledWidth = qRound(1920 * ratio);
            int scaledHeight = qRound(1080 * ratio);
            
            // 缩放图片
            QPixmap scaledBg = bg1v1Image.scaled(scaledWidth, scaledHeight, 
                                              Qt::IgnoreAspectRatio, 
                                              Qt::SmoothTransformation);
            
            // 绘制图片，从左上角(0,0)开始
            painter.drawPixmap(0, 0, scaledBg);
        }
        
        // 绘制第二张背景图（当鼠标不在左上角区域时显示）
        if (!bg1v12Image.isNull() && !mouseInTopLeftArea) {
            // 计算缩放后的尺寸
            int scaledWidth = qRound(1920 * ratio);
            int scaledHeight = qRound(1080 * ratio);
            
            // 缩放图片
            QPixmap scaledBg = bg1v12Image.scaled(scaledWidth, scaledHeight, 
                                               Qt::IgnoreAspectRatio, 
                                               Qt::SmoothTransformation);
            
            // 绘制图片，从左上角(0,0)开始
            painter.drawPixmap(0, 0, scaledBg);
        }
        
 
        

        
        // 绘制地图标签和地图名（使用与主播名称相同的样式）
        Globalsetting &gls = Globalsetting::getInstance();
        QString mapNameUtf = QString::fromStdString(g._gameInfo.mapNameUtf);
        if (!mapNameUtf.isEmpty()) {
            // 使用主播名称的字体设置地图名称
            QString streamerName = gls.sb.streamer_name;
            if (streamerName.isEmpty()) {
                streamerName = mapNameUtf; // 如果主播名称为空，使用地图名称
            }
            
            QFont mapFont(miSansHeavyFamily, qRound(12 * ratio), QFont::Normal);
            painter.setFont(mapFont);
            
            // 绘制地图标签，居中对齐
            QString mapLabel = "地图";
            QFontMetrics fm(mapFont);
            int labelWidth = fm.horizontalAdvance(mapLabel);
            int mapLabelPosX = qRound(mapLabelX * ratio) - labelWidth / 2;
            int mapLabelPosY = qRound(mapLabelY * ratio);
            
            // 绘制地图标签描边（使用与主播名称相同的颜色）
            QPen outlinePen(QColor("#60D1E0"));
            outlinePen.setWidthF(0.3 * ratio);
            painter.setPen(outlinePen);
            painter.drawText(mapLabelPosX, mapLabelPosY, mapLabel);
            
            // 绘制地图标签文字
            painter.setPen(Qt::white);
            painter.drawText(mapLabelPosX, mapLabelPosY, mapLabel);
            
            // 绘制地图名称内容，左对齐（显示地图名称）
            QString displayName = mapNameUtf;
            int mapContentX = qRound(mapNameX * ratio);
            int mapContentY = qRound(mapNameY * ratio);
            
            // 绘制地图名称描边
            painter.setPen(outlinePen);
            painter.drawText(mapContentX, mapContentY, displayName);
            
            // 绘制地图名称文字
            painter.setPen(Qt::white);
            painter.drawText(mapContentX, mapContentY, displayName);
        }
        
        // 绘制主播名字和赛事名
        
        // 绘制主播标签和内容
        if (!gls.sb.streamer_name.isEmpty()) {
            QFont streamerFont(miSansHeavyFamily, qRound(12 * ratio), QFont::Normal);
            painter.setFont(streamerFont);
            
            // 绘制主播标签，居中对齐
            QString streamerLabel = "主播";
            QFontMetrics fm(streamerFont);
            int labelWidth = fm.horizontalAdvance(streamerLabel);
            int streamerLabelPosX = qRound(streamerLabelX * ratio) - labelWidth / 2;
            int streamerLabelPosY = qRound(streamerLabelY * ratio);
            
            // 绘制描边
            QPen outlinePen(QColor("#60D1E0"));
            outlinePen.setWidthF(0.3 * ratio);
            painter.setPen(outlinePen);
            painter.drawText(streamerLabelPosX, streamerLabelPosY, streamerLabel);
            
            // 绘制文字
            painter.setPen(Qt::white);
            painter.drawText(streamerLabelPosX, streamerLabelPosY, streamerLabel);
            
            // 绘制主播名字内容，居中对齐
            QString streamerName = gls.sb.streamer_name;
            int nameWidth = fm.horizontalAdvance(streamerName);
            int streamerContentX = qRound(streamerNameX * ratio) - nameWidth / 2;
            int streamerContentY = qRound(streamerNameY * ratio);
            
            // 绘制描边
            painter.setPen(outlinePen);
            painter.drawText(streamerContentX, streamerContentY, streamerName);
            
            // 绘制文字
            painter.setPen(Qt::white);
            painter.drawText(streamerContentX, streamerContentY, streamerName);
        }
        
        // 绘制赛事标签和内容
        if (!gls.sb.event_name.isEmpty()) {
            QFont eventFont(miSansHeavyFamily, qRound(12 * ratio), QFont::Normal);
            painter.setFont(eventFont);
            
            // 绘制赛事标签，居中对齐
            QString eventLabel = "赛事";
            QFontMetrics fm(eventFont);
            int labelWidth = fm.horizontalAdvance(eventLabel);
            int eventLabelPosX = qRound(eventLabelX * ratio) - labelWidth / 2;
            int eventLabelPosY = qRound(eventLabelY * ratio);
            
            // 绘制描边
            QPen outlinePen(QColor("#60D1E0"));
            outlinePen.setWidthF(0.3 * ratio);
            painter.setPen(outlinePen);
            painter.drawText(eventLabelPosX, eventLabelPosY, eventLabel);
            
            // 绘制文字
            painter.setPen(Qt::white);
            painter.drawText(eventLabelPosX, eventLabelPosY, eventLabel);
            
            // 绘制赛事名字内容，居中对齐
            QString eventName = gls.sb.event_name;
            int nameWidth = fm.horizontalAdvance(eventName);
            int eventContentX = qRound(eventNameX * ratio) - nameWidth / 2;
            int eventContentY = qRound(eventNameY * ratio);
            
            // 绘制描边
            painter.setPen(outlinePen);
            painter.drawText(eventContentX, eventContentY, eventName);
            
            // 绘制文字
            painter.setPen(Qt::white);
            painter.drawText(eventContentX, eventContentY, eventName);
        }
        
        // 绘制BO数
        {
            QFont boFont(miSansHeavyFamily, qRound(14 * ratio), QFont::Bold);
            painter.setFont(boFont);
            
            QString boText = QString("BO %1").arg(boNumber);
            QFontMetrics fm(boFont);
            int textWidth = fm.horizontalAdvance(boText);
            int boPosX = qRound(boTextX * ratio) - textWidth / 2;
            int boPosY = qRound(boTextY * ratio);
            
            // 绘制白色文字
            painter.setPen(Qt::white);
            painter.drawText(boPosX, boPosY, boText);
        }
        
        // 绘制右侧颜色图片
        paintRightColors(&painter);
        
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
            
            // 计算PMB图片位置，应用自定义偏移量
            int pmbX = width() - scaledPmbWidth - qRound(16 * ratio) + pmbOffsetX; // 距离右边缘50像素加上自定义X偏移
            int pmbY = qRound(810 * ratio) + pmbOffsetY; // 向下偏移810像素（经过缩放）加上自定义Y偏移
            
            // 绘制PMB图片
            QPixmap scaledPmb = pmbImage.scaled(scaledPmbWidth, scaledPmbHeight, Qt::KeepAspectRatio, Qt::SmoothTransformation);
            painter.drawPixmap(pmbX, pmbY, scaledPmb);
        }
        
    }
    
    // 绘制单位数量信息栏（始终在最前方显示）
    if (g._gameInfo.valid) {
        paintUnits(&painter);
        
        // 绘制建造进度栏
        paintProducingBlocks(&painter);
        
        // 更新间谍渗透状态显示
        updateSpyInfiltrationDisplay();
    }
}

// 获取玩家颜色GIF（带缓存机制）
QMovie* Ob1::getPlayerColorGif(const QString& color) {
    // 使用统一的颜色映射方法
    QString colorName = getColorName(color);
    
    // 如果已经在缓存中，检查状态并返回
    if (colorCache.contains(colorName)) {
        QMovie* cachedMovie = colorCache[colorName];
        // 确保缓存的GIF处于运行状态（解决新游戏时GIF停止的问题）
        if (cachedMovie && cachedMovie->state() != QMovie::Running) {
            cachedMovie->start();
        }
        return cachedMovie;
    }
    
    // 构建颜色GIF路径并直接加载
    QString gifPath = QString("assets/1v1colors/%1.gif").arg(colorName);
    QMovie* colorMovie = new QMovie(gifPath);
    
    // 启动动画
    colorMovie->start();
    
    // 添加到缓存
    colorCache.insert(colorName, colorMovie);
    
    return colorMovie;
}

// 独立的颜色显示更新方法
void Ob1::updateColorDisplay() {
    if (!g._gameInfo.valid) {
        // 游戏无效时，隐藏标签但不卸载GIF资源
        lb_colorGif1->hide();
        lb_colorGif2->hide();
        // 重置加载状态，以便下次游戏重新加载
        color1Loaded = false;
        color2Loaded = false;
        return;
    }
    
    // 计算颜色标签位置，应用比例
    float ratio = Globalsetting::getInstance().l.ratio;
    
    // 玩家1颜色位置
    int color1XPos = qRound(colorGif1X * ratio);
    int color1YPos = qRound(colorGif1Y * ratio);
    int colorW = qRound(colorGifWidth * ratio);
    int colorH = qRound(colorGifHeight * ratio);
    lb_colorGif1->setGeometry(color1XPos, color1YPos, colorW, colorH);
    
    // 玩家2颜色位置
    int color2XPos = qRound(colorGif2X * ratio);
    int color2YPos = qRound(colorGif2Y * ratio);
    lb_colorGif2->setGeometry(color2XPos, color2YPos, colorW, colorH);
    
    // 更新玩家1颜色
    if (p1_index >= 0 && p1_index < Ra2ob::MAXPLAYER && g._gameInfo.players[p1_index].valid) {
        // 只有当颜色GIF未加载时才加载
        if (!color1Loaded) {
            std::string colorHex = g._gameInfo.players[p1_index].panel.color;
            QString colorStr = QString::fromStdString(colorHex);
            
            // 获取颜色GIF
            QMovie* colorGif = getPlayerColorGif(colorStr);
            
            // 如果有当前播放的动画，先停止
            if (lb_colorGif1->movie()) {
                lb_colorGif1->movie()->stop();
                // 注意：不要删除movie，因为它已经在缓存中
            }
            
            // 设置GIF到标签
            if (colorGif) {
                lb_colorGif1->setMovie(colorGif);
                
                // 确保GIF动画重新启动
                if (colorGif->state() != QMovie::Running) {
                    colorGif->start();
                }
                
                // 玩家1不翻转
                lb_colorGif1->setFlipped(false);
            }
            
            color1Loaded = true;
        }
        lb_colorGif1->show();
    } else {
        lb_colorGif1->hide();
    }
    
    // 更新玩家2颜色
    if (p2_index >= 0 && p2_index < Ra2ob::MAXPLAYER && g._gameInfo.players[p2_index].valid) {
        // 只有当颜色GIF未加载时才加载
        if (!color2Loaded) {
            std::string colorHex = g._gameInfo.players[p2_index].panel.color;
            QString colorStr = QString::fromStdString(colorHex);
            
            // 获取颜色GIF
            QMovie* colorGif = getPlayerColorGif(colorStr);
            
            // 如果有当前播放的动画，先停止
            if (lb_colorGif2->movie()) {
                lb_colorGif2->movie()->stop();
                // 注意：不要删除movie，因为它已经在缓存中
            }
            
            // 设置GIF到标签
            if (colorGif) {
                lb_colorGif2->setMovie(colorGif);
                
                // 确保GIF动画重新启动
                if (colorGif->state() != QMovie::Running) {
                    colorGif->start();
                }
                
                // 玩家2设置水平翻转
                lb_colorGif2->setFlipped(true);
            }
            
            color2Loaded = true;
        }
        lb_colorGif2->show();
    } else {
        lb_colorGif2->hide();
    }
}

// 获取单位图标
QPixmap Ob1::getUnitIcon(const QString& unitName) {
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
        // 尝试加载默认图标
        QString defaultIconPath = appDir + "/assets/obicons/default.png";
        unitIcon = QPixmap(defaultIconPath);
        
        if (unitIcon.isNull()) {
            qDebug() << "无法加载单位图标: " << iconPath << "，也无法加载默认图标";
            // 创建一个简单的默认图标
            QPixmap emptyPixmap(unitIconWidth, unitIconHeight);
            emptyPixmap.fill(QColor(128, 128, 128, 128)); // 半透明灰色
            
            // 缓存空图标以避免重复创建
            unitIconCache.insert(unitName, emptyPixmap);
            return emptyPixmap;
        } else {
            qDebug() << "使用默认图标替代: " << unitName;
        }
    }
    
    // 预处理图标：确保尺寸一致
    if (unitIcon.width() != unitIconWidth || unitIcon.height() != unitIconHeight) {
        unitIcon = unitIcon.scaled(unitIconWidth, unitIconHeight, 
                                 Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    }
    
    // 缓存并返回图标
    unitIconCache.insert(unitName, unitIcon);
    return unitIcon;
}

// 应用圆角效果（上方不圆角，下方圆角）
QPixmap Ob1::getRadiusPixmap(const QPixmap& src, int radius) {
    // 获取源图像的宽高
    int w = src.width();
    int h = src.height();
    
    // 创建一个透明的目标图像
    QPixmap dest(w, h);
    dest.fill(Qt::transparent);
    
    // 创建画家
    QPainter painter(&dest);
    painter.setRenderHint(QPainter::Antialiasing);
    
    // 创建上方不圆角，下方圆角的路径
    QPainterPath path;
    path.moveTo(0, 0);  // 左上角
    path.lineTo(w, 0);  // 右上角
    path.lineTo(w, h - radius);  // 右侧到圆角开始处
    path.arcTo(w - 2 * radius, h - 2 * radius, 2 * radius, 2 * radius, 0, -90);  // 右下圆角
    path.lineTo(radius, h);  // 底部到左下圆角
    path.arcTo(0, h - 2 * radius, 2 * radius, 2 * radius, 270, -90);  // 左下圆角
    path.lineTo(0, 0);  // 回到左上角
    
    // 设置裁剪路径
    painter.setClipPath(path);
    
    // 绘制源图像
    painter.drawPixmap(0, 0, w, h, src);
    
    // 结束绘制
    painter.end();
    
    return dest;
}

// 绘制单个玩家的单位
void Ob1::drawPlayerUnits(QPainter *painter, int playerIndex, int startX, int startY, float ratio) {
    // 获取玩家颜色
    QColor playerColor = QColor(QString::fromStdString("#" + g._gameInfo.players[playerIndex].panel.color));
    
    // 单位图标尺寸和位置
    int scaledUnitWidth = qRound(unitIconWidth * ratio);
    int scaledUnitHeight = qRound(unitIconHeight * ratio);
    int scaledUnitSpacing = qRound(unitSpacing * ratio);
    
    int unitX = startX;
    int unitY = startY;
    
    // 遍历玩家的单位，最多显示13个
    int displayedUnits = 0;
        
    // 根据玩家索引选择对应的排序后单位数据
    const std::vector<Ra2ob::tagUnitSingle>* unitsToDisplay = nullptr;
    if (playerIndex == p1_index && !sortedUnits1.empty()) {
        unitsToDisplay = &sortedUnits1;
    } else if (playerIndex == p2_index && !sortedUnits2.empty()) {
        unitsToDisplay = &sortedUnits2;
    }
    
    // 选择要使用的单位数据源
    const std::vector<Ra2ob::tagUnitSingle>* unitsSource;
    if (unitsToDisplay != nullptr) {
        unitsSource = unitsToDisplay;  // 使用排序后的数据
    } else {
        unitsSource = &g._gameInfo.players[playerIndex].units.units;  // 使用原始数据
    }
    
    // 遍历选定的单位数据
    for (const auto& unit : *unitsSource) {
        if (displayedUnits >= 11 || unit.num <= 0 || !unit.show) {
            continue; // 最多显示13个单位或跳过数量为0的单位
        }
        
        QString unitName = QString::fromStdString(unit.unitName);
        int unitCount = unit.num;
        
        // 获取单位图标
        QPixmap unitIcon = getUnitIcon(unitName);
        
        if (!unitIcon.isNull()) {
            // 创建缓存键用于缩放和圆角处理后的图标
            QString processedIconKey = QString("%1_%2_%3_%4").arg(unitName)
                                     .arg(scaledUnitWidth).arg(scaledUnitHeight).arg(qRound(8 * ratio));
            
            QPixmap radiusIcon;
            
            // 检查是否已有处理后的图标缓存
            if (unitIconCache.contains(processedIconKey)) {
                radiusIcon = unitIconCache[processedIconKey];
            } else {
                // 缩放单位图标
                QPixmap scaledIcon = unitIcon.scaled(scaledUnitWidth, scaledUnitHeight, 
                                                   Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
                
                // 应用圆角效果
                radiusIcon = getRadiusPixmap(scaledIcon, qRound(8 * ratio));
                
                // 缓存处理后的图标
                unitIconCache.insert(processedIconKey, radiusIcon);
            }
            
            // 绘制单位图标
            painter->drawPixmap(unitX, unitY, radiusIcon);
            
            // 绘制数量背景
            int bgWidth = scaledUnitWidth;
            int bgHeight = qRound(9 * ratio);
            int bgY = unitY + scaledUnitHeight - bgHeight;
            
            // 创建上方不圆角，下方圆角的数量背景路径
            QPainterPath path;
            int bgRadius = qRound(4 * ratio);
            path.moveTo(unitX, bgY);  // 左上角
            path.lineTo(unitX + bgWidth, bgY);  // 右上角
            path.lineTo(unitX + bgWidth, bgY + bgHeight - bgRadius);  // 右侧到圆角开始处
            path.arcTo(unitX + bgWidth - 2 * bgRadius, bgY + bgHeight - 2 * bgRadius, 2 * bgRadius, 2 * bgRadius, 0, -90);  // 右下圆角
            path.lineTo(unitX + bgRadius, bgY + bgHeight);  // 底部到左下圆角
            path.arcTo(unitX, bgY + bgHeight - 2 * bgRadius, 2 * bgRadius, 2 * bgRadius, 270, -90);  // 左下圆角
            path.lineTo(unitX, bgY);  // 回到左上角
            
            painter->save();
            painter->setRenderHint(QPainter::Antialiasing);
            painter->fillPath(path, playerColor);
            painter->restore();
            
            // 绘制单位数量
            painter->save();
            
            // 使用LLDEtechnoGlitch特效字体
            QFont technoFont(!technoGlitchFamily.isEmpty() ? 
                           technoGlitchFamily : "LLDEtechnoGlitch-Bold0", 
                           qRound(15 * ratio), QFont::Bold);
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
            
            // 如果已显示13个单位，停止绘制
            if (displayedUnits >= 11) {
                break;
            }
        }
    }
}

// 绘制玩家单位
void Ob1::paintUnits(QPainter *painter) {
    // 确保只在游戏有效时绘制单位
    if (!g._gameInfo.valid) {
        return; // 游戏无效，不绘制单位
    }

    // 获取全局设置中的比例
    float ratio = Globalsetting::getInstance().l.ratio;
    
    // 单位区域起始Y坐标
    int unitsStartY = qRound(240 * ratio);
    
    // 寻找两个主要玩家
    int player1 = -1;
    int player2 = -1;
    
    // 简单方法：选择前两个有效玩家
    for (int i = 0; i < Ra2ob::MAXPLAYER; i++) {
        if (g._gameInfo.players[i].valid) {
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
        int unitX = qRound(1772 * ratio);
        drawPlayerUnits(painter, player1, unitX, unitsStartY, ratio);
    }
    
    // 绘制第二个玩家的单位
    if (player2 != -1) {
        int unitX = qRound(1841 * ratio);
        drawPlayerUnits(painter, player2, unitX, unitsStartY, ratio);
    }
    
    // 标记单位图标已加载
    unitIconsLoaded = true;
}

// 清空单位显示
void Ob1::clearUnitDisplay() {
    // 清空单位缓存
    unitIconCache.clear();
    unitIconsLoaded = false;
    
    qDebug() << "清空单位显示，清理缓存和状态";
}

// 绘制右侧颜色图片和国旗
void Ob1::paintRightColors(QPainter *painter) {
    if (!g._gameInfo.valid) {
        return;
    }
    
    float ratio = Globalsetting::getInstance().l.ratio;
    
    // 先绘制右侧国旗（在后方）
    paintRightFlags(painter);
    
    // 计算右侧颜色图片的位置和尺寸
    int rightColorW = qRound(rightColorWidth * ratio);
    int rightColorH = qRound(rightColorHeight * ratio);
    
    // 绘制玩家1右侧颜色
    if (p1_index >= 0 && p1_index < Ra2ob::MAXPLAYER && g._gameInfo.players[p1_index].valid) {
        std::string colorHex = g._gameInfo.players[p1_index].panel.color;
        QString colorStr = QString::fromStdString(colorHex);
        
        QPixmap rightColorPixmap = getRightColorImage(colorStr);
        if (!rightColorPixmap.isNull()) {
            int rightColor1XPos = qRound(rightColor1X * ratio);
            int rightColor1YPos = qRound(rightColor1Y * ratio);
            
            // 缩放图片并绘制
            QPixmap scaledPixmap = rightColorPixmap.scaled(rightColorW, rightColorH, Qt::KeepAspectRatio, Qt::SmoothTransformation);
            painter->drawPixmap(rightColor1XPos, rightColor1YPos, scaledPixmap);
        }
    }
    
    // 绘制玩家2右侧颜色
    if (p2_index >= 0 && p2_index < Ra2ob::MAXPLAYER && g._gameInfo.players[p2_index].valid) {
        std::string colorHex = g._gameInfo.players[p2_index].panel.color;
        QString colorStr = QString::fromStdString(colorHex);
        
        QPixmap rightColorPixmap = getRightColorImage(colorStr);
        if (!rightColorPixmap.isNull()) {
            int rightColor2XPos = qRound(rightColor2X * ratio);
            int rightColor2YPos = qRound(rightColor2Y * ratio);
            
            // 缩放图片并绘制
            QPixmap scaledPixmap = rightColorPixmap.scaled(rightColorW, rightColorH, Qt::KeepAspectRatio, Qt::SmoothTransformation);
            painter->drawPixmap(rightColor2XPos, rightColor2YPos, scaledPixmap);
        }
    }
}

// 实现单位信息更新定时器的槽函数
void Ob1::onUnitUpdateTimerTimeout() {
    // 只在游戏有效且窗口可见时更新单位信息
    if (g._gameInfo.valid && this->isVisible()) {
        // 调用单位刷新函数进行筛选和排序
        refreshUnits();
        
        // 触发界面重绘以更新单位显示
        update();
    }
}

// 实现强制内存回收方法
void Ob1::forceMemoryCleanup() {
    qDebug() << "Ob1：执行定期内存回收";
    
    // 强制系统回收内存
    #ifdef Q_OS_WIN
        SetProcessWorkingSetSize(GetCurrentProcess(), (SIZE_T)-1, (SIZE_T)-1);
    #endif
    
    // 清理图像缓存，但仅在游戏无效时清理所有缓存
    if (!g._gameInfo.valid) {
        // 游戏无效，清理所有缓存
        flagCache.clear();
        newFlagCache.clear();
        unitIconCache.clear();
        colorCache.clear();
        QPixmapCache::clear();
        
        // 重置加载状态
        unitIconsLoaded = false;
        color1Loaded = false;
        color2Loaded = false;
        
        qDebug() << "Ob1：游戏无效，清理所有缓存";
    } else {
        // 游戏有效，只清理部分缓存以避免影响性能
        QPixmapCache::clear();
        qDebug() << "Ob1：游戏有效，只清理PixmapCache";
    }
}



// 初始化玩家名称标签
void Ob1::initPlayerNameLabels() {
    float ratio = Globalsetting::getInstance().l.ratio;
    
    // 创建玩家1名称标签
    lb_playerName1 = new QLabel(this);
    lb_playerName1->setObjectName("lb_playerName1");
    lb_playerName1->setStyleSheet("QLabel { color: white; background: transparent; }");
    
    // 设置MISANS-HEAVY字体，应用ratio缩放
    QFont font1;
    font1.setFamily(miSansHeavyFamily);
    font1.setPointSize(qRound(16 * ratio));
    font1.setWeight(QFont::Normal);
    lb_playerName1->setFont(font1);
    lb_playerName1->setAlignment(Qt::AlignCenter);
    lb_playerName1->hide();
    
    // 创建玩家2名称标签
    lb_playerName2 = new QLabel(this);
    lb_playerName2->setObjectName("lb_playerName2");
    lb_playerName2->setStyleSheet("QLabel { color: white; background: transparent; }");
    
    // 设置MISANS-HEAVY字体，应用ratio缩放
    QFont font2;
    font2.setFamily(miSansHeavyFamily);
    font2.setPointSize(qRound(16 * ratio));
    font2.setWeight(QFont::Normal);
    lb_playerName2->setFont(font2);
    lb_playerName2->setAlignment(Qt::AlignCenter);
    lb_playerName2->hide();
}

// 更新玩家名称显示
void Ob1::updatePlayerNameDisplay() {
    if (!g._gameInfo.valid) {
        // 游戏无效时隐藏标签
        if (lb_playerName1) lb_playerName1->hide();
        if (lb_playerName2) lb_playerName2->hide();
        return;
    }
    
    Globalsetting& gls = Globalsetting::getInstance();
    
    // 更新玩家1名称
    if (p1_index != -1 && p1_index < Ra2ob::MAXPLAYER && g._gameInfo.players[p1_index].valid) {
        QString nickname = QString::fromUtf8(g._gameInfo.players[p1_index].panel.playerNameUtf);
        
        if (nickname != currentNickname1) {
            currentNickname1 = nickname;
            
            // 查找映射的玩家名称
            QString playerName = gls.findNameByNickname(nickname);
            if (playerName.isEmpty()) {
                playerName = nickname;  // 如果没有映射，使用原始昵称
            }
            
            // 保存到全局设置
            gls.sb.p1_nickname = nickname;
            gls.sb.p1_nickname_cache = nickname;
            gls.sb.p1_playername_cache = playerName;  // 同步更新玩家名称缓存
            
            // 发射玩家名称需要更新的信号
            emit playernameNeedsUpdate();
            
            // 限制名称长度
            if (playerName.length() > 12) {
                playerName = playerName.left(12) + "...";
            }
            
            lb_playerName1->setText(playerName);
            lb_playerName1->adjustSize();
            
            // 应用ratio缩放到位置
            float ratio = Globalsetting::getInstance().l.ratio;
            int scaledX = qRound(playerName1X * ratio) - lb_playerName1->width() / 2;
            int scaledY = qRound(playerName1Y * ratio);
            lb_playerName1->move(scaledX, scaledY);
            lb_playerName1->show();
        }
    } else {
        lb_playerName1->hide();
        currentNickname1.clear();
    }
    
    // 更新玩家2名称
    if (p2_index != -1 && p2_index < Ra2ob::MAXPLAYER && g._gameInfo.players[p2_index].valid) {
        QString nickname = QString::fromUtf8(g._gameInfo.players[p2_index].panel.playerNameUtf);
        
        if (nickname != currentNickname2) {
            currentNickname2 = nickname;
            
            // 查找映射的玩家名称
            QString playerName = gls.findNameByNickname(nickname);
            if (playerName.isEmpty()) {
                playerName = nickname;  // 如果没有映射，使用原始昵称
            }
            
            // 保存到全局设置
            gls.sb.p2_nickname = nickname;
            gls.sb.p2_nickname_cache = nickname;
            gls.sb.p2_playername_cache = playerName;  // 同步更新玩家名称缓存
            
            // 发射玩家名称需要更新的信号
            emit playernameNeedsUpdate();
            
            // 限制名称长度
            if (playerName.length() > 12) {
                playerName = playerName.left(12) + "...";
            }
            
            lb_playerName2->setText(playerName);
            lb_playerName2->adjustSize();
            
            // 应用ratio缩放到位置
            float ratio = Globalsetting::getInstance().l.ratio;
            int scaledX = qRound(playerName2X * ratio) - lb_playerName2->width() / 2;
            int scaledY = qRound(playerName2Y * ratio);
            lb_playerName2->move(scaledX, scaledY);
            lb_playerName2->show();
        }
    } else {
        lb_playerName2->hide();
        currentNickname2.clear();
     }
 }

// 强制刷新玩家名称显示（用于外部更新玩家映射后通知界面刷新）
void Ob1::forceRefreshPlayerNames() {
    if (!g._gameInfo.valid) {
        return;
    }
    
    Globalsetting& gls = Globalsetting::getInstance();
    
    // 强制重新查找玩家1的映射名称
    if (p1_index != -1 && p1_index < Ra2ob::MAXPLAYER && g._gameInfo.players[p1_index].valid) {
        QString nickname = QString::fromUtf8(g._gameInfo.players[p1_index].panel.playerNameUtf);
        
        if (!nickname.isEmpty()) {
            // 重新查找映射的玩家名称
            QString playerName = gls.findNameByNickname(nickname);
            if (playerName.isEmpty()) {
                playerName = nickname;  // 如果没有映射，使用原始昵称
            }
            
            // 更新全局设置
            gls.sb.p1_nickname = nickname;
            gls.sb.p1_nickname_cache = nickname;
            gls.sb.p1_playername_cache = playerName;  // 同步更新玩家名称缓存
            
            
            // 限制名称长度
            if (playerName.length() > 12) {
                playerName = playerName.left(12) + "...";
            }
            
            // 更新显示
            lb_playerName1->setText(playerName);
            lb_playerName1->adjustSize();
            
            // 应用ratio缩放到位置
            float ratio = Globalsetting::getInstance().l.ratio;
            int scaledX = qRound(playerName1X * ratio) - lb_playerName1->width() / 2;
            int scaledY = qRound(playerName1Y * ratio);
            lb_playerName1->move(scaledX, scaledY);
            lb_playerName1->show();
        }
    }
    
    // 强制重新查找玩家2的映射名称
    if (p2_index != -1 && p2_index < Ra2ob::MAXPLAYER && g._gameInfo.players[p2_index].valid) {
        QString nickname = QString::fromUtf8(g._gameInfo.players[p2_index].panel.playerNameUtf);
        
        if (!nickname.isEmpty()) {
            // 重新查找映射的玩家名称
            QString playerName = gls.findNameByNickname(nickname);
            if (playerName.isEmpty()) {
                playerName = nickname;  // 如果没有映射，使用原始昵称
            }
            
            // 更新全局设置
            gls.sb.p2_nickname = nickname;
            gls.sb.p2_nickname_cache = nickname;
            gls.sb.p2_playername_cache = playerName;  // 同步更新玩家名称缓存
         
            
            // 限制名称长度
            if (playerName.length() > 12) {
                playerName = playerName.left(12) + "...";
            }
            
            // 更新显示
            lb_playerName2->setText(playerName);
            lb_playerName2->adjustSize();
            
            // 应用ratio缩放到位置
            float ratio = Globalsetting::getInstance().l.ratio;
            int scaledX = qRound(playerName2X * ratio) - lb_playerName2->width() / 2;
            int scaledY = qRound(playerName2Y * ratio);
            lb_playerName2->move(scaledX, scaledY);
            lb_playerName2->show();
        }
    }
    
    // 发射玩家名称需要更新的信号
    emit playernameNeedsUpdate();
}

// 初始化余额和总消耗标签
void Ob1::initEconomyLabels() {
    float ratio = Globalsetting::getInstance().l.ratio;
    
    // 初始化玩家1余额标签
    lb_balance1 = new QLabel(this);
    lb_balance1->setStyleSheet("QLabel { color: white; }");
    
    // 设置MISANS-HEAVY字体和右对齐，应用ratio缩放
    QFont balanceFont1;
    balanceFont1.setFamily(miSansHeavyFamily);
    balanceFont1.setPointSize(qRound(12 * ratio));
    balanceFont1.setWeight(QFont::Medium);
    lb_balance1->setFont(balanceFont1);
    lb_balance1->setAlignment(Qt::AlignCenter | Qt::AlignVCenter);
    lb_balance1->setFixedSize(qRound(economyLabelWidth * ratio), qRound(economyLabelHeight * ratio));
    lb_balance1->hide();
    
    // 初始化玩家2余额标签
    lb_balance2 = new QLabel(this);
    lb_balance2->setStyleSheet("QLabel { color: white; }");
    
    // 设置MISANS-HEAVY字体和左对齐，应用ratio缩放
    QFont balanceFont2;
    balanceFont2.setFamily(miSansHeavyFamily);
    balanceFont2.setPointSize(qRound(12 * ratio));
    balanceFont2.setWeight(QFont::Medium);
    lb_balance2->setFont(balanceFont2);
    lb_balance2->setAlignment(Qt::AlignCenter | Qt::AlignVCenter);
    lb_balance2->setFixedSize(qRound(economyLabelWidth * ratio), qRound(economyLabelHeight * ratio));
    lb_balance2->hide();
    
    // 初始化玩家1总消耗标签
    lb_spent1 = new QLabel(this);
    lb_spent1->setStyleSheet("QLabel { color: white; }");
    
    // 设置LLDEtechnoGlitch-Bold0字体和居中对齐，应用ratio缩放
    QFont spentFont1;
    spentFont1.setFamily(technoGlitchFamily);
    spentFont1.setPointSize(qRound(14 * ratio));
    spentFont1.setWeight(QFont::Medium);
    lb_spent1->setFont(spentFont1);
    lb_spent1->setAlignment(Qt::AlignCenter);
    lb_spent1->setFixedSize(qRound(economyLabelWidth * ratio), qRound(economyLabelHeight * ratio));
    lb_spent1->hide();
    
    // 初始化玩家2总消耗标签
    lb_spent2 = new QLabel(this);
    lb_spent2->setStyleSheet("QLabel { color: white; }");
    
    // 设置LLDEtechnoGlitch-Bold0字体和居中对齐，应用ratio缩放
    QFont spentFont2;
    spentFont2.setFamily(technoGlitchFamily);
    spentFont2.setPointSize(qRound(14 * ratio));
    spentFont2.setWeight(QFont::Medium);
    lb_spent2->setFont(spentFont2);
    lb_spent2->setAlignment(Qt::AlignCenter);
    lb_spent2->setFixedSize(qRound(economyLabelWidth * ratio), qRound(economyLabelHeight * ratio));
    lb_spent2->hide();
    
    // 设置初始位置
    lb_balance1->move(qRound(balance1X * ratio), qRound(balance1Y * ratio));
    lb_balance2->move(qRound(balance2X * ratio), qRound(balance2Y * ratio));
    lb_spent1->move(qRound(spent1X * ratio), qRound(spent1Y * ratio));
    lb_spent2->move(qRound(spent2X * ratio), qRound(spent2Y * ratio));
    
    // 确保标签在最上层
    lb_balance1->raise();
    lb_balance2->raise();
    lb_spent1->raise();
    lb_spent2->raise();
}

// 更新余额和总消耗显示
void Ob1::updateEconomyDisplay() {
    if (!g._gameInfo.valid) {
        // 游戏无效时隐藏所有经济标签
        if (lb_balance1) lb_balance1->hide();
        if (lb_balance2) lb_balance2->hide();
        if (lb_spent1) lb_spent1->hide();
        if (lb_spent2) lb_spent2->hide();
        return;
    }
    
    // 更新玩家1余额和总消耗
    if (p1_index >= 0 && p1_index < Ra2ob::MAXPLAYER && g._gameInfo.players[p1_index].valid) {
        int balance = g._gameInfo.players[p1_index].panel.balance;
        int spent = g._gameInfo.players[p1_index].panel.creditSpent;
        
        // 设置余额文本（格式：$X）
        lb_balance1->setText(QString("%1").arg(balance));
        lb_balance1->show();
        
        // 设置总消耗文本（格式：X.Yk）
        int spentK = spent / 1000;
        int spentY = (spent % 1000) / 100;
        lb_spent1->setText(QString("%1.%2k").arg(spentK).arg(spentY));
        lb_spent1->show();
    } else {
        if (lb_balance1) lb_balance1->hide();
        if (lb_spent1) lb_spent1->hide();
    }
    
    // 更新玩家2余额和总消耗
    if (p2_index >= 0 && p2_index < Ra2ob::MAXPLAYER && g._gameInfo.players[p2_index].valid) {
        int balance = g._gameInfo.players[p2_index].panel.balance;
        int spent = g._gameInfo.players[p2_index].panel.creditSpent;
        
        // 设置余额文本（格式：$X）
        lb_balance2->setText(QString("%1").arg(balance));
        lb_balance2->show();
        
        // 设置总消耗文本（格式：X.Yk）
        int spentK = spent / 1000;
        int spentY = (spent % 1000) / 100;
        lb_spent2->setText(QString("%1.%2k").arg(spentK).arg(spentY));
        lb_spent2->show();
    } else {
        if (lb_balance2) lb_balance2->hide();
        if (lb_spent2) lb_spent2->hide();
    }
}

// 设置余额标签位置
void Ob1::setBalancePosition() {
    float ratio = Globalsetting::getInstance().l.ratio;
    
    if (lb_balance1) {
        int scaledX = qRound(balance1X * ratio);
        int scaledY = qRound(balance1Y * ratio);
        lb_balance1->move(scaledX, scaledY);
    }
    
    if (lb_balance2) {
        int scaledX = qRound(balance2X * ratio);
        int scaledY = qRound(balance2Y * ratio);
        lb_balance2->move(scaledX, scaledY);
    }
}

// 设置总消耗标签位置
void Ob1::setSpentPosition() {
    float ratio = Globalsetting::getInstance().l.ratio;
    
    if (lb_spent1) {
        int scaledX = qRound(spent1X * ratio);
        int scaledY = qRound(spent1Y * ratio);
        lb_spent1->move(scaledX, scaledY);
    }
    
    if (lb_spent2) {
        int scaledX = qRound(spent2X * ratio);
        int scaledY = qRound(spent2Y * ratio);
        lb_spent2->move(scaledX, scaledY);
    }
}

// 初始化击杀数和幸存数标签
void Ob1::initStatsLabels() {
    float ratio = Globalsetting::getInstance().l.ratio;
    
    // 创建击杀数标签
    lb_kills1 = new QLabel(this);
    lb_kills2 = new QLabel(this);
    
    // 创建幸存数标签
    lb_alive1 = new QLabel(this);
    lb_alive2 = new QLabel(this);
    
    // 设置MISANS-HEAVY字体，应用ratio缩放
    QFont statsFont;
    statsFont.setFamily(miSansHeavyFamily);
    statsFont.setPointSize(qRound(12 * ratio));
    statsFont.setWeight(QFont::Medium);
    QString whiteStyle = "color: white; background: transparent;";
    
    // 设置击杀数标签属性，应用ratio缩放到尺寸
    lb_kills1->setFont(statsFont);
    lb_kills1->setStyleSheet(whiteStyle);
    lb_kills1->setAlignment(Qt::AlignCenter);
    lb_kills1->setFixedSize(qRound(statsLabelWidth * ratio), qRound(statsLabelHeight * ratio));
    
    lb_kills2->setFont(statsFont);
    lb_kills2->setStyleSheet(whiteStyle);
    lb_kills2->setAlignment(Qt::AlignCenter);
    lb_kills2->setFixedSize(qRound(statsLabelWidth * ratio), qRound(statsLabelHeight * ratio));
    
    // 设置幸存数标签属性，应用ratio缩放到尺寸
    lb_alive1->setFont(statsFont);
    lb_alive1->setStyleSheet(whiteStyle);
    lb_alive1->setAlignment(Qt::AlignCenter);
    lb_alive1->setFixedSize(qRound(statsLabelWidth * ratio), qRound(statsLabelHeight * ratio));
    
    lb_alive2->setFont(statsFont);
    lb_alive2->setStyleSheet(whiteStyle);
    lb_alive2->setAlignment(Qt::AlignCenter);
    lb_alive2->setFixedSize(qRound(statsLabelWidth * ratio), qRound(statsLabelHeight * ratio));
    
    // 设置初始位置
    lb_kills1->move(qRound(kills1X * ratio), qRound(kills1Y * ratio));
    lb_kills2->move(qRound(kills2X * ratio), qRound(kills2Y * ratio));
    lb_alive1->move(qRound(alive1X * ratio), qRound(alive1Y * ratio));
    lb_alive2->move(qRound(alive2X * ratio), qRound(alive2Y * ratio));
    
    // 确保标签在最上层
    lb_kills1->raise();
    lb_kills2->raise();
    lb_alive1->raise();
    lb_alive2->raise();
    
    // 默认隐藏
    lb_kills1->hide();
    lb_kills2->hide();
    lb_alive1->hide();
    lb_alive2->hide();
}

// 更新击杀数和幸存数显示
void Ob1::updateStatsDisplay() {
    if (!g._gameInfo.valid) {
        // 游戏无效时隐藏所有标签
        if (lb_kills1) lb_kills1->hide();
        if (lb_kills2) lb_kills2->hide();
        if (lb_alive1) lb_alive1->hide();
        if (lb_alive2) lb_alive2->hide();
        return;
    }
    
    float ratio = Globalsetting::getInstance().l.ratio;
    
    // 更新玩家1的击杀数和幸存数
    if (p1_index >= 0 && p1_index < Ra2ob::MAXPLAYER && g._gameInfo.players[p1_index].valid) {
        // 获取击杀数
        int kills = g._gameInfo.players[p1_index].score.kills;
        lb_kills1->setText(QString::number(kills));
        
        // 获取幸存数
        int alive = g._gameInfo.players[p1_index].score.alive;
        lb_alive1->setText(QString::number(alive));
        
        // 更新位置
        lb_kills1->move(qRound(kills1X * ratio), qRound(kills1Y * ratio));
        lb_alive1->move(qRound(alive1X * ratio), qRound(alive1Y * ratio));
        
        // 显示标签
        lb_kills1->show();
        lb_alive1->show();
    } else {
        lb_kills1->hide();
        lb_alive1->hide();
    }
    
    // 更新玩家2的击杀数和幸存数
    if (p2_index >= 0 && p2_index < Ra2ob::MAXPLAYER && g._gameInfo.players[p2_index].valid) {
        // 获取击杀数
        int kills = g._gameInfo.players[p2_index].score.kills;
        lb_kills2->setText(QString::number(kills));
        
        // 获取幸存数
        int alive = g._gameInfo.players[p2_index].score.alive;
        lb_alive2->setText(QString::number(alive));
        
        // 更新位置
        lb_kills2->move(qRound(kills2X * ratio), qRound(kills2Y * ratio));
        lb_alive2->move(qRound(alive2X * ratio), qRound(alive2Y * ratio));
        
        // 显示标签
        lb_kills2->show();
        lb_alive2->show();
    } else {
        lb_kills2->hide();
        lb_alive2->hide();
    }
}

// 设置击杀数位置
void Ob1::setKillsPosition() {
    float ratio = Globalsetting::getInstance().l.ratio;
    
    if (lb_kills1) {
        lb_kills1->move(qRound(kills1X * ratio), qRound(kills1Y * ratio));
    }
    
    if (lb_kills2) {
        lb_kills2->move(qRound(kills2X * ratio), qRound(kills2Y * ratio));
    }
}

// 设置幸存数位置
void Ob1::setAlivePosition() {
    float ratio = Globalsetting::getInstance().l.ratio;
    
    if (lb_alive1) {
        lb_alive1->move(qRound(alive1X * ratio), qRound(alive1Y * ratio));
    }
    
    if (lb_alive2) {
        lb_alive2->move(qRound(alive2X * ratio), qRound(alive2Y * ratio));
    }
}

// 初始化电量条标签
void Ob1::initPowerBarLabels() {
    float ratio = Globalsetting::getInstance().l.ratio;
    
    // 创建玩家1电量条标签
    lb_powerBar1 = new QLabel(this);
    lb_powerBar1->setObjectName("lb_powerBar1");
    lb_powerBar1->setFixedSize(qRound(powerBarWidth * ratio), qRound(powerBarHeight * ratio));
    lb_powerBar1->setStyleSheet("background: transparent;");
    lb_powerBar1->hide();
    
    // 创建玩家2电量条标签
    lb_powerBar2 = new QLabel(this);
    lb_powerBar2->setObjectName("lb_powerBar2");
    lb_powerBar2->setFixedSize(qRound(powerBarWidth * ratio), qRound(powerBarHeight * ratio));
    lb_powerBar2->setStyleSheet("background: transparent;");
    lb_powerBar2->hide();
    
    // 初始化电量条闪烁定时器
    powerBarBlinkTimer = new QTimer(this);
    connect(powerBarBlinkTimer, &QTimer::timeout, this, &Ob1::onPowerBarBlink);
    powerBarBlinkTimer->setInterval(400); // 400ms间隔
    
    // 设置电量条初始位置
    setPowerBarPosition();
}

// 创建电量条图像
QPixmap Ob1::createPowerBarPixmap(int powerOutput, int powerDrain, float ratio) {
    // 计算缩放后的尺寸
    int scaledWidth = qRound(powerBarWidth * ratio);
    int scaledHeight = qRound(powerBarHeight * ratio);
    
    // 创建透明背景的QPixmap
    QPixmap pixmap(scaledWidth, scaledHeight);
    pixmap.fill(Qt::transparent);
    
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    
    // 计算电力百分比
    float powerPercentage = 0.0f;
    if (powerOutput > 0) {
        powerPercentage = qMax(0.0f, qMin(1.0f, (float)(powerOutput - powerDrain) / powerOutput));
    }
    
    // 确定电量条颜色
    QColor batteryColor;
    bool shouldBlink = false;
    
    if (powerDrain > powerOutput) {
        // 电力不足 - 红色，需要闪烁
        batteryColor = QColor(255, 0, 0);
        shouldBlink = true;
    } else if (powerPercentage < 0.3f) {
        // 电力紧张 - 黄色
        batteryColor = QColor(255, 255, 0);
    } else {
        // 电力充足 - 绿色
        batteryColor = QColor(0, 255, 0);
    }
    
    // 电池外壳颜色（闪烁时在灰色和红色之间切换）
    QColor shellColor = QColor(128, 128, 128); // 默认灰色
    if (shouldBlink && powerBarBlinkState) {
        shellColor = QColor(255, 0, 0); // 闪烁时的红色
    }
    
    // 绘制电池外壳
    int shellThickness = qMax(1, qRound(2 * ratio));
    painter.setPen(QPen(shellColor, shellThickness));
    painter.setBrush(Qt::NoBrush);
    
    // 电池主体矩形
    int batteryWidth = scaledWidth - qRound(6 * ratio);
    int batteryHeight = scaledHeight - qRound(4 * ratio);
    int batteryX = qRound(2 * ratio);
    int batteryY = qRound(2 * ratio);
    
    QRect batteryRect(batteryX, batteryY, batteryWidth, batteryHeight);
    painter.drawRect(batteryRect);
    
    // 绘制电池正极头部
    int headWidth = qRound(3 * ratio);
    int headHeight = qRound(8 * ratio);
    int headX = batteryX + batteryWidth;
    int headY = batteryY + (batteryHeight - headHeight) / 2;
    
    QRect headRect(headX, headY, headWidth, headHeight);
    painter.fillRect(headRect, shellColor);
    
    // 绘制内部电量条
    if (powerPercentage > 0) {
        int fillWidth = qRound((batteryWidth - 4 * ratio) * powerPercentage);
        int fillHeight = batteryHeight - qRound(4 * ratio);
        int fillX = batteryX + qRound(2 * ratio);
        int fillY = batteryY + qRound(2 * ratio);
        
        if (fillWidth > 0 && fillHeight > 0) {
            QRect fillRect(fillX, fillY, fillWidth, fillHeight);
            painter.fillRect(fillRect, batteryColor);
        }
    }
    
    return pixmap;
}

// 更新电量条显示
void Ob1::updatePowerBars() {
    if (!g._gameInfo.valid) {
        // 游戏无效时隐藏电量条
        if (lb_powerBar1) lb_powerBar1->hide();
        if (lb_powerBar2) lb_powerBar2->hide();
        powerBarBlinkTimer->stop();
        return;
    }
    
    float ratio = Globalsetting::getInstance().l.ratio;
    bool needBlink = false;
    
    // 更新玩家1电量条
    if (p1_index >= 0 && p1_index < Ra2ob::MAXPLAYER && g._gameInfo.players[p1_index].valid) {
        int powerOutput = g._gameInfo.players[p1_index].panel.powerOutput;
        int powerDrain = g._gameInfo.players[p1_index].panel.powerDrain;
        
        QPixmap powerBarPixmap = createPowerBarPixmap(powerOutput, powerDrain, ratio);
        lb_powerBar1->setPixmap(powerBarPixmap);
        lb_powerBar1->show();
        
        // 调整位置
        int scaledX = qRound(powerBar1X * ratio);
        int scaledY = qRound(powerBar1Y * ratio);
        lb_powerBar1->move(scaledX, scaledY);
        
        // 检查是否需要闪烁
        if (powerDrain > powerOutput) {
            needBlink = true;
        }
    } else {
        lb_powerBar1->hide();
    }
    
    // 更新玩家2电量条
    if (p2_index >= 0 && p2_index < Ra2ob::MAXPLAYER && g._gameInfo.players[p2_index].valid) {
        int powerOutput = g._gameInfo.players[p2_index].panel.powerOutput;
        int powerDrain = g._gameInfo.players[p2_index].panel.powerDrain;
        
        QPixmap powerBarPixmap = createPowerBarPixmap(powerOutput, powerDrain, ratio);
        lb_powerBar2->setPixmap(powerBarPixmap);
        lb_powerBar2->show();
        
        // 调整位置
        int scaledX = qRound(powerBar2X * ratio);
        int scaledY = qRound(powerBar2Y * ratio);
        lb_powerBar2->move(scaledX, scaledY);
        
        // 检查是否需要闪烁
        if (powerDrain > powerOutput) {
            needBlink = true;
        }
    } else {
        lb_powerBar2->hide();
    }
    
    // 控制闪烁定时器
    if (needBlink && !powerBarBlinkTimer->isActive()) {
        powerBarBlinkTimer->start();
    } else if (!needBlink && powerBarBlinkTimer->isActive()) {
        powerBarBlinkTimer->stop();
        powerBarBlinkState = false;
    }
}

// 设置电量条位置
void Ob1::setPowerBarPosition() {
    float ratio = Globalsetting::getInstance().l.ratio;
    
    if (lb_powerBar1) {
        int scaledX = qRound(powerBar1X * ratio);
        int scaledY = qRound(powerBar1Y * ratio);
        lb_powerBar1->move(scaledX, scaledY);
    }
    
    if (lb_powerBar2) {
        int scaledX = qRound(powerBar2X * ratio);
        int scaledY = qRound(powerBar2Y * ratio);
        lb_powerBar2->move(scaledX, scaledY);
    }
}

// 电量条闪烁槽函数
void Ob1::onPowerBarBlink() {
    powerBarBlinkState = !powerBarBlinkState;
    updatePowerBars(); // 重新绘制电量条以应用闪烁效果
}
 
 // 绘制右侧国旗
 void Ob1::paintRightFlags(QPainter *painter) {
     if (!g._gameInfo.valid) {
         return;
     }
     
     float ratio = Globalsetting::getInstance().l.ratio;
     
     // 新国旗组尺寸：66x47px
     int newFlagW = qRound(66 * ratio);
     int newFlagH = qRound(47 * ratio);
     
     // 绘制玩家1右侧国旗
     if (p1_index >= 0 && p1_index < Ra2ob::MAXPLAYER && g._gameInfo.players[p1_index].valid) {
         std::string countryStr = g._gameInfo.players[p1_index].panel.country;
         QString country = QString::fromStdString(countryStr);
         
         // 获取新国旗组图片
         QPixmap newFlagPixmap = getNewCountryFlag(country);
         
         if (!newFlagPixmap.isNull()) {
             // 玩家1新国旗位置：1766, 179
             int newFlag1XPos = qRound(1767 * ratio);
             int newFlag1YPos = qRound(179 * ratio);
             
             // 缩放图片并绘制
             QPixmap scaledPixmap = newFlagPixmap.scaled(newFlagW, newFlagH, Qt::KeepAspectRatio, Qt::SmoothTransformation);
             painter->drawPixmap(newFlag1XPos, newFlag1YPos, scaledPixmap);
         }
     }
     
     // 绘制玩家2右侧国旗
     if (p2_index >= 0 && p2_index < Ra2ob::MAXPLAYER && g._gameInfo.players[p2_index].valid) {
         std::string countryStr = g._gameInfo.players[p2_index].panel.country;
         QString country = QString::fromStdString(countryStr);
         
         // 获取新国旗组图片
         QPixmap newFlagPixmap = getNewCountryFlag(country);
         
         if (!newFlagPixmap.isNull()) {
             // 玩家2新国旗位置：1836, 179
             int newFlag2XPos = qRound(1836 * ratio);
             int newFlag2YPos = qRound(179 * ratio);
             
             // 缩放图片并绘制
             QPixmap scaledPixmap = newFlagPixmap.scaled(newFlagW, newFlagH, Qt::KeepAspectRatio, Qt::SmoothTransformation);
             painter->drawPixmap(newFlag2XPos, newFlag2YPos, scaledPixmap);
         }
     }
 }

// 设置玩家名称位置
void Ob1::setPlayernamePosition() {
    float ratio = Globalsetting::getInstance().l.ratio;
    
    if (lb_playerName1) {
        lb_playerName1->move(qRound(playerName1X * ratio) - lb_playerName1->width() / 2, qRound(playerName1Y * ratio));
    }
    
    if (lb_playerName2) {
        lb_playerName2->move(qRound(playerName2X * ratio) - lb_playerName2->width() / 2, qRound(playerName2Y * ratio));
    }
}

// 初始化经济条
void Ob1::initEconomyBars() {
    // 创建经济条画布
    economyBarPixmap1 = QPixmap(economyBarWidth, economyBarHeight);
    economyBarPixmap2 = QPixmap(economyBarWidth, economyBarHeight);
    
    // 设置透明背景
    economyBarPixmap1.fill(Qt::transparent);
    economyBarPixmap2.fill(Qt::transparent);
    
    // 创建并启动经济条更新定时器
    economyBarTimer = new QTimer(this);
    connect(economyBarTimer, &QTimer::timeout, this, &Ob1::onEconomyBarUpdate);
    economyBarTimer->start(600); // 每300ms更新一次
}

// 绘制经济块
void Ob1::drawEconomyBlock(QPixmap& pixmap, int playerIndex) {
    if (!g._gameInfo.valid || playerIndex < 0 || playerIndex >= Ra2ob::MAXPLAYER) {
        return;
    }
    
    if (!g._gameInfo.players[playerIndex].valid) {
        return;
    }
    
    QPainter painter(&pixmap);
    
    // 将现有内容向左移动2像素
    QPixmap temp = pixmap.copy(economyBarBlockWidth, 0, 
                              economyBarWidth - economyBarBlockWidth, economyBarHeight);
    pixmap.fill(Qt::transparent);
    painter.drawPixmap(0, 0, temp);
    
    // 获取玩家当前余额
    int balance = g._gameInfo.players[playerIndex].panel.balance;
    
    // 根据余额决定颜色
    QColor blockColor;
    if (balance < 30) {
        blockColor = QColor(233, 69, 79); // 红色 #e9454f
    } else {
        blockColor = QColor(83, 171, 59); // 绿色 #53ab3b
    }
    
    // 在右侧绘制新的颜色块
    int blockX = economyBarWidth - economyBarBlockWidth;
    painter.fillRect(blockX, 0, economyBarBlockWidth, economyBarHeight, blockColor);
}

// 绘制经济条
void Ob1::paintEconomyBars(QPainter* painter) {
    if (!g._gameInfo.valid) {
        return;
    }
    
    float ratio = Globalsetting::getInstance().l.ratio;
    
    // 绘制玩家1经济条
    if (p1_index >= 0 && p1_index < Ra2ob::MAXPLAYER && g._gameInfo.players[p1_index].valid) {
        int scaledX = qRound(economyBar1X * ratio);
        int scaledY = qRound(economyBar1Y * ratio);
        int scaledW = qRound(economyBarWidth * ratio);
        int scaledH = qRound(economyBarHeight * ratio);
        
        QPixmap scaledPixmap = economyBarPixmap1.scaled(scaledW, scaledH, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        painter->drawPixmap(scaledX, scaledY, scaledPixmap);
    }
    
    // 绘制玩家2经济条
    if (p2_index >= 0 && p2_index < Ra2ob::MAXPLAYER && g._gameInfo.players[p2_index].valid) {
        int scaledX = qRound(economyBar2X * ratio);
        int scaledY = qRound(economyBar2Y * ratio);
        int scaledW = qRound(economyBarWidth * ratio);
        int scaledH = qRound(economyBarHeight * ratio);
        
        QPixmap scaledPixmap = economyBarPixmap2.scaled(scaledW, scaledH, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        painter->drawPixmap(scaledX, scaledY, scaledPixmap);
    }
}

// 设置玩家1经济条位置
void Ob1::setEconomyBar1Position(int x, int y) {
    economyBar1X = x;
    economyBar1Y = y;
}

// 设置玩家2经济条位置
void Ob1::setEconomyBar2Position(int x, int y) {
    economyBar2X = x;
    economyBar2Y = y;
}

// 设置经济条背景位置
void Ob1::setEconomyBarBgPosition(int x, int y) {
    economyBarBgX = x;
    economyBarBgY = y;
}

// 经济条更新槽函数
void Ob1::onEconomyBarUpdate() {
    if (!g._gameInfo.valid) {
        return;
    }
    
    // 为玩家1绘制新的经济块
    if (p1_index >= 0 && p1_index < Ra2ob::MAXPLAYER && g._gameInfo.players[p1_index].valid) {
        drawEconomyBlock(economyBarPixmap1, p1_index);
    }
    
    // 为玩家2绘制新的经济块
    if (p2_index >= 0 && p2_index < Ra2ob::MAXPLAYER && g._gameInfo.players[p2_index].valid) {
        drawEconomyBlock(economyBarPixmap2, p2_index);
    }
    
    // 触发界面重绘
    update();
}

// ====== 游戏时间相关方法实现 ======

// 游戏定时器超时处理
void Ob1::onGameTimerTimeout() {
    // 检测游戏状态
    detectGame();
    
    // 更新游戏时间显示
    updateGameTimeDisplay();
    
    // 控制窗口显示/隐藏
    toggleOb();
}

// 初始化游戏时间标签
void Ob1::initGameTimeLabel() {
    // 创建游戏时间标签
    lb_gameTime = new QLabel(this);
    lb_gameTime->setStyleSheet("color: #ffffff;");
    
    float ratio = Globalsetting::getInstance().l.ratio;
    QFont gameTimeFont(technoGlitchFamily);
    gameTimeFont.setPointSizeF(20 * ratio);
    gameTimeFont.setWeight(QFont::Normal); // Regular字重
    gameTimeFont.setStyleStrategy(QFont::PreferAntialias);
    
    lb_gameTime->setFont(gameTimeFont);
    lb_gameTime->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    lb_gameTime->setText("0:00");
    lb_gameTime->move(qRound(gameTimeX * ratio), qRound(gameTimeY * ratio));
    lb_gameTime->resize(qRound(100 * ratio), qRound(30 * ratio));
    lb_gameTime->hide();
}

// 更新游戏时间显示
void Ob1::updateGameTimeDisplay() {
    if (!lb_gameTime || !g._gameInfo.valid) {
        if (lb_gameTime) {
            lb_gameTime->hide();
        }
        return;
    }
    
    // 从内存中读取真实游戏时间（秒）
    int gameTimeSeconds = 0;
    if (g._gameInfo.realGameTime > 0) {
        gameTimeSeconds = g._gameInfo.realGameTime;
    } else {
        gameTimeSeconds = g._gameInfo.currentFrame / Ra2ob::GAMESPEED;
    }
    
    // 格式化为分:秒
    int minutes = gameTimeSeconds / 60;
    int seconds = gameTimeSeconds % 60;
    QString timeText = QString("%1:%2").arg(minutes, 2, 10, QChar('0')).arg(seconds, 2, 10, QChar('0'));
    
    lb_gameTime->setText(timeText);
    lb_gameTime->show();
}

// 初始化间谍渗透状态标签
void Ob1::initSpyInfiltrationLabels() {
    qDebug() << "[SpyInfiltration] 初始化间谍渗透标签";
    
    float ratio = Globalsetting::getInstance().l.ratio;
    qDebug() << "[SpyInfiltration] 缩放比例:" << ratio;
    
    // 创建玩家1的三个间谍渗透状态标签
    lb_spyInfiltration1_tech = new QLabel(this);
    lb_spyInfiltration1_tech->setStyleSheet("background: transparent;");
    lb_spyInfiltration1_tech->setAlignment(Qt::AlignCenter);
    lb_spyInfiltration1_tech->hide();
    
    lb_spyInfiltration1_barracks = new QLabel(this);
    lb_spyInfiltration1_barracks->setStyleSheet("background: transparent;");
    lb_spyInfiltration1_barracks->setAlignment(Qt::AlignCenter);
    lb_spyInfiltration1_barracks->hide();
    
    lb_spyInfiltration1_warfactory = new QLabel(this);
    lb_spyInfiltration1_warfactory->setStyleSheet("background: transparent;");
    lb_spyInfiltration1_warfactory->setAlignment(Qt::AlignCenter);
    lb_spyInfiltration1_warfactory->hide();
    
    // 创建玩家2的三个间谍渗透状态标签
    lb_spyInfiltration2_tech = new QLabel(this);
    lb_spyInfiltration2_tech->setStyleSheet("background: transparent;");
    lb_spyInfiltration2_tech->setAlignment(Qt::AlignCenter);
    lb_spyInfiltration2_tech->hide();
    
    lb_spyInfiltration2_barracks = new QLabel(this);
    lb_spyInfiltration2_barracks->setStyleSheet("background: transparent;");
    lb_spyInfiltration2_barracks->setAlignment(Qt::AlignCenter);
    lb_spyInfiltration2_barracks->hide();
    
    lb_spyInfiltration2_warfactory = new QLabel(this);
    lb_spyInfiltration2_warfactory->setStyleSheet("background: transparent;");
    lb_spyInfiltration2_warfactory->setAlignment(Qt::AlignCenter);
    lb_spyInfiltration2_warfactory->hide();
    
    // 设置间谍渗透标签大小（位置由updateSpyInfiltrationDisplay动态计算）
    int labelWidth = qRound(spyInfiltrationWidth * ratio);
    int labelHeight = qRound(spyInfiltrationHeight * ratio);
    
    // 设置所有标签的大小
    lb_spyInfiltration1_tech->resize(labelWidth, labelHeight);
    lb_spyInfiltration1_barracks->resize(labelWidth, labelHeight);
    lb_spyInfiltration1_warfactory->resize(labelWidth, labelHeight);
    
    lb_spyInfiltration2_tech->resize(labelWidth, labelHeight);
    lb_spyInfiltration2_barracks->resize(labelWidth, labelHeight);
    lb_spyInfiltration2_warfactory->resize(labelWidth, labelHeight);
    
    // 确保标签在最顶层
    lb_spyInfiltration1_tech->raise();
    lb_spyInfiltration1_barracks->raise();
    lb_spyInfiltration1_warfactory->raise();
    lb_spyInfiltration2_tech->raise();
    lb_spyInfiltration2_barracks->raise();
    lb_spyInfiltration2_warfactory->raise();
    
    // 初始化间谍渗透提示标签
    lb_spyAlert1 = new QLabel(this);
    lb_spyAlert1->setStyleSheet("background: transparent;");
    lb_spyAlert1->setAlignment(Qt::AlignCenter);
    lb_spyAlert1->resize(qRound(spyAlertWidth * ratio), qRound(spyAlertHeight * ratio));
    lb_spyAlert1->move(qRound(spyAlert1X * ratio), qRound(spyAlert1Y * ratio));
    lb_spyAlert1->hide();
    lb_spyAlert1->raise();
    
    lb_spyAlert2 = new QLabel(this);
    lb_spyAlert2->setStyleSheet("background: transparent;");
    lb_spyAlert2->setAlignment(Qt::AlignCenter);
    lb_spyAlert2->resize(qRound(spyAlertWidth * ratio), qRound(spyAlertHeight * ratio));
    lb_spyAlert2->move(qRound(spyAlert2X * ratio), qRound(spyAlert2Y * ratio));
    lb_spyAlert2->hide();
    lb_spyAlert2->raise();
    
    // 初始化间谍提示定时器
    spyAlertTimer1 = new QTimer(this);
    spyAlertTimer1->setSingleShot(true);
    connect(spyAlertTimer1, &QTimer::timeout, [this]() {
        lb_spyAlert1->hide();
        qDebug() << "[SpyAlert] 玩家1间谍提示已隐藏";
    });
    
    spyAlertTimer2 = new QTimer(this);
    spyAlertTimer2->setSingleShot(true);
    connect(spyAlertTimer2, &QTimer::timeout, [this]() {
        lb_spyAlert2->hide();
        qDebug() << "[SpyAlert] 玩家2间谍提示已隐藏";
    });
    
    qDebug() << "[SpyInfiltration] 玩家1高科标签位置:" << lb_spyInfiltration1_tech->pos() << "大小:" << lb_spyInfiltration1_tech->size();
    qDebug() << "[SpyInfiltration] 玩家1兵营标签位置:" << lb_spyInfiltration1_barracks->pos() << "大小:" << lb_spyInfiltration1_barracks->size();
    qDebug() << "[SpyInfiltration] 玩家1重工标签位置:" << lb_spyInfiltration1_warfactory->pos() << "大小:" << lb_spyInfiltration1_warfactory->size();
    qDebug() << "[SpyInfiltration] 玩家2高科标签位置:" << lb_spyInfiltration2_tech->pos() << "大小:" << lb_spyInfiltration2_tech->size();
    qDebug() << "[SpyInfiltration] 玩家2兵营标签位置:" << lb_spyInfiltration2_barracks->pos() << "大小:" << lb_spyInfiltration2_barracks->size();
    qDebug() << "[SpyInfiltration] 玩家2重工标签位置:" << lb_spyInfiltration2_warfactory->pos() << "大小:" << lb_spyInfiltration2_warfactory->size();
    qDebug() << "[SpyAlert] 玩家1间谍提示标签位置:" << lb_spyAlert1->pos() << "大小:" << lb_spyAlert1->size();
    qDebug() << "[SpyAlert] 玩家2间谍提示标签位置:" << lb_spyAlert2->pos() << "大小:" << lb_spyAlert2->size();
    
    qDebug() << "[SpyInfiltration] 主窗口大小:" << this->size();
}

// 获取间谍渗透状态图片
QPixmap Ob1::getSpyInfiltrationImage(const QString& imageType) {
    qDebug() << "[SpyInfiltration] 请求加载图片:" << imageType;
    
    if (spyInfiltrationCache.contains(imageType)) {
        qDebug() << "[SpyInfiltration] 从缓存中获取图片:" << imageType;
        return spyInfiltrationCache[imageType];
    }
    
    QString appDir = QCoreApplication::applicationDirPath();
    QString imagePath = QString("%1/assets/panels/%2").arg(appDir, imageType);
    qDebug() << "[SpyInfiltration] 图片路径:" << imagePath;
    
    // 检查文件是否存在
    QFile file(imagePath);
    qDebug() << "[SpyInfiltration] 文件存在:" << file.exists();
    
    QPixmap pixmap(imagePath);
    
    if (!pixmap.isNull()) {
        qDebug() << "[SpyInfiltration] 图片加载成功，原始大小:" << pixmap.size();
        float ratio = Globalsetting::getInstance().l.ratio;
        qDebug() << "[SpyInfiltration] 缩放比例:" << ratio << "目标宽度:" << spyInfiltrationWidth << "目标高度:" << spyInfiltrationHeight;
        
        int targetWidth = qRound(spyInfiltrationWidth * ratio);
        int targetHeight = qRound(spyInfiltrationHeight * ratio);
        qDebug() << "[SpyInfiltration] 计算的目标大小:" << targetWidth << "x" << targetHeight;
        
        if (targetWidth > 0 && targetHeight > 0) {
            QPixmap scaledPixmap = pixmap.scaled(
                targetWidth, 
                targetHeight, 
                Qt::KeepAspectRatio, 
                Qt::SmoothTransformation
            );
            qDebug() << "[SpyInfiltration] 缩放后大小:" << scaledPixmap.size();
            spyInfiltrationCache[imageType] = scaledPixmap;
            return scaledPixmap;
        } else {
            qDebug() << "[SpyInfiltration] 目标大小无效，返回原始图片";
            spyInfiltrationCache[imageType] = pixmap;
            return pixmap;
        }
    } else {
        qDebug() << "[SpyInfiltration] 图片加载失败:" << imagePath;
    }
    
    return QPixmap();
}

// 更新间谍渗透状态显示
void Ob1::updateSpyInfiltrationDisplay() {
    qDebug() << "[SpyInfiltration] 开始更新间谍渗透显示";
    qDebug() << "[SpyInfiltration] 游戏信息有效性:" << g._gameInfo.valid;
    qDebug() << "[SpyInfiltration] 标签指针检查 - 高科1:" << (lb_spyInfiltration1_tech != nullptr)
             << "兵营1:" << (lb_spyInfiltration1_barracks != nullptr)
             << "重工1:" << (lb_spyInfiltration1_warfactory != nullptr);
    
    // 检查配置开关和会员权限
    if (!Globalsetting::getInstance().s.show_spy_infiltration || 
        !MemberManager::getInstance().hasFeatureAccess(MemberManager::FeatureType::SpyInfiltration)) {
        qDebug() << "[SpyInfiltration] 配置关闭或非会员用户，隐藏所有标签";
        // 隐藏所有玩家1的标签
        if (lb_spyInfiltration1_tech) lb_spyInfiltration1_tech->hide();
        if (lb_spyInfiltration1_barracks) lb_spyInfiltration1_barracks->hide();
        if (lb_spyInfiltration1_warfactory) lb_spyInfiltration1_warfactory->hide();
        // 隐藏所有玩家2的标签
        if (lb_spyInfiltration2_tech) lb_spyInfiltration2_tech->hide();
        if (lb_spyInfiltration2_barracks) lb_spyInfiltration2_barracks->hide();
        if (lb_spyInfiltration2_warfactory) lb_spyInfiltration2_warfactory->hide();
        return;
    }
    
    if (!g._gameInfo.valid || !lb_spyInfiltration1_tech || !lb_spyInfiltration2_tech) {
        qDebug() << "[SpyInfiltration] 条件不满足，隐藏所有标签";
        // 隐藏所有玩家1的标签
        if (lb_spyInfiltration1_tech) lb_spyInfiltration1_tech->hide();
        if (lb_spyInfiltration1_barracks) lb_spyInfiltration1_barracks->hide();
        if (lb_spyInfiltration1_warfactory) lb_spyInfiltration1_warfactory->hide();
        // 隐藏所有玩家2的标签
        if (lb_spyInfiltration2_tech) lb_spyInfiltration2_tech->hide();
        if (lb_spyInfiltration2_barracks) lb_spyInfiltration2_barracks->hide();
        if (lb_spyInfiltration2_warfactory) lb_spyInfiltration2_warfactory->hide();
        return;
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
    
    // 动态检测所有有效玩家的间谍渗透状态
    qint64 currentTime = QDateTime::currentMSecsSinceEpoch();
    
    int validPlayerCount = 0; // 记录有效玩家的相对顺序
    
    for (int i = 0; i < Ra2ob::MAXPLAYER; ++i) {
        if (!g._gameInfo.players[i].valid) {
            continue;
        }
        
        uint32_t playerBaseAddr = g._playerBases[i];
        if (playerBaseAddr == 0) {
            continue;
        }
        
        qDebug() << "[SpyInfiltration] 检测玩家" << i << "基地址: 0x" << QString::number(playerBaseAddr, 16).toUpper();
        
        // 读取玩家的间谍渗透状态
        bool techInfiltrated = g.r.getBool(playerBaseAddr + Ra2ob::SIDE0_TECH_INFILTRATED_OFFSET) ||
                              g.r.getBool(playerBaseAddr + Ra2ob::SIDE1_TECH_INFILTRATED_OFFSET) ||
                              g.r.getBool(playerBaseAddr + Ra2ob::SIDE2_TECH_INFILTRATED_OFFSET);
        bool barracksInfiltrated = g.r.getBool(playerBaseAddr + Ra2ob::BARRACKS_INFILTRATED_OFFSET);
        bool warFactoryInfiltrated = g.r.getBool(playerBaseAddr + Ra2ob::WARFACTORY_INFILTRATED_OFFSET);
        
        qDebug() << "[SpyInfiltration] 玩家" << i << "渗透检测 - Side0:" << g.r.getBool(playerBaseAddr + Ra2ob::SIDE0_TECH_INFILTRATED_OFFSET)
                 << "Side1:" << g.r.getBool(playerBaseAddr + Ra2ob::SIDE1_TECH_INFILTRATED_OFFSET)
                 << "Side2:" << g.r.getBool(playerBaseAddr + Ra2ob::SIDE2_TECH_INFILTRATED_OFFSET)
                 << "兵营:" << barracksInfiltrated << "重工:" << warFactoryInfiltrated;
        
        // 只处理前两个有效玩家的UI显示（由于UI限制）
        if (validPlayerCount == 0) {
            // 记录第一个有效玩家的索引
            validPlayerIndex1 = i;
            
            // 检查高科渗透
            QString techImage;
            if (g.r.getBool(playerBaseAddr + Ra2ob::SIDE0_TECH_INFILTRATED_OFFSET)) {
                techImage = "Lab 1.png";  // 盟军高科
            } else if (g.r.getBool(playerBaseAddr + Ra2ob::SIDE1_TECH_INFILTRATED_OFFSET)) {
                techImage = "Lab 2.png";  // 苏军高科
            } else if (g.r.getBool(playerBaseAddr + Ra2ob::SIDE2_TECH_INFILTRATED_OFFSET)) {
                techImage = "Lab 3.png";  // 尤里高科
            }
            
            // 记录高科渗透时间和类型
            if (!techImage.isEmpty() && !player1InfiltrationTimes.contains("tech")) {
                player1InfiltrationTimes["tech"] = currentTime;
                player1InfiltrationOrder.append("tech");
                player1TechInfiltrationImage = techImage;
                qDebug() << "[间谍渗透] 玩家" << i << "高科渗透检测到，类型:" << techImage << "时间:" << currentTime;
                showSpyAlert(validPlayerCount, "spy3");
            }
            
            // 记录兵营渗透时间
            if (barracksInfiltrated && !player1InfiltrationTimes.contains("barracks")) {
                player1InfiltrationTimes["barracks"] = currentTime;
                player1InfiltrationOrder.append("barracks");
                qDebug() << "[间谍渗透] 玩家" << i << "兵营渗透检测到，时间:" << currentTime;
                showSpyAlert(validPlayerCount, "spy2");
            }
            
            // 记录重工渗透时间
            if (warFactoryInfiltrated && !player1InfiltrationTimes.contains("warfactory")) {
                player1InfiltrationTimes["warfactory"] = currentTime;
                player1InfiltrationOrder.append("warfactory");
                qDebug() << "[间谍渗透] 玩家" << i << "重工渗透检测到，时间:" << currentTime;
                showSpyAlert(validPlayerCount, "spy1");
            }
        } else if (validPlayerCount == 1) {
            // 记录第二个有效玩家的索引
            validPlayerIndex2 = i;
            
            // 检查高科渗透
            QString techImage;
            if (g.r.getBool(playerBaseAddr + Ra2ob::SIDE0_TECH_INFILTRATED_OFFSET)) {
                techImage = "Lab 1.png";  // 盟军高科
            } else if (g.r.getBool(playerBaseAddr + Ra2ob::SIDE1_TECH_INFILTRATED_OFFSET)) {
                techImage = "Lab 2.png";  // 苏军高科
            } else if (g.r.getBool(playerBaseAddr + Ra2ob::SIDE2_TECH_INFILTRATED_OFFSET)) {
                techImage = "Lab 3.png";  // 尤里高科
            }
            
            // 记录高科渗透时间和类型
            if (!techImage.isEmpty() && !player2InfiltrationTimes.contains("tech")) {
                player2InfiltrationTimes["tech"] = currentTime;
                player2InfiltrationOrder.append("tech");
                player2TechInfiltrationImage = techImage;
                qDebug() << "[间谍渗透] 玩家" << i << "高科渗透检测到，类型:" << techImage << "时间:" << currentTime;
                showSpyAlert(validPlayerCount, "spy3");
            }
            
            // 记录兵营渗透时间
            if (barracksInfiltrated && !player2InfiltrationTimes.contains("barracks")) {
                player2InfiltrationTimes["barracks"] = currentTime;
                player2InfiltrationOrder.append("barracks");
                qDebug() << "[间谍渗透] 玩家" << i << "兵营渗透检测到，时间:" << currentTime;
                showSpyAlert(validPlayerCount, "spy2");
            }
            
            // 记录重工渗透时间
            if (warFactoryInfiltrated && !player2InfiltrationTimes.contains("warfactory")) {
                player2InfiltrationTimes["warfactory"] = currentTime;
                player2InfiltrationOrder.append("warfactory");
                qDebug() << "[间谍渗透] 玩家" << i << "重工渗透检测到，时间:" << currentTime;
                showSpyAlert(validPlayerCount, "spy1");
            }
        }
        // 对于其他玩家（第3个及以上有效玩家），只记录日志但不显示UI（由于UI限制）
        else {
            if (techInfiltrated || barracksInfiltrated || warFactoryInfiltrated) {
                qDebug() << "[间谍渗透] 玩家" << i << "检测到渗透但无UI显示 - 高科:" << techInfiltrated 
                         << "兵营:" << barracksInfiltrated << "重工:" << warFactoryInfiltrated;
            }
        }
        
        // 增加有效玩家计数
        validPlayerCount++;
    }
    
    // 按检测顺序更新玩家显示
    updatePlayer1SpyInfiltrationDisplay();
    updatePlayer2SpyInfiltrationDisplay();
}

// 更新玩家1间谍渗透显示（按检测时间顺序）
void Ob1::updatePlayer1SpyInfiltrationDisplay() {
    float ratio = Globalsetting::getInstance().l.ratio;
    int iconWidth = qRound(spyInfiltrationWidth * ratio);
    int baseX = qRound(spyInfiltration1X * ratio);
    int baseY = qRound(spyInfiltration1Y * ratio);
    
    // 隐藏所有图标
    lb_spyInfiltration1_tech->hide();
    lb_spyInfiltration1_barracks->hide();
    lb_spyInfiltration1_warfactory->hide();
    
    // 检查是否有有效的第一个玩家
    if (validPlayerIndex1 == -1) {
        qDebug() << "[间谍渗透] 第一个有效玩家索引无效，跳过显示";
        return;
    }
    
    // 获取动态玩家数据
    QMap<QString, qint64> playerInfiltrationTimes;
    QStringList playerInfiltrationOrder;
    QString playerTechInfiltrationImage;
    
    // 根据实际玩家索引获取间谍渗透状态
    if (!g._gameInfo.valid || validPlayerIndex1 >= Ra2ob::MAXPLAYER || !g._gameInfo.players[validPlayerIndex1].valid) {
        qDebug() << "[间谍渗透] 玩家" << validPlayerIndex1 << "数据无效，跳过显示";
        return;
    }
    
    uintptr_t playerBaseAddr = g._playerBases[validPlayerIndex1];
    if (playerBaseAddr == 0) {
        qDebug() << "[间谍渗透] 玩家" << validPlayerIndex1 << "基址无效，跳过显示";
        return;
    }
    
    // 检查当前玩家的间谍渗透状态并构建显示数据
    bool techInfiltrated = g.r.getBool(playerBaseAddr + Ra2ob::SIDE0_TECH_INFILTRATED_OFFSET) ||
                          g.r.getBool(playerBaseAddr + Ra2ob::SIDE1_TECH_INFILTRATED_OFFSET) ||
                          g.r.getBool(playerBaseAddr + Ra2ob::SIDE2_TECH_INFILTRATED_OFFSET);
    bool barracksInfiltrated = g.r.getBool(playerBaseAddr + Ra2ob::BARRACKS_INFILTRATED_OFFSET);
    bool warFactoryInfiltrated = g.r.getBool(playerBaseAddr + Ra2ob::WARFACTORY_INFILTRATED_OFFSET);
    
    // 构建渗透顺序（使用player1的时间数据作为参考）
    if (techInfiltrated && player1InfiltrationTimes.contains("tech")) {
        playerInfiltrationOrder.append("tech");
        playerTechInfiltrationImage = player1TechInfiltrationImage;
    }
    if (barracksInfiltrated && player1InfiltrationTimes.contains("barracks")) {
        playerInfiltrationOrder.append("barracks");
    }
    if (warFactoryInfiltrated && player1InfiltrationTimes.contains("warfactory")) {
        playerInfiltrationOrder.append("warfactory");
    }
    
    // 按检测顺序显示图标（从右到左）
    int displayIndex = 0;
    for (const QString& type : playerInfiltrationOrder) {
        QLabel* label = nullptr;
        QPixmap pixmap;
        if (type == "tech") {
            label = lb_spyInfiltration1_tech;
            pixmap = getSpyInfiltrationImage(playerTechInfiltrationImage.isEmpty() ? "Lab 1.png" : playerTechInfiltrationImage);
        } else if (type == "barracks") {
            label = lb_spyInfiltration1_barracks;
            pixmap = getSpyInfiltrationImage("Upgrade 1.png");
        } else if (type == "warfactory") {
            label = lb_spyInfiltration1_warfactory;
            pixmap = getSpyInfiltrationImage("Upgrade 2.png");
        }
        
        if (label && !pixmap.isNull()) {
            // 左边玩家从右到左显示，最先检测到的在最右边
            int xPos = baseX - iconWidth * displayIndex;
            label->move(xPos, baseY);
            label->setPixmap(pixmap);
            label->show();
            displayIndex++;
            qDebug() << "[间谍渗透] 玩家" << validPlayerIndex1 << type << "图标显示在位置:" << xPos << "," << baseY;
        }
    }
}

// 更新玩家2间谍渗透显示（按检测时间顺序）
void Ob1::updatePlayer2SpyInfiltrationDisplay() {
    float ratio = Globalsetting::getInstance().l.ratio;
    int iconWidth = qRound(spyInfiltrationWidth * ratio);
    int baseX = qRound(spyInfiltration2X * ratio);
    int baseY = qRound(spyInfiltration2Y * ratio);
    
    // 隐藏所有图标
    lb_spyInfiltration2_tech->hide();
    lb_spyInfiltration2_barracks->hide();
    lb_spyInfiltration2_warfactory->hide();
    
    // 检查是否有有效的第二个玩家
    if (validPlayerIndex2 == -1) {
        qDebug() << "[间谍渗透] 第二个有效玩家索引无效，跳过显示";
        return;
    }
    
    // 获取动态玩家数据
    QMap<QString, qint64> playerInfiltrationTimes;
    QStringList playerInfiltrationOrder;
    QString playerTechInfiltrationImage;
    
    // 根据实际玩家索引获取间谍渗透状态
    if (!g._gameInfo.valid || validPlayerIndex2 >= Ra2ob::MAXPLAYER || !g._gameInfo.players[validPlayerIndex2].valid) {
        qDebug() << "[间谍渗透] 玩家" << validPlayerIndex2 << "数据无效，跳过显示";
        return;
    }
    
    uintptr_t playerBaseAddr = g._playerBases[validPlayerIndex2];
    if (playerBaseAddr == 0) {
        qDebug() << "[间谍渗透] 玩家" << validPlayerIndex2 << "基址无效，跳过显示";
        return;
    }
    
    // 检查当前玩家的间谍渗透状态并构建显示数据
    bool techInfiltrated = g.r.getBool(playerBaseAddr + Ra2ob::SIDE0_TECH_INFILTRATED_OFFSET) ||
                          g.r.getBool(playerBaseAddr + Ra2ob::SIDE1_TECH_INFILTRATED_OFFSET) ||
                          g.r.getBool(playerBaseAddr + Ra2ob::SIDE2_TECH_INFILTRATED_OFFSET);
    bool barracksInfiltrated = g.r.getBool(playerBaseAddr + Ra2ob::BARRACKS_INFILTRATED_OFFSET);
    bool warFactoryInfiltrated = g.r.getBool(playerBaseAddr + Ra2ob::WARFACTORY_INFILTRATED_OFFSET);
    
    // 构建渗透顺序（使用player2的时间数据作为参考）
    if (techInfiltrated && player2InfiltrationTimes.contains("tech")) {
        playerInfiltrationOrder.append("tech");
        playerTechInfiltrationImage = player2TechInfiltrationImage;
    }
    if (barracksInfiltrated && player2InfiltrationTimes.contains("barracks")) {
        playerInfiltrationOrder.append("barracks");
    }
    if (warFactoryInfiltrated && player2InfiltrationTimes.contains("warfactory")) {
        playerInfiltrationOrder.append("warfactory");
    }
    
    // 按检测顺序显示图标（从左到右）
    int displayIndex = 0;
    for (const QString& type : playerInfiltrationOrder) {
        QLabel* label = nullptr;
        QPixmap pixmap;
        
        if (type == "tech") {
            label = lb_spyInfiltration2_tech;
            pixmap = getSpyInfiltrationImage(playerTechInfiltrationImage.isEmpty() ? "Lab 1.png" : playerTechInfiltrationImage);
        } else if (type == "barracks") {
            label = lb_spyInfiltration2_barracks;
            pixmap = getSpyInfiltrationImage("Upgrade 1.png");
        } else if (type == "warfactory") {
            label = lb_spyInfiltration2_warfactory;
            pixmap = getSpyInfiltrationImage("Upgrade 2.png");
        }
        
        if (label && !pixmap.isNull()) {
            // 右边玩家从左到右显示，最先检测到的在最左边
            int xPos = baseX + iconWidth * displayIndex;
            label->move(xPos, baseY);
            label->setPixmap(pixmap);
            label->show();
            displayIndex++;
            qDebug() << "[间谍渗透] 玩家" << validPlayerIndex2 << type << "图标显示在位置:" << xPos << "," << baseY;
        }
    }
}

// 设置玩家1间谍渗透状态位置
void Ob1::setSpyInfiltration1Position(int x, int y) {
    spyInfiltration1X = x;
    spyInfiltration1Y = y;
    // 位置更新后重新排列显示
    updatePlayer1SpyInfiltrationDisplay();
}

// 设置玩家2间谍渗透状态位置
void Ob1::setSpyInfiltration2Position(int x, int y) {
    spyInfiltration2X = x;
    spyInfiltration2Y = y;
    // 位置更新后重新排列显示
    updatePlayer2SpyInfiltrationDisplay();
}

// 显示间谍渗透提示
void Ob1::showSpyAlert(int player, const QString& spyType) {
    // 只处理前两个有效玩家的UI显示（由于UI限制）
    if (player > 1) {
        qDebug() << "[间谍提示] 玩家索引超出UI显示范围，玩家:" << player << "类型:" << spyType;
        return;
    }
    
    QLabel* alertLabel = (player == 0) ? lb_spyAlert1 : lb_spyAlert2;
    QTimer* alertTimer = (player == 0) ? spyAlertTimer1 : spyAlertTimer2;
    
    if (!alertLabel || !alertTimer) {
        qDebug() << "[间谍提示] 标签或定时器未初始化，玩家:" << player;
        return;
    }
    
    // 直接加载原始图片，避免双重缩放
    QString appDir = QCoreApplication::applicationDirPath();
    QString imagePath = QString("%1/assets/panels/%2").arg(appDir, spyType);
    QPixmap originalPixmap(imagePath);
    
    if (originalPixmap.isNull()) {
        qDebug() << "[间谍提示] 无法加载图片:" << spyType;
        return;
    }
    
    // 设置图片并显示 - 直接从原始图片进行一次高质量缩放
    float ratio = Globalsetting::getInstance().l.ratio;
    QPixmap scaledPixmap = originalPixmap.scaled(
        qRound(spyAlertWidth * ratio), 
        qRound(spyAlertHeight * ratio), 
        Qt::KeepAspectRatio, 
        Qt::SmoothTransformation
    );
    
    alertLabel->setPixmap(scaledPixmap);
    alertLabel->show();
    alertLabel->raise(); // 确保在最上层显示
    
    // 停止之前的定时器并启动新的5秒定时器
    alertTimer->stop();
    alertTimer->start(5000); // 5秒后隐藏
    
    qDebug() << "[间谍提示] 显示间谍提示，玩家:" << player << "类型:" << spyType;
}

// 设置游戏时间显示位置
void Ob1::setGameTimePosition(int x, int y) {
    gameTimeX = x;
    gameTimeY = y;
    if (lb_gameTime) {
        float ratio = Globalsetting::getInstance().l.ratio;
        lb_gameTime->move(qRound(gameTimeX * ratio), qRound(gameTimeY * ratio));
    }
    qDebug() << "游戏时间位置已更新: (" << gameTimeX << "," << gameTimeY << ")";
}

// ====== 地图名相关方法实现 ======

// Shadow.png相关函数已删除

// 设置地图名显示位置
void Ob1::setMapNamePosition(int x, int y) {
    mapNameX = x;
    mapNameY = y;
    // 触发重绘以应用新位置
    update();
    qDebug() << "地图名位置已更新: (" << mapNameX << "," << mapNameY << ")";
}

// 设置地图标签位置
void Ob1::setMapLabelPosition(int x, int y) {
    mapLabelX = x;
    mapLabelY = y;
    // 触发重绘以应用新位置
    update();
    qDebug() << "地图标签位置已更新: (" << mapLabelX << "," << mapLabelY << ")";
}



// ====== 主播名称和赛事名称位置设置方法实现 ======

// 设置主播名标签位置
void Ob1::setStreamerLabelPosition(int x, int y) {
    streamerLabelX = x;
    streamerLabelY = y;
    // 触发重绘以应用新位置
    update();
    qDebug() << "主播名标签位置已更新: (" << streamerLabelX << "," << streamerLabelY << ")";
}

// 设置主播名内容位置
void Ob1::setStreamerNamePosition(int x, int y) {
    streamerNameX = x;
    streamerNameY = y;
    // 触发重绘以应用新位置
    update();
    qDebug() << "主播名内容位置已更新: (" << streamerNameX << "," << streamerNameY << ")";
}

// 设置赛事名标签位置
void Ob1::setEventLabelPosition(int x, int y) {
    eventLabelX = x;
    eventLabelY = y;
    // 触发重绘以应用新位置
    update();
    qDebug() << "赛事名标签位置已更新: (" << eventLabelX << "," << eventLabelY << ")";
}

// 设置赛事名内容位置
void Ob1::setEventNamePosition(int x, int y) {
    eventNameX = x;
    eventNameY = y;
    // 触发重绘以应用新位置
    update();
    qDebug() << "赛事名内容位置已更新: (" << eventNameX << "," << eventNameY << ")";
}

// ====== BO数相关方法实现 ======

// 设置BO数
void Ob1::setBONumber(int number) {
    boNumber = number;
    // 触发重绘以应用新的BO数
    update();
    qDebug() << "BO数已更新: " << boNumber;
}

// 获取BO数
int Ob1::getBoNumber() const {
    return boNumber;
}

// 设置BO数显示位置
void Ob1::setBoTextPosition(int x, int y) {
    boTextX = x;
    boTextY = y;
    // 触发重绘以应用新位置
    update();
    qDebug() << "BO数位置已更新: (" << boTextX << "," << boTextY << ")";
}

// ====== 玩家分数相关方法实现 ======

// 初始化分数标签
void Ob1::initScoreLabels() {
    float ratio = Globalsetting::getInstance().l.ratio;
    
    // 创建玩家1分数标签
    lb_score1 = new QLabel(this);
    lb_score1->setAlignment(Qt::AlignCenter);
    lb_score1->setStyleSheet("QLabel { color: white; background-color: transparent; }");
    
    // 设置字体为miSansHeavyFamily
    QFont scoreFont(miSansHeavyFamily, qRound(14 * ratio), QFont::Bold);
    lb_score1->setFont(scoreFont);
    
    lb_score1->installEventFilter(this);
    lb_score1->hide();
    
    // 创建玩家2分数标签
    lb_score2 = new QLabel(this);
    lb_score2->setAlignment(Qt::AlignCenter);
    lb_score2->setStyleSheet("QLabel { color: white; background-color: transparent; }");
    
    // 设置字体为miSansHeavyFamily
    lb_score2->setFont(scoreFont);
    
    lb_score2->installEventFilter(this);
    lb_score2->hide();
    
    qDebug() << "分数标签初始化完成";
}

// 更新分数显示
void Ob1::updateScoreDisplay() {
    if (!g._gameInfo.valid) {
        if (lb_score1) lb_score1->hide();
        if (lb_score2) lb_score2->hide();
        return;
    }
    
    float ratio = Globalsetting::getInstance().l.ratio;
    
    // 获取全局分数设置
    Globalsetting &gls = Globalsetting::getInstance();
    
    // 更新玩家1分数
    if (lb_score1) {
        // 使用实际的玩家昵称作为键值，如果没有昵称则使用"player1"作为后备
        QString player1Key = currentNickname1.isEmpty() ? "player1" : currentNickname1;
        int score1 = gls.playerScores.value(player1Key, 0);
        QString scoreText = QString::number(score1);
        lb_score1->setText(scoreText);
        
        // 设置字体为miSansHeavyFamily，确保居中显示
        QFont scoreFont(miSansHeavyFamily, qRound(21 * ratio), QFont::Black);
        lb_score1->setFont(scoreFont);
        
        // 设置位置和大小
        int scaledX = qRound(score1X * ratio);
        int scaledY = qRound(score1Y * ratio);
        int scaledWidth = qRound(scoreLabelWidth * ratio);
        int scaledHeight = qRound(scoreLabelHeight * ratio);
        
        lb_score1->setGeometry(scaledX, scaledY, scaledWidth, scaledHeight);
        lb_score1->show();
    }
    
    // 更新玩家2分数
    if (lb_score2) {
        // 使用实际的玩家昵称作为键值，如果没有昵称则使用"player2"作为后备
        QString player2Key = currentNickname2.isEmpty() ? "player2" : currentNickname2;
        int score2 = gls.playerScores.value(player2Key, 0);
        QString scoreText = QString::number(score2);
        lb_score2->setText(scoreText);
        
        // 设置字体为miSansHeavyFamily，确保居中显示
        QFont scoreFont(miSansHeavyFamily, qRound(21 * ratio), QFont::Black);
        lb_score2->setFont(scoreFont);
        
        // 设置位置和大小
        int scaledX = qRound(score2X * ratio);
        int scaledY = qRound(score2Y * ratio);
        int scaledWidth = qRound(scoreLabelWidth * ratio);
        int scaledHeight = qRound(scoreLabelHeight * ratio);
        
        lb_score2->setGeometry(scaledX, scaledY, scaledWidth, scaledHeight);
        lb_score2->show();
    }
}

// 设置玩家1分数位置
void Ob1::setScore1Position(int x, int y) {
    score1X = x;
    score1Y = y;
    // 更新分数显示以应用新位置
    updateScoreDisplay();
    qDebug() << "玩家1分数位置已更新: (" << score1X << "," << score1Y << ")";
}

// 设置玩家2分数位置
void Ob1::setScore2Position(int x, int y) {
    score2X = x;
    score2Y = y;
    // 更新分数显示以应用新位置
    updateScoreDisplay();
    qDebug() << "玩家2分数位置已更新: (" << score2X << "," << score2Y << ")";
}

// 更新分数（对外接口）
void Ob1::updateScores() {
    updateScoreDisplay();
}

// 事件过滤器实现
bool Ob1::eventFilter(QObject *watched, QEvent *event) {
    // 检查是否是分数标签上的鼠标事件
    if ((watched == lb_score1 || watched == lb_score2) && event->type() == QEvent::MouseButtonPress) {
        QMouseEvent *mouseEvent = static_cast<QMouseEvent*>(event);
        
        // 检查Ctrl键是否按下
        if (QApplication::keyboardModifiers() & Qt::ControlModifier) {
            // 确定是哪个玩家的分数标签
            bool isPlayer1 = (watched == lb_score1);
            
            // 获取当前分数
            Globalsetting &gls = Globalsetting::getInstance();
            // 使用实际的玩家昵称作为键值，如果没有昵称则使用"player1"/"player2"作为后备
            QString playerKey;
            if (isPlayer1) {
                playerKey = currentNickname1.isEmpty() ? "player1" : currentNickname1;
            } else {
                playerKey = currentNickname2.isEmpty() ? "player2" : currentNickname2;
            }
            int currentScore = gls.playerScores.value(playerKey, 0);
            
            // 左键点击增加分数
            if (mouseEvent->button() == Qt::LeftButton) {
                currentScore++;
                
                // 更新全局分数记录
                gls.playerScores[playerKey] = currentScore;
                
                // 更新分数显示
                updateScoreDisplay();
                
                qDebug() << "玩家" << (isPlayer1 ? 1 : 2) << "(" << playerKey << ")分数增加到:" << currentScore;
                return true;
            }
            // 右键点击减少分数
            else if (mouseEvent->button() == Qt::RightButton) {
                if (currentScore > 0) {
                    currentScore--;
                    
                    // 更新全局分数记录
                    gls.playerScores[playerKey] = currentScore;
                    
                    // 更新分数显示
                    updateScoreDisplay();
                    
                    qDebug() << "玩家" << (isPlayer1 ? 1 : 2) << "(" << playerKey << ")分数减少到:" << currentScore;
                }
                return true;
            }
        }
    }
    
    // 其他事件传递给基类处理
    return QWidget::eventFilter(watched, event);
}

// ====== PMB图片相关方法实现 ======

// 设置PMB图片
void Ob1::setPmbImage(const QString &imagePath) {
    QString appDir = QCoreApplication::applicationDirPath();
    QString targetPath = appDir + "/assets/panels/ra2.png";
    QString backupPath = appDir + "/assets/panels/ra2_backup.png";
    
    // 如果备份文件不存在，先备份原始文件
    if (!QFile::exists(backupPath) && QFile::exists(targetPath)) {
        QFile::copy(targetPath, backupPath);
    }
    
    // 复制新图片到目标路径
    if (QFile::exists(targetPath)) {
        QFile::remove(targetPath);
    }
    
    if (QFile::copy(imagePath, targetPath)) {
        // 重新加载图片
        pmbImage.load(targetPath);
        showPmbImage = true;
        
        // 保存设置到全局配置
        Globalsetting::getInstance().s.show_pmb_image = showPmbImage;
        
        // 触发重绘
        update();
        
        qDebug() << "PMB图片已设置: " << imagePath;
    } else {
        qDebug() << "设置PMB图片失败: " << imagePath;
    }
}

// 重置PMB图片
void Ob1::resetPmbImage() {
    QString appDir = QCoreApplication::applicationDirPath();
    QString targetPath = appDir + "/assets/panels/ra2.png";
    QString backupPath = appDir + "/assets/panels/ra2_backup.png";
    
    // 如果备份文件存在，恢复备份
    if (QFile::exists(backupPath)) {
        if (QFile::exists(targetPath)) {
            QFile::remove(targetPath);
        }
        QFile::copy(backupPath, targetPath);
    }
    
    // 清空当前图片，下次绘制时会重新加载
    pmbImage = QPixmap();
    
    // 触发重绘
    update();
    
    qDebug() << "PMB图片已重置";
}

// 设置PMB图片显示状态
void Ob1::setShowPmbImage(bool show) {
    showPmbImage = show;
    
    // 保存设置到全局配置
    Globalsetting::getInstance().s.show_pmb_image = showPmbImage;
    
    // 触发重绘
    update();
    
    qDebug() << "PMB图片显示状态已设置: " << (show ? "显示" : "隐藏");
}

// 设置PMB图片位置
void Ob1::setPmbPosition(int offsetX, int offsetY) {
    pmbOffsetX = offsetX;
    pmbOffsetY = offsetY;
    
    // 触发重绘
    update();
    
    qDebug() << "PMB图片位置已设置: (" << pmbOffsetX << "," << pmbOffsetY << ")";
}

// 设置PMB图片大小
void Ob1::setPmbSize(int width, int height) {
    pmbWidth = width;
    pmbHeight = height;
    
    // 触发重绘
    update();
    
    qDebug() << "PMB图片大小已设置: " << pmbWidth << "x" << pmbHeight;
}

// ====== 单位筛选和排序功能实现 ======

// 过滤重复单位 - 合并类似单位（如Yuri和Yuri Clone）
void Ob1::filterDuplicateUnits(Ra2ob::tagUnitsInfo &uInfo) {
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

// 单位排序 - 优先显示共有单位，再显示特有单位
void Ob1::sortUnitsForDisplay(Ra2ob::tagUnitsInfo &uInfo1, Ra2ob::tagUnitsInfo &uInfo2, 
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
            bool isSameUnit = (it1->unitName == it2->unitName);
            
            if (isSameUnit) {
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

// 刷新单位信息
void Ob1::refreshUnits() {
    if (!g._gameInfo.valid || p1_index < 0 || p2_index < 0) {
        return;
    }

    // 创建玩家单位信息的副本，避免修改原始数据
    Ra2ob::tagUnitsInfo uInfo1 = g._gameInfo.players[p1_index].units;
    Ra2ob::tagUnitsInfo uInfo2 = g._gameInfo.players[p2_index].units;
    
    // 过滤重复单位（在副本上操作）
    filterDuplicateUnits(uInfo1);
    filterDuplicateUnits(uInfo2);
    
    // 进行单位排序
    std::vector<Ra2ob::tagUnitSingle> sortedUnits1;
    std::vector<Ra2ob::tagUnitSingle> sortedUnits2;
    sortUnitsForDisplay(uInfo1, uInfo2, sortedUnits1, sortedUnits2);
    
    // 更新单位显示
    updateUnitDisplay(sortedUnits1, sortedUnits2);
    
    // 更新头像显示
    updateAvatarDisplay();
}

// 更新单位显示
void Ob1::updateUnitDisplay(const std::vector<Ra2ob::tagUnitSingle> &units1, 
                         const std::vector<Ra2ob::tagUnitSingle> &units2) {
    // 存储排序后的单位数据到成员变量中，供绘制时使用
    sortedUnits1 = units1;
    sortedUnits2 = units2;
    
    // 触发重绘以更新单位显示
    update();
}

// ====== 地图视频动画相关方法实现 ======

// 初始化地图视频标签
void Ob1::initMapVideoLabel() {
    lb_mapVideo = new QLabel(this);
    lb_mapVideo->setScaledContents(true);
    lb_mapVideo->setAttribute(Qt::WA_TransparentForMouseEvents);
    lb_mapVideo->hide(); // 初始隐藏
    mapVideoLoaded = false;
}

// ====== 延迟加载相关方法实现 ======

// 初始化帧缓存
void Ob1::initFrameCache() {
    // 初始化LRU链表
    cacheHead = new FrameCacheNode(-1);
    cacheTail = new FrameCacheNode(-1);
    cacheHead->next = cacheTail;
    cacheTail->prev = cacheHead;
    currentCacheSize = 0;
    frameCache.clear();
    
    qDebug() << "帧缓存初始化完成，最大缓存大小:" << maxCacheSize;
}

// 清理帧缓存
void Ob1::clearFrameCache() {
    // 先清理哈希表，但不删除节点（避免双重释放）
    frameCache.clear();
    
    // 然后通过链表清理所有节点（包括头尾哨兵节点）
    if (cacheHead) {
        FrameCacheNode* current = cacheHead;
        while (current) {
            FrameCacheNode* next = current->next;
            delete current;
            current = next;
        }
    }
    
    cacheHead = nullptr;
    cacheTail = nullptr;
    currentCacheSize = 0;
    
    qDebug() << "帧缓存已清理";
}

// 将节点移到链表头
void Ob1::moveToHead(FrameCacheNode* node) {
    if (!node || !cacheHead || !cacheTail) return;
    
    // 从当前位置移除
    if (node->prev) node->prev->next = node->next;
    if (node->next) node->next->prev = node->prev;
    
    // 插入到头部
    node->next = cacheHead->next;
    node->prev = cacheHead;
    if (cacheHead->next) {
        cacheHead->next->prev = node;
    }
    cacheHead->next = node;
}

// 从缓存中移除节点
void Ob1::removeFromCache(FrameCacheNode* node) {
    if (!node) return;
    
    // 从链表中移除
    if (node->prev) node->prev->next = node->next;
    if (node->next) node->next->prev = node->prev;
    
    // 从哈希表中移除
    frameCache.remove(node->frameIndex);
    
    delete node;
    currentCacheSize--;
}

// 添加帧到缓存
void Ob1::addToCache(int frameIndex, const QPixmap& pixmap) {
    if (!cacheHead || !cacheTail) return;
    
    // 如果已存在，更新并移到头部
    if (frameCache.contains(frameIndex)) {
        FrameCacheNode* node = frameCache[frameIndex];
        node->pixmap = pixmap;
        moveToHead(node);
        return;
    }
    
    // 如果缓存已满，移除最久未使用的帧
    if (currentCacheSize >= maxCacheSize) {
        FrameCacheNode* tail = cacheTail->prev;
        if (tail && tail != cacheHead) {
            removeFromCache(tail);
        }
    }
    
    // 创建新节点并添加到头部
    FrameCacheNode* newNode = new FrameCacheNode(frameIndex);
    newNode->pixmap = pixmap;
    
    newNode->next = cacheHead->next;
    newNode->prev = cacheHead;
    if (cacheHead->next) {
        cacheHead->next->prev = newNode;
    }
    cacheHead->next = newNode;
    
    frameCache[frameIndex] = newNode;
    currentCacheSize++;
}

// 从磁盘加载帧
QPixmap Ob1::loadFrameFromDisk(int frameIndex) {
    if (frameIndex < 0 || frameIndex >= frameSequence.size()) {
        return QPixmap();
    }
    
    QString framePath = videoDir + frameSequence[frameIndex];
    QPixmap frame(framePath);
    
    if (frame.isNull()) {
        qDebug() << "警告: 无法加载动画帧:" << framePath;
    }
    
    return frame;
}

// 初始化地图视频序列信息（不加载实际帧）
void Ob1::initMapVideoSequence(const QString& mapName) {
    frameSequence.clear();
    currentMapVideoFrame = 0;
    videoDir = QCoreApplication::applicationDirPath() + "/assets/1v1png/";
    
    qDebug() << "初始化地图视频序列，地图名称:" << mapName;
    
    // 根据地图名称确定要加载的动画帧序列
    if (mapName == "2025冰天王-星夜冰天") {
        // MAP3_00000.png-MAP3_00087.png-MAP2_00088.png-MAP2_00149.png
        for (int i = 0; i <= 87; ++i) {
            frameSequence.append(QString("MAP3_%1.png").arg(i, 5, 10, QChar('0')));
        }
        for (int i = 88; i <= 149; ++i) {
            frameSequence.append(QString("MAP2_%1.png").arg(i, 5, 10, QChar('0')));
        }
        qDebug() << "初始化星夜冰天动画帧序列";
    }
    else if (mapName == "2025冰天王-星夜黄金冰天") {
        // MAP4_00000.png-MAP4_00087.png-MAP2_00088.png-MAP2_00149.png
        for (int i = 0; i <= 87; ++i) {
            frameSequence.append(QString("MAP4_%1.png").arg(i, 5, 10, QChar('0')));
        }
        for (int i = 88; i <= 149; ++i) {
            frameSequence.append(QString("MAP2_%1.png").arg(i, 5, 10, QChar('0')));
        }
        qDebug() << "初始化星夜黄金冰天动画帧序列";
    }
    else if (mapName == "2025冰天王-黄金冰天") {
        // MAP2_00000.png-MAP2_00149.png
        for (int i = 0; i <= 149; ++i) {
            frameSequence.append(QString("MAP2_%1.png").arg(i, 5, 10, QChar('0')));
        }
        qDebug() << "初始化黄金冰天动画帧序列";
    }
    else if (mapName == "2025冰天王-宝石冰天") {
        // MAP1_00000.png-MAP1_00087.png-MAP2_00088.png-MAP2_00149.png
        for (int i = 0; i <= 87; ++i) {
            frameSequence.append(QString("MAP1_%1.png").arg(i, 5, 10, QChar('0')));
        }
        for (int i = 88; i <= 149; ++i) {
            frameSequence.append(QString("MAP2_%1.png").arg(i, 5, 10, QChar('0')));
        }
        qDebug() << "初始化宝石冰天动画帧序列";
    }
    else {
        // 其他地图名称不初始化动画帧
        qDebug() << "地图名称不匹配，不初始化PNG动画帧序列";
        mapVideoInitialized = true;
        return;
    }
    
    totalFrames = frameSequence.size();
    qDebug() << "地图视频序列初始化完成，总帧数:" << totalFrames;
    
    // 设置初始化完成标志
    mapVideoInitialized = true;
}

// 获取指定帧（延迟加载）
QPixmap Ob1::getMapVideoFrame(int frameIndex) {
    if (frameIndex < 0 || frameIndex >= totalFrames) {
        return QPixmap();
    }
    
    // 检查缓存中是否存在
    if (frameCache.contains(frameIndex)) {
        FrameCacheNode* node = frameCache[frameIndex];
        moveToHead(node); // 更新LRU顺序
        return node->pixmap;
    }
    
    // 从磁盘加载
    QPixmap frame = loadFrameFromDisk(frameIndex);
    if (!frame.isNull()) {
        addToCache(frameIndex, frame);
        
        // 预加载下几帧（异步优化）
        for (int i = 1; i <= 3 && frameIndex + i < totalFrames; ++i) {
            int nextIndex = frameIndex + i;
            if (!frameCache.contains(nextIndex)) {
                QPixmap nextFrame = loadFrameFromDisk(nextIndex);
                if (!nextFrame.isNull()) {
                    addToCache(nextIndex, nextFrame);
                }
            }
        }
    }
    
    return frame;
}

// 获取当前帧（延迟加载版本）
QPixmap Ob1::getCurrentMapVideoFrame() {
    if (!mapVideoInitialized || totalFrames == 0) {
        return QPixmap();
    }
    
    return getMapVideoFrame(currentMapVideoFrame);
}

// 播放地图视频
void Ob1::playMapVideo() {
    if (totalFrames > 0 && mapVideoTimer && !mapVideoPlaying) {
        mapVideoTimer->start(40); // 每40毫秒一帧，25FPS
        mapVideoPlaying = true;
        qDebug() << "地图视频播放开始，总帧数:" << totalFrames;
    }
}

// 停止地图视频
void Ob1::stopMapVideo() {
    if (mapVideoTimer) {
        mapVideoTimer->stop();
        mapVideoPlaying = false;
        qDebug() << "地图视频播放停止";
    }
}

// 获取当前地图视频帧
QPixmap Ob1::getMapVideoImage() {
    return getCurrentMapVideoFrame();
}

// 设置地图视频帧到标签
void Ob1::setMapVideoImage() {
    if (!lb_mapVideo) return;
    
    QPixmap pixmap = getMapVideoImage();
    if (!pixmap.isNull()) {
        lb_mapVideo->setPixmap(pixmap);
    }
}

// 更新地图视频显示
void Ob1::updateMapVideoDisplay() {
    if (!lb_mapVideo || !this->isVisible()) {
        if (lb_mapVideo) lb_mapVideo->hide();
        return;
    }
    
    if (!mapVideoPlaying || totalFrames == 0 || currentMapVideoFrame < 0 || currentMapVideoFrame >= totalFrames) {
        lb_mapVideo->hide();
        return;
    }
    
    // 设置当前帧
    setMapVideoImage();
    
    // 计算位置和大小（与ob1 - 副本.cpp保持一致）
    float ratio = Globalsetting::getInstance().l.ratio;
    int frameW = qRound(900 * ratio);
    int frameH = qRound(513 * ratio);
    int x = (width() - frameW) / 2 - qRound(84 * ratio);
    int y = (height() - frameH) / 2;
    
    // 设置位置和大小
    lb_mapVideo->setGeometry(x, y, frameW, frameH);
    lb_mapVideo->show();
}

// 设置地图视频位置
void Ob1::setMapVideoPosition(int x, int y) {
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

// ====== 头像相关方法实现 ======

// 初始化头像标签
void Ob1::initAvatarLabels() {
    // 创建玩家1头像标签
    lb_avatar1 = new QLabel(this);
    lb_avatar1->setScaledContents(true);
    lb_avatar1->setAttribute(Qt::WA_TransparentForMouseEvents);
    lb_avatar1->hide(); // 初始隐藏
    
    // 创建玩家2头像标签
    lb_avatar2 = new QLabel(this);
    lb_avatar2->setScaledContents(true);
    lb_avatar2->setAttribute(Qt::WA_TransparentForMouseEvents);
    lb_avatar2->hide(); // 初始隐藏
    
    
    qDebug() << "头像标签初始化完成";
}

// 设置玩家头像
void Ob1::setAvatar(int playerIndex) {
    // 确保玩家索引有效
    if (playerIndex < 0 || playerIndex >= Ra2ob::MAXPLAYER || !g._gameInfo.valid || !g._gameInfo.players[playerIndex].valid) {
        return;
    }
    
    // 确定是玩家1还是玩家2
    bool isPlayer1 = (playerIndex == p1_index);
    bool& remoteLoaded = isPlayer1 ? remoteAvatarLoaded1 : remoteAvatarLoaded2;
    QString& currentNick = isPlayer1 ? currentNickname1 : currentNickname2;
    QPixmap& playerAvatar = isPlayer1 ? playerAvatar1 : playerAvatar2;
    
    // 获取玩家昵称和游戏名称
    std::string name = g._gameInfo.players[playerIndex].panel.playerNameUtf;
    QString nickname = QString::fromUtf8(name);
    Globalsetting &gls = Globalsetting::getInstance();
    QString playername = gls.findNameByNickname(nickname);
    
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
bool Ob1::checkRemoteFileExists(const QString &url) {
    if (!networkManager) {
        return false;
    }
    
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
void Ob1::downloadAvatar(const QString &url, int playerIndex) {
    if (!networkManager) {
        return;
    }
    
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
void Ob1::useDefaultAvatar(int playerIndex) {
    // 确保玩家索引有效
    if (playerIndex < 0 || playerIndex >= Ra2ob::MAXPLAYER || !g._gameInfo.valid || !g._gameInfo.players[playerIndex].valid) {
        return;
    }
    
    // 确定是玩家1还是玩家2
    bool isPlayer1 = (playerIndex == p1_index);
    QPixmap& playerAvatar = isPlayer1 ? playerAvatar1 : playerAvatar2;
    
    QString defaultAvatarPath;
    
    // 获取玩家国家
    std::string country = g._gameInfo.players[playerIndex].panel.country;
    QString countryName = QString::fromStdString(country);
    
    QString appDir = QCoreApplication::applicationDirPath();
    
    // 根据国家选择头像 - 区分阵营
    if (countryName == "Russians" || countryName == "Cuba" || 
        countryName == "Iraq" || countryName == "Libya") {
        // 苏联阵营使用logo2.png
        defaultAvatarPath = appDir + "/assets/panels/logo2.png";
    } else if (countryName == "Americans" || countryName == "British" || 
               countryName == "France" || countryName == "Germans" || 
               countryName == "Korea") {
        // 盟军阵营使用logo.png
        defaultAvatarPath = appDir + "/assets/panels/logo.png";
    } else if (countryName == "Yuri") {
        // 尤里阵营可以使用专门的头像
        defaultAvatarPath = appDir + "/assets/panels/logo3.png";
        // 如果logo3.png不存在，回退到logo.png
        if (!QFile::exists(defaultAvatarPath)) {
            defaultAvatarPath = appDir + "/assets/panels/logo.png";
        }
    } else {
        // 如果是未知国家，尝试直接使用国家图标
        defaultAvatarPath = appDir + "/assets/3v3countries/" + countryName + ".png";
    }
    
    // 检查文件是否存在
    if (!QFile::exists(defaultAvatarPath)) {
        // 如果特定头像不存在，使用通用默认头像
        defaultAvatarPath = appDir + "/avatars/default.png";
        
        // 如果连默认头像都不存在，使用程序图标
        if (!QFile::exists(defaultAvatarPath)) {
            QPixmap iconPixmap = QPixmap(":/icon.ico");
            if (!iconPixmap.isNull()) {
                playerAvatar = iconPixmap;
                displayAvatar(playerIndex);
                return;
            }
        }
    }
    
    // 加载默认头像
    QPixmap defaultAvatar(defaultAvatarPath);
    if (!defaultAvatar.isNull()) {
        playerAvatar = defaultAvatar;
        displayAvatar(playerIndex);
    }
}

// 显示当前加载的头像
void Ob1::displayAvatar(int playerIndex) {
    // 确保玩家索引有效
    if (playerIndex < 0 || playerIndex >= Ra2ob::MAXPLAYER || !g._gameInfo.valid || !g._gameInfo.players[playerIndex].valid) {
        return;
    }
    
    // 确定是玩家1还是玩家2
    bool isPlayer1 = (playerIndex == p1_index);
    QPixmap& playerAvatar = isPlayer1 ? playerAvatar1 : playerAvatar2;
    QLabel* avatarLabel = isPlayer1 ? lb_avatar1 : lb_avatar2;
    
    if (playerAvatar.isNull() || !avatarLabel) {
        return;
    }
    
    // 计算适当的头像大小（考虑分辨率）
    float ratio = Globalsetting::getInstance().l.ratio;
    int avatarSize = qRound(41.5 * ratio);
    
    // 将头像设置为正方形
    QPixmap scaledAvatar = playerAvatar.scaled(avatarSize, avatarSize, 
                                            Qt::KeepAspectRatio, 
                                            Qt::SmoothTransformation);
    
    // 设置到标签
    avatarLabel->setPixmap(scaledAvatar);
    avatarLabel->setFixedSize(avatarSize, avatarSize);
}

// 更新玩家头像
void Ob1::updatePlayerAvatars() {
    // 强制重置头像加载状态，确保重新加载
    remoteAvatarLoaded1 = false;
    remoteAvatarLoaded2 = false;
    
    // 清空当前头像缓存
    playerAvatar1 = QPixmap();
    playerAvatar2 = QPixmap();
    
    // 更新玩家1头像
    if (p1_index >= 0 && p1_index < Ra2ob::MAXPLAYER && g._gameInfo.valid && g._gameInfo.players[p1_index].valid) {
        setAvatar(p1_index);
    }
    
    // 更新玩家2头像
    if (p2_index >= 0 && p2_index < Ra2ob::MAXPLAYER && g._gameInfo.valid && g._gameInfo.players[p2_index].valid) {
        setAvatar(p2_index);
    }
    
    qDebug() << "强制更新玩家头像完成";
}

// 更新头像显示
void Ob1::updateAvatarDisplay() {
    if (!g._gameInfo.valid) {
        // 游戏无效时隐藏头像
        if (lb_avatar1) lb_avatar1->hide();
        if (lb_avatar2) lb_avatar2->hide();
        return;
    }
    
    // 计算头像标签位置和大小
    float ratio = Globalsetting::getInstance().l.ratio;
    int avatarSize = qRound(41.5 * ratio);
    
    // 玩家1头像
    if (p1_index >= 0 && p1_index < Ra2ob::MAXPLAYER && g._gameInfo.players[p1_index].valid) {
        if (lb_avatar1) {
            // 获取玩家昵称
            std::string name = g._gameInfo.players[p1_index].panel.playerNameUtf;
            QString nickname = QString::fromUtf8(name);
            
            // 检查昵称是否变化或头像是否为空
            if (nickname != currentNickname1 || playerAvatar1.isNull()) {
                currentNickname1 = nickname;
                setAvatar(p1_index);
            } else {
                // 确保头像显示
                displayAvatar(p1_index);
            }
            
            // 设置位置
            int avatarX = qRound(avatar1X * ratio);
            int avatarY = qRound(avatar1Y * ratio);
            lb_avatar1->setGeometry(avatarX, avatarY, avatarSize, avatarSize);
            
            lb_avatar1->show();
        }
    } else {
        if (lb_avatar1) lb_avatar1->hide();
    }
    
    // 玩家2头像
    if (p2_index >= 0 && p2_index < Ra2ob::MAXPLAYER && g._gameInfo.players[p2_index].valid) {
        if (lb_avatar2) {
            // 获取玩家昵称
            std::string name = g._gameInfo.players[p2_index].panel.playerNameUtf;
            QString nickname = QString::fromUtf8(name);
            
            // 检查昵称是否变化或头像是否为空
            if (nickname != currentNickname2 || playerAvatar2.isNull()) {
                currentNickname2 = nickname;
                setAvatar(p2_index);
            } else {
                // 确保头像显示
                displayAvatar(p2_index);
            }
            
            // 设置位置
            int avatarX = qRound(avatar2X * ratio);
            int avatarY = qRound(avatar2Y * ratio);
            lb_avatar2->setGeometry(avatarX, avatarY, avatarSize, avatarSize);
            
            lb_avatar2->show();
        }
    } else {
        if (lb_avatar2) lb_avatar2->hide();
    }
}

// 重置头像加载状态
void Ob1::resetAvatarLoadStatus() {
    remoteAvatarLoaded1 = false;
    remoteAvatarLoaded2 = false;
    
    // 重新加载头像
    updatePlayerAvatars();
    
    qDebug() << "头像加载状态已重置";
}

// 设置玩家1头像位置
void Ob1::setAvatar1Position(int x, int y) {
    avatar1X = x;
    avatar1Y = y;
    
    if (lb_avatar1) {
        float ratio = Globalsetting::getInstance().l.ratio;
        int scaledX = qRound(avatar1X * ratio);
        int scaledY = qRound(avatar1Y * ratio);
        int avatarSize = qRound(41.5 * ratio);
        lb_avatar1->setGeometry(scaledX, scaledY, avatarSize, avatarSize);
    }
    
    qDebug() << "玩家1头像位置已更新: (" << avatar1X << "," << avatar1Y << ")";
}

// 设置玩家2头像位置
void Ob1::setAvatar2Position(int x, int y) {
    avatar2X = x;
    avatar2Y = y;
    
    if (lb_avatar2) {
        float ratio = Globalsetting::getInstance().l.ratio;
        int scaledX = qRound(avatar2X * ratio);
        int scaledY = qRound(avatar2Y * ratio);
        int avatarSize = qRound(41.5 * ratio);
        lb_avatar2->setGeometry(scaledX, scaledY, avatarSize, avatarSize);
    }
    
    qDebug() << "玩家2头像位置已更新: (" << avatar2X << "," << avatar2Y << ")";
}

// 处理玩家分数变化
void Ob1::onPlayerScoreChanged(const QString& playerName, int newScore) {
    // 更新全局分数记录
    Globalsetting& gls = Globalsetting::getInstance();
    gls.playerScores[playerName] = newScore;
    
    // 更新界面分数显示
    updateScoreDisplay();
    
    qDebug() << "玩家分数已更新:" << playerName << "->" << newScore;
    
    // 加分完毕后，延迟2秒后执行OBS源控制
    QTimer::singleShot(1200, [this]() {
        // 显示转场源和BGM源
        showObsSource("转场");      // 显示转场源
        showObsSource("BGM");       // 显示BGM源
        qDebug() << "延迟2秒后已显示转场源和BGM源";
        
        // 再延迟1700毫秒后隐藏游戏中源
        QTimer::singleShot(1700, [this]() {
            switchObsScene("游戏中");  // 隐藏"游戏中"源
            qDebug() << "延迟1700ms后已隐藏游戏中源";
        });
    });
}



// 鼠标移动事件处理（保留用于兼容性，但主要使用全局钩子）
void Ob1::mouseMoveEvent(QMouseEvent *event) {
    QWidget::mouseMoveEvent(event);
}

// 鼠标离开事件处理（保留用于兼容性，但主要使用全局钩子）
void Ob1::leaveEvent(QEvent *event) {
    QWidget::leaveEvent(event);
}

// 全局鼠标钩子相关实现
#ifdef Q_OS_WIN
// 全局鼠标钩子回调函数
LRESULT CALLBACK Ob1::MouseHookProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode >= 0 && wParam == WM_MOUSEMOVE && instance) {
        MSLLHOOKSTRUCT* pMouseStruct = (MSLLHOOKSTRUCT*)lParam;
        if (pMouseStruct) {
            // 将全局坐标转换为窗口坐标
            POINT globalPos = pMouseStruct->pt;
            HWND hwnd = (HWND)instance->winId();
            POINT localPos = globalPos;
            ScreenToClient(hwnd, &localPos);
            
            // 调用实例的鼠标移动处理函数
            instance->handleGlobalMouseMove(localPos.x, localPos.y);
        }
    }
    
    return CallNextHookEx(mouseHook, nCode, wParam, lParam);
}

// 安装全局鼠标钩子
void Ob1::installGlobalMouseHook() {
    if (!mouseHook) {
        mouseHook = SetWindowsHookEx(WH_MOUSE_LL, MouseHookProc, GetModuleHandle(nullptr), 0);
        if (mouseHook) {
            qDebug() << "全局鼠标钩子安装成功";
        } else {
            qDebug() << "全局鼠标钩子安装失败";
        }
    }
}

// 卸载全局鼠标钩子
void Ob1::uninstallGlobalMouseHook() {
    if (mouseHook) {
        UnhookWindowsHookEx(mouseHook);
        mouseHook = nullptr;
        qDebug() << "全局鼠标钩子已卸载";
    }
}
#endif

// 处理全局鼠标移动
void Ob1::handleGlobalMouseMove(int x, int y) {
    // 获取当前分辨率比例
    float ratio = Globalsetting::getInstance().l.ratio;
    
    // 定义左上角区域的大小（基于1920x1080分辨率）
    int topLeftAreaWidth = qRound(400 * ratio);
    int topLeftAreaHeight = qRound(35 * ratio);
    
    // 检查鼠标是否在左上角区域内
    bool wasInTopLeftArea = mouseInTopLeftArea;
    mouseInTopLeftArea = (x >= 0 && x <= topLeftAreaWidth && y >= 0 && y <= topLeftAreaHeight);
    
    // 如果鼠标状态发生变化，触发重绘
    if (wasInTopLeftArea != mouseInTopLeftArea) {
        update();
    }
}

// 绘制建造进度栏
void Ob1::paintProducingBlocks(QPainter *painter) {
    if (!g._gameInfo.valid || !showProducingBlocks) {
        return;
    }
    
    float ratio = Globalsetting::getInstance().l.ratio;
    
    // 绘制玩家1的建造进度
    if (p1_index >= 0 && p1_index < Ra2ob::MAXPLAYER && g._gameInfo.players[p1_index].valid) {
        const auto& buildingList = g._gameInfo.players[p1_index].building.list;
        QColor playerColor = QColor(QString::fromStdString("#" + g._gameInfo.players[p1_index].panel.color));
        
        int currentX = qRound(producing1X * ratio);
        int currentY = qRound(producing1Y * ratio);
        int displayedCount = 0;
        
        for (const auto& building : buildingList) {
            if (displayedCount >= maxProducingPerRow) {
                break;
            }
            
            if (building.number > 0) {
                drawProducingBlock(painter, building, currentX, currentY, ratio, playerColor);
                currentX += qRound((producingBlockWidth + producingItemSpacing) * ratio);
                displayedCount++;
            }
        }
    }
    
    // 绘制玩家2的建造进度
    if (p2_index >= 0 && p2_index < Ra2ob::MAXPLAYER && g._gameInfo.players[p2_index].valid) {
        const auto& buildingList = g._gameInfo.players[p2_index].building.list;
        QColor playerColor = QColor(QString::fromStdString("#" + g._gameInfo.players[p2_index].panel.color));
        
        int currentX = qRound(producing2X * ratio);
        int currentY = qRound(producing2Y * ratio);
        int displayedCount = 0;
        
        for (const auto& building : buildingList) {
            if (displayedCount >= maxProducingPerRow) {
                break;
            }
            
            if (building.number > 0) {
                drawProducingBlock(painter, building, currentX, currentY, ratio, playerColor);
                currentX += qRound((producingBlockWidth + producingItemSpacing) * ratio);
                displayedCount++;
            }
        }
    }
}

// 绘制单个建造块
void Ob1::drawProducingBlock(QPainter *painter, const Ra2ob::tagBuildingNode& building, 
                            int x, int y, float ratio, const QColor& playerColor) {
    int blockWidth = qRound(producingBlockWidth * ratio);
    int blockHeight = qRound(producingBlockHeight * ratio);
    
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);
    
    // 创建科技风格的背景
    QLinearGradient bgGradient(x, y, x, y + blockHeight);
    bgGradient.setColorAt(0, QColor(20, 25, 35, 200));
    bgGradient.setColorAt(0.5, QColor(15, 20, 30, 220));
    bgGradient.setColorAt(1, QColor(10, 15, 25, 240));
    
    // 绘制背景矩形（带圆角）
    QPainterPath bgPath;
    int cornerRadius = qRound(6 * ratio);
    bgPath.addRoundedRect(x, y, blockWidth, blockHeight, cornerRadius, cornerRadius);
    painter->fillPath(bgPath, bgGradient);
    
    // 绘制边框（使用玩家颜色）
    QColor borderColor = playerColor;
    borderColor.setAlpha(180);
    QPen borderPen(borderColor, qRound(1.5 * ratio));
    painter->setPen(borderPen);
    painter->drawPath(bgPath);
    
    // 绘制单位图标
    QString unitName = QString::fromStdString(building.name);
    QPixmap unitIcon = getUnitIcon(unitName);
    
    if (!unitIcon.isNull()) {
        // 设置图标尺寸
        int iconWidth = qRound(58 * ratio);
        int iconHeight = qRound(47 * ratio);
        
        // 缩放图标到指定尺寸
        QPixmap scaledIcon = unitIcon.scaled(iconWidth, iconHeight, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
        
        // 计算图标在建造块中心的位置
        int iconX = x + (blockWidth - scaledIcon.width()) / 2;
        int iconY = y + (blockHeight - scaledIcon.height()) / 2;
        
        // 应用圆角效果
        QPixmap roundedIcon = getRadiusPixmap(scaledIcon, qRound(4 * ratio));
        
        painter->drawPixmap(iconX, iconY, roundedIcon);
    }
    
    // 计算进度百分比
    float progressPercentage = 0.0f;
    if (building.progress > 0) {
        progressPercentage = static_cast<float>(building.progress) / 54.0f;
        progressPercentage = qBound(0.0f, progressPercentage, 1.0f);
    }
    
    // 绘制沿边框的进度条
    if (progressPercentage > 0.0f) {
        int borderWidth = qRound(5 * ratio); // 进度条宽度
        int innerMargin = qRound(2 * ratio); // 内边距
        
        // 计算内部矩形区域
        int innerX = x + innerMargin;
        int innerY = y + innerMargin;
        int innerWidth = blockWidth - 2 * innerMargin;
        int innerHeight = blockHeight - 2 * innerMargin;
        int innerCornerRadius = qRound(4 * ratio);
        
        // 计算总周长和当前进度对应的长度
        float totalPerimeter = 2 * (innerWidth + innerHeight) - 8 * innerCornerRadius + 2 * M_PI * innerCornerRadius;
        float progressLength = totalPerimeter * progressPercentage;
        
        // 创建进度条路径
        QPainterPath progressPath;
        float currentLength = 0;
        
        // 从左上角开始，顺时针绘制
        QPointF startPoint(innerX + innerCornerRadius, innerY);
        progressPath.moveTo(startPoint);
        
        // 上边
        float topLength = innerWidth - 2 * innerCornerRadius;
        if (currentLength + topLength <= progressLength) {
            progressPath.lineTo(innerX + innerWidth - innerCornerRadius, innerY);
            currentLength += topLength;
        } else {
            float remainingLength = progressLength - currentLength;
            progressPath.lineTo(innerX + innerCornerRadius + remainingLength, innerY);
            currentLength = progressLength;
        }
        
        // 右上角圆弧
        if (currentLength < progressLength) {
            float arcLength = M_PI * innerCornerRadius / 2;
            if (currentLength + arcLength <= progressLength) {
                progressPath.arcTo(innerX + innerWidth - 2 * innerCornerRadius, innerY, 
                                 2 * innerCornerRadius, 2 * innerCornerRadius, 90, -90);
                currentLength += arcLength;
            } else {
                float remainingAngle = (progressLength - currentLength) / innerCornerRadius * 180 / M_PI;
                progressPath.arcTo(innerX + innerWidth - 2 * innerCornerRadius, innerY, 
                                 2 * innerCornerRadius, 2 * innerCornerRadius, 90, -remainingAngle);
                currentLength = progressLength;
            }
        }
        
        // 右边
        if (currentLength < progressLength) {
            float rightLength = innerHeight - 2 * innerCornerRadius;
            if (currentLength + rightLength <= progressLength) {
                progressPath.lineTo(innerX + innerWidth, innerY + innerHeight - innerCornerRadius);
                currentLength += rightLength;
            } else {
                float remainingLength = progressLength - currentLength;
                progressPath.lineTo(innerX + innerWidth, innerY + innerCornerRadius + remainingLength);
                currentLength = progressLength;
            }
        }
        
        // 右下角圆弧
        if (currentLength < progressLength) {
            float arcLength = M_PI * innerCornerRadius / 2;
            if (currentLength + arcLength <= progressLength) {
                progressPath.arcTo(innerX + innerWidth - 2 * innerCornerRadius, innerY + innerHeight - 2 * innerCornerRadius, 
                                 2 * innerCornerRadius, 2 * innerCornerRadius, 0, -90);
                currentLength += arcLength;
            } else {
                float remainingAngle = (progressLength - currentLength) / innerCornerRadius * 180 / M_PI;
                progressPath.arcTo(innerX + innerWidth - 2 * innerCornerRadius, innerY + innerHeight - 2 * innerCornerRadius, 
                                 2 * innerCornerRadius, 2 * innerCornerRadius, 0, -remainingAngle);
                currentLength = progressLength;
            }
        }
        
        // 下边
        if (currentLength < progressLength) {
            float bottomLength = innerWidth - 2 * innerCornerRadius;
            if (currentLength + bottomLength <= progressLength) {
                progressPath.lineTo(innerX + innerCornerRadius, innerY + innerHeight);
                currentLength += bottomLength;
            } else {
                float remainingLength = progressLength - currentLength;
                progressPath.lineTo(innerX + innerWidth - innerCornerRadius - remainingLength, innerY + innerHeight);
                currentLength = progressLength;
            }
        }
        
        // 左下角圆弧
        if (currentLength < progressLength) {
            float arcLength = M_PI * innerCornerRadius / 2;
            if (currentLength + arcLength <= progressLength) {
                progressPath.arcTo(innerX, innerY + innerHeight - 2 * innerCornerRadius, 
                                 2 * innerCornerRadius, 2 * innerCornerRadius, 270, -90);
                currentLength += arcLength;
            } else {
                float remainingAngle = (progressLength - currentLength) / innerCornerRadius * 180 / M_PI;
                progressPath.arcTo(innerX, innerY + innerHeight - 2 * innerCornerRadius, 
                                 2 * innerCornerRadius, 2 * innerCornerRadius, 270, -remainingAngle);
                currentLength = progressLength;
            }
        }
        
        // 左边
        if (currentLength < progressLength) {
            float leftLength = innerHeight - 2 * innerCornerRadius;
            if (currentLength + leftLength <= progressLength) {
                progressPath.lineTo(innerX, innerY + innerCornerRadius);
                currentLength += leftLength;
            } else {
                float remainingLength = progressLength - currentLength;
                progressPath.lineTo(innerX, innerY + innerHeight - innerCornerRadius - remainingLength);
                currentLength = progressLength;
            }
        }
        
        // 左上角圆弧
        if (currentLength < progressLength) {
            float arcLength = M_PI * innerCornerRadius / 2;
            if (currentLength + arcLength <= progressLength) {
                progressPath.arcTo(innerX, innerY, 2 * innerCornerRadius, 2 * innerCornerRadius, 180, -90);
                currentLength += arcLength;
            } else {
                float remainingAngle = (progressLength - currentLength) / innerCornerRadius * 180 / M_PI;
                progressPath.arcTo(innerX, innerY, 2 * innerCornerRadius, 2 * innerCornerRadius, 180, -remainingAngle);
                currentLength = progressLength;
            }
        }
        
        // 设置进度条颜色
        QColor progressColor;
        if (building.status == 1 && progressPercentage < 1.0f) { // 暂停状态
            progressColor = QColor(255, 0, 0, 200); // 红色
        } else { // 建造中或完成状态都使用玩家颜色
            progressColor = playerColor;
            progressColor.setAlpha(200); // 设置透明度
        }
        
        // 绘制进度条
        QPen progressPen(progressColor, borderWidth, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
        painter->setPen(progressPen);
        painter->drawPath(progressPath);
        
        // 添加发光效果
        QPen glowPen(QColor(255, 255, 255, 80), borderWidth + qRound(1 * ratio), Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
        painter->setPen(glowPen);
        painter->drawPath(progressPath);
        
        // 重新绘制主进度条
        painter->setPen(progressPen);
        painter->drawPath(progressPath);
    }
    
    // 绘制状态文本（在框内中间）
    QString statusText;
    QColor textColor = Qt::white;
    
    if (building.status == 1 && progressPercentage < 1.0f) {
        statusText = "暂停";
        textColor = QColor(255, 255, 255);
    } else if (progressPercentage >= 1.0f) {
        statusText = "完成";
        textColor = QColor(255, 255, 255);
    } else {
        statusText = "";
        textColor = QColor(0, 255, 0);
    }
    
    if (!statusText.isEmpty()) {
        // 设置字体
        QFont statusFont(miSansHeavyFamily.isEmpty() ? "Arial" : miSansHeavyFamily, 
                        qRound(11 * ratio), QFont::Bold);
        painter->setFont(statusFont);
        
        // 绘制文本阴影（在整个框的中间）
        painter->setPen(QColor(0, 0, 0, 150));
        QRect textRect(x + 1, y + 1, blockWidth, blockHeight);
        painter->drawText(textRect, Qt::AlignCenter, statusText);
        
        // 绘制文本（在整个框的中间）
        painter->setPen(textColor);
        textRect.adjust(-1, -1, -1, -1);
        painter->drawText(textRect, Qt::AlignCenter, statusText);
    }
    
    // 绘制数量（如果大于1）
    if (building.number > 1) {
        QString numberText = QString::number(building.number);
        QFont numberFont(technoGlitchFamily.isEmpty() ? "Arial" : technoGlitchFamily, 
                        qRound(14 * ratio), QFont::Bold); // 数字字体变大
        painter->setFont(numberFont);
        
        // 绘制数量背景
        int numberBgSize = qRound(20 * ratio); // 背景大小保持固定
        
        int numberX = x + blockWidth - numberBgSize - qRound(2 * ratio);
        int numberY = y + qRound(0 * ratio);
        
        // 绘制圆形背景
        painter->setBrush(playerColor);
        painter->setPen(Qt::NoPen);
        painter->drawEllipse(numberX, numberY, numberBgSize, numberBgSize);
        
        // 绘制数量文本
        painter->setPen(Qt::white);
        QRect numberRect(numberX, numberY, numberBgSize, numberBgSize);
        painter->drawText(numberRect, Qt::AlignCenter, numberText);
    }
    
    painter->restore();
}

// 计算建造栏区域
void Ob1::calculateProducingBlocksRegions() {
    if (!g._gameInfo.valid) {
        return;
    }
    
    float ratio = Globalsetting::getInstance().l.ratio;
    
    // 计算玩家1建造栏区域（检测前两个建造单位）
    if (p1_index >= 0 && p1_index < Ra2ob::MAXPLAYER && g._gameInfo.players[p1_index].valid) {
        const auto& buildingList = g._gameInfo.players[p1_index].building.list;
        int validBuildingCount = 0;
        
        for (const auto& building : buildingList) {
            if (building.number > 0) {
                validBuildingCount++;
                if (validBuildingCount >= 2) {
                    break;
                }
            }
        }
        
        if (validBuildingCount > 0) {
            int startX = qRound(producing1X * ratio);
            int startY = qRound(producing1Y * ratio);
            int blockWidth = qRound(producingBlockWidth * ratio);
            int spacing = qRound(producingItemSpacing * ratio);
            int height = qRound(producingBlockHeight * ratio);
            
            // 计算宽度：如果有两个或以上建造单位，包含两个块和一个间距；否则只包含一个块
            int totalWidth = (validBuildingCount >= 2) ? (2 * blockWidth + spacing) : blockWidth;
            
            player1ProducingRect = QRect(startX, startY, totalWidth, height);
        } else {
            player1ProducingRect = QRect();
        }
    } else {
        player1ProducingRect = QRect();
    }
    
    // 计算玩家2建造栏区域（检测前两个建造单位）
    if (p2_index >= 0 && p2_index < Ra2ob::MAXPLAYER && g._gameInfo.players[p2_index].valid) {
        const auto& buildingList = g._gameInfo.players[p2_index].building.list;
        int validBuildingCount = 0;
        
        for (const auto& building : buildingList) {
            if (building.number > 0) {
                validBuildingCount++;
                if (validBuildingCount >= 2) {
                    break;
                }
            }
        }
        
        if (validBuildingCount > 0) {
            int startX = qRound(producing2X * ratio);
            int startY = qRound(producing2Y * ratio);
            int blockWidth = qRound(producingBlockWidth * ratio);
            int spacing = qRound(producingItemSpacing * ratio);
            int height = qRound(producingBlockHeight * ratio);
            
            // 计算宽度：如果有两个或以上建造单位，包含两个块和一个间距；否则只包含一个块
            int totalWidth = (validBuildingCount >= 2) ? (2 * blockWidth + spacing) : blockWidth;
            
            player2ProducingRect = QRect(startX, startY, totalWidth, height);
        } else {
            player2ProducingRect = QRect();
        }
    } else {
        player2ProducingRect = QRect();
    }
}

// 检查鼠标位置是否在建造栏区域内
void Ob1::checkMousePositionForProducingBlocks() {
    // 更新建造栏区域
    calculateProducingBlocksRegions();
    
    // 获取鼠标位置
    QPoint mousePos = mapFromGlobal(QCursor::pos());
    
    // 检查鼠标是否在建造栏区域内
    bool mouseInProducingArea = false;
    
    if (!player1ProducingRect.isEmpty() && player1ProducingRect.contains(mousePos)) {
        mouseInProducingArea = true;
    }
    
    if (!player2ProducingRect.isEmpty() && player2ProducingRect.contains(mousePos)) {
        mouseInProducingArea = true;
    }
    
    // 如果鼠标在建造栏区域内，隐藏建造栏；否则显示建造栏
    bool newShowProducingBlocks = !mouseInProducingArea;
    
    if (newShowProducingBlocks != showProducingBlocks) {
        showProducingBlocks = newShowProducingBlocks;
        update(); // 触发重绘
    }
}

// OBS相关方法实现
void Ob1::initObsConnection() {
    // 创建WebSocket连接
    obsWebSocket = new QWebSocket("", QWebSocketProtocol::VersionLatest, this);
    
    // 连接信号
    connect(obsWebSocket, &QWebSocket::connected, this, &Ob1::onObsConnected);
    connect(obsWebSocket, &QWebSocket::disconnected, this, &Ob1::onObsDisconnected);
    connect(obsWebSocket, &QWebSocket::textMessageReceived, this, &Ob1::onObsMessageReceived);
    
    // 尝试连接到OBS
    qDebug() << "正在连接OBS WebSocket:" << obsWebSocketUrl;
    obsWebSocket->open(QUrl(obsWebSocketUrl));
}

void Ob1::onObsConnected() {
    obsConnected = true;
    obsAuthenticated = false;
    qDebug() << "OBS WebSocket连接成功，等待Hello消息";
}

void Ob1::onObsDisconnected() {
    obsConnected = false;
    obsAuthenticated = false;
    qDebug() << "OBS WebSocket连接断开";
}

void Ob1::onObsMessageReceived(const QString &message) {
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

void Ob1::sendObsMessage(const QJsonObject &message) {
    if (!obsWebSocket || !obsConnected) {
        qDebug() << "OBS WebSocket未连接，无法发送消息";
        return;
    }
    
    QJsonDocument doc(message);
    QString jsonString = doc.toJson(QJsonDocument::Compact);
    obsWebSocket->sendTextMessage(jsonString);
    qDebug() << "发送OBS消息:" << jsonString;
}

void Ob1::handleObsHello(const QJsonObject &message) {
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

void Ob1::handleObsIdentified(const QJsonObject &message) {
    qDebug() << "OBS认证成功";
    obsAuthenticated = true;
}

void Ob1::handleObsRequestResponse(const QJsonObject &message) {
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

void Ob1::switchObsScene(const QString &sourceName) {
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

void Ob1::showObsSource(const QString &sourceName) {
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

void Ob1::refreshBrowserSource(const QString &sourceName) {
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

// 滚动文字相关方法实现
void Ob1::initScrollText() {
    if (!lb_scrollText) {
        lb_scrollText = new QLabel(this);
        lb_scrollText->setStyleSheet("color: white; background: transparent;");
        lb_scrollText->hide();
    }
    
    if (!scrollTimer) {
        scrollTimer = new QTimer(this);
        connect(scrollTimer, &QTimer::timeout, this, &Ob1::updateScrollText);
    }
}

void Ob1::updateScrollText() {
    if (!lb_scrollText || scrollText.isEmpty()) {
        return;
    }
    
    // 获取当前缩放比例
    float ratio = static_cast<float>(height()) / 1080.0f;
    
    // 设置滚动文字字体
    QFont scrollFont(!miSansHeavyFamily.isEmpty() ? 
                    miSansHeavyFamily : "Microsoft YaHei", 
                    qRound(12 * ratio), QFont::Normal);
    
    lb_scrollText->setFont(scrollFont);
    
    // 计算文本尺寸
    QFontMetrics fm(scrollFont);
    int textWidth = fm.horizontalAdvance(scrollText);
    int textHeight = fm.height();
    
    // 固定滚动区域宽度为200px（按比例缩放）
    int scrollAreaWidth = qRound(542 * ratio);
    
    // 计算滚动容器位置（右下角区域）
    int containerX = width() - qRound(715 * ratio);
    int containerY = height() - textHeight - qRound(5 * ratio);
    
    // 设置滚动容器的位置和大小
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

void Ob1::startScrollText() {
    if (scrollTimer && !scrollText.isEmpty()) {
        // 获取当前缩放比例
        float ratio = static_cast<float>(height()) / 1080.0f;
        int scrollAreaWidth = qRound(200 * ratio);
        
        scrollPosition = scrollAreaWidth; // 从滚动区域右边开始
        scrollTimer->start(50); // 每50毫秒更新一次
        
        // 显示滚动文字标签
        if (lb_scrollText) {
            lb_scrollText->show();
        }
    }
}

void Ob1::stopScrollText() {
    if (scrollTimer) {
        scrollTimer->stop();
    }
    if (lb_scrollText) {
        lb_scrollText->hide();
    }
}

void Ob1::setScrollText(const QString &text) {
    scrollText = text;
    if (!text.isEmpty()) {
        startScrollText();
    } else {
        stopScrollText();
    }
}

void Ob1::onBuildingTheftDetected(const QString& thiefPlayerName, const QString& victimPlayerName, 
                                 const QString& buildingName, int count) {
    qDebug() << "Ob1: 检测到建筑盗窃事件 -" << thiefPlayerName << "从" << victimPlayerName 
             << "偷取了" << count << "个" << buildingName;
    if (theftAlertManager) {
        theftAlertManager->showTheftAlert(thiefPlayerName, victimPlayerName, buildingName, count);
    }
}

void Ob1::onMinerLossDetected(const QString& playerName, const QString& minerName, int count) {
    qDebug() << "Ob1: 检测到矿车损失事件 -" << playerName << "损失了" << count << "个" << minerName;
    if (theftAlertManager) {
        theftAlertManager->showMinerLossAlert(playerName, minerName, count);
    }
}