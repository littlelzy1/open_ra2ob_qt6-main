#ifndef OBTEAM_H
#define OBTEAM_H

#include <QWidget>
#include <QLabel>
#include <QPixmap>
#include <QTimer>
#include <QPainterPath>
#include <QMap>
#include <QVector>
#include "./Ra2ob/src/Game.hpp"
#include "./globalsetting.h"
#include "./qoutlinelabel.h"
#include <QHash>
#include <QParallelAnimationGroup>
#include <QPropertyAnimation>
#include <QGraphicsEffect>
#include <QPauseAnimation>
#include <QSequentialAnimationGroup>
#include <QGraphicsOpacityEffect>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>

// 前向声明
class PlayerStatusTracker;
class BuildingDetector;
class TheftAlertManager;

// 定义电力状态枚举
enum class PowerState {
    Normal,   // 正常 - 绿色
    Warning,  // 警告 - 黄色
    Critical  // 危急 - 红色
};

class ObTeam : public QWidget {
    Q_OBJECT

    // 添加属性动画支持
    Q_PROPERTY(qreal panelAnimProgress READ getPanelAnimProgress WRITE setPanelAnimProgress)
    Q_PROPERTY(qreal elementsAnimProgress READ getElementsAnimProgress WRITE setElementsAnimProgress)
    Q_PROPERTY(qreal glowAnimProgress READ getGlowAnimProgress WRITE setGlowAnimProgress)

public:
    explicit ObTeam(QWidget *parent = nullptr);
    ~ObTeam();

    // 获取游戏实例指针
    Ra2ob::Game* getGamePtr() { return &g; }
    
    // 设置玩家信息布局
    void setPlayerNameFont(const QFont& font) { playerNameFont = font; update(); }
    void setEconomyFont(const QFont& font) { economyFont = font; update(); }
    void setPlayerNameOffset(int x, int y) { playerNameOffsetX = x; playerNameOffsetY = y; update(); }
    void setEconomyOffset(int x, int y) { economyOffsetX = x; economyOffsetY = y; update(); }
    void setUnitCountFont(const QFont& font) { unitCountFont = font; update(); }

    // 添加显示窗口的公共方法
    void showObTeam() { this->show(); }
    
    // 添加获取窗口可见性状态的方法
    bool isObTeamVisible() const { return this->isVisible(); }
    
    // 切换窗口可见性
    void toggleVisibility() { this->isVisible() ? this->hide() : this->show(); }
    
    // 设置BO数
    void setBONumber(int boNumber);

    // 更新队伍比分显示
    void updateTeamScores();
    // 设置队伍信息
    void setTeamInfo(const QString &team1Members, const QString &team2Members);
    // 获取队伍ID
    QString getTeam1Id() const { return team1Id; }
    QString getTeam2Id() const { return team2Id; }

    // 动画属性访问器
    qreal getPanelAnimProgress() const;
    void setPanelAnimProgress(qreal value);
    qreal getElementsAnimProgress() const;
    void setElementsAnimProgress(qreal value);
    qreal getGlowAnimProgress() const;
    void setGlowAnimProgress(qreal value);

protected:
    void paintEvent(QPaintEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override; // 重写鼠标移动事件

private slots:
    // 游戏检测定时器超时处理函数
    void onGameTimerTimeout();
    
    // 检查团队状态
    void checkTeamStatus();
    
    // 根据游戏结果更新团队分数
    void updateTeamScoreFromGameResult();
    void forceMemoryCleanup(); // 添加强制内存清理方法
    
    // 检测鼠标位置是否在建造栏区域
    void checkMousePositionForProducingBlocks();
    
    // 重置头像加载状态，允许重新加载
    void resetAvatarLoadStatus();
    
    // 建筑偷取检测槽函数
    void onBuildingTheftDetected(const QString& thiefPlayerName, const QString& victimPlayerName, const QString& buildingName, int count);
    
    // 矿车丢失检测槽函数
    void onMinerLossDetected(const QString& playerName, const QString& minerType, int count);

private:
    // 检查是否应该启动动画
    void checkForAnimationStart();
    
    // 绘制顶部面板
    void paintTopPanel(QPainter *painter);
    
    // 绘制右侧面板
    void paintRightPanel(QPainter *painter);
    
    // 绘制玩家名称
    void paintPlayerNames(QPainter *painter);
    
    // 绘制国旗
    void paintCountryFlags(QPainter *painter);
    
    // 绘制单位信息
    void paintUnits(QPainter *painter);
    
    // 绘制电力状态
    void paintPowerStatus(QPainter *painter);
    
    // 绘制队伍经济长条
    void paintTeamEconomyBars(QPainter *painter);
    
    // 绘制玩家颜色背景图片
    void paintPlayerColorBgs(QPainter *painter);
    
    // 绘制地图名称
    void paintMapName(QPainter *painter);
    
    // 获取玩家电力状态
    PowerState getPlayerPowerState(int playerIndex);
    
    // 检测玩家队伍关系，返回值表示是否成功检测到队伍
    bool detectTeams();
    
    // 间谍渗透相关函数
    void initSpyInfiltrationLabels();      // 初始化间谍渗透标签
    void resetSpyInfiltrationStatus();     // 重置间谍渗透状态跟踪变量
    void updateSpyInfiltrationDisplay();   // 更新间谍渗透显示
    void showSpyAlert(int playerIndex, const QString& spyType); // 显示间谍渗透提示

    // 设置屏幕布局
    void setLayoutByScreen();

    // 检查游戏是否真的结束
    bool isGameReallyOver();

    // 检测游戏状态
    void detectGame();

    // 控制窗口显示/隐藏
    void toggleOb();
    
    // 清理单位显示
    void clearUnitDisplay();
    
    // 根据颜色代码获取对应的背景图片 (功能已移除)
    QPixmap getPlayerColorBackground(const std::string& color);
    
    // 根据国家名称获取对应的国旗图片
    QPixmap getCountryFlag(const QString& country);

    // 获取单位图标
    QPixmap getUnitIcon(const QString& unitName);

    // 获取圆角图片
    QPixmap getRadiusPixmap(const QPixmap& src, int radius);

    // 绘制建造栏
    void paintProducingBlocks(QPainter *painter);

    // 绘制建造栏辅助方法
    void drawProducingBlock(QPainter *painter, const Ra2ob::tagBuildingNode& building, 
                          int x, int y, const QColor& color, float ratio);

    // 获取玩家颜色背景图片
    QPixmap getPlayerColorImage(const std::string& color);
    


    Ra2ob::Game &g;            // 游戏实例
    Globalsetting *gls;        // 全局设置
    QTimer *gameTimer;         // 游戏检测定时器
    QTimer *powerBlinkTimer;   // 电力状态闪烁定时器
    bool powerBlinkState;      // 电力闪烁状态
    
    // 建造栏显示控制
    bool showProducingBlocks;           // 控制建造栏显示状态

    QTimer *producingBlocksTimer;       // 定时检测鼠标位置的定时器
    int team1DisplayedBuildings;        // 团队1实际显示的建造项目数量
    int team2DisplayedBuildings;        // 团队2实际显示的建造项目数量
    
    // BO数相关
    int boNumber = 5;          // BO数，默认为5
    QOutlineLabel* lb_bo_number; // BO数标签
    
    // 图片资源
    QPixmap topBg1Image;       // 2v2顶部背景图1
    QPixmap topBg3Image;       // 2v2顶部背景图3
    QPixmap rightBarImage;     // 右侧栏背景图
    
    // 颜色背景图片尺寸和位置
    int colorBgWidth = 219;        // 颜色背景图片宽度
    int colorBgHeight = 75;        // 颜色背景图片高度
    int player1ColorBgX = 10;      // 第一位玩家颜色背景X偏移
    int player1ColorBgY = 0;       // 第一位玩家颜色背景Y偏移
    int player2ColorBgX = 229;     // 第二位玩家颜色背景X偏移
    int player2ColorBgY = 0;       // 第二位玩家颜色背景Y偏移
    
    // 国旗图片缓存
    QMap<QString, QPixmap> flagCache;
    
    // 单位图标缓存
    QMap<QString, QPixmap> unitIconCache;
    
    // 颜色背景图片缓存
    QMap<QString, QPixmap> colorBgCache;
    
    // 游戏状态
    bool lastGameValid = false;        // 上次游戏有效状态
    bool gameFinished = false;         // 当前游戏是否已结束处理
    bool teamStatusTracking = false;   // 是否正在跟踪团队状态
    bool teamsDetected = false;        // 是否已检测过队伍关系
    bool animationPlayedForCurrentGame = false; // 当前游戏是否已播放过动画
    QTimer *teamStatusTimer = nullptr; // 团队状态跟踪计时器
    QTimer *memoryCleanupTimer = nullptr; // 内存清理定时器
    bool enoughPlayersDetected = false;  // 新增：标记是否已经检测到足够的玩家（4位或以上）
    
    // 队伍数据
    QMap<int, QList<int>> teamPlayers;  // 按队伍号分组的玩家索引
    QList<int> uniqueTeams;             // 唯一的队伍号列表
    int team1Index = -1;                  // 队伍1的索引
    int team2Index = -1;                  // 队伍2的索引
    QList<int> team1Players;            // 队伍1的玩家列表
    QList<int> team2Players;            // 队伍2的玩家列表
    
    // 玩家信息布局配置
    QFont playerNameFont;               // 玩家名称字体
    QFont economyFont;                  // 经济信息字体
    QFont unitCountFont;                // 单位数量字体
    QFont producingStatusFont;           // 建造状态字体
    QFont mapNameFont;                  // 地图名称字体
    
    // 单独的玩家偏移量设置
    int player1NameOffsetX = 14;         // 第一位玩家名称X偏移
    int player1NameOffsetY = 0;          // 第一位玩家名称Y偏移
    int player1BalanceOffsetX = 15;      // 第一位玩家余额X偏移
    int player1BalanceOffsetY = 60;      // 第一位玩家余额Y偏移
    int player1SpentOffsetX = 85;        // 第一位玩家总花费X偏移
    int player1SpentOffsetY = 60;        // 第一位玩家总花费Y偏移
    int player1KillsOffsetX = 144;       // 第一位玩家摧毁数X偏移
    int player1KillsOffsetY = 57;        // 第一位玩家摧毁数Y偏移
    int player1PowerOffsetX = 170;       // 第一位玩家电力指示灯X偏移
    int player1PowerOffsetY = 65;        // 第一位玩家电力指示灯Y偏移
    
    int player2NameOffsetX = 15;         // 第二位玩家名称X偏移
    int player2NameOffsetY = 0;          // 第二位玩家名称Y偏移
    int player2BalanceOffsetX = 15;      // 第二位玩家余额X偏移
    int player2BalanceOffsetY = 60;      // 第二位玩家余额Y偏移
    int player2SpentOffsetX = 85;        // 第二位玩家总花费X偏移
    int player2SpentOffsetY = 60;        // 第二位玩家总花费Y偏移
    int player2KillsOffsetX = 365;       // 第二位玩家摧毁数X偏移
    int player2KillsOffsetY = 57;        // 第二位玩家摧毁数Y偏移
    int player2PowerOffsetX = 170;       // 第二位玩家电力指示灯X偏移
    int player2PowerOffsetY = 65;        // 第二位玩家电力指示灯Y偏移
    
    // 通用偏移量（用于其他玩家）
    int playerNameOffsetX = 0;          // 玩家名称X偏移
    int playerNameOffsetY = 0;          // 玩家名称Y偏移
    int economyOffsetX = 0;             // 经济信息X偏移
    int economyOffsetY = 0;             // 经济信息Y偏移
    int powerIndicatorSize = 16;        // 电力指示灯大小
    
    // 加载状态标志
    bool flagsLoaded = false;           // 国旗是否已加载
    bool unitIconsLoaded = false;        // 单位图标是否已加载
    bool producingIconsLoaded = false;   // 建造图标是否已加载
    bool colorBgsLoaded = false;         // 颜色背景图片是否已加载
    
    // 单位图标尺寸
    static const int unitIconWidth = 70; // 单位图标宽度
    static const int unitIconHeight = 48; // 单位图标高度
    static const int unitSpacing = 5; // 单位图标间距
    
    // 建造栏尺寸
    static const int producingBlockWidth = 70; // 建造栏宽度
    static const int producingBlockHeight = 80; // 建造栏高度
    static const int producingSpacing = 5; // 建造栏间距
    static const int producingDisplayOffsetY = 30; // 建造栏与顶部间距
    
    // 单位显示区域距离国旗底部的偏移量
    int unitDisplayOffsetY = 65;

    // 字体设置
    QString technoGlitchFontFamily; // 特效字体的实际字体族名称
    int killsFontSize = 14;         // 摧毁数量字体大小

    // 队伍比分相关
    QString team1Id; // 格式: A+B (已排序)
    QString team2Id; // 格式: C+D (已排序)
    int team1Score = 0;
    int team2Score = 0;

    // 比分显示标签
    QLabel *team1ScoreLabel = nullptr;
    QLabel *team2ScoreLabel = nullptr;
    QOutlineLabel* lb_mapname = nullptr; // 地图名称标签

    // 初始化比分UI
    void initScoreUI();
    // 计算队伍ID (排序玩家名称)
    QString calculateTeamId(const QString &player1, const QString &player2);

    // 动画系统
    qreal panelAnimProgress;      // 面板垂直滑入进度
    qreal elementsAnimProgress;   // 元素淡入进度
    qreal glowAnimProgress;       // 发光效果进度
    QParallelAnimationGroup *mainAnimGroup;
    QPropertyAnimation *panelSlideAnim;
    QPropertyAnimation *elementsRevealAnim;
    QPropertyAnimation *glowEffectAnim;
    bool animationActive;
    
    // 初始化和清理动画系统
    void initAnimationSystem();
    QGraphicsEffect* createFadeEffect(qreal progress);

    // New methods for avatar support
    QPixmap getPlayerAvatar(int playerIndex);
    QString getPlayerNickname(int playerIndex);
    void loadAvatarFromRemote(int playerIndex, const QString &nickname);
    QPixmap getCountryAvatar(const QString &country);
    
    // Avatar cache and properties
    QMap<QString, QPixmap> avatarCache; // Cache avatars by player nickname
    int avatarSize = 16;  // Default size for avatars (更小的头像尺寸)
    bool avatarsLoaded = false;
    
    // 头像刷新定时器
    QTimer *avatarRefreshTimer = nullptr;
    
    // 网络管理器
    QNetworkAccessManager *networkManager = nullptr;
    
    // 间谍渗透相关成员变量
    QLabel *lb_spyAlert1 = nullptr;        // 玩家1间谍渗透提示标签
    QLabel *lb_spyAlert2 = nullptr;        // 玩家2间谍渗透提示标签
    QLabel *lb_spyAlert3 = nullptr;        // 玩家3间谍渗透提示标签
    QLabel *lb_spyAlert4 = nullptr;        // 玩家4间谍渗透提示标签
    QTimer *spyAlertTimer1 = nullptr;      // 玩家1间谍渗透提示定时器
    QTimer *spyAlertTimer2 = nullptr;      // 玩家2间谍渗透提示定时器
    QTimer *spyAlertTimer3 = nullptr;      // 玩家3间谍渗透提示定时器
    QTimer *spyAlertTimer4 = nullptr;      // 玩家4间谍渗透提示定时器
    QMap<QString, QPixmap> spyInfiltrationCache; // 间谍渗透图片缓存
    int spyAlertWidth = 186;                // 间谍提示图片宽度
    int spyAlertHeight = 29;                // 间谍提示图片高度
    
    // PlayerStatusTracker
    PlayerStatusTracker *statusTracker = nullptr;
    BuildingDetector *buildingDetector = nullptr;
    TheftAlertManager *theftAlertManager = nullptr;
};

#endif // OBTEAM_H