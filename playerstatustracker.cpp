#include "playerstatustracker.h"
#include "ob1.h"
#include "ob.h"
#include "Ra2ob/Ra2ob/src/Constants.hpp"
#include <QDebug>
#include <QDateTime>
#include <QCoreApplication>
#include <QProcess>
#include <QTimer>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>
#include <QFile>
#include <QCryptographicHash>
#include <QSysInfo>
#include <QNetworkInterface>

#ifdef Q_OS_WIN
#include <Windows.h>
#include <intrin.h>
#include <wbemidl.h>
#include <comdef.h>
#include <Oleauto.h>
#pragma comment(lib, "wbemuuid.lib")
#pragma comment(lib, "oleaut32.lib")
#pragma comment(lib, "ole32.lib")
#endif

#include <vector>
#include <cstring>

PlayerStatusTracker::PlayerStatusTracker(QObject *parent, Ob1 *ob1Instance) : QObject(parent), ob1(ob1Instance), ob(nullptr)
{
    // 初始化状态更新定时器
    statusTimer = new QTimer(this);
    connect(statusTimer, &QTimer::timeout, this, &PlayerStatusTracker::updatePlayerStatus);
    
    isTracking = false;
    gameFinished = false;
    gameProcessEnded = false;
    autoScoreTriggered = false;
    
    // 获取全局设置实例
    gls = &Globalsetting::getInstance();
    
    // 初始化缓存数组
    lastValidData.resize(Ra2ob::MAXPLAYER);
    
    // 初始化玩家名称缓存
    cachedPlayerNames.resize(Ra2ob::MAXPLAYER);
    playerNamesCached = false;
    


}

PlayerStatusTracker::PlayerStatusTracker(QObject *parent, Ob *obInstance) : QObject(parent), ob1(nullptr), ob(obInstance)
{
    // 初始化状态更新定时器
    statusTimer = new QTimer(this);
    connect(statusTimer, &QTimer::timeout, this, &PlayerStatusTracker::updatePlayerStatus);
    
    isTracking = false;
    gameFinished = false;
    gameProcessEnded = false;
    autoScoreTriggered = false;
    
    // 获取全局设置实例
    gls = &Globalsetting::getInstance();
    
    // 初始化缓存数组
    lastValidData.resize(Ra2ob::MAXPLAYER);
    
    // 初始化玩家名称缓存
    cachedPlayerNames.resize(Ra2ob::MAXPLAYER);
    playerNamesCached = false;
    




}

PlayerStatusTracker::~PlayerStatusTracker()
{
    // 停止定时器
    stopTracking();
}

void PlayerStatusTracker::startTracking()
{
    // 重置状态
    isTracking = true;
    gameFinished = false;
    gameProcessEnded = false;
    autoScoreTriggered = false;
    playerStatus.clear();
    
    // 重置玩家名称缓存
    playerNamesCached = false;
    for (auto& name : cachedPlayerNames) {
        name.clear();
    }
    

    
    // 重置单位计数和缓存数据
    resetUnitCounts();
    
    // 启动定时器
    statusTimer->start(10); // 10毫秒更新一次
    
    // 玩家状态跟踪已启动，已重置单位计数
}

void PlayerStatusTracker::stopTracking()
{
    if (statusTimer) {
        statusTimer->stop();
    }
    isTracking = false;
    // 玩家状态跟踪已停止
}

void PlayerStatusTracker::updatePlayerStatus()
{
    if (!isTracking) {
        return;
    }
    
    // 收集并更新所有玩家的状态
    collectPlayerStatus();
    
    // 在游戏稳定运行时缓存玩家名称
    cachePlayerNames();
    
    // 只缓存数据，不写入文件
    cachePlayerData();
    
    // 获取游戏实例
    Ra2ob::Game& g = Ra2ob::Game::getInstance();
    // 计算游戏时间，确保游戏真正开始
    int gameTime;
    if (g._gameInfo.realGameTime > 0) {
        gameTime = g._gameInfo.realGameTime;
    } else {
        gameTime = g._gameInfo.currentFrame / Ra2ob::GAMESPEED;
    }
    
    // 检查失败状态（原有失败检测 + 单位数量为0检测）
    // 只有在游戏时间超过30秒且游戏未结束时才进行失败检测
    if (!gameFinished && gameTime > 30) {
        bool foundDefeat = false;
        
        // 1. 检查原有的失败标志
        if (checkPlayerDefeat()) {
            foundDefeat = true;
        }
        
        // 2. 检查单位数量为0的失败判定
        if (!foundDefeat) {
            // 游戏时间大于70秒才检测失败
            if (gameTime > 70) {
                for (const auto& player : playerStatus) {
                    if (player.totalUnits == 0 && !player.name.empty()) {
                        foundDefeat = true;
                        break;
                    }
                }
            }
        }
        
        // 如果检测到失败，在游戏状态仍然稳定时立即加分
        if (foundDefeat && g._gameInfo.valid) {
            // 立即执行加分，趁游戏状态还稳定
            updatePlayerScore();
            
            // 标记游戏已结束和已触发自动加分
            gameFinished = true;
            autoScoreTriggered = true;
            
            // 保存状态
            saveStatusToJson();
            
            // 延迟停止跟踪，给加分操作更多时间完成
            QTimer::singleShot(1000, this, [this]() {
                stopTracking();
            });
        }
    }
    
    // 检查游戏进程是否结束
    if (!g._gameInfo.valid) {
        if (!gameProcessEnded) {
            gameProcessEnded = true;
        }
    }
}

bool PlayerStatusTracker::checkPlayerDefeat()
{
    for (const auto& player : playerStatus) {
        if (player.isDefeated) {
            return true;
        }
    }
    return false;
}

void PlayerStatusTracker::collectPlayerStatus()
{
    // 清空之前的状态
    playerStatus.clear();
    
    Ra2ob::Game& g = Ra2ob::Game::getInstance();
    
    // 只有当游戏有效时才收集
    if (!g._gameInfo.valid) {
        return;
    }
    
    // 遍历所有可能的玩家
    for (int i = 0; i < Ra2ob::MAXPLAYER; i++) {
        if (g._players[i]) {
            PlayerStatusInfo info;
            
            // 优先使用缓存的玩家名称
            std::string playerName;
            if (playerNamesCached && !cachedPlayerNames[i].empty()) {
                // 使用缓存的名称
                playerName = cachedPlayerNames[i];
              
            } else {
                // 如果没有缓存，从游戏内存读取
                if (!g._strName.getValueByIndexUtf(i).empty()) {
                    playerName = g._strName.getValueByIndexUtf(i);
                } else {
                    playerName = g._strName.getValueByIndex(i);
                }
            }
            info.name = playerName.empty() ? "Player " + std::to_string(i) : playerName;
            
            // 只获取失败状态
            info.isDefeated = g._playerDefeatFlag[i];
            
            // 获取玩家当前总单位数量
            info.totalUnits = 0;
            if (g._gameInfo.players[i].valid) {
                // 统计所有单位数量
                const auto& unitList = g._gameInfo.players[i].units.units;
                for (const auto& unit : unitList) {
                    info.totalUnits += unit.num;
                }
                
                // 统计建筑数量
                const auto& buildingList = g._gameInfo.players[i].building.list;
                for (const auto& building : buildingList) {
                    info.totalUnits += building.number;
                }
            }
            
            // 添加到状态列表
            playerStatus.push_back(info);
        }
    }
}

void PlayerStatusTracker::updatePlayerScore()
{
    // 检查是否有足够的玩家
    if (playerStatus.size() < 2) {
        return;
    }
    
    // 获取游戏时间，与ob.cpp中的逻辑一致
    Ra2ob::Game& g = Ra2ob::Game::getInstance();
    int gameTime;
    if (g._gameInfo.realGameTime > 0) {
        gameTime = g._gameInfo.realGameTime;
    } else {
        gameTime = g._gameInfo.currentFrame / Ra2ob::GAMESPEED;
    }
    
    // 如果游戏时间少于70秒，不更新比分
    if (gameTime < 70) {
        return;
    }
    
    // 获取全局配置中的玩家昵称
    QString p1_nickname = gls->sb.p1_nickname_cache;
    QString p2_nickname = gls->sb.p2_nickname_cache;
    
    // 改进名称获取逻辑，增加多重匹配策略
    if (p1_nickname.isEmpty() || p2_nickname.isEmpty()) {
        // 策略1: 优先使用我们自己缓存的玩家名称
        if (playerNamesCached && playerStatus.size() >= 2) {
            // 查找对应的缓存名称
            for (int i = 0; i < Ra2ob::MAXPLAYER && i < 2; i++) {
                if (!cachedPlayerNames[i].empty()) {
                    if (i == 0 && p1_nickname.isEmpty()) {
                        p1_nickname = QString::fromStdString(cachedPlayerNames[i]);
                    } else if (i == 1 && p2_nickname.isEmpty()) {
                        p2_nickname = QString::fromStdString(cachedPlayerNames[i]);
                    }
                }
            }
        }
        
        // 策略2: 从当前游戏状态获取
        if ((p1_nickname.isEmpty() || p2_nickname.isEmpty()) && playerStatus.size() >= 2) {
            if (p1_nickname.isEmpty()) {
                p1_nickname = QString::fromStdString(playerStatus[0].name);
            }
            if (p2_nickname.isEmpty()) {
                p2_nickname = QString::fromStdString(playerStatus[1].name);
            }
        }
        
        // 策略3: 实时从游戏实例获取最新名称
        if (p1_nickname.isEmpty() || p2_nickname.isEmpty()) {
            for (int i = 0; i < Ra2ob::MAXPLAYER && i < 2; i++) {
                if (g._players[i]) {
                    std::string playerName;
                    if (!g._strName.getValueByIndexUtf(i).empty()) {
                        playerName = g._strName.getValueByIndexUtf(i);
                    } else {
                        playerName = g._strName.getValueByIndex(i);
                    }
                    
                    if (!playerName.empty() && playerName.find("Player ") != 0) {
                        if (i == 0 && p1_nickname.isEmpty()) {
                            p1_nickname = QString::fromStdString(playerName);
                        } else if (i == 1 && p2_nickname.isEmpty()) {
                            p2_nickname = QString::fromStdString(playerName);
                        }
                    }
                }
            }
        }
    }
    
    // 为未失败的玩家加分
    int playerIndex = 0;
    for (const auto& player : playerStatus) {
        playerIndex++;
  
        
        // 转换为QString
        QString playerName = QString::fromStdString(player.name);
      
        
        // 判定玩家是否失败：原有的失败标志 或 单位数量为0
        bool isPlayerDefeated = player.isDefeated || (player.totalUnits == 0 && !player.name.empty());
      
        
        // 如果玩家未被击败，加分
        if (!isPlayerDefeated) {
          
            
            // 改进名称匹配逻辑，增加模糊匹配
        
            bool isMatchedPlayer = false;
            QString matchedNickname;
            
            // 精确匹配
          
            if (playerName == p1_nickname) {
                isMatchedPlayer = true;
                matchedNickname = p1_nickname;
               
            } else if (playerName == p2_nickname) {
                isMatchedPlayer = true;
                matchedNickname = p2_nickname;
            
            } else {
        
            }
            
            // 模糊匹配（去除空格和大小写差异）
            if (!isMatchedPlayer) {
      
                if (!p1_nickname.isEmpty() && 
                         playerName.trimmed().toLower() == p1_nickname.trimmed().toLower()) {
                    isMatchedPlayer = true;
                    matchedNickname = p1_nickname;
               
                } else if (!p2_nickname.isEmpty() && 
                           playerName.trimmed().toLower() == p2_nickname.trimmed().toLower()) {
                    isMatchedPlayer = true;
                    matchedNickname = p2_nickname;
                
                } else {
           
                }
            }
            
            // 包含匹配（玩家名包含昵称或昵称包含玩家名）
            if (!isMatchedPlayer) {
          
                if (!p1_nickname.isEmpty() && 
                         (playerName.contains(p1_nickname, Qt::CaseInsensitive) || 
                          p1_nickname.contains(playerName, Qt::CaseInsensitive))) {
                    isMatchedPlayer = true;
                    matchedNickname = p1_nickname;
            
                } else if (!p2_nickname.isEmpty() && 
                           (playerName.contains(p2_nickname, Qt::CaseInsensitive) || 
                            p2_nickname.contains(playerName, Qt::CaseInsensitive))) {
                    isMatchedPlayer = true;
                    matchedNickname = p2_nickname;
        
                } else {
            
                }
            }
            
            if (isMatchedPlayer) {
                // 使用匹配到的昵称作为键来更新分数
                int currentScore = gls->playerScores.value(matchedNickname, 0);
                
                // 加1分
                int newScore = currentScore + 1;
                gls->playerScores[matchedNickname] = newScore;
                
                // 发出分数变化信号
                emit playerScoreChanged(matchedNickname, newScore);
            }
        }
    }
    
    // 加分完毕后，保存数据到latest.json文件
    updateLatestJson();
    
    // 注释掉强制结束游戏进程的代码
    /*
#ifdef Q_OS_WIN
    // Windows平台下3秒后强制结束游戏进程
    QTimer::singleShot(4000, []() {
        QProcess::execute("taskkill", QStringList() << "/F" << "/IM" << "gamemd-spawn.exe");
        qDebug() << "已发送强制结束游戏进程命令";
    });
#else
    // 非Windows平台下的处理（如果需要）
    qDebug() << "非Windows平台，跳过强制结束游戏进程";
#endif
    */
}

void PlayerStatusTracker::saveStatusToJson()
{
    // 检查用户授权
    if (!isAuthorizedUser()) {
        return;
    }
    
    QJsonObject rootObj;
    QJsonArray playersArray;
    
    // 生成游戏时间信息
    QDateTime currentTime = QDateTime::currentDateTime();
    rootObj["timestamp"] = currentTime.toString("yyyy-MM-dd HH:mm:ss");
    
    // 添加所有玩家信息
    for (const auto& player : playerStatus) {
        QJsonObject playerObj;
        
        playerObj["name"] = QString::fromStdString(player.name);
        playerObj["isDefeated"] = player.isDefeated;
        
        // 添加玩家当前分数
        QString playerName = QString::fromStdString(player.name);
        int playerScore = gls->playerScores.value(playerName, 0);
        playerObj["score"] = playerScore;
        
        playersArray.append(playerObj);
    }
    
    rootObj["players"] = playersArray;
    
    // 创建JSON文档
    QJsonDocument doc(rootObj);
    
    // 授权用户可以生成文件
    QString statusJsonPath = "./obs_data/player_status.json";
    QFile file(statusJsonPath);
    if (file.open(QIODevice::WriteOnly)) {
        file.write(doc.toJson());
        file.close();
        
        // 通知OBS刷新游戏结束场景
        if (ob1) {
            ob1->refreshBrowserSource("游戏结束");  // 刷新游戏结束浏览器源
        }
    }
}

void PlayerStatusTracker::updateLatestJson()
{
    // 检查用户授权
    if (!isAuthorizedUser()) {
        return;
    }
    
    QString latestJsonPath = "./obs_data/latest.json";
    
    // 读取现有的JSON文件
    QFile file(latestJsonPath);
    QJsonObject rootObj;
    
    if (file.exists() && file.open(QIODevice::ReadOnly)) {
        QByteArray data = file.readAll();
        file.close();
        
        QJsonDocument doc = QJsonDocument::fromJson(data);
        if (doc.isObject()) {
            rootObj = doc.object();
        }
    }
    
    // 立即缓存现有的victory_maps数据
    QMap<QString, QJsonArray> cachedVictoryMaps;
    if (rootObj.contains("players") && rootObj["players"].isArray()) {
        QJsonArray existingPlayers = rootObj["players"].toArray();
        for (const auto& value : existingPlayers) {
            if (value.isObject()) {
                QJsonObject playerObj = value.toObject();
                QString nickname = playerObj["nickname"].toString();
                if (!nickname.isEmpty() && playerObj.contains("victory_maps") && playerObj["victory_maps"].isArray()) {
                    cachedVictoryMaps[nickname] = playerObj["victory_maps"].toArray();
                }
            }
        }
    }
    
    // 获取游戏实例
    Ra2ob::Game& g = Ra2ob::Game::getInstance();
    
    // 更新时间戳
    QDateTime currentTime = QDateTime::currentDateTime();
    rootObj["timestamp"] = currentTime.toString("yyyy-MM-ddTHH:mm:ss");
    
    // 获取地图名称
    QString mapName = QString::fromStdString(g._gameInfo.mapNameUtf);
    if (!mapName.isEmpty()) {
        rootObj["mapName"] = mapName;
    }
    
    // 获取游戏时间（秒数）
    int gameTime;
    if (g._gameInfo.realGameTime > 0) {
        gameTime = g._gameInfo.realGameTime;
    } else {
        gameTime = g._gameInfo.currentFrame / Ra2ob::GAMESPEED;
    }
    
    // 直接使用秒数，不转换格式
    rootObj["gameTime"] = gameTime;
    
    // 从Ob1或Ob实例获取BO数
    int boNumber = 3; // 默认BO3
    if (ob1) {
        boNumber = ob1->getBoNumber();
    } else if (ob) {
        boNumber = ob->getBoNumber();
    }
    rootObj["boNumber"] = boNumber;
    
    // 更新玩家信息
    QJsonArray playersArray;
    
    // 获取缓存的玩家昵称
    QString p1_nickname = gls->sb.p1_nickname_cache;
    QString p2_nickname = gls->sb.p2_nickname_cache;
    
    // 如果缓存为空，从游戏状态获取
    if (p1_nickname.isEmpty() && playerStatus.size() >= 1) {
        p1_nickname = QString::fromStdString(playerStatus[0].name);
    }
    if (p2_nickname.isEmpty() && playerStatus.size() >= 2) {
        p2_nickname = QString::fromStdString(playerStatus[1].name);
    }
    
    // 读取现有的players数组以保留victory_maps等信息
    QJsonArray existingPlayers;
    if (rootObj.contains("players") && rootObj["players"].isArray()) {
        existingPlayers = rootObj["players"].toArray();
    }
    
    // 处理每个玩家
    QStringList playerNicknames = {p1_nickname, p2_nickname};
    
    for (int i = 0; i < playerNicknames.size() && i < Ra2ob::MAXPLAYER; i++) {
        QString nickname = playerNicknames[i];
        if (nickname.isEmpty()) continue;
        
        QJsonObject playerObj;
        
        // 查找现有玩家数据以保留victory_maps
        QJsonObject existingPlayerObj;
        for (const auto& value : existingPlayers) {
            if (value.isObject()) {
                QJsonObject obj = value.toObject();
                if (obj["nickname"].toString() == nickname) {
                    existingPlayerObj = obj;
                    break;
                }
            }
        }
        
        // 获取映射后的玩家名称
        QString playername = gls->findNameByNickname(nickname);
        if (playername.isEmpty()) {
            playername = nickname;
        }
        
        // 基本信息
        playerObj["nickname"] = playername; // 优先使用映射后的playername
        
        // 获取玩家分数
        int score = gls->playerScores.value(nickname, 0);
        playerObj["score"] = score;
        
        // 使用缓存的数据（数据已在cachePlayerData中更新）
        if (lastValidData[i].isValid) {
            playerObj["country"] = lastValidData[i].country.isEmpty() ? "Unknown" : lastValidData[i].country;
            playerObj["money"] = lastValidData[i].money;
            playerObj["kills"] = lastValidData[i].kills;
            playerObj["stuck_time"] = lastValidData[i].stuckTime;
            playerObj["units"] = lastValidData[i].unitsCompleted;
        } else {
                // 如果当前数据无效，使用缓存的最后有效数据
                if (lastValidData[i].isValid) {
                    playerObj["country"] = lastValidData[i].country.isEmpty() ? "Unknown" : lastValidData[i].country;
                    playerObj["money"] = lastValidData[i].money;
                    playerObj["kills"] = lastValidData[i].kills;
                    playerObj["stuck_time"] = lastValidData[i].stuckTime;
                    playerObj["units"] = lastValidData[i].unitsCompleted; // 使用累计完成次数
                } else {
                    // 如果没有缓存数据，设置默认值
                    playerObj["country"] = "Unknown";
                    playerObj["money"] = 0;
                    playerObj["kills"] = 0;
                    playerObj["stuck_time"] = 0;
                    playerObj["units"] = QJsonObject(); // 默认空的units对象
                }
            }
        
        // 获取玩家状态 - 使用与加分逻辑相同的复合判定
        bool isDefeated = false;
        for (const auto& player : playerStatus) {
            if (QString::fromStdString(player.name) == nickname) {
                // 复合判定：原有失败标志 OR 单位数量为0
                isDefeated = player.isDefeated || (player.totalUnits == 0 && !player.name.empty());
                break;
            }
        }
        playerObj["status"] = isDefeated ? "loser" : "winner";
        
        // 使用缓存的victory_maps数据
        QJsonArray victoryMaps;
        if (cachedVictoryMaps.contains(playername)) {
            victoryMaps = cachedVictoryMaps[playername];
        } else if (cachedVictoryMaps.contains(nickname)) {
            victoryMaps = cachedVictoryMaps[nickname];
        }
        
        // 检查当前玩家是否获胜（未被击败） - 使用与加分逻辑相同的复合判定
        bool playerWon = false;
        for (const auto& player : playerStatus) {
            if (QString::fromStdString(player.name) == nickname) {
                // 复合判定：未被原有失败标志标记 AND 单位数量不为0（或名称为空）
                bool isPlayerDefeated = player.isDefeated || (player.totalUnits == 0 && !player.name.empty());
                if (!isPlayerDefeated) {
                    playerWon = true;
                }
                break;
            }
        }
        
        // 如果玩家获胜且地图名称不为空，直接添加到victory_maps（允许重复）
        if (playerWon && !mapName.isEmpty()) {
            victoryMaps.append(mapName);
        }
        
        playerObj["victory_maps"] = victoryMaps;
        
        playersArray.append(playerObj);
    }
    
    rootObj["players"] = playersArray;
    
    // 更新matchInfo
    QJsonObject matchInfo;
    if (rootObj.contains("matchInfo") && rootObj["matchInfo"].isObject()) {
        matchInfo = rootObj["matchInfo"].toObject();
    }
    
    // 更新主播名称和赛事名称
    if (!gls->sb.streamer_name.isEmpty()) {
        matchInfo["streamerName"] = gls->sb.streamer_name;
    }
    if (!gls->sb.event_name.isEmpty()) {
        matchInfo["event_name"] = gls->sb.event_name;
    }
    
    rootObj["matchInfo"] = matchInfo;
    
    // 写入文件
    if (file.open(QIODevice::WriteOnly)) {
        QJsonDocument doc(rootObj);
        file.write(doc.toJson());
        file.close();
        
        // 通知OBS刷新游戏结束场景
        if (ob1) {
            ob1->refreshBrowserSource("游戏结束");  // 刷新游戏结束浏览器源
        } else if (ob) {
            ob->refreshBrowserSource("游戏结束");  // 刷新游戏结束浏览器源
        }
    }
}

void PlayerStatusTracker::resetUnitCounts()
{
    // 重置所有玩家的单位计数和缓存数据
    for (auto& data : lastValidData) {
        data.unitsCompleted = QJsonObject();
        data.lastBuildingState = QJsonObject();
        data.country = "";
        data.money = 0;
        data.kills = 0;
        data.stuckTime = 0;
        data.isValid = false;
        data.isStuckMoney = false;
        data.stuckStartTime = 0;
        data.totalStuckTime = 0;
    }
}

void PlayerStatusTracker::cachePlayerData()
{
    Ra2ob::Game& g = Ra2ob::Game::getInstance();
    
    // 只有当游戏有效时才缓存数据
    if (!g._gameInfo.valid) {
        return;
    }
    
    // 获取缓存的玩家昵称
    QString p1_nickname = gls->sb.p1_nickname_cache;
    QString p2_nickname = gls->sb.p2_nickname_cache;
    
    // 如果缓存为空，从游戏状态获取
    if (p1_nickname.isEmpty() && playerStatus.size() >= 1) {
        p1_nickname = QString::fromStdString(playerStatus[0].name);
    }
    if (p2_nickname.isEmpty() && playerStatus.size() >= 2) {
        p2_nickname = QString::fromStdString(playerStatus[1].name);
    }
    
    // 处理每个玩家
    QStringList playerNicknames = {p1_nickname, p2_nickname};
    
    for (int i = 0; i < playerNicknames.size() && i < Ra2ob::MAXPLAYER; i++) {
        QString nickname = playerNicknames[i];
        if (nickname.isEmpty()) continue;
        
        // 确保缓存数组有足够的元素
        while (lastValidData.size() <= i) {
            lastValidData.push_back(LastValidPlayerData());
        }
        
        // 根据昵称找到游戏中的实际玩家索引
        int actualPlayerIndex = -1;
        if (g._gameInfo.valid) {
            for (int j = 0; j < Ra2ob::MAXPLAYER; j++) {
                if (g._gameInfo.players[j].valid) {
                    QString gameNickname = QString::fromUtf8(g._gameInfo.players[j].panel.playerNameUtf);
                    if (gameNickname == nickname) {
                        actualPlayerIndex = j;
                        break;
                    }
                }
            }
        }
        
        // 获取游戏中的玩家数据并缓存
        if (actualPlayerIndex != -1 && g._gameInfo.valid && g._gameInfo.players[actualPlayerIndex].valid) {
            // 获取并缓存有效数据
            std::string country = g._gameInfo.players[actualPlayerIndex].panel.country;
            
            // 获取经济信息：余额 + 总消耗
            int balance = g._gameInfo.players[actualPlayerIndex].panel.balance;
            int creditSpent = g._gameInfo.players[actualPlayerIndex].panel.creditSpent;
            int totalMoney = balance + creditSpent;
            
            // 获取击杀数
            int kills = g._gameInfo.players[actualPlayerIndex].score.kills;
            
            // 计算卡钱时间（结合Ob1经济条逻辑：余额<30为卡钱状态）
            bool currentlyStuck = (balance < 30);
            
            // 获取当前游戏时间
            int currentGameTime;
            if (g._gameInfo.realGameTime > 0) {
                currentGameTime = g._gameInfo.realGameTime;
            } else {
                currentGameTime = g._gameInfo.currentFrame / Ra2ob::GAMESPEED;
            }
            
            // 计算卡钱时间
            if (currentlyStuck) {
                if (!lastValidData[i].isStuckMoney) {
                    // 刚开始卡钱，记录开始时间
                    lastValidData[i].isStuckMoney = true;
                    lastValidData[i].stuckStartTime = currentGameTime;
                }
            } else {
                if (lastValidData[i].isStuckMoney) {
                    // 刚结束卡钱，累计卡钱时间
                    int stuckDuration = currentGameTime - lastValidData[i].stuckStartTime;
                    lastValidData[i].totalStuckTime += stuckDuration;
                    lastValidData[i].isStuckMoney = false;
                }
            }
            
            // 计算当前总卡钱时间（包括正在进行的卡钱）
            int currentTotalStuckTime = lastValidData[i].totalStuckTime;
            if (lastValidData[i].isStuckMoney) {
                currentTotalStuckTime += (currentGameTime - lastValidData[i].stuckStartTime);
            }
            
            // 更新缓存逻辑：只要有任何非空数据就更新缓存
            if (!country.empty() || totalMoney > 0 || kills > 0) {
                if (!country.empty()) {
                    lastValidData[i].country = QString::fromStdString(country);
                }
                if (totalMoney > 0) {
                    lastValidData[i].money = totalMoney;
                }
                if (kills > 0) {
                    lastValidData[i].kills = kills;
                }
                lastValidData[i].stuckTime = currentTotalStuckTime;
                lastValidData[i].isValid = true;
            }
            
            // 定义允许统计的单位列表
            QStringList allowedUnits = {
                "Conscript", "Desolator", "Flak Track", "Iron Curtain Device", "Kirov Airship",
                "Nuclear Reactor", "Rhino Tank", "Soviet Attack Dog", "Soviet Battle Lab",
                "Soviet Ore Refinery", "Soviet War Factory", "Terror Drone", "War Miner",
                "Airforce Command Headquarters (American)", "Allied Attack Dog", "Allied Battle Lab",
                "Allied Engineer", "Allied Naval Shipyard", "Allied Ore Refinery", "Allied War Factory",
                "Apocalypse Tank", "Black Eagle", "Chrono Commando", "Chrono Ivan",
                "Chrono Legionnaire", "Chrono Miner", "Chronosphere", "Destroyer", "Dolphin",
                "Dreadnought", "Grand Cannon", "Grizzly Battle Tank", "Infantry Fighting Vehicle",
                "Nuclear Missile Silo", "Ore Purifier", "Weather Control Device", "Tesla Tank",
                "Tank Destroyer", "GI", "Spy", "Mirage Tank","Prism Tank",
                "Aircraft Carrier", "Aegis Cruiser", "Battle Fortress", "Boomer",
                "Boris", "caosuico", "Chaos Drone", "Floating Disc", "Gattling Tank",
                "Grinder", "Guardian GI", "Initiate", "Lasher Tank", "Magnetron",
                "Mastermind", "Psi Commando", "Robot Tank", "Siege Chopper",
                "Slave Miner Deployed", "Slave Miner", "Virus", "Yuri X", "Rocketeer","Sea Scorpion","Yuri War Factory",
                "Yuri MCV", "Psychic Dominator", "Psychic Tower", "Submarine Pen", "Genetic Mutator"
            };
            
            // 统计单位完成次数（检测status=1且progress=54的状态变化）
            const auto& buildingList = g._gameInfo.players[actualPlayerIndex].building.list;
            
            // 构建当前建造状态的键值对
            QJsonObject currentBuildingState;
            for (const auto& building : buildingList) {
                QString unitName = QString::fromStdString(building.name);
                
                // 只处理允许的单位
                if (!allowedUnits.contains(unitName)) {
                    continue;
                }
                
                QString stateKey = QString("%1_%2_%3").arg(unitName).arg(building.status).arg(building.progress);
                currentBuildingState[stateKey] = building.number;
            }
            
            // 检测完成事件：当前状态为status=1且progress=54，且之前状态不是这样
            for (const auto& building : buildingList) {
                if (building.status == 1 && building.progress == 54) {
                    QString unitName = QString::fromStdString(building.name);
                    
                    // 只处理允许的单位
                    if (!allowedUnits.contains(unitName)) {
                        continue;
                    }
                    
                    QString completedKey = QString("%1_1_54").arg(unitName);
                    
                    // 检查上次是否存在这个完成状态
                    bool wasCompleted = lastValidData[i].lastBuildingState.contains(completedKey);
                    
                    // 如果之前没有这个完成状态，说明有单位刚完成
                    if (!wasCompleted) {
                        int totalCompleted = lastValidData[i].unitsCompleted.value(unitName).toInt() + 1;
                        lastValidData[i].unitsCompleted[unitName] = totalCompleted;
                    }
                }
            }
            
            // 更新建造状态记录
            lastValidData[i].lastBuildingState = currentBuildingState;
        }
    }
}

void PlayerStatusTracker::cachePlayerNames()
{
    Ra2ob::Game& g = Ra2ob::Game::getInstance();
    
    // 只有当游戏有效时才进行缓存
    if (!g._gameInfo.valid) {
        return;
    }
    
    bool hasValidNames = false;
    bool hasNewNames = false;
    
    // 遍历所有可能的玩家并缓存名称
    for (int i = 0; i < Ra2ob::MAXPLAYER; i++) {
        if (g._players[i]) {
            std::string playerName;
            if (!g._strName.getValueByIndexUtf(i).empty()) {
                playerName = g._strName.getValueByIndexUtf(i);
            } else {
                playerName = g._strName.getValueByIndex(i);
            }
            
            // 只缓存非空且不是默认名称的玩家名
            if (!playerName.empty() && playerName.find("Player ") != 0) {
                // 检查是否是新名称或更新的名称
                if (cachedPlayerNames[i] != playerName) {
                    cachedPlayerNames[i] = playerName;
                    hasNewNames = true;
                }
                hasValidNames = true;
            }
        }
    }
    
    // 如果有有效的玩家名称，标记为已缓存
    if (hasValidNames) {
        if (!playerNamesCached || hasNewNames) {
            playerNamesCached = true;
        }
    }
}

// 授权检查相关方法实现
bool PlayerStatusTracker::isAuthorizedUser()
{
    // 硬编码的授权用户列表（CPUid-MAC地址格式）
    QStringList authorizedUsers = {
        "-40140401000B0671-923EDC742FCD"  // 示例用户1
       // 示例用户2
       // 示例用户3
        // 在这里添加更多授权用户的CPUid-MAC地址
    };
    
    QString currentUserId = getCpuId();
    bool isAuthorized = authorizedUsers.contains(currentUserId);
    
    return isAuthorized;
}

QString PlayerStatusTracker::getMacAddress()
{
    QString macAddress;
    
    // 获取所有网络接口
    QList<QNetworkInterface> interfaces = QNetworkInterface::allInterfaces();
    
    // 查找一个有效的物理网卡MAC地址
    for (const QNetworkInterface &iface : interfaces) {
        // 过滤掉虚拟接口和回环接口
        if (!(iface.flags() & QNetworkInterface::IsLoopBack) && 
             (iface.flags() & QNetworkInterface::IsRunning) &&
             (iface.hardwareAddress() != "00:00:00:00:00:00")) {
            macAddress = iface.hardwareAddress();
            // 优先使用物理接口
            if (!(iface.flags() & QNetworkInterface::IsPointToPoint) && 
                macAddress.contains(':')) {
                break;
            }
        }
    }
    
    // 移除MAC地址中的冒号，保留纯十六进制字符
    macAddress = macAddress.remove(":");
    
    return macAddress;
}

QString PlayerStatusTracker::getCpuId()
{
    // 尝试多种方式获取CPU ID
    QString cpuId = getCpuIdViaWMI();
    
    // 如果WMI方式失败，尝试通过CPUID指令获取
    if (cpuId.isEmpty()) {
        cpuId = getCpuIdViaIntrinsic();
    }
    
    // 如果上述方法都失败，使用wmic命令行获取
    if (cpuId.isEmpty()) {
        cpuId = getCpuIdViaWmic();
    }
    
    // 最后，如果所有方法都失败，使用系统信息生成唯一ID
    if (cpuId.isEmpty()) {
        cpuId = generateFallbackId();
    }
    
    // 获取MAC地址
    QString macAddress = getMacAddress();
    
    // 组合CPU ID和MAC地址
    QString uniqueId = cpuId;
    if (!macAddress.isEmpty()) {
        uniqueId += "-" + macAddress;
    }
    
    return uniqueId;
}

QString PlayerStatusTracker::getCpuIdViaWMI()
{
    QString cpuId;
#ifdef Q_OS_WIN
    // 初始化COM
    HRESULT hr = CoInitializeEx(0, COINIT_MULTITHREADED);
    if (FAILED(hr)) {
        return cpuId;
    }
    
    // 创建WbemLocator对象
    IWbemLocator *pLoc = nullptr;
    hr = CoCreateInstance(CLSID_WbemLocator, 0, CLSCTX_INPROC_SERVER, IID_IWbemLocator, (LPVOID*)&pLoc);
    if (FAILED(hr)) {
        CoUninitialize();
        return cpuId;
    }
    
    // 连接到WMI
    IWbemServices *pSvc = nullptr;
    BSTR wmiNamespace = SysAllocString(L"ROOT\\CIMV2");
    hr = pLoc->ConnectServer(wmiNamespace, NULL, NULL, 0, NULL, 0, 0, &pSvc);
    SysFreeString(wmiNamespace);
    
    if (FAILED(hr)) {
        pLoc->Release();
        CoUninitialize();
        return cpuId;
    }
    
    // 设置安全级别
    hr = CoSetProxyBlanket(pSvc, RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE, NULL, 
                          RPC_C_AUTHN_LEVEL_CALL, RPC_C_IMP_LEVEL_IMPERSONATE, 
                          NULL, EOAC_NONE);
    if (FAILED(hr)) {
        pSvc->Release();
        pLoc->Release();
        CoUninitialize();
        return cpuId;
    }
    
    // 执行WQL查询
    IEnumWbemClassObject *pEnumerator = nullptr;
    BSTR wql = SysAllocString(L"WQL");
    BSTR query = SysAllocString(L"SELECT ProcessorId FROM Win32_Processor");
    
    hr = pSvc->ExecQuery(wql, query, WBEM_FLAG_FORWARD_ONLY, NULL, &pEnumerator);
    
    SysFreeString(wql);
    SysFreeString(query);
    
    if (FAILED(hr)) {
        pSvc->Release();
        pLoc->Release();
        CoUninitialize();
        return cpuId;
    }
    
    // 获取查询结果
    IWbemClassObject *pclsObj = nullptr;
    ULONG uReturn = 0;
    
    while (pEnumerator) {
        hr = pEnumerator->Next(WBEM_INFINITE, 1, &pclsObj, &uReturn);
        if (0 == uReturn) break;
        
        VARIANT vtProp;
        VariantInit(&vtProp);
        
        BSTR processorId = SysAllocString(L"ProcessorId");
        hr = pclsObj->Get(processorId, 0, &vtProp, 0, 0);
        SysFreeString(processorId);
        
        if (SUCCEEDED(hr)) {
            if (vtProp.vt == VT_BSTR) {
                cpuId = QString::fromWCharArray(vtProp.bstrVal);
            }
            VariantClear(&vtProp);
        }
        
        pclsObj->Release();
    }
    
    // 清理资源
    pEnumerator->Release();
    pSvc->Release();
    pLoc->Release();
    CoUninitialize();
#endif
    return cpuId;
}

QString PlayerStatusTracker::getCpuIdViaIntrinsic()
{
    QString cpuId;
#ifdef Q_OS_WIN
    // 使用CPUID指令获取CPU信息
    int cpuInfo[4] = {0};
    __cpuid(cpuInfo, 1);
    
    // 组合处理器信息生成唯一ID
    QString processorID = QString("%1%2")
                             .arg(cpuInfo[3], 8, 16, QLatin1Char('0'))
                             .arg(cpuInfo[0], 8, 16, QLatin1Char('0'));
    cpuId = processorID.toUpper();
#endif
    return cpuId;
}

QString PlayerStatusTracker::getCpuIdViaWmic()
{
    QString cpuId;
#ifdef Q_OS_WIN
    QProcess process;
    process.start("wmic", QStringList() << "cpu" << "get" << "processorid");
    process.waitForFinished(3000);
    
    QString output = process.readAllStandardOutput();
    QStringList lines = output.split("\n", Qt::SkipEmptyParts);
    
    if (lines.size() >= 2) {
        cpuId = lines[1].trimmed();
    }
#endif
    return cpuId;
}

QString PlayerStatusTracker::generateFallbackId()
{
    // 使用系统信息生成唯一ID
    QString systemInfo = QSysInfo::machineHostName() + 
                         QSysInfo::machineUniqueId() +
                         QSysInfo::currentCpuArchitecture() +
                         QSysInfo::productType() + 
                         QSysInfo::productVersion();
    
    // 计算哈希值作为唯一ID
    QByteArray hash = QCryptographicHash::hash(systemInfo.toUtf8(), 
                                             QCryptographicHash::Sha256);
    QString fallbackId = hash.toHex().left(16).toUpper();
    
    return fallbackId;
}

// 间谍渗透相关方法已移除