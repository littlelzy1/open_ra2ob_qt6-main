#include "buildingdetector.h"
#include "Ra2ob/Ra2ob/src/Game.hpp"
#include <QDebug>
#include <set>
#include <vector>
#include <map>

BuildingDetector::BuildingDetector(QObject *parent)
    : QObject(parent)
    , m_detectionTimer(new QTimer(this))
    , m_stealCheckTimer(new QTimer(this))
    , m_game(&Ra2ob::Game::getInstance())
    , m_memberManager(&MemberManager::getInstance())
{
    // 连接定时器信号
    connect(m_detectionTimer, &QTimer::timeout, this, &BuildingDetector::onDetectionTimer);
    connect(m_stealCheckTimer, &QTimer::timeout, this, &BuildingDetector::checkDelayedTheftEvents);
    
    // 设置偷取检测定时器为单次触发
    m_stealCheckTimer->setSingleShot(true);
    
    qDebug() << "BuildingDetector initialized";
}

BuildingDetector::~BuildingDetector()
{
    stopDetection();
    if (m_stealCheckTimer->isActive()) {
        m_stealCheckTimer->stop();
    }
    qDebug() << "BuildingDetector destroyed";
}

void BuildingDetector::startDetection(int intervalMs)
{
    if (!m_detectionTimer->isActive()) {
        // 设置检测间隔为500ms用于矿车检测
        m_detectionTimer->start(500);
        m_stealCheckTimer->start(1000); // 偷取检测保持1秒间隔
        qDebug() << "Building detection started with interval:" << 500 << "ms";
    }
}

void BuildingDetector::stopDetection()
{
    if (m_detectionTimer->isActive()) {
        m_detectionTimer->stop();
        qDebug() << "Building detection stopped";
    }
}

bool BuildingDetector::isGameValid()
{
    return m_game && m_game->_gameInfo.valid && !m_game->_gameInfo.isGameOver;
}

bool BuildingDetector::isBuildingUnit(const std::string& unitName)
{
    // 定义建筑类型的单位名称列表（基于unit_offsets.json中的Building数组）
    static const std::set<std::string> buildingUnits = {
        // 苏军建筑
        "Soviet War Factory", "Tesla Reactor", "Soviet Barracks",
        "Soviet Ore Refinery", "Radar Tower", "Soviet Battle Lab", "Nuclear Reactor",
        "Soviet Cloning Vat", "Soviet Service Depot", "Soviet Construction Yard",
        "Soviet Psychic Sensor", "Industrial Plant",
        
        // 盟军建筑
        "Allied War Factory", "Power Plant", "Allied Barracks",
        "Allied Ore Refinery", "Airforce Command Headquarters (American)",
        "Airforce Command Headquarters", "Allied Battle Lab", "Ore Purifier",
        "Allied Service Depot", "Allied Construction Yard",
        "Spy Satellite Uplink", "Robot Control Center",
        
        // 尤里建筑
        "Yuri War Factory", "Bio Reactor", "Yuri Barracks",
        "Yuri Psychic Radar", "Grinder", "Yuri Battle Lab",
        "Yuri Cloning Vats",
        
        // 中立/科技建筑
        "Oil Derrick"
    };
    
    return buildingUnits.find(unitName) != buildingUnits.end();
}

bool BuildingDetector::isEngineerUnit(const std::string& unitName)
{
    // 定义工程师单位名称列表
    static const std::set<std::string> engineerUnits = {
        "Allied Engineer",
        "Soviet Engineer", 
        "Yuri Engineer"
    };
    
    return engineerUnits.find(unitName) != engineerUnits.end();
}

bool BuildingDetector::isMinerUnit(const std::string& unitName)
{
    // 定义矿车单位名称列表
    static const std::set<std::string> minerUnits = {
     "Slave Miner Deployed",
        "Slave Miner", 
        "Chrono Miner",
        "War Miner"
    };
    
    return minerUnits.find(unitName) != minerUnits.end();
}

int BuildingDetector::getEngineerCount(int playerIndex)
{
    if (playerIndex < 0 || playerIndex >= MAXPLAYER) {
        return 0;
    }
    
    const auto& player = m_game->_gameInfo.players[playerIndex];
    if (!player.valid) {
        return 0;
    }
    
    int totalEngineers = 0;
    for (const auto& unit : player.units.units) {
        if (!unit.unitName.empty() && unit.num > 0) {
            if (isEngineerUnit(unit.unitName)) {
                totalEngineers += unit.num;
            }
        }
    }
    
    return totalEngineers;
}

QString BuildingDetector::getBuildingChineseName(const std::string& unitName)
{
    static const std::unordered_map<std::string, QString> buildingNameMap = {
        // 基地建筑
        {"Allied Construction Yard", "盟军基地"},
        {"Soviet Construction Yard", "苏军基地"},
        {"Yuri Construction Yard", "尤里基地"},
        
        // 兵营
        {"Allied Barracks", "盟军兵营"},
        {"Soviet Barracks", "苏军兵营"},
        {"Yuri Barracks", "尤里兵营"},
        
        // 重工
        {"Allied War Factory", "盟军重工"},
        {"Soviet War Factory", "苏军重工"},
        {"Yuri War Factory", "尤里重工"},
        
        // 矿厂
        {"Allied Ore Refinery", "盟军矿厂"},
        {"Soviet Ore Refinery", "苏军矿厂"},
        {"Yuri Ore Refinery", "尤里矿厂"},
        
        // 电厂
        {"Power Plant", "盟军电厂"},
        {"Tesla Reactor", "苏军电厂"},
        {"Bio Reactor", "生化反应炉"},
        {"Nuclear Reactor", "核子反应炉"},
        
        // 雷达
        {"Radar Tower", "雷达"},
        {"Spy Satellite Uplink", "间谍卫星"},
        {"Yuri Psychic Radar", "尤里心灵雷达"},
        
        // 高科
        {"Allied Battle Lab", "盟军高科"},
        {"Soviet Battle Lab", "苏军高科"},
        {"Yuri Battle Lab", "尤里高科"},
        
        // 特殊建筑
        {"Ore Purifier", "矿石精炼器"},
        {"Industrial Plant", "工业工厂"},
        {"Grinder", "部队回收厂"},
        {"Robot Control Center", "控制中心"},
        {"Soviet Cloning Vat", "苏军复制中心"},
        {"Soviet Service Depot", "苏军维修厂"},
        {"Soviet Psychic Sensor", "苏军心灵探测器"},
        {"Allied Service Depot", "盟军维修厂"},
        {"Yuri Cloning Vats", "尤里复制中心"},
        
        // 空指部
        {"Airforce Command Headquarters (American)", "美国空指部"},
        {"Airforce Command Headquarters", "空指部"},
        
  
        
        // 中立建筑
        {"Oil Derrick", "油井"}
    };
    
    auto it = buildingNameMap.find(unitName);
    return it != buildingNameMap.end() ? it->second : QString::fromStdString(unitName);
}

QString BuildingDetector::getMinerChineseName(const std::string& unitName)
{
    static const std::unordered_map<std::string, QString> minerNameMap = {
        {"Slave Miner", "奴隶矿车"}, // 合并后统一显示为"奴隶矿车"
        {"Chrono Miner", "牛车"},
        {"War Miner", "牛车"}
    };
    
    auto it = minerNameMap.find(unitName);
    return it != minerNameMap.end() ? it->second : QString::fromStdString(unitName);
}

void BuildingDetector::updatePlayerSnapshot(int playerIndex)
{
    if (playerIndex < 0 || playerIndex >= MAXPLAYER) {
        return;
    }
    
    const auto& player = m_game->_gameInfo.players[playerIndex];
    if (!player.valid) {
        m_currentSnapshots[playerIndex].clear();
        return;
    }
    
    // 清空当前快照
    m_currentSnapshots[playerIndex].clear();
    
    // 遍历该玩家的单位列表，筛选出建筑类型、工程师和矿车
    for (const auto& unit : player.units.units) {
        if (!unit.unitName.empty() && unit.num > 0) {
            // 判断是否为建筑类型（通过单位名称判断）
            if (isBuildingUnit(unit.unitName)) {
                m_currentSnapshots[playerIndex].buildingCounts[unit.unitName] = unit.num;
            }
            // 判断是否为工程师单位
            else if (isEngineerUnit(unit.unitName)) {
                m_currentSnapshots[playerIndex].engineerCount += unit.num;
            }
            // 判断是否为矿车单位
            else if (isMinerUnit(unit.unitName)) {
                // 将奴隶矿车的两种状态视为同一种单位
                std::string minerKey = unit.unitName;
                if (unit.unitName == "Slave Miner Deployed" || unit.unitName == "Slave Miner") {
                    minerKey = "Slave Miner"; // 统一使用"Slave Miner"作为键
                }
                m_currentSnapshots[playerIndex].minerCounts[minerKey] += unit.num;
            }
        }
    }
    
    // 更新时间戳
    m_currentSnapshots[playerIndex].timestamp = std::chrono::steady_clock::now();
}

void BuildingDetector::detectAllPlayerBuildings()
{
    if (!isGameValid()) {
        return;
    }
    
    // 更新所有玩家的建筑快照
    for (int i = 0; i < MAXPLAYER; i++) {
        updatePlayerSnapshot(i);
    }
    
    // 检测建筑偷取
    detectBuildingTheft();
}

void BuildingDetector::printBuildingCounts()
{
    if (!isGameValid()) {
        qDebug() << "Game is not valid or not running";
        return;
    }
    
    qDebug() << "=== Building Counts Debug Info ===";
    qDebug() << "Game Time:" << m_game->_gameInfo.realGameTime << "seconds";
    qDebug() << "Map:" << QString::fromStdString(m_game->_gameInfo.mapNameUtf);
    
    for (int i = 0; i < MAXPLAYER; i++) {
        const auto& player = m_game->_gameInfo.players[i];
        if (!player.valid) {
            continue;
        }
        
        QString playerName = QString::fromStdString(player.panel.playerNameUtf);
        if (playerName.isEmpty()) {
            playerName = QString::fromStdString(player.panel.playerName);
        }
        
        qDebug() << "Player" << i << "(" << playerName << "):";
        
        const auto& snapshot = m_currentSnapshots[i];
        if (snapshot.buildingCounts.empty()) {
            qDebug() << "  No buildings found";
        } else {
            for (const auto& [buildingName, count] : snapshot.buildingCounts) {
                qDebug() << "  " << QString::fromStdString(buildingName) << ":" << count;
            }
        }
        
        // 显示总建筑数量
        int totalBuildings = 0;
        for (const auto& [name, count] : snapshot.buildingCounts) {
            totalBuildings += count;
        }
        qDebug() << "  Total buildings:" << totalBuildings;
        qDebug() << "";
    }
    
    qDebug() << "=== End Building Counts ===";
}

void BuildingDetector::onDetectionTimer()
{
    // 保存上一次的快照
    m_lastSnapshots = m_currentSnapshots;
    
    // 检测当前所有玩家的建筑
    detectAllPlayerBuildings();
    
    // 检测建筑偷取事件
    detectBuildingTheft();
    
    // 检测矿车损失事件
    detectMinerLoss();
    
    // 输出调试信息
    printBuildingCounts();
}

void BuildingDetector::detectBuildingTheft()
{
    if (!isGameValid()) {
        return;
    }
    
    auto currentTime = std::chrono::steady_clock::now();
    
    // 检查每个玩家的建筑数量变化
    for (int playerIdx = 0; playerIdx < MAXPLAYER; playerIdx++) {
        const auto& player = m_game->_gameInfo.players[playerIdx];
        if (!player.valid) {
            continue;
        }
        
        const auto& currentSnapshot = m_currentSnapshots[playerIdx];
        const auto& lastSnapshot = m_lastSnapshots[playerIdx];
        
        // 比较建筑数量变化
        for (const auto& [buildingName, lastCount] : lastSnapshot.buildingCounts) {
            auto currentIt = currentSnapshot.buildingCounts.find(buildingName);
            int currentCount = (currentIt != currentSnapshot.buildingCounts.end()) ? currentIt->second : 0;
            
            // 如果建筑数量减少了
            if (currentCount < lastCount) {
                int lostCount = lastCount - currentCount;
                
                // 检查是否已经存在相同的丢失事件（避免重复添加）
                bool alreadyExists = false;
                for (const auto& existingEvent : m_pendingLossEvents) {
                    if (existingEvent.playerIndex == playerIdx && 
                        existingEvent.buildingName == buildingName &&
                        existingEvent.lostCount == lostCount) {
                        alreadyExists = true;
                        break;
                    }
                }
                
                if (!alreadyExists) {
                    // 记录建筑丢失事件
                    BuildingLossEvent lossEvent;
                    lossEvent.playerIndex = playerIdx;
                    lossEvent.buildingName = buildingName;
                    lossEvent.lostCount = lostCount;
                    lossEvent.eventTime = currentTime;
                    
                    m_pendingLossEvents.push_back(lossEvent);
                    
                    qDebug() << "检测到建筑减少 - 玩家" << playerIdx 
                             << "丢失了" << lostCount << "个" 
                             << QString::fromStdString(buildingName);
                    
                    // 启动200ms延迟检测
                    if (!m_stealCheckTimer->isActive()) {
                        m_stealCheckTimer->start(200);
                    }
                }
            }
        }
    }
}

void BuildingDetector::detectMinerLoss()
{
    if (!isGameValid()) {
        return;
    }
    
    // 遍历所有玩家
    for (int playerIndex = 0; playerIndex < MAXPLAYER; playerIndex++) {
        const auto& player = m_game->_gameInfo.players[playerIndex];
        if (!player.valid) {
            continue;
        }
        
        // 获取当前和上一次的矿车快照
        const auto& currentMinerCounts = m_currentSnapshots[playerIndex].minerCounts;
        const auto& lastMinerCounts = m_lastSnapshots[playerIndex].minerCounts;
        
        // 检查每种矿车类型的数量变化
        for (const auto& [minerName, currentCount] : currentMinerCounts) {
            int lastCount = 0;
            auto lastIt = lastMinerCounts.find(minerName);
            if (lastIt != lastMinerCounts.end()) {
                lastCount = lastIt->second;
            }
            
            // 如果矿车数量减少，检查会员权限后发出信号
            if (currentCount < lastCount) {
                int lossCount = lastCount - currentCount;
                QString playerName = QString::fromStdString(player.panel.playerNameUtf);
                if (playerName.isEmpty()) {
                    playerName = QString::fromStdString(player.panel.playerName);
                }
                QString minerChineseName = getMinerChineseName(minerName);
                
                qDebug() << "Miner loss detected:" << playerName << "lost" << lossCount << minerChineseName;
                
                // 检查会员权限
                if (m_memberManager->hasFeatureAccess(MemberManager::FeatureType::MinerLossDetection)) {
                    emit minerLossDetected(playerName, minerChineseName, lossCount);
                } else {
                    qDebug() << "矿车损失检测功能需要会员权限，信号未发射";
                }
            }
        }
        
        // 检查在上一次快照中存在但当前快照中不存在的矿车类型
        for (const auto& [minerName, lastCount] : lastMinerCounts) {
            if (currentMinerCounts.find(minerName) == currentMinerCounts.end() && lastCount > 0) {
                QString playerName = QString::fromStdString(player.panel.playerNameUtf);
                if (playerName.isEmpty()) {
                    playerName = QString::fromStdString(player.panel.playerName);
                }
                QString minerChineseName = getMinerChineseName(minerName);
                
                qDebug() << "Miner loss detected:" << playerName << "lost" << lastCount << minerChineseName;
                
                // 检查会员权限
                if (m_memberManager->hasFeatureAccess(MemberManager::FeatureType::MinerLossDetection)) {
                    emit minerLossDetected(playerName, minerChineseName, lastCount);
                } else {
                    qDebug() << "矿车损失检测功能需要会员权限，信号未发射";
                }
            }
        }
    }
}

void BuildingDetector::checkDelayedTheftEvents()
{
    if (!isGameValid() || m_pendingLossEvents.empty()) {
        m_pendingLossEvents.clear();
        return;
    }
    
    auto currentTime = std::chrono::steady_clock::now();
    
    // 检查每个待处理的建筑丢失事件
    for (auto it = m_pendingLossEvents.begin(); it != m_pendingLossEvents.end();) {
        const auto& lossEvent = *it;
        
        // 检查是否超过了检测时间窗口（1秒）
        auto timeDiff = std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - lossEvent.eventTime);
        if (timeDiff.count() > 1000) {
            it = m_pendingLossEvents.erase(it);
            continue;
        }
        
        // 检查其他玩家是否获得了相同类型和数量的建筑
        bool theftDetected = false;
        for (int playerIdx = 0; playerIdx < MAXPLAYER; playerIdx++) {
            if (playerIdx == lossEvent.playerIndex) {
                continue; // 跳过丢失建筑的玩家
            }
            
            const auto& player = m_game->_gameInfo.players[playerIdx];
            if (!player.valid) {
                continue;
            }
            
            const auto& currentSnapshot = m_currentSnapshots[playerIdx];
            const auto& lastSnapshot = m_lastSnapshots[playerIdx];
            
            // 检查这个玩家是否获得了相同的建筑
            auto currentIt = currentSnapshot.buildingCounts.find(lossEvent.buildingName);
            auto lastIt = lastSnapshot.buildingCounts.find(lossEvent.buildingName);
            
            int currentCount = (currentIt != currentSnapshot.buildingCounts.end()) ? currentIt->second : 0;
            int lastCount = (lastIt != lastSnapshot.buildingCounts.end()) ? lastIt->second : 0;
            
            if (currentCount > lastCount) {
                int gainedCount = currentCount - lastCount;
                
                // 如果获得的建筑数量匹配丢失的数量，则认为发生了偷取
                if (gainedCount >= lossEvent.lostCount) {
                    // 检查偷取玩家的工程师数量是否减少了至少1个
                    int thiefPlayerCurrentEngineers = m_currentSnapshots[playerIdx].engineerCount;
                    int thiefPlayerLastEngineers = m_lastSnapshots[playerIdx].engineerCount;
                    int thiefEngineerLoss = thiefPlayerLastEngineers - thiefPlayerCurrentEngineers;
                    
                    // 只有当偷取玩家的工程师数量减少至少1个时，才认为是偷取事件
                    if (thiefEngineerLoss >= 1) {
                        QString lossPlayerName = QString::fromStdString(m_game->_gameInfo.players[lossEvent.playerIndex].panel.playerNameUtf);
                        if (lossPlayerName.isEmpty()) {
                            lossPlayerName = QString::fromStdString(m_game->_gameInfo.players[lossEvent.playerIndex].panel.playerName);
                        }
                        
                        QString gainPlayerName = QString::fromStdString(player.panel.playerNameUtf);
                        if (gainPlayerName.isEmpty()) {
                            gainPlayerName = QString::fromStdString(player.panel.playerName);
                        }
                        
                        qDebug() << "=== 检测到建筑偷取事件! ===";
                        qDebug() << "被偷玩家:" << lossPlayerName << "(玩家" << lossEvent.playerIndex << ")";
                        qDebug() << "偷取玩家:" << gainPlayerName << "(玩家" << playerIdx << ")";
                        qDebug() << "偷取建筑:" << QString::fromStdString(lossEvent.buildingName);
                        qDebug() << "偷取数量:" << lossEvent.lostCount;
                        qDebug() << "偷取玩家工程师减少数量:" << thiefEngineerLoss;
                        qDebug() << "游戏时间:" << m_game->_gameInfo.realGameTime << "秒";
                        qDebug() << "========================";
                        
                        // 检查会员权限
                        if (m_memberManager->hasFeatureAccess(MemberManager::FeatureType::BuildingTheftDetection)) {
                            // 发射建筑偷取事件信号
                            emit buildingTheftDetected(gainPlayerName, lossPlayerName, 
                                                     QString::fromStdString(lossEvent.buildingName), 
                                                     lossEvent.lostCount);
                        } else {
                            qDebug() << "建筑偷取检测功能需要会员权限，信号未发射";
                        }
                        
                        theftDetected = true;
                        break;
                    } else {
                        qDebug() << "建筑转移但偷取玩家工程师数量未减少，不认为是偷取事件 - 偷取玩家" << playerIdx 
                                 << "建筑:" << QString::fromStdString(lossEvent.buildingName)
                                 << "偷取玩家工程师变化:" << thiefEngineerLoss;
                    }
                }
            }
        }
        
        if (theftDetected) {
            it = m_pendingLossEvents.erase(it);
        } else {
            ++it;
        }
    }
    
    // 如果还有待处理的事件，继续检测
    if (!m_pendingLossEvents.empty()) {
        m_stealCheckTimer->start(50); // 50ms后再次检查
    }
}