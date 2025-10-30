#include "theftalertmanager.h"
#include "buildingdetector.h"
#include "globalsetting.h"
#include "Ra2ob/Ra2ob/src/Game.hpp"
#include <QDebug>
#include <QFontDatabase>

TheftAlertManager::TheftAlertManager(QWidget* parentWidget, QObject* parent)
    : QObject(parent), parentWidget(parentWidget), buildingDetector(nullptr)
{
    // 加载字体
    QFontDatabase::addApplicationFont(":/assets/fonts/MiSans-Heavy.ttf");
    miSansHeavyFamily = "MiSans Heavy";
}

TheftAlertManager::~TheftAlertManager()
{
    clearAllAlerts();
}

void TheftAlertManager::initTheftAlertSystem()
{
    // 清空现有的偷取提示标签
    clearAllAlerts();
    
    // 初始化多标签系统的参数
    float ratio = Globalsetting::getInstance().l.ratio;
    theftAlertStartY = qRound(500 * ratio);  // 起始Y位置
    theftAlertSpacing = qRound(35 * ratio);  // 标签间距
    maxTheftAlerts = 5;  // 最大同时显示的标签数量
    
    qDebug() << "建筑偷取事件系统初始化完成";
}

void TheftAlertManager::setBuildingDetector(BuildingDetector* detector) {
    buildingDetector = detector;
}

void TheftAlertManager::clearAllAlerts()
{
    for (auto& alertInfo : theftAlertLabels) {
        if (alertInfo.timer) {
            alertInfo.timer->stop();
            alertInfo.timer->deleteLater();
        }
        if (alertInfo.label) {
            alertInfo.label->hide();
            alertInfo.label->deleteLater();
        }
    }
    theftAlertLabels.clear();
}

QLabel* TheftAlertManager::createTheftAlertLabel()
{
    // 获取缩放比例
    float ratio = Globalsetting::getInstance().l.ratio;
    
    QLabel* label = new QLabel(parentWidget);
    
    // 创建并设置字体
    QFont theftAlertFont(miSansHeavyFamily, qRound(14 * ratio));
    theftAlertFont.setBold(true);
    theftAlertFont.setStyleStrategy(QFont::PreferAntialias);
    label->setFont(theftAlertFont);
    
    label->setStyleSheet(
        "QLabel {"
        "color: red;"
        "background-color: rgba(0, 0, 0, 150);"
        "padding: 5px;"
        "border-radius: 3px;"
        "}"
    );
    label->hide();
    
    return label;
}

void TheftAlertManager::updateTheftAlertPositions()
{
    float ratio = Globalsetting::getInstance().l.ratio;
    int currentY = theftAlertStartY;
    
    for (int i = 0; i < theftAlertLabels.size(); ++i) {
        TheftAlertInfo& alertInfo = theftAlertLabels[i];
        alertInfo.yPosition = currentY;
        
        // 设置标签位置，X坐标为0
        alertInfo.label->move(0, currentY);
        
        // 下一个标签的Y位置
        currentY += theftAlertSpacing;
    }
}

void TheftAlertManager::removeExpiredTheftAlert(int index)
{
    if (index >= 0 && index < theftAlertLabels.size()) {
        TheftAlertInfo& alertInfo = theftAlertLabels[index];
        
        // 停止并删除定时器
        if (alertInfo.timer) {
            alertInfo.timer->stop();
            alertInfo.timer->deleteLater();
        }
        
        // 隐藏并删除标签
        if (alertInfo.label) {
            alertInfo.label->hide();
            alertInfo.label->deleteLater();
        }
        
        // 从列表中移除
        theftAlertLabels.removeAt(index);
        
        // 更新剩余标签的位置
        updateTheftAlertPositions();
    }
}

QString TheftAlertManager::getPlayerColorByName(const QString& playerName) {
    // 获取游戏实例
    Ra2ob::Game& g = Ra2ob::Game::getInstance();
    
    // 遍历所有玩家，查找匹配的玩家名字并返回其颜色
    for (int i = 0; i < MAXPLAYER; i++) {
        if (!g._gameInfo.players[i].valid) {
            continue;
        }
        
        // 获取玩家名字（优先使用UTF-8版本）
        QString currentPlayerName;
        if (!g._strName.getValueByIndexUtf(i).empty()) {
            currentPlayerName = QString::fromStdString(g._strName.getValueByIndexUtf(i));
        } else {
            currentPlayerName = QString::fromStdString(g._strName.getValueByIndex(i));
        }
        
        // 比较玩家名字
        if (currentPlayerName == playerName) {
            // 获取玩家颜色
            std::string colorStr = g._gameInfo.players[i].panel.color;
            if (!colorStr.empty() && colorStr != "0") {
                return QString::fromStdString("#" + colorStr);
            }
        }
    }
    
    // 如果没有找到匹配的玩家，返回默认白色
    return "#FFFFFF";
}

void TheftAlertManager::showTheftAlert(const QString& thiefName, const QString& victimName, 
                                     const QString& buildingName, int count)
{
    qDebug() << "showTheftAlert called:" << thiefName << "stole" << buildingName << "from" << victimName;
    
    // 获取全局设置实例
    Globalsetting& gls = Globalsetting::getInstance();
    float ratio = gls.l.ratio;
    
    // 检查是否超过最大标签数量，如果是则移除最旧的
    if (theftAlertLabels.size() >= maxTheftAlerts) {
        removeExpiredTheftAlert(0);  // 移除最旧的（第一个）
    }
    
    // 获取映射后的玩家名称
    QString mappedThiefName = gls.findNameByNickname(thiefName);
    if (mappedThiefName.isEmpty()) {
        mappedThiefName = thiefName;  // 如果没有映射，使用原始名称
    }
    
    QString mappedVictimName = gls.findNameByNickname(victimName);
    if (mappedVictimName.isEmpty()) {
        mappedVictimName = victimName;  // 如果没有映射，使用原始名称
    }
    
    // 获取建筑的中文名称
    QString chineseBuildingName = buildingName;
    if (buildingDetector) {
        chineseBuildingName = buildingDetector->getBuildingChineseName(buildingName.toStdString());
    }
    
    // 获取玩家颜色（使用原始名称，因为游戏内存中存储的是原始名称）
    QString thiefColor = getPlayerColorByName(thiefName);
    QString victimColor = getPlayerColorByName(victimName);
    
    // 构建HTML富文本，使用玩家对应的颜色显示名字，其他文字为白色
    QString alertText = QString("<span style='color: %1'>%2</span> <span style='color: white'>窃取了</span> <span style='color: %3'>%4</span> <span style='color: white'>的 %5</span>")
                       .arg(thiefColor)
                       .arg(mappedThiefName)
                       .arg(victimColor)
                       .arg(mappedVictimName)
                       .arg(chineseBuildingName);
    
    // 创建新的标签和定时器
    QLabel* newLabel = createTheftAlertLabel();
    QTimer* newTimer = new QTimer(this);
    newTimer->setSingleShot(true);
    
    // 设置标签文本
    newLabel->setText(alertText);
    newLabel->adjustSize();
    
    // 创建新的偷取提示信息
    TheftAlertInfo alertInfo;
    alertInfo.label = newLabel;
    alertInfo.timer = newTimer;
    alertInfo.createTime = QDateTime::currentDateTime().toMSecsSinceEpoch();
    alertInfo.yPosition = 0;  // 位置将在 updateTheftAlertPositions 中设置
    
    // 添加到列表中（按时间顺序，新的在后面）
    theftAlertLabels.append(alertInfo);
    
    // 更新所有标签的位置
    updateTheftAlertPositions();
    
    // 显示新标签
    newLabel->show();
    
    // 设置定时器，6秒后移除这个标签
    connect(newTimer, &QTimer::timeout, [this, newTimer]() {
        // 查找当前标签在列表中的实际位置
        for (int i = 0; i < theftAlertLabels.size(); ++i) {
            if (theftAlertLabels[i].timer == newTimer) {
                removeExpiredTheftAlert(i);
                break;
            }
        }
    });
    
    newTimer->start(6000);
}

void TheftAlertManager::showMinerLossAlert(const QString& playerName, const QString& minerName, int count)
{
    qDebug() << "showMinerLossAlert called:" << playerName << "lost" << count << minerName;
    
    // 获取全局设置实例
    Globalsetting& gls = Globalsetting::getInstance();
    float ratio = gls.l.ratio;
    
    // 检查是否超过最大标签数量，如果是则移除最旧的
    if (theftAlertLabels.size() >= maxTheftAlerts) {
        removeExpiredTheftAlert(0);  // 移除最旧的（第一个）
    }
    
    // 获取映射后的玩家名称
    QString mappedPlayerName = gls.findNameByNickname(playerName);
    if (mappedPlayerName.isEmpty()) {
        mappedPlayerName = playerName;  // 如果没有映射，使用原始名称
    }
    
    // 获取玩家颜色（使用原始名称，因为游戏内存中存储的是原始名称）
    QString playerColor = getPlayerColorByName(playerName);
    
    // 构建HTML富文本，显示矿车损失信息
    QString alertText;
    if (count > 1) {
        alertText = QString("<span style='color: %1'>%2</span> <span style='color: white'>损失了 %3 个 %4</span>")
                   .arg(playerColor)
                   .arg(mappedPlayerName)
                   .arg(count)
                   .arg(minerName);
    } else {
        alertText = QString("<span style='color: %1'>%2</span> <span style='color: white'>损失了 %3</span>")
                   .arg(playerColor)
                   .arg(mappedPlayerName)
                   .arg(minerName);
    }
    
    // 创建新的标签和定时器
    QLabel* newLabel = createTheftAlertLabel();
    QTimer* newTimer = new QTimer(this);
    newTimer->setSingleShot(true);
    
    // 设置标签文本
    newLabel->setText(alertText);
    newLabel->adjustSize();
    
    // 创建新的提示信息
    TheftAlertInfo alertInfo;
    alertInfo.label = newLabel;
    alertInfo.timer = newTimer;
    alertInfo.createTime = QDateTime::currentDateTime().toMSecsSinceEpoch();
    alertInfo.yPosition = 0;  // 位置将在 updateTheftAlertPositions 中设置
    
    // 添加到列表中（按时间顺序，新的在后面）
    theftAlertLabels.append(alertInfo);
    
    // 更新所有标签的位置
    updateTheftAlertPositions();
    
    // 显示新标签
    newLabel->show();
    
    // 设置定时器，4秒后移除这个标签（矿车损失提示时间稍短）
    connect(newTimer, &QTimer::timeout, [this, newTimer]() {
        // 查找当前标签在列表中的实际位置
        for (int i = 0; i < theftAlertLabels.size(); ++i) {
            if (theftAlertLabels[i].timer == newTimer) {
                removeExpiredTheftAlert(i);
                break;
            }
        }
    });
    
    newTimer->start(5000);
}