#ifndef THEFTALERTMANAGER_H
#define THEFTALERTMANAGER_H

#include <QWidget>
#include <QLabel>
#include <QTimer>
#include <QList>
#include <QString>
#include <QDateTime>

class BuildingDetector;

// 偷取提示信息结构体
struct TheftAlertInfo {
    QLabel* label = nullptr;
    QTimer* timer = nullptr;
    qint64 createTime = 0;
    int yPosition = 0;
};

class TheftAlertManager : public QObject
{
    Q_OBJECT

public:
    explicit TheftAlertManager(QWidget* parentWidget, QObject* parent = nullptr);
    ~TheftAlertManager();

    // 初始化偷取提示系统
    void initTheftAlertSystem();
    
public slots:
    // 显示建筑偷取提示
    void showTheftAlert(const QString& thiefName, const QString& victimName, 
                       const QString& buildingName, int count);
    
    // 显示矿车损失提示
    void showMinerLossAlert(const QString& playerName, const QString& minerName, int count);
    
    // 设置建筑检测器引用（用于获取中文建筑名称）
    void setBuildingDetector(BuildingDetector* detector);
    
    // 清理所有提示
    void clearAllAlerts();

private:
    QWidget* parentWidget;                  // 父窗口部件
    BuildingDetector* buildingDetector;     // 建筑检测器引用
    
    QList<TheftAlertInfo> theftAlertLabels; // 多个建筑偷取事件显示标签
    int theftAlertStartY = 500;             // 偷取提示起始Y坐标
    int theftAlertSpacing = 35;             // 偷取提示间距
    int maxTheftAlerts = 5;                 // 最大同时显示的偷取提示数量
    QString miSansHeavyFamily;              // 字体族名称
    
    // 私有方法
    QLabel* createTheftAlertLabel();        // 创建偷取提示标签
    void updateTheftAlertPositions();       // 更新偷取提示位置
    void removeExpiredTheftAlert(int index); // 移除过期的偷取提示
    QString getPlayerColorByName(const QString& playerName); // 根据玩家名字获取颜色
};

#endif // THEFTALERTMANAGER_H