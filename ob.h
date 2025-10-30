#ifndef OB_H
#define OB_H

#include <QWidget>
#include <QString>
#include <QPixmap>
#include <QLabel>
#include <QVector>
#include <QMap>
#include <QTimer>
#include <QNetworkAccessManager>
#include <QtWebSockets/QWebSocket>
#include <QJsonDocument>
#include <QJsonObject>
#include <vector>

// 前向声明
namespace Ra2ob {
    class Game;
    struct tagBuildingNode; // 添加tagBuildingNode的前向声明
    struct tagUnitsInfo; // 添加tagUnitsInfo的前向声明
    struct tagUnitSingle; // 添加tagUnitSingle的前向声明
}
class MainWindow;
class PlayerStatusTracker;
class BuildingDetector; // 添加BuildingDetector前向声明
class TheftAlertManager; // 添加TheftAlertManager前向声明

// Layout命名空间和必要的常量
namespace layout {
    const char* const OPPO_M = "Arial";
    const std::string COLOR_DEFAULT = "FF0000";
}

class Ob : public QWidget {
    Q_OBJECT

public:
    explicit Ob(QWidget *parent = nullptr);
    ~Ob();
    
    // MainWindow中引用的必要方法
    bool setPmbImage(const QString &filePath);
    void resetPmbImage();
    void setShowPmbImage(bool show);
    void updatePlayerAvatars();
    void updateScores();
    void setBONumber(int number);
    int getBoNumber() const;
    void setPmbPosition(int offsetX, int offsetY);
    void setPmbSize(int width, int height);
    void setHideDate(bool hide);
    void setScrollText(const QString &text);
    void showObsSource(const QString &sourceName);  // 显示OBS源
    void refreshBrowserSource(const QString &sourceName); // 刷新浏览器源缓存
    
    Ra2ob::Game* getGamePtr() const;

public slots:
    void detectGame();
    void toggleOb();
    void switchScreen();

signals:
    void playernameNeedsUpdate();
    void scoreChanged(const QString &nickname, int newScore);

protected:
    void paintEvent(QPaintEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override; // 添加事件过滤器声明

private:
    Ra2ob::Game *g;
    PlayerStatusTracker* statusTracker;
    BuildingDetector* buildingDetector; // 建筑检测器
    TheftAlertManager* theftAlertManager; // 建筑偷取提示管理器
    bool showPmbImage;
    
    // 编译所需的最小私有成员
    int p1_index;
    int p2_index;
    int boNumber;
    QMap<QString, QPixmap> flagCache;
    QTimer* memoryCleanupTimer;
    
    // 游戏状态和图像资源
    bool lastGameValid = false;  // 上一次检测的游戏状态
    QPixmap rightBarImage;       // 右侧栏背景图片
    QPixmap topBgImage;          // 顶部背景图片(bg.png)
    QPixmap topBg1Image;         // 顶部背景图片(bg1.png)
    QPixmap pmbImage;            // PMB图片(ra2.png)
    QTimer* gameTimer = nullptr; // 游戏状态检测定时器
    QTimer *unitUpdateTimer; // 添加单位信息刷新定时器
    QTimer *obsDelayTimer = nullptr; // OBS切换延迟定时器
    

    
    // 字体相关
    QString technoGlitchFontFamily; // 特效字体族名称
    
    // 显示标签
    QLabel* lb_gametime = nullptr; // 游戏时间标签
    QLabel* lb_date = nullptr; // 日期标签
    QLabel* lb_scrollText = nullptr; // 滚动文字标签
    QTimer* scrollTimer = nullptr; // 滚动文字定时器
    QString scrollText; // 滚动文字内容
    int scrollPosition; // 滚动位置
    bool hideDate; // 是否隐藏日期显示
    
    // 国旗相关成员变量
    bool flagsLoaded = false;
    int flagWidth = 85;
    int flagHeight = 55;
    int flagSpacing = 5;

    // 单位图标相关
    QMap<QString, QPixmap> unitIconCache;
    bool unitIconsLoaded = false;
    int unitIconWidth = 70;  // 单位图标宽度
    int unitIconHeight = 55; // 单位图标高度
    int unitSpacing = 5;     // 单位图标间距
    int unitDisplayOffsetY = 10; // 单位显示区域Y轴偏移量
    
    // 辅助方法
    bool isGameReallyOver();     // 判断游戏是否真正结束
    void onGameTimerTimeout();   // 游戏定时器超时处理
    void onUnitUpdateTimerTimeout(); // 添加单位更新定时器的槽函数
    void forceMemoryCleanup(); // 强制内存回收
    void paintTopPanel(QPainter *painter); // 绘制顶部面板
    void paintPlayerFlags(QPainter *painter); // 添加绘制国旗的方法
    void drawPlayerFlag(QPainter *painter, int playerIndex, int flagX, int colorBarY, int colorBarWidth, int colorBarHeight, int flagY, int scaledFlagWidth, int scaledFlagHeight); // 绘制单个玩家国旗
    QPixmap getCountryFlag(const QString& country); // 添加获取国旗的方法
    void setLayoutByScreen();    // 设置屏幕布局
    
    // 单位绘制相关方法
    void paintUnits(QPainter *painter);
    void drawPlayerUnits(QPainter *painter, int playerIndex, int startX, int startY, float ratio);
    QPixmap getUnitIcon(const QString& unitName);
    QPixmap getRadiusPixmap(const QPixmap& src, int radius);
    void clearUnitDisplay();
    
    // 添加单位排序和显示优化相关方法
    void filterDuplicateUnits(Ra2ob::tagUnitsInfo &uInfo);
    void sortUnitsForDisplay(Ra2ob::tagUnitsInfo &uInfo1, Ra2ob::tagUnitsInfo &uInfo2, 
                           std::vector<Ra2ob::tagUnitSingle> &sortedUnits1, 
                           std::vector<Ra2ob::tagUnitSingle> &sortedUnits2);
    void refreshUnits();
    void updateUnitDisplay(const std::vector<Ra2ob::tagUnitSingle> &units1, 
                         const std::vector<Ra2ob::tagUnitSingle> &units2);
    
    // 建造栏相关变量
    const int producingBlockWidth = 70;  // 建造栏宽度
    const int producingBlockHeight = 80; // 建造栏高度
    const int producingItemSpacing = 5;  // 建造项目间距
    bool showProducingBlocks = true;     // 是否显示建造栏
    
    // 绘制建造栏的方法
    void paintProducingBlocks(QPainter *painter);
    void drawProducingBlock(QPainter *painter, const Ra2ob::tagBuildingNode& building,
                           int x, int y, const QColor& color, float ratio);
    

    
    // 建造栏区域检测
    QRect player1ProducingRect; // 玩家1建造栏区域
    QRect player2ProducingRect; // 玩家2建造栏区域
    QTimer* producingBlocksTimer; // 建造栏区域检测定时器
    
    // 计算建造栏区域
    void calculateProducingBlocksRegions();
    // 检测鼠标位置是否在建造栏区域
    void checkMousePositionForProducingBlocks();

    // 添加私有成员变量，放在private:下
private:
    // 字体族
    QString miSansBoldFamily;
    QString miSansMediumFamily;
    QString miSansHeavyFamily;
    
    // 字体对象
    QFont technoFont;        // 用于击杀数和存活单位
    QFont technoFontLarge;   // 用于余额显示
    QFont miSansBold;        // 用于玩家名称
    QFont miSansMedium;      // 用于分数
    
    // 经济条相关成员变量
    std::vector<int> insufficient_fund_bar_1; // 玩家1资金不足历史
    std::vector<int> insufficient_fund_bar_2; // 玩家2资金不足历史
    std::string qs_1; // 玩家1颜色
    std::string qs_2; // 玩家2颜色
    QLabel *credit_1 = nullptr; // 玩家1信用点数标签
    QLabel *credit_2 = nullptr; // 玩家2信用点数标签
    QLabel *lb_mapname = nullptr; // 地图名称标签
    
    // 标签控件
    QLabel *lb_playerName1 = nullptr;
    QLabel *lb_playerName2 = nullptr;
    QLabel *lb_balance1 = nullptr;
    QLabel *lb_balance2 = nullptr;
    QLabel *lb_kills1 = nullptr;
    QLabel *lb_kills2 = nullptr;
    QLabel *lb_alive1 = nullptr;
    QLabel *lb_alive2 = nullptr;
    QLabel *lb_power1 = nullptr;
    QLabel *lb_power2 = nullptr;
    QLabel *lb_score1 = nullptr;
    QLabel *lb_score2 = nullptr;
    
    // 玩家头像相关
    QLabel *lb_avatar1 = nullptr;
    QLabel *lb_avatar2 = nullptr;
    

    QPixmap playerAvatar1;
    QPixmap playerAvatar2;
    

    QString currentNickname1;
    QString currentNickname2;
    QNetworkAccessManager *networkManager = nullptr;
    QTimer *avatarRefreshTimer = nullptr;
    static bool remoteAvatarLoaded1;
    static bool remoteAvatarLoaded2;
    
    // 玩家头像方法
    void setAvatar(int playerIndex);
    bool checkRemoteFileExists(const QString &url);
    void downloadAvatar(const QString &url, int playerIndex);
    void useDefaultAvatar(int playerIndex);
    void displayAvatar(int playerIndex);
    void resetAvatarLoadStatus();
    


    // 经济条相关方法
    void paintBottomPanel(QPainter *painter); // 绘制底部经济条
    void initIfBar(bool clean = false); // 初始化资金不足指示器
    void setPlayerColor(); // 设置玩家颜色
    void refreshPanel(); // 刷新玩家面板
    int getInsufficientFund(int playerIndex); // 获取玩家资金不足状态
    void checkAndProcessRedBlocks(); // 检查并处理红色块汇集统计
    void reloadEconomyBarsFromLongestRed(int maxRed1, int maxRed2); // 基于最长红色块重新加载经济条

    // 添加私有方法，放在private slots:下
private slots:
    void updatePlayerInfo();
    void onPlayerScoreChanged(const QString& playerName, int newScore); // 处理玩家分数变化
    
    // 添加私有方法，放在private:下
private:
    void initPlayerInfoLabels();
    void setPowerState(QLabel *label, int powerDrain, int powerOutput);
    
    // PMB位置设置
    int pmbOffsetX;
    int pmbOffsetY;
    
    // PMB大小设置
    int pmbWidth;
    int pmbHeight;
    
    // 存储排序后的单位数据
    std::vector<Ra2ob::tagUnitSingle> sortedUnits1;
    std::vector<Ra2ob::tagUnitSingle> sortedUnits2;
    
    // 地图视频动画相关
    QTimer *mapVideoTimer = nullptr;    // 地图视频播放定时器
    QTimer *mapVideoLoadTimer = nullptr; // 地图视频加载延迟定时器
    QVector<QPixmap> mapVideoFrames;    // 地图视频帧序列
    int currentMapVideoFrame = 0;       // 当前播放帧索引
    bool mapVideoPlaying = false;       // 地图视频是否正在播放
    bool mapVideoLoaded = false;        // 地图视频是否已加载
    bool mapVideoCompleted = false;     // 地图视频是否已播放完成
    QLabel *lb_mapVideo = nullptr;      // 地图视频显示标签
    int mapVideoX = 0;                  // 地图视频X坐标
    int mapVideoY = 1000;               // 地图视频Y坐标
    int mapVideoWidth = 371;            // 地图视频宽度
    int mapVideoHeight = 24;            // 地图视频高度
    
    // 地图视频动画相关方法
    void initMapVideoLabel();           // 初始化地图视频标签
    void loadMapVideoFramesByMapName(const QString& mapName); // 根据地图名加载视频帧
    void playMapVideo();                // 播放地图视频
    void stopMapVideo();                // 停止地图视频
    QPixmap getMapVideoImage();         // 获取当前地图视频帧
    void setMapVideoImage();            // 设置地图视频帧到标签
    void updateMapVideoDisplay();       // 更新地图视频显示
    void setMapVideoPosition(int x, int y); // 设置地图视频位置

    // 滚动文字相关方法
    void initScrollText(); // 初始化滚动文字
    void updateScrollText(); // 更新滚动文字位置
    void startScrollText(); // 开始滚动文字
    void stopScrollText(); // 停止滚动文字

    // OBS WebSocket相关成员变量
    QWebSocket *obsWebSocket = nullptr;     // OBS WebSocket连接
    QString obsWebSocketUrl;                // OBS WebSocket地址
    QString obsPassword;                    // OBS WebSocket密码
    QString gameEndSceneName;               // 游戏结束时切换到的场景名称
    bool obsConnected = false;              // OBS连接状态
    bool obsAuthenticated = false;          // OBS认证状态
    int obsRequestId = 1;                   // OBS请求ID计数器
    
    // OBS相关方法
    void initObsConnection();               // 初始化OBS连接
    void sendObsMessage(const QJsonObject &message); // 发送OBS消息
    void switchObsScene(const QString &sourceName); // 隐藏OBS源
    void handleObsHello(const QJsonObject &message); // 处理Hello消息
    void handleObsIdentified(const QJsonObject &message); // 处理Identified消息
    void handleObsRequestResponse(const QJsonObject &response); // 处理OBS请求响应
    
    // 间谍渗透相关
    QLabel *lb_spyAlert1 = nullptr;         // 玩家1间谍渗透提示标签
    QLabel *lb_spyAlert2 = nullptr;         // 玩家2间谍渗透提示标签
    QTimer *spyAlertTimer1 = nullptr;       // 玩家1间谍提示定时器
    QTimer *spyAlertTimer2 = nullptr;       // 玩家2间谍提示定时器
    int spyAlertWidth = 186;                // 间谍提示图片宽度
    int spyAlertHeight = 29;                // 间谍提示图片高度
    
    // 间谍渗透状态跟踪
    QMap<QString, qint64> player1InfiltrationTimes; // 玩家1渗透检测时间记录
    QMap<QString, qint64> player2InfiltrationTimes; // 玩家2渗透检测时间记录
    QStringList player1InfiltrationOrder;           // 玩家1渗透检测顺序
    QStringList player2InfiltrationOrder;           // 玩家2渗透检测顺序
    QString player1TechInfiltrationImage;           // 玩家1高科渗透类型图片
    QString player2TechInfiltrationImage;           // 玩家2高科渗透类型图片
    
    void initSpyInfiltrationLabels();       // 初始化间谍渗透标签
    void updateSpyInfiltrationDisplay();    // 更新间谍渗透显示
    void showSpyAlert(int playerIndex, const QString& spyType); // 显示间谍渗透提示
    void resetSpyInfiltrationStatus();      // 重置间谍渗透状态跟踪变量

private slots:
    void onObsConnected();                  // OBS连接成功槽函数
    void onObsDisconnected();               // OBS断开连接槽函数
    void onObsMessageReceived(const QString &message); // OBS消息接收槽函数
    void onBuildingTheftDetected(const QString& thiefPlayerName, const QString& victimPlayerName, 
                                const QString& buildingName, int count); // 建筑偷取事件槽函数
    void onMinerLossDetected(const QString& playerName, const QString& minerType, int count); // 矿车丢失事件槽函数

};

#endif // OB_H
