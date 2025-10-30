#ifndef MEMBERMANAGER_H
#define MEMBERMANAGER_H

#include <QObject>
#include <QString>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QTimer>

class MemberManager : public QObject
{
    Q_OBJECT

public:
    enum class MembershipLevel {
        Free = 0,      // 免费用户
        Premium = 1    // 会员用户
    };

    enum class FeatureType {
        SpyInfiltration = 0,  // 间谍渗透显示功能
        TeamSwap = 1,         // 队伍切换功能
        SuperWeaponDisplay = 2, // 超武显示功能
        BuildingTheftDetection = 3, // 建筑偷取检测功能
        MinerLossDetection = 4 // 矿车损失检测功能
    };

    explicit MemberManager(QObject *parent = nullptr);
    ~MemberManager();

    // 获取单例实例
    static MemberManager& getInstance();

    // 初始化会员管理器
    void initialize(const QString& cpuId, const QString& macId);

    // 检查会员状态
    bool isMember() const;
    MembershipLevel getMembershipLevel() const;

    // 检查功能权限
    bool hasFeatureAccess(FeatureType feature) const;

    // 手动刷新会员状态
    void refreshMembershipStatus();

    // 获取用户标识
    QString getUserId() const;

signals:
    void membershipStatusChanged(bool isMember);
    void membershipCheckCompleted(bool success);

private slots:
    void onMembershipCheckFinished();
    void onNetworkError(QNetworkReply::NetworkError error);
    void onAutoRefreshTimer();

private:
    void checkMembershipStatus();
    void parseMembershipResponse(const QByteArray& data);
    void setMembershipLevel(MembershipLevel level);

    QString m_userId;           // 用户唯一标识 (CPU ID + MAC ID)
    MembershipLevel m_membershipLevel;
    QNetworkAccessManager* m_networkManager;
    QNetworkReply* m_currentReply;
    QTimer* m_autoRefreshTimer;
    
    bool m_initialized;
    static const QString MEMBERSHIP_CHECK_URL;
    static const int AUTO_REFRESH_INTERVAL; // 自动刷新间隔(毫秒)
};

#endif // MEMBERMANAGER_H