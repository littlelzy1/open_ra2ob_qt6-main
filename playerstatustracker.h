#ifndef PLAYERSTATUSTRACKER_H
#define PLAYERSTATUSTRACKER_H

#include <QObject>
#include <QTimer>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>
#include <QFile>
#include <QDir>
#include <string>
#include <vector>
#include "./Ra2ob/Ra2ob/src/Game.hpp"
#include "./globalsetting.h"

// 前向声明
class Ob1;
class Ob;



// 简化的玩家状态信息结构体
struct PlayerStatusInfo {
    std::string name;       // 玩家名称
    bool isDefeated;        // 是否被击败
    int totalUnits;         // 玩家当前总单位数量
    
    PlayerStatusInfo() : isDefeated(false), totalUnits(0) {}
};

class PlayerStatusTracker : public QObject
{
    Q_OBJECT

public:
    explicit PlayerStatusTracker(QObject *parent = nullptr, Ob1 *ob1Instance = nullptr);
    explicit PlayerStatusTracker(QObject *parent, Ob *obInstance);
    ~PlayerStatusTracker();
    
    // 开始跟踪
    void startTracking();
    
    // 停止跟踪
    void stopTracking();

signals:
    // 添加玩家得分信号
    void playerScoreChanged(const QString &nickname, int newScore);
    


private slots:
    // 更新玩家状态
    void updatePlayerStatus();

private:
    QTimer* statusTimer;          // 状态更新定时器
    bool isTracking;              // 是否正在跟踪
    bool gameFinished;            // 游戏是否已结束
    bool gameProcessEnded;        // 游戏进程是否已结束
    bool autoScoreTriggered;      // 是否已触发自动加分
    
    // 所有玩家的状态信息
    std::vector<PlayerStatusInfo> playerStatus;
    
    // 收集玩家状态信息
    void collectPlayerStatus();
    
    // 保存状态到JSON文件
    void saveStatusToJson();
    
    // 检查是否有玩家失败
    bool checkPlayerDefeat();
    
    // 根据游戏结果给玩家加分
    void updatePlayerScore();
    
    // 更新latest.json文件
    void updateLatestJson();
    
    // 重置单位计数和缓存数据
    void resetUnitCounts();
    
    // 缓存玩家数据（不写入文件）
    void cachePlayerData();
    
    // 缓存玩家名称（在游戏稳定运行时调用）
    void cachePlayerNames();
    
    // 授权检查相关方法
    bool isAuthorizedUser();
    QString getMacAddress();
    QString getCpuId();
    QString getCpuIdViaWMI();
    QString getCpuIdViaIntrinsic();
    QString getCpuIdViaWmic();
    QString generateFallbackId();
    
    // 全局设置实例
    Globalsetting *gls;
    
    // Ob1实例指针，用于获取BO数
    Ob1 *ob1;
    
    // Ob实例指针
    Ob *ob;
    
    // 缓存最后一次有效的玩家数据
    struct LastValidPlayerData {
        QString country;
        int money;
        int kills;
        int stuckTime;
        bool isValid;
        
        // 卡钱时间相关
        bool isStuckMoney;          // 当前是否卡钱（余额<30）
        int stuckStartTime;         // 开始卡钱的游戏时间（秒）
        int totalStuckTime;         // 累计卡钱时间（秒）
        
        // 单位完成次数统计
        QJsonObject unitsCompleted; // 记录每个单位类型完成的累计次数
        QJsonObject lastBuildingState; // 记录上次建造状态，用于检测完成事件
        
        LastValidPlayerData() : money(0), kills(0), stuckTime(0), isValid(false), 
                               isStuckMoney(false), stuckStartTime(0), totalStuckTime(0) {}
    };
    
    std::vector<LastValidPlayerData> lastValidData;
    
    // 玩家名称缓存
    std::vector<std::string> cachedPlayerNames;
    bool playerNamesCached;
    
    // 间谍渗透状态检测相关

    

    
    // 间谍渗透状态检测方法

    

    

};


#endif // PLAYERSTATUSTRACKER_H