#include "membermanager.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDebug>
#include <QCryptographicHash>
#include <QTimer>

// 静态常量定义
const QString MemberManager::MEMBERSHIP_CHECK_URL = "https://your-server.com/api/check_member.json";
const int MemberManager::AUTO_REFRESH_INTERVAL = 300000; // 5分钟自动刷新一次

MemberManager::MemberManager(QObject *parent)
    : QObject(parent)
    , m_membershipLevel(MembershipLevel::Free)
    , m_networkManager(nullptr)
    , m_currentReply(nullptr)
    , m_autoRefreshTimer(nullptr)
    , m_initialized(false)
{
    m_networkManager = new QNetworkAccessManager(this);
    
    // 设置自动刷新定时器
    m_autoRefreshTimer = new QTimer(this);
    m_autoRefreshTimer->setSingleShot(false);
    m_autoRefreshTimer->setInterval(AUTO_REFRESH_INTERVAL);
    connect(m_autoRefreshTimer, &QTimer::timeout, this, &MemberManager::onAutoRefreshTimer);
}

MemberManager::~MemberManager()
{
    if (m_currentReply) {
        m_currentReply->abort();
        m_currentReply->deleteLater();
    }
}

MemberManager& MemberManager::getInstance()
{
    static MemberManager instance;
    return instance;
}

void MemberManager::initialize(const QString& cpuId, const QString& macId)
{
    if (m_initialized) {
        return;
    }

    // 生成用户唯一标识，格式：-CPUID-MACID
    m_userId = "-" + cpuId + "-" + macId;
    
    qDebug() << "[MemberManager] 初始化会员管理器，用户ID:" << m_userId;
    
    m_initialized = true;
    
    // 延迟检查会员状态，避免阻塞授权验证
    QTimer::singleShot(1000, this, &MemberManager::checkMembershipStatus);
    
    // 启动自动刷新定时器
    m_autoRefreshTimer->start();
}

bool MemberManager::isMember() const
{
    return m_membershipLevel == MembershipLevel::Premium;
}

MemberManager::MembershipLevel MemberManager::getMembershipLevel() const
{
    return m_membershipLevel;
}

bool MemberManager::hasFeatureAccess(FeatureType feature) const
{
    switch (feature) {
        case FeatureType::SpyInfiltration:
            // 间谍渗透功能需要会员权限
            return isMember();
        case FeatureType::TeamSwap:
            // 队伍切换功能需要会员权限
            return isMember();
        case FeatureType::SuperWeaponDisplay:
            // 超武显示功能需要会员权限
            return isMember();
        case FeatureType::BuildingTheftDetection:
            // 建筑偷取检测功能需要会员权限
            return isMember();
        case FeatureType::MinerLossDetection:
            // 矿车损失检测功能需要会员权限
            return isMember();
        default:
            return false;
    }
}

void MemberManager::refreshMembershipStatus()
{
    if (!m_initialized) {
        qDebug() << "[MemberManager] 未初始化，无法刷新会员状态";
        return;
    }
    
    checkMembershipStatus();
}

QString MemberManager::getUserId() const
{
    return m_userId;
}

void MemberManager::checkMembershipStatus()
{
    if (!m_networkManager || !m_initialized) {
        return;
    }

    // 如果有正在进行的请求，先取消
    if (m_currentReply) {
        m_currentReply->abort();
        m_currentReply->deleteLater();
        m_currentReply = nullptr;
    }

    QNetworkRequest request;
    request.setUrl(QUrl(MEMBERSHIP_CHECK_URL));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    
    // 设置超时
    request.setTransferTimeout(10000); // 10秒超时

    qDebug() << "[MemberManager] 开始检查会员状态，URL:" << MEMBERSHIP_CHECK_URL;

    m_currentReply = m_networkManager->get(request);
    
    connect(m_currentReply, &QNetworkReply::finished, this, &MemberManager::onMembershipCheckFinished);
    connect(m_currentReply, QOverload<QNetworkReply::NetworkError>::of(&QNetworkReply::errorOccurred),
            this, &MemberManager::onNetworkError);
}

void MemberManager::onMembershipCheckFinished()
{
    if (!m_currentReply) {
        return;
    }

    QNetworkReply::NetworkError error = m_currentReply->error();
    
    if (error == QNetworkReply::NoError) {
        QByteArray data = m_currentReply->readAll();
        qDebug() << "[MemberManager] 会员状态检查响应:" << data;
        
        parseMembershipResponse(data);
        emit membershipCheckCompleted(true);
    } else {
        qDebug() << "[MemberManager] 会员状态检查失败:" << m_currentReply->errorString();
        
        // 网络错误时保持当前状态，不改变会员级别
        emit membershipCheckCompleted(false);
    }

    m_currentReply->deleteLater();
    m_currentReply = nullptr;
}

void MemberManager::onNetworkError(QNetworkReply::NetworkError error)
{
    qDebug() << "[MemberManager] 网络错误:" << error;
}

void MemberManager::onAutoRefreshTimer()
{
    qDebug() << "[MemberManager] 自动刷新会员状态";
    checkMembershipStatus();
}

void MemberManager::parseMembershipResponse(const QByteArray& data)
{
    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);
    
    if (parseError.error != QJsonParseError::NoError) {
        qDebug() << "[MemberManager] JSON解析错误:" << parseError.errorString();
        return;
    }

    QJsonObject rootObj = doc.object();
    
    // 检查是否包含会员列表
    if (!rootObj.contains("members") || !rootObj["members"].isArray()) {
        qDebug() << "[MemberManager] 响应格式错误：缺少members数组";
        return;
    }

    QJsonArray membersArray = rootObj["members"].toArray();
    bool foundMember = false;

    // 遍历会员列表，查找当前用户
    for (const QJsonValue& memberValue : membersArray) {
        if (memberValue.isString()) {
            QString memberId = memberValue.toString();
            
            if (memberId == m_userId) {
                foundMember = true;
                qDebug() << "[MemberManager] 用户在会员列表中，设置为会员";
                break;
            }
        }
    }

    // 根据是否在会员列表中设置会员状态
    MembershipLevel newLevel = foundMember ? MembershipLevel::Premium : MembershipLevel::Free;
    
    if (newLevel != m_membershipLevel) {
        setMembershipLevel(newLevel);
    }
    
    if (foundMember) {
        qDebug() << "[MemberManager] 用户在会员列表中，享有会员权限";
    } else {
        qDebug() << "[MemberManager] 用户不在会员列表中，无会员权限";
    }
}

void MemberManager::setMembershipLevel(MembershipLevel level)
{
    if (m_membershipLevel != level) {
        MembershipLevel oldLevel = m_membershipLevel;
        m_membershipLevel = level;
        
        qDebug() << "[MemberManager] 会员级别变更:" 
                 << static_cast<int>(oldLevel) << "->" << static_cast<int>(level);
        
        emit membershipStatusChanged(isMember());
    }
}