#ifndef OB1_H
#define OB1_H

#include <QWidget>
#include <QPainter>
#include <QPixmap>
#include <QTimer>
#include <QDebug>
#include <QMap>
#include <QSet>
#include <QLabel>
#include <QMovie>
#include <QCoreApplication>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QEventLoop>
#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>
#include <QtWebSockets/QWebSocket>
#include "./Ra2ob/src/Game.hpp"
#include "globalsetting.h"
#include "playerstatustracker.h"

// 前向声明
class BuildingDetector;
class TheftAlertManager;

#ifdef Q_OS_WIN
#include <windows.h>
#endif

// 自定义可翻转的QLabel类
class FlippedLabel : public QLabel
{
    Q_OBJECT
public:
    explicit FlippedLabel(QWidget *parent = nullptr) : QLabel(parent), m_flipped(false) {}
    
    void setFlipped(bool flipped) {
        m_flipped = flipped;
        update();
    }
    
    bool isFlipped() const { return m_flipped; }
    
protected:
    void paintEvent(QPaintEvent *event) override {
        QPainter painter(this);
        
        if (m_flipped) {
            painter.translate(width(), 0);
            painter.scale(-1, 1);
        }
        
        // 绘制电影帧，强制缩放到label尺寸
        if (movie() && movie()->currentPixmap().isNull() == false) {
            QPixmap pixmap = movie()->currentPixmap();
            painter.drawPixmap(0, 0, width(), height(), pixmap);
        }
    }
    
private:
    bool m_flipped;
};

class Ob1 : public QWidget {
    Q_OBJECT

public:
    explicit Ob1(QWidget *parent = nullptr);
    ~Ob1();

    void detectGame();
    void toggleOb();
    void switchScreen();
    void onGameTimerTimeout();  // 游戏定时器超时处理
    
    // 位置设置方法（公开接口）
    void setPlayernamePosition(); // 设置玩家名称位置
    void setKillsPosition(); // 设置击杀数位置
    void setAlivePosition(); // 设置幸存数位置
    void setBalancePosition(); // 设置余额位置
    void setSpentPosition();   // 设置总消耗位置
    void setPowerBarPosition(); // 设置电量条位置
    
    // 强制刷新玩家名称显示（用于外部更新玩家映射后通知界面刷新）
    void forceRefreshPlayerNames();
    
    // 更新玩家头像（公有方法，供外部调用）
    void updatePlayerAvatars();

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    // 游戏实例引用 - 使用单例模式
    Ra2ob::Game &g = Ra2ob::Game::getInstance();
    
    // 图片资源
    QPixmap bg1v1Image;    // 背景图1
    QPixmap ecobgImage;    // 经济背景图
    QPixmap bg1v12Image;   // 背景图2
    
    // 玩家颜色相关

    QLabel *lb_smallColor1 = nullptr;  // 玩家1小颜色标签
    QLabel *lb_smallColor2 = nullptr;  // 玩家2小颜色标签
    int smallColor1X = 13.5;  // 玩家1小颜色X坐标
    int smallColor1Y = 1030;  // 玩家1小颜色Y坐标
    int smallColor2X = 13.5;  // 玩家2小颜色X坐标
    int smallColor2Y = 1055;  // 玩家2小颜色Y坐标
    std::string qs_1;  // 玩家1颜色
    std::string qs_2;  // 玩家2颜色
    
    // 玩家颜色GIF相关
    FlippedLabel *lb_colorGif1 = nullptr;  // 玩家1颜色GIF标签
    FlippedLabel *lb_colorGif2 = nullptr;  // 玩家2颜色GIF标签
    QMap<QString, QMovie*> colorCache;     // 颜色GIF缓存
    bool color1Loaded = false;             // 玩家1颜色GIF是否已加载
    bool color2Loaded = false;             // 玩家2颜色GIF是否已加载
    int colorGif1X = 493;  // 玩家1颜色GIF X坐标
    int colorGif1Y = 0;    // 玩家1颜色GIF Y坐标
    int colorGif2X = 974;  // 玩家2颜色GIF X坐标
    int colorGif2Y = 0;    // 玩家2颜色GIF Y坐标
    int colorGifWidth = 284;  // 颜色GIF宽度
    int colorGifHeight = 48;  // 颜色GIF高度
    
    // 右侧颜色PNG相关
    QLabel *lb_rightColor1 = nullptr;      // 玩家1右侧颜色标签
    QLabel *lb_rightColor2 = nullptr;      // 玩家2右侧颜色标签

    int rightColor1X = 1766;  // 玩家1右侧颜色X坐标
    int rightColor1Y = 179;   // 玩家1右侧颜色Y坐标
    int rightColor2X = 1835;  // 玩家2右侧颜色X坐标
    int rightColor2Y = 179;   // 玩家2右侧颜色Y坐标
    int rightColorWidth = 68;  // 右侧颜色宽度
    int rightColorHeight = 605; // 右侧颜色高度
    
    // 国旗相关
    QLabel *lb_flag1 = nullptr;      // 玩家1国旗标签
    QLabel *lb_flag2 = nullptr;      // 玩家2国旗标签
    QMap<QString, QPixmap> flagCache; // 国旗图片缓存
    int flag1X = 402;   // 玩家1国旗X坐标
    int flag1Y = 2.7;   // 玩家1国旗Y坐标
    int flag2X = 1318;  // 玩家2国旗X坐标
    int flag2Y = 2.7;   // 玩家2国旗Y坐标
    int flagWidth = 30;  // 国旗宽度
    int flagHeight = 22; // 国旗高度
    
    // 新国旗组相关
    QLabel *lb_newFlag1 = nullptr;      // 玩家1新国旗标签
    QLabel *lb_newFlag2 = nullptr;      // 玩家2新国旗标签
    QMap<QString, QPixmap> newFlagCache; // 新国旗图片缓存
    
    // 游戏状态
    bool lastGameValid = false;
    int p1_index = -1;  // 玩家1索引
    int p2_index = -1;  // 玩家2索引
    
    // 玩家名称相关
    QLabel *lb_playerName1 = nullptr;  // 玩家1名称标签
    QLabel *lb_playerName2 = nullptr;  // 玩家2名称标签
    QString currentNickname1;          // 玩家1当前昵称
    QString currentNickname2;          // 玩家2当前昵称
    int playerName1X = 620;            // 玩家1名称X坐标
    int playerName1Y = 10;              // 玩家1名称Y坐标
    int playerName2X =1110;            // 玩家2名称X坐标
    int playerName2Y = 10;              // 玩家2名称Y坐标
    
    // 定时器
    QTimer *gameTimer;
    QTimer *memoryCleanupTimer;     // 内存清理定时器
    QTimer *unitUpdateTimer;        // 单位信息刷新定时器
    QTimer *avatarRefreshTimer;     // 头像刷新定时器
    QTimer *powerBarBlinkTimer;     // 电量条闪烁定时器
    QTimer *producingBlocksTimer;   // 建造栏区域检测定时器
    QTimer *obsDelayTimer = nullptr; // OBS切换延迟定时器
    
    // 玩家状态跟踪器
    PlayerStatusTracker *statusTracker = nullptr;  // 玩家状态跟踪器
    
    // 建筑盗窃检测器
    BuildingDetector *buildingDetector = nullptr;   // 建筑盗窃检测器
    TheftAlertManager *theftAlertManager = nullptr; // 盗窃警报管理器
    
    // 游戏时间相关
    QLabel *lb_gameTime = nullptr;  // 游戏时间标签
    int gameTimeX = 1815;           // 游戏时间X坐标
    int gameTimeY = 8;              // 游戏时间Y坐标
    
    // 电量条相关
    QLabel *lb_powerBar1 = nullptr;  // 玩家1电量条标签
    QLabel *lb_powerBar2 = nullptr;  // 玩家2电量条标签
    bool powerBarBlinkState = false; // 电量条闪烁状态
    int powerBar1X = 213;            // 玩家1电量条X坐标
    int powerBar1Y = 3;             // 玩家1电量条Y坐标
    int powerBar2X = 1483;           // 玩家2电量条X坐标
    int powerBar2Y = 3;             // 玩家2电量条Y坐标
    int powerBarWidth = 60;          // 电量条宽度
    int powerBarHeight = 20;         // 电量条高度
    
    // 击杀数和幸存数相关
    QLabel *lb_kills1 = nullptr;     // 玩家1击杀数标签
    QLabel *lb_kills2 = nullptr;     // 玩家2击杀数标签
    QLabel *lb_alive1 = nullptr;     // 玩家1幸存数标签
    QLabel *lb_alive2 = nullptr;     // 玩家2幸存数标签
    int kills1X = 110;                // 玩家1击杀数X坐标
    int kills1Y = 0;                // 玩家1击杀数Y坐标
    int kills2X = 1580;               // 玩家2击杀数X坐标
    int kills2Y = 0;                // 玩家2击杀数Y坐标
    int alive1X = 12;                // 玩家1幸存数X坐标
    int alive1Y = 0;                // 玩家1幸存数Y坐标
    int alive2X = 1675;               // 玩家2幸存数X坐标
    int alive2Y = 0;                // 玩家2幸存数Y坐标
    int statsLabelWidth = 60;        // 统计标签宽度
    int statsLabelHeight = 25;       // 统计标签高度
    
    // 余额和总消耗经济相关
    QLabel *lb_balance1 = nullptr;   // 玩家1余额标签
    QLabel *lb_balance2 = nullptr;   // 玩家2余额标签
    QLabel *lb_spent1 = nullptr;     // 玩家1总消耗标签
    QLabel *lb_spent2 = nullptr;     // 玩家2总消耗标签
    int balance1X = 300;             // 玩家1余额X坐标
    int balance1Y = 0;              // 玩家1余额Y坐标
    int balance2X = 1370;             // 玩家2余额X坐标
    int balance2Y = 0;              // 玩家2余额Y坐标
    int spent1X = 6;               // 玩家1总消耗X坐标
    int spent1Y = 1030;                // 玩家1总消耗Y坐标
    int spent2X = 6;             // 玩家2总消耗X坐标
    int spent2Y = 1054;                // 玩家2总消耗Y坐标
    int economyLabelWidth = 80;      // 经济标签宽度
    int economyLabelHeight = 25;     // 经济标签高度
    
    // 头像相关
    QLabel *lb_avatar1 = nullptr;    // 玩家1头像标签
    QLabel *lb_avatar2 = nullptr;    // 玩家2头像标签
    QPixmap playerAvatar1;           // 玩家1头像缓存
    QPixmap playerAvatar2;           // 玩家2头像缓存
    QNetworkAccessManager *networkManager = nullptr; // 网络管理器
    bool remoteAvatarLoaded1 = false; // 玩家1远程头像是否已加载
    bool remoteAvatarLoaded2 = false; // 玩家2远程头像是否已加载
    int avatar1X = 443;              // 玩家1头像X坐标
    int avatar1Y = 2;               // 玩家1头像Y坐标
    int avatar2X = 1264.5;             // 玩家2头像X坐标
    int avatar2Y = 2;               // 玩家2头像Y坐标
    int avatarSize = 41.5;             // 头像大小
    

    
    // 字体相关
    QString miSansHeavyFamily;       // MISANS-HEAVY字体族名称
    QString technoGlitchFamily;      // LLDEtechnoGlitch-Bold0字体族名称
    
    // 小颜色图片相关函数
    void initSmallColorLabels();  // 初始化小颜色标签
    QPixmap getSmallColorImage(const QString& color);  // 获取小颜色图片
    void setPlayerColor(int playerIndex);  // 设置玩家颜色
    void updateSmallColorDisplay();  // 更新小颜色显示
    
    // 颜色GIF相关函数
    void initColorGifLabels();  // 初始化颜色GIF标签


    
    // 新增的颜色GIF方法
    QMovie* getPlayerColorGif(const QString& color);  // 获取玩家颜色GIF（带缓存）
    void updateColorDisplay();  // 更新颜色显示（独立方法）
    
    // 右侧颜色PNG相关方法
    void initRightColorLabels();  // 初始化右侧颜色标签
    QPixmap getRightColorImage(const QString& color);  // 获取右侧颜色图片
    void updateRightColorDisplay();  // 更新右侧颜色显示
    
    // 国旗相关方法
    void initFlagLabels();  // 初始化国旗标签
    void initNewFlagLabels();  // 初始化新国旗组标签
    QPixmap getCountryFlag(const QString& country);  // 获取国旗图片
    QPixmap getNewCountryFlag(const QString& country);  // 获取新国旗组图片
    void updateFlagDisplay();  // 更新国旗显示
    void updateNewFlagDisplay();  // 更新新国旗组显示
    
    // 统一颜色映射方法
    QString getColorName(const QString& colorCode);  // 根据颜色代码获取颜色名称
    
    // 单位相关
    QMap<QString, QPixmap> unitIconCache;  // 单位图标缓存
    bool unitIconsLoaded = false;          // 单位图标是否已加载
    int unitIconWidth = 55;                // 单位图标宽度
    int unitIconHeight = 44;               // 单位图标高度
    int unitSpacing = 5;                   // 单位间距
    int unitDisplayOffsetY = 10;           // 单位显示Y偏移
    
    // 单位相关方法
    QPixmap getUnitIcon(const QString& unitName);  // 获取单位图标
    QPixmap getRadiusPixmap(const QPixmap& src, int radius);  // 应用圆角效果
    void drawPlayerUnits(QPainter *painter, int playerIndex, int startX, int startY, float ratio);  // 绘制单个玩家的单位
    void paintUnits(QPainter *painter);  // 绘制玩家单位
    void clearUnitDisplay();  // 清空单位显示
    
    // 建造进度相关
    int producingBlockWidth = 70;          // 建造块宽度
    int producingBlockHeight = 60;         // 建造块高度
    int producingItemSpacing = 5;          // 建造项间距
    int producing1X = 13;                 // 玩家1建造栏X坐标
    int producing1Y = 92;                  // 玩家1建造栏Y坐标
    int producing2X = 13;                // 玩家2建造栏X坐标01
    int producing2Y = 157;                  // 玩家2建造栏Y坐标0
    int maxProducingPerRow = 5;            // 每行最多显示的建造项数量
    
    // 建造进度相关方法
    void paintProducingBlocks(QPainter *painter);  // 绘制建造进度栏
    void drawProducingBlock(QPainter *painter, const Ra2ob::tagBuildingNode& building, 
                           int x, int y, float ratio, const QColor& playerColor);  // 绘制单个建造块
    
    // 建造栏鼠标检测相关
    bool showProducingBlocks = true;               // 是否显示建造栏
    QRect player1ProducingRect;                    // 玩家1建造栏区域
    QRect player2ProducingRect;                    // 玩家2建造栏区域
    void calculateProducingBlocksRegions();        // 计算建造栏区域
    void checkMousePositionForProducingBlocks();   // 检测鼠标位置是否在建造栏区域
    

    
    // 排序后的单位数据存储
    std::vector<Ra2ob::tagUnitSingle> sortedUnits1;  // 玩家1排序后的单位数据
    std::vector<Ra2ob::tagUnitSingle> sortedUnits2;  // 玩家2排序后的单位数据
    
    // 单位筛选和排序相关方法
    void filterDuplicateUnits(Ra2ob::tagUnitsInfo &uInfo);  // 过滤重复单位
    void sortUnitsForDisplay(Ra2ob::tagUnitsInfo &uInfo1, Ra2ob::tagUnitsInfo &uInfo2, 
                           std::vector<Ra2ob::tagUnitSingle> &sortedUnits1, 
                           std::vector<Ra2ob::tagUnitSingle> &sortedUnits2);  // 排序单位显示
    void refreshUnits();  // 刷新单位信息
    void updateUnitDisplay(const std::vector<Ra2ob::tagUnitSingle> &units1, 
                         const std::vector<Ra2ob::tagUnitSingle> &units2);  // 更新单位显示
    
    // 右侧颜色绘制方法
    void paintRightColors(QPainter *painter);  // 绘制右侧颜色图片
    void paintRightFlags(QPainter *painter);   // 绘制右侧国旗
    
    // 性能优化相关方法
    void onUnitUpdateTimerTimeout();    // 单位信息更新定时器槽函数
    void forceMemoryCleanup();          // 强制内存回收
    void resetAvatarLoadStatus();       // 重置头像加载状态
    
    // 头像相关方法
    void initAvatarLabels();            // 初始化头像标签
    void setAvatar(int playerIndex);    // 设置玩家头像
    void displayAvatar(int playerIndex); // 显示头像
    void downloadAvatar(const QString &url, int playerIndex); // 下载头像
    bool checkRemoteFileExists(const QString &url); // 检查远程文件是否存在
    void useDefaultAvatar(int playerIndex); // 使用默认头像

    void updateAvatarDisplay();         // 更新头像显示
    void setAvatar1Position(int x, int y); // 设置玩家1头像位置
    void setAvatar2Position(int x, int y); // 设置玩家2头像位置
    

    
    // 滚动文字相关成员变量
    QLabel* lb_scrollText = nullptr; // 滚动文字标签
    QTimer* scrollTimer = nullptr;   // 滚动文字定时器
    QString scrollText;              // 滚动文字内容
    int scrollPosition;              // 滚动位置
    
    // 玩家名称相关方法
    void initPlayerNameLabels();        // 初始化玩家名称标签
    void updatePlayerNameDisplay();     // 更新玩家名称显示
    
    // 电量条相关方法
    void initPowerBarLabels();          // 初始化电量条标签
    QPixmap createPowerBarPixmap(int powerOutput, int powerDrain, float ratio); // 创建电量条图像
    void updatePowerBars();             // 更新电量条显示
    
    // 击杀数和幸存数相关方法
    void initStatsLabels();             // 初始化击杀数和幸存数标签
    void updateStatsDisplay();          // 更新击杀数和幸存数显示
    
    // 余额和总消耗经济相关方法
    void initEconomyLabels();           // 初始化余额和总消耗标签
    void updateEconomyDisplay();        // 更新余额和总消耗显示
    
    // 经济条相关
    QPixmap economyBarPixmap1;          // 玩家1经济条画布
    QPixmap economyBarPixmap2;          // 玩家2经济条画布
    QTimer *economyBarTimer = nullptr;  // 经济条更新定时器
    int economyBarWidth = 356;          // 经济条宽度
    int economyBarHeight = 20;          // 经济条高度
    int economyBarBlockWidth = 2;       // 经济条块宽度
    int economyBar1X = 76;             // 玩家1经济条X坐标
    int economyBar1Y = 1033;              // 玩家1经济条Y坐标
    int economyBar2X = 76;            // 玩家2经济条X坐标
    int economyBar2Y = 1057;              // 玩家2经济条Y坐标
    int economyBarBgX = 0;              // 经济条背景X坐标
    int economyBarBgY = 0;              // 经济条背景Y坐标
    
    // 经济条相关方法
    void initEconomyBars();             // 初始化经济条
    void drawEconomyBlock(QPixmap& pixmap, int playerIndex); // 绘制经济块
    void paintEconomyBars(QPainter* painter); // 绘制经济条
    void setEconomyBar1Position(int x, int y); // 设置玩家1经济条位置
    void setEconomyBar2Position(int x, int y); // 设置玩家2经济条位置
    void setEconomyBarBgPosition(int x, int y); // 设置经济条背景位置
    
    // 游戏时间相关方法
    void initGameTimeLabel();           // 初始化游戏时间标签
    void updateGameTimeDisplay();       // 更新游戏时间显示
    void setGameTimePosition(int x, int y); // 设置游戏时间显示位置
    

    
    // 地图名相关
    int mapNameX = 935;                 // 地图名X坐标
    int mapNameY = 1070;                   // 地图名Y坐标
    
    // 地图标签显示位置
    int mapLabelX = 882;                // 地图标签X位置
    int mapLabelY = 1070;               // 地图标签Y位置
    
    // BO数相关
    int boNumber = 5;                 // BO数，默认为5
    int boTextX = 873.75;                // BO数显示X坐标
    int boTextY = 83.32;               // BO数显示Y坐标
    
    // 玩家分数相关
    QLabel *lb_score1 = nullptr;      // 玩家1分数标签
    QLabel *lb_score2 = nullptr;      // 玩家2分数标签
    int score1X = 825;                // 玩家1分数X坐标
    int score1Y = 16;               // 玩家1分数Y坐标
    int score2X = 891;               // 玩家2分数X坐标
    int score2Y = 16;               // 玩家2分数Y坐标
    int scoreLabelWidth = 35;         // 分数标签宽度
    int scoreLabelHeight = 25;        // 分数标签高度
    
    // 主播名标签显示位置
    int streamerLabelX = 475; // 默认X位置
    int streamerLabelY = 1070; // 默认Y位置
    
    // 主播名内容显示位置
    int streamerNameX = 570; // 默认X位置
    int streamerNameY = 1070; // 默认Y位置
    
    // 赛事名标签显示位置
    int eventLabelX = 680; // 默认X位置
    int eventLabelY = 1070; // 默认Y位置
    
    // 赛事名内容显示位置
    int eventNameX = 780; // 默认X位置
    int eventNameY = 1070; // 默认Y位置
    
    // Shadow.png相关代码已删除
    
    // 间谍渗透状态相关
    QLabel *lb_spyInfiltration1_tech = nullptr;     // 玩家1高科渗透状态标签
    QLabel *lb_spyInfiltration1_barracks = nullptr; // 玩家1兵营渗透状态标签
    QLabel *lb_spyInfiltration1_warfactory = nullptr; // 玩家1重工渗透状态标签
    QLabel *lb_spyInfiltration2_tech = nullptr;     // 玩家2高科渗透状态标签
    QLabel *lb_spyInfiltration2_barracks = nullptr; // 玩家2兵营渗透状态标签
    QLabel *lb_spyInfiltration2_warfactory = nullptr; // 玩家2重工渗透状态标签
    QMap<QString, QPixmap> spyInfiltrationCache; // 间谍渗透状态图片缓存
    int spyInfiltration1X = 727;            // 玩家1间谍渗透状态X坐标
    int spyInfiltration1Y = 50;             // 玩家1间谍渗透状态Y坐标（玩家名称下方）
    int spyInfiltration2X = 975;           // 玩家2间谍渗透状态X坐标
    int spyInfiltration2Y = 50;             // 玩家2间谍渗透状态Y坐标（玩家名称下方）
    int spyInfiltrationWidth = 50;          // 间谍渗透状态图片宽度
    int spyInfiltrationHeight = 38;         // 间谍渗透状态图片高度
    int spyInfiltrationSpacing = 53;        // 间谍渗透状态图片间距
    
    // 间谍渗透检测时间跟踪
    QMap<QString, qint64> player1InfiltrationTimes; // 玩家1渗透检测时间记录
    QMap<QString, qint64> player2InfiltrationTimes; // 玩家2渗透检测时间记录
    QStringList player1InfiltrationOrder;           // 玩家1渗透检测顺序
    QStringList player2InfiltrationOrder;           // 玩家2渗透检测顺序
    QString player1TechInfiltrationImage;           // 玩家1高科渗透类型图片
    QString player2TechInfiltrationImage;           // 玩家2高科渗透类型图片
    
    // 动态玩家索引存储（用于间谍渗透状态显示）
    int validPlayerIndex1 = -1;                     // 第一个有效玩家的实际索引
    int validPlayerIndex2 = -1;                     // 第二个有效玩家的实际索引
    
    // 间谍渗透提示功能（显示5秒后消失）
    QLabel *lb_spyAlert1 = nullptr;                 // 玩家1间谍渗透提示标签
    QLabel *lb_spyAlert2 = nullptr;                 // 玩家2间谍渗透提示标签
    QTimer *spyAlertTimer1 = nullptr;               // 玩家1间谍提示定时器
    QTimer *spyAlertTimer2 = nullptr;               // 玩家2间谍提示定时器
    int spyAlert1X = 443;                           // 玩家1间谍提示X坐标（头像下方）
    int spyAlert1Y = 48;                            // 玩家1间谍提示Y坐标
    int spyAlert2X = 1264;                          // 玩家2间谍提示X坐标（头像下方）
    int spyAlert2Y = 48;                            // 玩家2间谍提示Y坐标
    int spyAlertWidth = 186;                         // 间谍提示图片宽度
    int spyAlertHeight = 29;                        // 间谍提示图片高度
    
    // 间谍渗透状态相关方法
    void initSpyInfiltrationLabels();       // 初始化间谍渗透状态标签
    void updateSpyInfiltrationStatus();     // 更新间谍渗透状态
    void updateSpyInfiltrationDisplay();    // 更新间谍渗透状态显示
    void updatePlayer1SpyInfiltrationDisplay(); // 更新玩家1间谍渗透显示
    void updatePlayer2SpyInfiltrationDisplay(); // 更新玩家2间谍渗透显示
    QPixmap getSpyInfiltrationImage(const QString& imageName); // 获取间谍渗透状态图片
    void setSpyInfiltration1Position(int x, int y); // 设置玩家1间谍渗透状态位置
    void setSpyInfiltration2Position(int x, int y); // 设置玩家2间谍渗透状态位置
    void showSpyAlert(int player, const QString& spyType); // 显示间谍渗透提示
    
    // PMB图片相关
    QPixmap pmbImage;                   // PMB图片
    bool showPmbImage;                  // 是否显示PMB图片
    int pmbOffsetX;                     // PMB图片X偏移量
    int pmbOffsetY;                     // PMB图片Y偏移量
    int pmbWidth;                       // PMB图片宽度
    int pmbHeight;                      // PMB图片高度
    
    // 地图视频动画相关 - 延迟加载版本
    QTimer *mapVideoTimer = nullptr;    // 地图视频播放定时器
    
    // LRU缓存相关
    struct FrameCacheNode {
        int frameIndex;
        QPixmap pixmap;
        FrameCacheNode* prev;
        FrameCacheNode* next;
        FrameCacheNode(int idx = -1) : frameIndex(idx), prev(nullptr), next(nullptr) {}
    };
    
    QHash<int, FrameCacheNode*> frameCache;  // 帧索引到缓存节点的映射
    FrameCacheNode* cacheHead = nullptr;     // LRU缓存链表头
    FrameCacheNode* cacheTail = nullptr;     // LRU缓存链表尾
    int maxCacheSize = 10;                   // 最大缓存帧数
    int currentCacheSize = 0;                // 当前缓存大小
    
    // 动画序列信息
    QStringList frameSequence;              // 帧文件名序列
    QString videoDir;                       // 视频帧目录
    int currentMapVideoFrame = 0;           // 当前播放帧索引
    int totalFrames = 0;                    // 总帧数
    bool mapVideoPlaying = false;           // 地图视频是否正在播放
    bool mapVideoInitialized = false;      // 地图视频是否已初始化
    bool mapVideoCompleted = false;        // 地图视频是否已播放完成
    bool mapVideoLoaded = false;            // 地图视频是否已加载
    
    QLabel *lb_mapVideo = nullptr;          // 地图视频显示标签
    int mapVideoX = 0;                      // 地图视频X坐标
    int mapVideoY = 1000;                   // 地图视频Y坐标
    int mapVideoWidth = 371;                // 地图视频宽度
    int mapVideoHeight = 24;                // 地图视频高度
    
    // 地图名相关方法
    void setMapNamePosition(int x, int y); // 设置地图名显示位置
    void setMapLabelPosition(int x, int y); // 设置地图标签位置
    
    // 地图视频动画相关方法 - 延迟加载版本
    void initMapVideoLabel();           // 初始化地图视频标签
    void initMapVideoSequence(const QString& mapName); // 初始化地图视频序列信息
    void playMapVideo();                // 播放地图视频
    void stopMapVideo();                // 停止地图视频
    QPixmap getMapVideoFrame(int frameIndex); // 获取指定帧（延迟加载）
    QPixmap getCurrentMapVideoFrame();  // 获取当前播放帧
    QPixmap getMapVideoImage();         // 获取地图视频图像
    void setMapVideoImage();            // 设置地图视频帧到标签
    void updateMapVideoDisplay();       // 更新地图视频显示
    void setMapVideoPosition(int x, int y); // 设置地图视频位置
    
    // 滚动文字相关方法
    void initScrollText();              // 初始化滚动文字
    void updateScrollText();            // 更新滚动文字位置
    void startScrollText();             // 开始滚动文字
    void stopScrollText();              // 停止滚动文字
    
    // 帧缓存相关方法
    void initFrameCache();              // 初始化帧缓存
    void clearFrameCache();             // 清理帧缓存
    void addToCache(int frameIndex, const QPixmap& pixmap); // 添加帧到缓存
    void moveToHead(FrameCacheNode* node); // 将节点移到链表头
    void removeFromCache(FrameCacheNode* node); // 从缓存中移除节点
    QPixmap loadFrameFromDisk(int frameIndex); // 从磁盘加载帧
    
    // 主播名称和赛事名称位置设置方法
    void setStreamerLabelPosition(int x, int y); // 设置主播名标签位置
    void setStreamerNamePosition(int x, int y);  // 设置主播名内容位置
    void setEventLabelPosition(int x, int y);    // 设置赛事名标签位置
    void setEventNamePosition(int x, int y);     // 设置赛事名内容位置
    
    // BO数相关方法
    void setBoTextPosition(int x, int y);       // 设置BO数显示位置
    
    // 玩家分数相关方法
    void initScoreLabels();                     // 初始化分数标签
    void updateScoreDisplay();                  // 更新分数显示
    void setScore1Position(int x, int y);       // 设置玩家1分数位置
    void setScore2Position(int x, int y);       // 设置玩家2分数位置
    
    // 事件过滤器
    bool eventFilter(QObject *watched, QEvent *event) override;
    
    // 鼠标事件处理
    void mouseMoveEvent(QMouseEvent *event) override;
    void leaveEvent(QEvent *event) override;
    
    // 鼠标位置相关
    bool mouseInTopLeftArea = false; // 鼠标是否在左上角区域
    
    // OBS WebSocket相关
    QWebSocket *obsWebSocket = nullptr;                 // OBS WebSocket连接
    QString obsWebSocketUrl = "ws://localhost:8063";    // OBS WebSocket URL
    QString obsPassword = "";                           // OBS WebSocket密码
    QString gameEndSceneName = "游戏中";                 // 游戏结束时要隐藏的源名称
    bool obsConnected = false;                          // OBS连接状态
    bool obsAuthenticated = false;                      // OBS认证状态
    
    // OBS相关方法
    void sendObsMessage(const QJsonObject &message);    // 发送OBS消息
    void initObsConnection();                           // 初始化OBS连接
    void onObsConnected();                              // OBS连接成功处理
    void onObsDisconnected();                           // OBS断开连接处理
    void onObsMessageReceived(const QString &message);  // OBS消息接收处理
    void handleObsHello(const QJsonObject &message);    // 处理Hello消息
    void handleObsIdentified(const QJsonObject &message); // 处理Identified消息
    void handleObsRequestResponse(const QJsonObject &message); // 处理RequestResponse消息
    
    // 全局鼠标钩子相关
#ifdef Q_OS_WIN
    static HHOOK mouseHook;          // 全局鼠标钩子句柄
    static Ob1* instance;            // 静态实例指针
    static LRESULT CALLBACK MouseHookProc(int nCode, WPARAM wParam, LPARAM lParam); // 鼠标钩子回调函数
#endif
    void installGlobalMouseHook();   // 安装全局鼠标钩子
    void uninstallGlobalMouseHook(); // 卸载全局鼠标钩子
    void handleGlobalMouseMove(int x, int y); // 处理全局鼠标移动

public:
    // BO数相关方法
    void setBONumber(int number);               // 设置BO数
    int getBoNumber() const;                    // 获取BO数
    
    // 玩家分数相关方法
    void updateScores();                        // 更新分数（对外接口）
    
    // PMB图片相关方法
    void setPmbImage(const QString &filePath);  // 设置PMB图片
    void resetPmbImage();                       // 重置PMB图片
    void setShowPmbImage(bool show);            // 设置是否显示PMB图片
    void setPmbPosition(int offsetX, int offsetY); // 设置PMB位置
    void setPmbSize(int width, int height);     // 设置PMB大小
    
    // OBS控制方法
    void switchObsScene(const QString &sourceName);     // 控制OBS源显示/隐藏
    void showObsSource(const QString &sourceName);      // 显示OBS源
    void refreshBrowserSource(const QString &sourceName); // 刷新浏览器源缓存
    void setScrollText(const QString &text);             // 设置滚动文字内容

private:
    
private slots:
    void onEconomyBarUpdate();          // 经济条更新槽函数
    void onPowerBarBlink();             // 电量条闪烁槽函数
    void onPlayerScoreChanged(const QString& playerName, int newScore); // 处理玩家分数变化
    void onBuildingTheftDetected(const QString& thiefPlayerName, const QString& victimPlayerName, 
                                const QString& buildingName, int count);     // 建筑盗窃检测槽函数
    void onMinerLossDetected(const QString& playerName, const QString& minerName, int count); // 矿车损失检测槽函数


signals:
    void playernameNeedsUpdate();       // 玩家名称需要更新信号
};

#endif // OB1_H