#ifndef BUILDINGDETECTOR_H
#define BUILDINGDETECTOR_H

#include <QObject>
#include <QTimer>
#include <QDebug>
#include <unordered_map>
#include <string>
#include <array>
#include <chrono>
#include "membermanager.h"

// 前向声明
namespace Ra2ob {
    class Game;
}

const int MAXPLAYER = 8;

// 建筑快照结构
struct BuildingSnapshot {
    std::unordered_map<std::string, int> buildingCounts; // 建筑名称 -> 数量
    std::unordered_map<std::string, int> minerCounts; // 矿车名称 -> 数量
    int engineerCount = 0; // 工程师数量
    std::chrono::steady_clock::time_point timestamp; // 快照时间戳
    
    void clear() { 
        buildingCounts.clear(); 
        minerCounts.clear();
        engineerCount = 0;
        timestamp = std::chrono::steady_clock::now();
    }
    
    BuildingSnapshot() {
        engineerCount = 0;
        timestamp = std::chrono::steady_clock::now();
    }
};

class BuildingDetector : public QObject
{
    Q_OBJECT

public:
    explicit BuildingDetector(QObject *parent = nullptr);
    ~BuildingDetector();

    // 启动/停止检测
    void startDetection(int intervalMs = 1000);
    void stopDetection();
    
    // 获取当前所有玩家的建筑数量
    void detectAllPlayerBuildings();
    
    // 调试输出功能
    void printBuildingCounts();
    
    // 获取建筑中文名称
    QString getBuildingChineseName(const std::string& unitName);
    
    // 获取矿车中文名称
    QString getMinerChineseName(const std::string& unitName);

signals:
    // 建筑偷取检测信号
    void buildingTheftDetected(const QString& thiefPlayerName, const QString& victimPlayerName, 
                              const QString& buildingName, int count);
    
    // 矿车损失检测信号
    void minerLossDetected(const QString& playerName, const QString& minerName, int count);

private slots:
    void onDetectionTimer();

private:
    QTimer* m_detectionTimer;
    QTimer* m_stealCheckTimer; // 用于延迟检测建筑偷取的定时器
    Ra2ob::Game* m_game;
    MemberManager* m_memberManager; // 会员管理器引用
    
    // 存储每个玩家的建筑快照
    std::array<BuildingSnapshot, MAXPLAYER> m_currentSnapshots;
    std::array<BuildingSnapshot, MAXPLAYER> m_lastSnapshots;
    
    // 待检查的建筑减少事件
    struct BuildingLossEvent {
        int playerIndex;
        std::string buildingName;
        int lostCount;
        std::chrono::steady_clock::time_point eventTime;
    };
    std::vector<BuildingLossEvent> m_pendingLossEvents;
    
    // 更新指定玩家的建筑快照
    void updatePlayerSnapshot(int playerIndex);
    
    // 检查游戏是否有效
    bool isGameValid();
    
    // 判断是否为建筑单位
    bool isBuildingUnit(const std::string& unitName);
    
    // 判断是否为工程师单位
    bool isEngineerUnit(const std::string& unitName);
    
    // 矿车相关检测函数
    bool isMinerUnit(const std::string& unitName);
    
    // 获取指定玩家的工程师数量
    int getEngineerCount(int playerIndex);
    
    // 检测建筑偷取
    void detectBuildingTheft();
    
    // 矿车损失检测
    void detectMinerLoss();
    
    // 检查延迟的建筑偷取事件
    void checkDelayedTheftEvents();
};

#endif // BUILDINGDETECTOR_H