// 超武系统实现 - ob3_superweapon.cpp
#include "ob3.h"
#include <QPainter>
#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <algorithm>

void Ob3::initSuperWeaponSystem() {
    // 初始化超武更新定时器
    superWeaponTimer = new QTimer(this);
    connect(superWeaponTimer, &QTimer::timeout, this, &Ob3::updateSuperWeapons);
    superWeaponTimer->setInterval(100); // 每0.5秒更新一次，与游戏数据同步
    superWeaponTimer->start();
    
    // 加载超武资源
    loadSuperWeaponResources();
    
    qDebug() << "超武系统初始化完成";
}

void Ob3::updateSuperWeapons() {
    // 检查超武显示功能权限
    if (!MemberManager::getInstance().hasFeatureAccess(MemberManager::FeatureType::SuperWeaponDisplay)) {
        superWeapons.clear(); // 清空超武列表
        return;
    }
    
    if (!g._gameInfo.valid) {
        return;
    }
    
    // 保存上一次的超武状态用于比较
    QMap<QString, SuperWeaponInfo> previousWeapons;
    for (const SuperWeaponInfo& weapon : superWeapons) {
        QString key = QString("%1_%2").arg(weapon.playerIndex).arg(weapon.weaponType);
        previousWeapons[key] = weapon;
    }
    
    // 清空当前超武列表
    superWeapons.clear();
    
    // 获取当前时间戳
    qint64 currentTime = QDateTime::currentMSecsSinceEpoch();
    
    // 遍历所有玩家获取超武信息
    for (int playerIndex = 0; playerIndex < Ra2ob::MAXPLAYER; playerIndex++) {
        // 检查玩家是否有效
        if (!g._gameInfo.players[playerIndex].valid) {
            continue;
        }
        
        // 获取玩家队伍信息
        int teamIndex = -1;
        QList<int> displayTeam1Players = getDisplayTeamPlayers(1);
        QList<int> displayTeam2Players = getDisplayTeamPlayers(2);
        
        if (displayTeam1Players.contains(playerIndex)) {
            teamIndex = 1;
        } else if (displayTeam2Players.contains(playerIndex)) {
            teamIndex = 2;
        }
        
        if (teamIndex == -1) {
            continue; // 跳过不在队伍中的玩家
        }
        
        // 获取玩家颜色
        QColor playerColor = Qt::white; // 默认颜色
        std::string colorStr = g._gameInfo.players[playerIndex].panel.color;
        if (!colorStr.empty() && colorStr != "0") {
            // 将十六进制颜色字符串转换为QColor
            playerColor = QColor(QString::fromStdString("#" + colorStr));
        }
        
        // 获取玩家的真实超武数据
        const auto& playerSuperTimer = g._gameInfo.players[playerIndex].superTimer;
        
        // 遍历该玩家的所有超武
        for (const auto& superNode : playerSuperTimer.list) {
            SuperWeaponInfo info;
            info.playerIndex = playerIndex;
            
            // 将游戏内部名称转换为显示名称
            QString internalName = QString::fromStdString(superNode.name);
            
            // 添加调试输出，查看实际读取到的超武名称
            qDebug() << "读取到超武名称:" << internalName;
            
            QMap<QString, QString> weaponTypeMap = {
                {"N U K E !!!", "nuclear"},
                {"Lightning Storm", "weather"},
                {"Iron Curtain", "ironcurtain"},
                {"Chrono Sphere", "chronosphere"},
                {"Nuke", "nuclear"},
                {"Nuclear Strike", "nuclear"},
                {"Nuclear Missile", "nuclear"},
                {"Weather Storm", "weather"},
                {"Storm", "weather"},
                {"Chronosphere", "chronosphere"},
                {"Chrono", "chronosphere"},
                {"Iron Curtain Device", "ironcurtain"},
                {"Psychic Dominator", "psychic"},
                {"Force Shield", "forceshield"},
                {"Genetic Mutator", "genetic"},
                {"Paratrooper Drop", "spyplane"}
            };
            
            info.weaponType = weaponTypeMap.value(internalName, internalName.toLower());
            
            // 添加调试输出，查看映射后的武器类型
            qDebug() << "映射后的武器类型:" << info.weaponType;
            
            // 将帧数转换为秒数（游戏内部每秒15帧）
            info.remainingTime = superNode.left / 15;
            info.isReady = (superNode.left <= 0 && superNode.status == 0);
            info.isActivating = false; // 默认为false，后面会检测
            info.playerColor = playerColor;
            info.teamIndex = teamIndex;
            info.activationStartTime = 0; // 默认为0
            info.animationDuration = 3000; // 默认3秒动画时间
            
            // 从缓存中获取资源
            info.icon = getSuperWeaponIcon(info.weaponType);
            info.background = getSuperWeaponBackground(info.weaponType, info.playerColor);
            info.activationGif = getSuperWeaponGif(info.weaponType);
            
            // 检测超武激活状态
            QString weaponKey = QString("%1_%2").arg(playerIndex).arg(info.weaponType);
            
            if (previousWeapons.contains(weaponKey)) {
                const SuperWeaponInfo& prevWeapon = previousWeapons[weaponKey];
                // 保持之前的动画状态和时间
                info.activationStartTime = prevWeapon.activationStartTime;
                info.animationDuration = prevWeapon.animationDuration;
                
                // 检测超武被使用：之前是READY状态，现在开始倒计时
                bool wasReady = prevWeapon.isReady;
                bool nowCounting = info.remainingTime > 0 && !info.isReady;
                
                if (wasReady && nowCounting) {
                    info.isActivating = true;
                    info.activationStartTime = currentTime;
                    info.animationDuration = 3000; // 3秒动画时间
                    qDebug() << "检测到超武被使用:" << internalName << "玩家:" << playerIndex;
                    
                    // 启动激活动画
                    if (info.activationGif) {
                        info.activationGif->start();
                        qDebug() << "启动GIF动画:" << info.weaponType;
                    }
                }
                // 检查现有动画是否应该继续播放
                else if (info.activationStartTime > 0) {
                    qint64 elapsedTime = currentTime - info.activationStartTime;
                    if (elapsedTime < info.animationDuration) {
                        info.isActivating = true;
                        qDebug() << "动画继续播放 - 玩家:" << playerIndex << "类型:" << info.weaponType 
                                 << "已播放:" << elapsedTime << "ms / 总时长:" << info.animationDuration << "ms";
                    } else {
                        // 动画播放完毕，重置状态
                        info.isActivating = false;
                        info.activationStartTime = 0;
                        if (info.activationGif) {
                            info.activationGif->stop();
                        }
                        qDebug() << "动画播放完毕 - 玩家:" << playerIndex << "类型:" << info.weaponType;
                    }
                }
            }
            
            superWeapons.append(info);
        }
    }
    
    // 按队伍分组超武
    groupSuperWeaponsByTeam();
    
    // 计算超武位置
    calculateSuperWeaponPositions();
    
    // 触发重绘
    update();
}

void Ob3::loadSuperWeaponResources() {
    QString appDir = QCoreApplication::applicationDirPath();
    QString superWeaponDir = appDir + "/assets/superweapons/";
    
    // 确保超武资源目录存在
    QDir dir(superWeaponDir);
    if (!dir.exists()) {
        qDebug() << "超武资源目录不存在:" << superWeaponDir;
        return;
    }
    
    // 定义超武类型列表
    QStringList weaponTypes = {"nuclear", "weather", "ironcurtain", "chronosphere", "psychic", "forceshield", "genetic", "spyplane"};
    
    // 加载每种超武的资源
    for (const QString& weaponType : weaponTypes) {
        // 加载图标
        QString iconPath = superWeaponDir + weaponType + ".png";
        if (QFile::exists(iconPath)) {
            QPixmap icon(iconPath);
            if (!icon.isNull()) {
                // 使用新的尺寸配置
                int actualWidth = static_cast<int>(superWeaponBaseWidth * superWeaponSizeRatio);
                int actualHeight = static_cast<int>(superWeaponBaseHeight * superWeaponSizeRatio);
                superWeaponIconCache[weaponType] = icon.scaled(actualWidth, actualHeight, Qt::KeepAspectRatio, Qt::SmoothTransformation);
                qDebug() << "加载超武图标:" << iconPath;
            }
        }
        
        // 加载背景
        QString bgPath = superWeaponDir + weaponType + "_bg.png";
        if (QFile::exists(bgPath)) {
            QPixmap bg(bgPath);
            if (!bg.isNull()) {
                superWeaponBgCache[weaponType] = bg;
                qDebug() << "加载超武背景:" << bgPath;
            }
        }
        
        // 加载GIF动画 - 使用正确的文件名格式，缩放到与PNG相同尺寸
        QString gifPath = superWeaponDir + weaponType + "_activate.gif";
        if (QFile::exists(gifPath)) {
            QMovie* movie = new QMovie(gifPath, QByteArray(), this);
            if (movie->isValid()) {
                // 使用与PNG图标相同的尺寸配置
                int actualWidth = static_cast<int>(superWeaponBaseWidth * superWeaponSizeRatio);
                int actualHeight = static_cast<int>(superWeaponBaseHeight * superWeaponSizeRatio);
                movie->setScaledSize(QSize(actualWidth, actualHeight));
                superWeaponGifCache[weaponType] = movie;
                qDebug() << "加载超武GIF:" << gifPath << "缩放到:" << actualWidth << "x" << actualHeight;
            } else {
                delete movie;
            }
        } else {
            // 如果没有找到特定的激活动画，尝试加载通用动画
            QString fallbackGifPath = superWeaponDir + "superweapon_activation.gif";
            if (QFile::exists(fallbackGifPath)) {
                QMovie* movie = new QMovie(fallbackGifPath, QByteArray(), this);
                if (movie->isValid()) {
                    // 使用与PNG图标相同的尺寸配置
                    int actualWidth = static_cast<int>(superWeaponBaseWidth * superWeaponSizeRatio);
                    int actualHeight = static_cast<int>(superWeaponBaseHeight * superWeaponSizeRatio);
                    movie->setScaledSize(QSize(actualWidth, actualHeight));
                    superWeaponGifCache[weaponType] = movie;
                    qDebug() << "加载通用超武GIF:" << fallbackGifPath << "用于类型:" << weaponType << "缩放到:" << actualWidth << "x" << actualHeight;
                } else {
                    delete movie;
                }
            }
        }
    }
    
    qDebug() << "超武资源加载完成";
}

QPixmap Ob3::getSuperWeaponIcon(const QString &weaponType) {
    if (superWeaponIconCache.contains(weaponType)) {
        return superWeaponIconCache[weaponType];
    }
    
    // 返回默认图标
    int actualWidth = static_cast<int>(superWeaponBaseWidth * superWeaponSizeRatio);
    int actualHeight = static_cast<int>(superWeaponBaseHeight * superWeaponSizeRatio);
    QPixmap defaultIcon(actualWidth, actualHeight);
    defaultIcon.fill(Qt::gray);
    return defaultIcon;
}

QPixmap Ob3::getSuperWeaponBackground(const QString &weaponType, const QColor &playerColor) {
    QString cacheKey = QString("%1_%2").arg(weaponType).arg(playerColor.name());
    
    if (superWeaponBgCache.contains(cacheKey)) {
        return superWeaponBgCache[cacheKey];
    }
    
    // 创建玩家颜色背景
    int actualWidth = static_cast<int>(superWeaponBaseWidth * superWeaponSizeRatio);
    int actualHeight = static_cast<int>(superWeaponBaseHeight * superWeaponSizeRatio);
    QPixmap playerBg(actualWidth, actualHeight);
    
    // 使用完全不透明的玩家颜色作为背景
    playerBg.fill(playerColor);
    
    // 缓存背景
    superWeaponBgCache[cacheKey] = playerBg;
    
    return playerBg;
}

QMovie* Ob3::getSuperWeaponGif(const QString &weaponType) {
    if (superWeaponGifCache.contains(weaponType)) {
        return superWeaponGifCache[weaponType];
    }
    return nullptr;
}

void Ob3::groupSuperWeaponsByTeam() {
    // 按队伍分组，然后按时间排序（时间快要转完的排在前面）
    std::sort(superWeapons.begin(), superWeapons.end(), [](const SuperWeaponInfo& a, const SuperWeaponInfo& b) {
        // 首先按队伍分组
        if (a.teamIndex != b.teamIndex) {
            return a.teamIndex < b.teamIndex;
        }
        
        // 在同一队伍内，按优先级排序：
        // 1. READY状态的超武排在最前面
        if (a.isReady && !b.isReady) {
            return true;  // a是READY，排在前面
        }
        if (!a.isReady && b.isReady) {
            return false; // b是READY，排在前面
        }
        
        // 2. 如果都是READY状态，按玩家索引排序
        if (a.isReady && b.isReady) {
            return a.playerIndex < b.playerIndex;
        }
        
        // 3. 如果都不是READY状态，按剩余时间排序
        if (!a.isReady && !b.isReady) {
            // 都在倒计时中，剩余时间少的排在前面（时间快要转完的排前面）
            if (a.remainingTime > 0 && b.remainingTime > 0) {
                return a.remainingTime < b.remainingTime;
            }
            // 如果有一个是0或负数（On hold状态），有倒计时的排在前面
            if (a.remainingTime > 0 && b.remainingTime <= 0) {
                return true;  // a有倒计时，排在前面
            }
            if (b.remainingTime > 0 && a.remainingTime <= 0) {
                return false; // b有倒计时，排在前面
            }
            // 如果都是0或负数，按玩家索引排序
            return a.playerIndex < b.playerIndex;
        }
        
        // 最后按玩家索引排序（保持稳定排序）
        return a.playerIndex < b.playerIndex;
    });
}

void Ob3::calculateSuperWeaponPositions() {
    // 这个方法将在paintSuperWeapons中实时计算位置
    // 这里可以预计算一些基础信息
}

QPoint Ob3::getTeam1SuperWeaponPosition(int index) {
    // 队伍1：使用配置的XY坐标和比例
    int actualWidth = static_cast<int>(superWeaponBaseWidth * superWeaponSizeRatio);
    int actualSpacing = static_cast<int>(superWeaponBaseSpacing * superWeaponSizeRatio);
    
    int x = static_cast<int>(superWeaponTeam1X * superWeaponPositionRatioX) + 
            index * (actualWidth + actualSpacing);
    int y = static_cast<int>(superWeaponTeam1Y * superWeaponPositionRatioY);
    
    return QPoint(x, y);
}

QPoint Ob3::getTeam2SuperWeaponPosition(int index) {
    // 队伍2：使用配置的XY坐标和比例，从左向右显示
    int actualWidth = static_cast<int>(superWeaponBaseWidth * superWeaponSizeRatio);
    int actualSpacing = static_cast<int>(superWeaponBaseSpacing * superWeaponSizeRatio);
    
    int x = static_cast<int>(superWeaponTeam2X * superWeaponPositionRatioX) + 
            index * (actualWidth + actualSpacing);
    int y = static_cast<int>(superWeaponTeam2Y * superWeaponPositionRatioY);
    
    return QPoint(x, y);
}

void Ob3::paintSuperWeapons(QPainter *painter) {
    // 检查超武显示功能权限
    if (!MemberManager::getInstance().hasFeatureAccess(MemberManager::FeatureType::SuperWeaponDisplay)) {
        return;
    }
    
    if (superWeapons.isEmpty()) {
        return;
    }
    
    painter->setRenderHint(QPainter::Antialiasing);
    
    int team1Index = 0;
    int team2Index = 0;
    
    // 计算实际尺寸
    int actualWidth = static_cast<int>(superWeaponBaseWidth * superWeaponSizeRatio);
    int actualHeight = static_cast<int>(superWeaponBaseHeight * superWeaponSizeRatio);
    
    for (const SuperWeaponInfo& weapon : superWeapons) {
        QPoint position;
        
        if (weapon.teamIndex == 1) {
            position = getTeam1SuperWeaponPosition(team1Index);
            team1Index++;
        } else if (weapon.teamIndex == 2) {
            position = getTeam2SuperWeaponPosition(team2Index);
            team2Index++;
        } else {
            continue;
        }
        
        // 不绘制背景，直接绘制图标
        // if (!weapon.background.isNull()) {
        //     painter->drawPixmap(position, weapon.background);
        // }
        
        // 绘制图标
        if (!weapon.icon.isNull()) {
            painter->drawPixmap(position, weapon.icon);
        }
        
        // 绘制玩家颜色边框
        QPen borderPen(weapon.playerColor, 3);  // 增大边框粗细从2到3
        painter->setPen(borderPen);
        painter->setBrush(Qt::NoBrush);
        painter->drawRect(position.x(), position.y(), actualWidth, actualHeight);
        
        // 绘制状态信息
        if (weapon.isReady) {
            // 绘制"READY"文字 - 使用玩家颜色字体
            int bgHeight = 12;  // 减小背景框高度
            painter->fillRect(position.x() + 2, position.y() + actualHeight - bgHeight - 2, actualWidth - 4, bgHeight, QColor(0, 0, 0, 255));
            painter->setPen(QPen(weapon.playerColor, 1));  // 使用玩家颜色
            painter->setFont(QFont("Arial", 8, QFont::Bold));  // 减小字体大小
            QRect textRect(position.x() + 2, position.y() + actualHeight - bgHeight - 2, actualWidth - 4, bgHeight);
            painter->drawText(textRect, Qt::AlignCenter, "READY");
        } else if (weapon.remainingTime > 0) {
            // 绘制倒计时 - 使用玩家颜色字体
            int bgHeight = 10;  // 减小背景框高度
            painter->fillRect(position.x() + 2, position.y() + actualHeight - bgHeight - 2, actualWidth - 4, bgHeight, QColor(0, 0, 0, 255));
            painter->setPen(QPen(weapon.playerColor, 1));  // 使用玩家颜色
            painter->setFont(QFont("Arial", 11, QFont::Bold));  // 减小字体大小
            
            // 格式化时间显示
            int minutes = weapon.remainingTime / 60;
            int seconds = weapon.remainingTime % 60;
            QString timeText = QString("%1:%2").arg(minutes).arg(seconds, 2, 10, QChar('0'));
            
            QRect textRect(position.x() + 2, position.y() + actualHeight - bgHeight - 2, actualWidth - 4, bgHeight);
            painter->drawText(textRect, Qt::AlignCenter, timeText);
        } else {
            // 绘制"On hold"文字 - 使用玩家颜色字体
            int bgHeight = 12;  // 背景框高度
            painter->fillRect(position.x() + 2, position.y() + actualHeight - bgHeight - 2, actualWidth - 4, bgHeight, QColor(0, 0, 0, 255));
            painter->setPen(QPen(weapon.playerColor, 1));  // 使用玩家颜色
            painter->setFont(QFont("Arial", 7, QFont::Bold));  // 字体大小
            QRect textRect(position.x() + 2, position.y() + actualHeight - bgHeight - 2, actualWidth - 4, bgHeight);
            painter->drawText(textRect, Qt::AlignCenter, "On hold");
        }
        
        // 绘制激活动画
        if (weapon.isActivating && weapon.activationGif) {
            qDebug() << "绘制GIF动画 - 玩家:" << weapon.playerIndex << "类型:" << weapon.weaponType;
            
            // 启动动画（如果还没启动）
            if (weapon.activationGif->state() != QMovie::Running) {
                weapon.activationGif->start();
                qDebug() << "启动GIF动画:" << weapon.weaponType;
            }
            
            // 获取当前帧并绘制
            QPixmap currentFrame = weapon.activationGif->currentPixmap();
            if (!currentFrame.isNull()) {
                // 绘制激活动画，使用与图标相同的位置和尺寸
                painter->drawPixmap(position, currentFrame);
                qDebug() << "绘制GIF帧 - 位置:" << position << "尺寸:" << currentFrame.size();
            }
        }
    }
}