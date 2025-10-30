#ifndef PLAYERINFO_H
#define PLAYERINFO_H

#include <QWidget>
#include <QPaintEvent>
#include <QFont>
#include <QPixmap>
#include <QLabel>
#include <QTimer>

#include "./globalsetting.h"
#include "Ra2ob/Ra2ob"

// 前向声明
class QNetworkAccessManager;
class QPropertyAnimation;
class QGraphicsOpacityEffect;

namespace Ui {
class PlayerInfo;
}

class PlayerInfo : public QWidget {
    Q_OBJECT

public:
    // 电力状态枚举
    enum class PowerState {
        Normal,   // 正常状态
        Warning,  // 警告状态（黄色）
        Critical  // 危急状态（红色）
    };

    explicit PlayerInfo(QWidget *parent = nullptr);
    ~PlayerInfo();

    void setAll(int index);
    void setPlayerNameByIndex(int index);
    void setBalanceByIndex(int index);
    void setKillsByIndex(int index);
    void setAliveUnitsByIndex(int index);
    void setPowerByIndex(int index);  // 设置电力状态
    void setScore(int score); // 设置比分
    int getScore() const; // 获取比分
    void updateScoreFromNickname(); // 根据昵称更新比分
    QString getCurrentNickname() const; // 获取当前玩家昵称
    
    // 设置玩家头像
    void setAvatar(const QPixmap &avatar);
    
    // 设置玩家军衔
    void setRank(const QString &rankType);
    
    // 更新字体后刷新昵称
    void refreshPlayerName(int index);
    
    // 设置镜像模式
    void setMirror();

    // 更新所有元素的缩放比例
    void updateScaling();

    // 重新排列UI元素
    void rearrange();

    // 设置自定义标签文本
    void setCustomLabelText(const QString &text);

    int getInsufficientFund(int index);

    Ra2ob::Game *g;
    Globalsetting *gls;
    bool mirrored = false;

    // 重写事件过滤器
    bool eventFilter(QObject *watched, QEvent *event) override;

    // 添加重置头像加载状态的方法
    void resetAvatarLoadStatus();

signals:
    // 添加分数变化信号
    void scoreChanged(const QString &nickname, int newScore);

protected:
    void paintEvent(QPaintEvent *event) override;

private slots:
    // 更新电力状态动画
    void updatePowerAnimation();

private:
    Ui::PlayerInfo *ui;
    
    // 存储玩家比分
    int playerScore = 0;
    
    // 存储当前玩家昵称
    QString currentNickname;
    
    // 当前玩家索引
    int currentPlayerIndex = -1;
    
    // 玩家头像
    QPixmap playerAvatar;
    
    // 玩家军衔标签
    QLabel *rankLabel = nullptr;
    
    // 自定义文本标签
    QLabel *customLabel = nullptr;
    
    // 玩家军衔图片
    QPixmap rankPixmap;
    
    // Techno Glitch字体
    QFont technoFont;
    QFont technoFontLarge; // 更大尺寸的技术字体，用于余额
    
    // MiSans字体
    QFont miSansBold;    // 用于玩家名称
    QFont miSansMedium;  // 用于分数
    
    // 电力状态动画相关
    QTimer *powerTimer = nullptr;
    QPropertyAnimation *powerAnimation = nullptr;
    QGraphicsOpacityEffect *powerEffect = nullptr;
    PowerState powerState = PowerState::Normal;
    
    // 从配置文件获取玩家军衔
    QString getRankFromConfig(const QString &nickname);

    // 远程头像加载状态
    static bool remoteAvatarLoaded;
    
    // 远程头像加载相关方法
    bool checkRemoteFileExists(const QString &url);
    void downloadAvatar(const QString &url);
    void useDefaultAvatar();
    void displayAvatar();

    // 网络管理器，用于远程加载头像
    QNetworkAccessManager *networkManager;

    QTimer *avatarRefreshTimer; // 用于定期刷新头像的定时器
};

#endif  // PLAYERINFO_H
