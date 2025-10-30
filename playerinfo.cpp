#include "./playerinfo.h"

#include <QFile>
#include <QString>
#include <string>
#include <QPainter>
#include <QPainterPath>
#include <QDebug>
#include <QCoreApplication>
#include <QDirIterator>
#include <QFileInfo>
#include <QFontDatabase>
#include <QRegularExpression>
#include <QMouseEvent>
#include <QApplication>
#include <QTimer>
#include <QPropertyAnimation>
#include <QGraphicsOpacityEffect>
#include <QJsonObject>
#include <QJsonDocument>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QEventLoop>
#include <QDateTime>

#include "./layoutsetting.h"
#include "./ui_playerinfo.h"

// 添加静态字体族变量，确保字体只加载一次
static QString technoGlitchFontFamily;
static QString miSansBoldFamily;
static QString miSansMediumFamily;
static QString miSansHeavyFamily;
static bool fontsLoaded = false;

// 添加一个布尔值成员变量，记录远程头像是否已经成功加载
bool PlayerInfo::remoteAvatarLoaded = false;

// 静态方法加载所有字体
static void loadAllFonts() {
    if (fontsLoaded) {
        return;
    }
    
    QString appDir = QCoreApplication::applicationDirPath();
    
    // 加载LLDEtechnoGlitch-Bold0.ttf字体
    QString technoFontPath = appDir + "/assets/fonts/LLDEtechnoGlitch-Bold0.ttf";
    int fontId = QFontDatabase::addApplicationFont(technoFontPath);
    if (fontId != -1) {
        technoGlitchFontFamily = QFontDatabase::applicationFontFamilies(fontId).at(0);
        qDebug() << "成功加载Techno Glitch字体:" << technoGlitchFontFamily;
    } else {
        qDebug() << "无法加载Techno Glitch字体，使用内置字体名称";
        technoGlitchFontFamily = "LLDEtechnoGlitch-Bold0";
    }
    
    // 加载MiSans-Bold.ttf字体
    QString miSansBoldPath = appDir + "/assets/fonts/MiSans-Bold.ttf";
    int miSansBoldId = QFontDatabase::addApplicationFont(miSansBoldPath);
    if (miSansBoldId != -1) {
        miSansBoldFamily = QFontDatabase::applicationFontFamilies(miSansBoldId).at(0);
        qDebug() << "成功加载MiSans Bold字体:" << miSansBoldFamily;
    } else {
        qDebug() << "无法加载MiSans Bold字体，使用内置字体名称";
        miSansBoldFamily = "MiSans-Bold";
    }
    
    // 加载MISANS-MEDIUM.TTF字体
    QString miSansMediumPath = appDir + "/assets/fonts/MISANS-MEDIUM.TTF";
    int miSansMediumId = QFontDatabase::addApplicationFont(miSansMediumPath);
    if (miSansMediumId != -1) {
        miSansMediumFamily = QFontDatabase::applicationFontFamilies(miSansMediumId).at(0);
        qDebug() << "成功加载MiSans Medium字体:" << miSansMediumFamily;
    } else {
        qDebug() << "无法加载MiSans Medium字体，使用内置字体名称";
        miSansMediumFamily = "MiSans-Medium";
    }
    
    // 加载MISANS-HEAVY.TTF字体用于比分显示
    QString miSansHeavyPath = appDir + "/assets/fonts/MISANS-HEAVY.TTF";
    int miSansHeavyId = QFontDatabase::addApplicationFont(miSansHeavyPath);
    if (miSansHeavyId != -1) {
        miSansHeavyFamily = QFontDatabase::applicationFontFamilies(miSansHeavyId).at(0);
        qDebug() << "成功加载MiSans Heavy字体:" << miSansHeavyFamily;
    } else {
        qDebug() << "无法加载MiSans Heavy字体，使用内置字体名称";
        miSansHeavyFamily = "MiSans-Heavy";
    }
    
    fontsLoaded = true;
}

PlayerInfo::PlayerInfo(QWidget* parent) : QWidget(parent), ui(new Ui::PlayerInfo) {
    ui->setupUi(this);

    g   = &Ra2ob::Game::getInstance();
    gls = &Globalsetting::getInstance();
    
    // 确保字体只加载一次
    loadAllFonts();
    
    // 应用缩放比例到字体大小
    float fontRatio = gls->l.ratio;
    
    // 为击杀数和存活单位创建字体
    technoFont = QFont(technoGlitchFontFamily, qRound(14 * fontRatio));
    technoFont.setStyleStrategy(QFont::PreferAntialias);
    
    // 为余额创建更大的字体
    technoFontLarge = QFont(technoGlitchFontFamily, qRound(16 * fontRatio));
    technoFontLarge.setStyleStrategy(QFont::PreferAntialias);
    
    // 为玩家名称创建字体
    miSansBold = QFont(miSansBoldFamily, qRound(22 * fontRatio));
    miSansBold.setStyleStrategy(QFont::PreferAntialias);
    miSansBold.setWeight(QFont::Bold); // 明确设置字体为粗体
    
    // 为分数创建字体，使用Heavy字体
    miSansMedium = QFont(miSansHeavyFamily, qRound(25 * fontRatio));
    miSansMedium.setStyleStrategy(QFont::PreferAntialias);
    miSansMedium.setWeight(QFont::Black); // 明确设置字体粗细为最粗（相当于HEAVY）
    
    // 设置玩家昵称字体样式
    ui->lb_playerName->setStyleSheet("color: #ffffff;");
    // 设置玩家名称固定左对齐
    ui->lb_playerName->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    
    // 使用MiSans-Bold字体
    ui->lb_playerName->setFont(miSansBold);

    // 使用更大的Techno字体设置余额
    ui->lb_balance->setStyleSheet("color: #ffffff;");
    ui->lb_balance->setFont(technoFontLarge);
    
    // 初始化比分标签，使用Heavy字体
    ui->lb_score->setStyleSheet("color: #ffffff;");
    ui->lb_score->setFont(miSansMedium);
    ui->lb_score->setText("0");
    
    // 使用Techno字体设置击杀和存活单位
    ui->lb_kills->setStyleSheet("color: #ffffff;");
    ui->lb_kills->setFont(technoFont);
    
    ui->lb_alive->setStyleSheet("color: #ffffff;");
    ui->lb_alive->setFont(technoFont);
    
    // 设置比分标签可以接受鼠标事件
    ui->lb_score->setMouseTracking(true);
    ui->lb_score->installEventFilter(this);
    
    // 创建军衔标签
    rankLabel = new QLabel(this);
    // 应用缩放比例，初始位置和大小将在rearrange()中设置
    rankLabel->setScaledContents(true);
    rankLabel->setVisible(true);
    
    // 创建电力状态动画效果
    powerEffect = new QGraphicsOpacityEffect(ui->lb_power);
    ui->lb_power->setGraphicsEffect(powerEffect);
    
    // 创建电力状态动画
    powerAnimation = new QPropertyAnimation(powerEffect, "opacity");
    powerAnimation->setLoopCount(-1); // 无限循环
    powerAnimation->setEasingCurve(QEasingCurve::InOutSine); // 平滑的正弦曲线
    
    // 创建电力状态计时器，用于更新动画状态
    powerTimer = new QTimer(this);
    connect(powerTimer, &QTimer::timeout, this, &PlayerInfo::updatePowerAnimation);
    powerTimer->start(500); // 每100毫秒检查一次
    
    // 初始化电力状态
    powerState = PowerState::Normal;
    
    // 初始化网络管理器
    networkManager = new QNetworkAccessManager(this);
    
    // 初始化头像刷新定时器（50秒刷新一次）
    avatarRefreshTimer = new QTimer(this);
    connect(avatarRefreshTimer, &QTimer::timeout, this, &PlayerInfo::resetAvatarLoadStatus);
    avatarRefreshTimer->setInterval(50000); // 设置为50秒
    avatarRefreshTimer->start();
    
    rearrange();
}

PlayerInfo::~PlayerInfo() { 
    // 停止动画和计时器
    powerAnimation->stop();
    powerTimer->stop();
    avatarRefreshTimer->stop();
    
    // 释放资源
    delete powerAnimation;
    delete powerEffect;
    delete powerTimer;
    delete avatarRefreshTimer;
    delete rankLabel;
    delete ui; 
    
    // 释放网络管理器
    if (networkManager) {
        networkManager->deleteLater();
    }
}

// 从配置文件获取玩家军衔
QString PlayerInfo::getRankFromConfig(const QString &nickname) {
    if (nickname.isEmpty()) {
        return QString();
    }
    
    // 读取配置文件
    QString configPath = QCoreApplication::applicationDirPath() + "/config/config.json";
    QFile configFile(configPath);
    
    if (!configFile.exists()) {
        qDebug() << "配置文件不存在:" << configPath;
        return QString();
    }
    
    if (!configFile.open(QIODevice::ReadOnly)) {
        qDebug() << "无法打开配置文件:" << configPath;
        return QString();
    }
    
    QByteArray configData = configFile.readAll();
    configFile.close();
    
    QJsonDocument doc = QJsonDocument::fromJson(configData);
    if (doc.isNull() || !doc.isObject()) {
        qDebug() << "配置文件格式无效";
        return QString();
    }
    
    QJsonObject config = doc.object();
    
    // 检查是否存在rank部分
    if (!config.contains("rank") || !config["rank"].isObject()) {
        qDebug() << "配置文件中不存在rank部分或格式无效";
        return QString();
    }
    
    QJsonObject rankObj = config["rank"].toObject();
    
    // 检查是否存在nickname部分
    if (!rankObj.contains("nickname") || !rankObj["nickname"].isObject()) {
        qDebug() << "配置文件中rank部分不存在nickname或格式无效";
        return QString();
    }
    
    QJsonObject nicknameObj = rankObj["nickname"].toObject();
    
    // 检查是否存在对应的昵称
    if (nicknameObj.contains(nickname)) {
        QString rankType = nicknameObj[nickname].toString();
        qDebug() << "找到玩家" << nickname << "的军衔:" << rankType;
        return rankType;
    }
    
    qDebug() << "未找到玩家" << nickname << "的军衔配置";
    return QString();
}

// 设置玩家军衔
void PlayerInfo::setRank(const QString &rankType) {
    if (rankType.isEmpty()) {
        rankLabel->clear();
        rankLabel->setVisible(false);
        return;
    }
    
    QString rankImagePath = QCoreApplication::applicationDirPath() + "/assets/Rank/" + rankType + ".png";
    QFile rankFile(rankImagePath);
    
    if (!rankFile.exists()) {
        qDebug() << "军衔图片不存在:" << rankImagePath;
        rankLabel->clear();
        rankLabel->setVisible(false);
        return;
    }
    
    rankPixmap.load(rankImagePath);
    if (rankPixmap.isNull()) {
        qDebug() << "无法加载军衔图片:" << rankImagePath;
        rankLabel->clear();
        rankLabel->setVisible(false);
        return;
    }
    
    // 计算适当的军衔图片大小（考虑分辨率）
    int rankSize = qRound(30 * gls->l.ratio); // 与rearrange中的军衔大小一致
    
    // 缩放军衔图片
    QPixmap scaledRankPixmap = rankPixmap.scaled(rankSize, rankSize, 
                                              Qt::KeepAspectRatio, 
                                              Qt::SmoothTransformation);
    
    rankLabel->setPixmap(scaledRankPixmap);
    rankLabel->setVisible(true);
    
    qDebug() << "设置军衔图片:" << rankImagePath << "，大小:" << rankSize << "x" << rankSize;
}

void PlayerInfo::paintEvent(QPaintEvent *event) {
    QPainter painter(this);
    
    // 启用抗锯齿
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    
    // 调用父类的绘制方法，确保其他元素正常显示
    QWidget::paintEvent(event);
}

void PlayerInfo::setMirror() {
    // 记录当前的镜像状态
    bool wasMirrored = mirrored;
    
    // 设置为镜像模式
    mirrored = true;
    
    // 如果已经是镜像模式，不需要重复设置
    if (wasMirrored) {
        return;
    }
    
    // 重新排列所有元素
    rearrange();
}

void PlayerInfo::setAll(int index) {
    if (index < 0 || index >= Ra2ob::MAXPLAYER || !g->_gameInfo.valid || !g->_gameInfo.players[index].valid) {
        return;
    }

    // 保存当前玩家索引
    currentPlayerIndex = index;

    setPlayerNameByIndex(index);
    setBalanceByIndex(index);
    setKillsByIndex(index);
    setAliveUnitsByIndex(index);
    setPowerByIndex(index);
    
    // 根据昵称更新比分
    updateScoreFromNickname();
    
    // 获取玩家昵称
    std::string name = g->_gameInfo.players[index].panel.playerNameUtf;
    QString nickname = QString::fromUtf8(name);
    
    // 设置军衔
    QString rankType = getRankFromConfig(nickname);
    setRank(rankType);
    
    // 只有在以下情况才尝试更新头像：
    // 1. 昵称发生变化
    // 2. 远程头像尚未成功加载
    // 3. 头像为空
    if (nickname != currentNickname || !remoteAvatarLoaded || playerAvatar.isNull()) {
        // 更新当前昵称
        currentNickname = nickname;
        
        // 尝试设置头像
        setAvatar(QPixmap());
    }
}

void PlayerInfo::setPlayerNameByIndex(int index) {
    qDebug() << "开始设置玩家" << index << "的名称";
    
    // 保存当前玩家索引
    currentPlayerIndex = index;
    
    refreshPlayerName(index);
}

void PlayerInfo::refreshPlayerName(int index) {
    if (!g || !g->_gameInfo.valid || index < 0 || index >= Ra2ob::MAXPLAYER || !g->_gameInfo.players[index].valid) {
        return;
    }

    std::string name = g->_gameInfo.players[index].panel.playerNameUtf;
    QString nickname = QString::fromUtf8(name);
    
    // 检查昵称是否变化
    bool nicknameChanged = (nickname != currentNickname);
    
    // 保存当前玩家昵称
    currentNickname = nickname;

    QString playername = gls->findNameByNickname(nickname);
    ui->lb_playerName->setText(playername.isEmpty() ? nickname : playername);
    ui->lb_playerName->adjustSize();
    
    // 更新昵称后，同时更新比分
    updateScoreFromNickname();
    
    // 更新军衔
    QString rankType = getRankFromConfig(nickname);
    setRank(rankType);
    
    // 如果昵称变化，重置远程头像加载标志并更新头像
    if (nicknameChanged) {
        remoteAvatarLoaded = false;
        setAvatar(QPixmap());
    }
    
    // 重新排列所有元素以确保布局正确
    rearrange();
}

void PlayerInfo::setBalanceByIndex(int index) {
    qDebug() << "开始设置玩家" << index << "的余额";
    
    if (!g || !g->_gameInfo.valid || index < 0 || index >= Ra2ob::MAXPLAYER || !g->_gameInfo.players[index].valid) {
        qDebug() << "无效的玩家索引或游戏状态";
        return;
    }
    
    int num = g->_gameInfo.players[index].panel.balance;
    qDebug() << "玩家" << index << "余额:" << num;
    ui->lb_balance->setText("$ " + QString::number(num));
}

void PlayerInfo::setKillsByIndex(int index) {
    qDebug() << "开始设置玩家" << index << "的击杀数";
    
    if (!g || !g->_gameInfo.valid || index < 0 || index >= Ra2ob::MAXPLAYER || !g->_gameInfo.players[index].valid) {
        qDebug() << "无效的玩家索引或游戏状态";
        return;
    }
    
    int kills = g->_gameInfo.players[index].score.kills;
    qDebug() << "玩家" << index << "击杀数:" << kills;
    ui->lb_kills->setText(QString::number(kills));
}

void PlayerInfo::setAliveUnitsByIndex(int index) {
    qDebug() << "开始设置玩家" << index << "的存活单位数";
    
    if (!g || !g->_gameInfo.valid || index < 0 || index >= Ra2ob::MAXPLAYER || !g->_gameInfo.players[index].valid) {
        qDebug() << "无效的玩家索引或游戏状态";
        return;
    }
    
    int alive = g->_gameInfo.players[index].score.alive;
    qDebug() << "玩家" << index << "存活单位数:" << alive;
    ui->lb_alive->setText(QString::number(alive));
}

void PlayerInfo::setPowerByIndex(int index) {
    qDebug() << "开始设置玩家" << index << "的电力状态";
    
    if (!g || !g->_gameInfo.valid || index < 0 || index >= Ra2ob::MAXPLAYER || !g->_gameInfo.players[index].valid) {
        qDebug() << "无效的玩家索引或游戏状态";
        return;
    }
    
    int powerDrain = g->_gameInfo.players[index].panel.powerDrain;
    int powerOutput = g->_gameInfo.players[index].panel.powerOutput;
    
    QString color;
    PowerState newState = PowerState::Normal;
    
    if (powerDrain == 0 && powerOutput == 0) {
        // 初始状态
        color = "grey";
        newState = PowerState::Normal;
    } else if (powerDrain > 0 && powerOutput == 0) {
        // 完全断电
        color = "red";
        newState = PowerState::Critical;
    } else if (powerOutput > powerDrain) {
        if (powerOutput * 0.85 < powerDrain) {
            // 电力紧张
            color = "yellow";
            newState = PowerState::Warning;
        } else {
            // 电力充足
            color = "green";
            newState = PowerState::Normal;
        }
    } else {
        // 电力不足
        color = "red";
        newState = PowerState::Critical;
    }
    
    // 更新电力状态
    powerState = newState;
    
    // 确保电量指示器大小与缩放匹配
    int powerSize = qRound(16 * gls->l.ratio);
    ui->lb_power->setFixedSize(powerSize, powerSize);
    
    // 要使正方形变成圆形，border-radius需要是宽度的一半
    int borderRadius = powerSize / 2;
    
    // 设置电力指示器样式
    QString style;
    if (newState == PowerState::Critical) {
        // 红色警报状态，使用径向渐变效果增强视觉效果，更鲜艳的红色
        style = QString("background-color: qradialgradient(cx:0.5, cy:0.5, radius:0.8, fx:0.5, fy:0.5, stop:0 #ff3333, stop:1 #ff0000); "
                       "border-radius: %1px; width: %2px; height: %2px;").arg(borderRadius).arg(powerSize);
    } else if (newState == PowerState::Warning) {
        // 黄色警告状态
        style = QString("background-color: qradialgradient(cx:0.5, cy:0.5, radius:0.8, fx:0.5, fy:0.5, stop:0 #ffff77, stop:1 #dddd00); "
                       "border-radius: %1px; width: %2px; height: %2px;").arg(borderRadius).arg(powerSize);
    } else if (color == "green") {
        // 绿色正常状态，更鲜亮的绿色
        style = QString("background-color: qradialgradient(cx:0.5, cy:0.5, radius:0.8, fx:0.5, fy:0.5, stop:0 #33ff33, stop:1 #00cc00); "
                       "border-radius: %1px; width: %2px; height: %2px;").arg(borderRadius).arg(powerSize);
    } else {
        // 其他正常状态
        style = QString("background-color: %1; "
                       "border-radius: %2px; width: %3px; height: %3px;").arg(color).arg(borderRadius).arg(powerSize);
    }
    
    ui->lb_power->setStyleSheet(style);
    
    qDebug() << "玩家" << index << "电力状态已更新，颜色:" << color << "，状态:" << (int)newState;
}

// 设置玩家比分
void PlayerInfo::setScore(int score) {
    playerScore = score;
    ui->lb_score->setText(QString::number(score));
    qDebug() << "玩家比分已更新为:" << score;
}

// 获取玩家比分
int PlayerInfo::getScore() const {
    return playerScore;
}

// 根据昵称更新比分
void PlayerInfo::updateScoreFromNickname() {
    if (currentNickname.isEmpty()) {
        return;
    }
    
    // 从全局映射中查找当前昵称对应的比分
    if (gls->playerScores.contains(currentNickname)) {
        int score = gls->playerScores[currentNickname];
        setScore(score);
        qDebug() << "根据昵称" << currentNickname << "更新比分为" << score;
    } else {
        // 如果没有找到，设置为0
        setScore(0);
        qDebug() << "昵称" << currentNickname << "没有对应的比分记录，设置为0";
    }
}

// 获取当前玩家昵称
QString PlayerInfo::getCurrentNickname() const {
    return currentNickname;
}

void PlayerInfo::rearrange() {
    // 面板宽度和高度
    int panelWidth = this->width();
    int panelHeight = this->height();
    
    // 应用缩放比例
    float ratio = gls->l.ratio;
    
    // 定义基础位置和尺寸
    // 所有尺寸都是针对左侧玩家(非镜像)设计的
    // 右侧玩家(镜像)会自动计算镜像位置
    
    // 头像位置和尺寸
    int avatarSize = qRound(48 * ratio);
    int avatarX = qRound(255 * ratio); // 固定X位置
    int avatarY = qRound(12 * ratio);  // 固定Y位置
    
    // 玩家名称位置和尺寸
    int nameX = qRound(73 * ratio); // 固定X位置
    int nameY = qRound(25 * ratio); // 固定Y位置
    int nameWidth = qRound(153 * ratio);
    int nameHeight = qRound(25 * ratio);
    
    // 比分位置和尺寸
    int scoreWidth = qRound(40 * ratio);
    int scoreHeight = qRound(30 * ratio);
    int scoreX = qRound(11 * ratio); // 固定X位置
    int scoreY = qRound(25 * ratio); // 固定Y位置
    
    // 余额位置和尺寸
    int balanceX = qRound(460 * ratio); // 固定X位置
    int balanceY = qRound(28 * ratio);  // 固定Y位置
    int balanceWidth = qRound(60 * ratio);
    int balanceHeight = qRound(18 * ratio);
    
    // 击杀数位置和尺寸
    int killsX = qRound(346 * ratio); // 固定X位置
    int killsY = qRound(45 * ratio);  // 固定Y位置
    int killsWidth = qRound(60 * ratio);
    int killsHeight = qRound(18 * ratio);
    
    // 存活单位数位置和尺寸
    int aliveX = qRound(310 * ratio); // 固定X位置
    int aliveY = qRound(45 * ratio);  // 固定Y位置
    int aliveWidth = qRound(60 * ratio);
    int aliveHeight = qRound(18 * ratio);
    
    // 电力状态指示器位置和尺寸
    int powerSize = qRound(16 * ratio);
    int powerX = qRound(405 * ratio); // 固定X位置
    int powerY = qRound(47 * ratio);  // 固定Y位置
    
    // 军衔标签位置和尺寸
    int rankSize = qRound(30 * ratio);
    int rankX = qRound(290 * ratio); // 固定X位置
    int rankY = qRound(38 * ratio);  // 固定Y位置
    
    // 根据镜像状态设置位置
    if (mirrored) {
        // 镜像模式(右侧玩家)
        // 需要水平翻转所有位置
        
        // 镜像模式下使用默认位置设置
        ui->lb_avatar->setGeometry(panelWidth - avatarX - avatarSize, avatarY, avatarSize, avatarSize);
        
        ui->lb_playerName->setGeometry(panelWidth - nameX - nameWidth, nameY, nameWidth, nameHeight);
        ui->lb_playerName->setAlignment(Qt::AlignCenter | Qt::AlignVCenter);
        
        ui->lb_score->setGeometry(panelWidth - scoreX - scoreWidth, scoreY, scoreWidth, scoreHeight);
        ui->lb_score->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        
        ui->lb_balance->setGeometry(panelWidth - balanceX - balanceWidth, balanceY, balanceWidth, balanceHeight);
        ui->lb_balance->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        
        ui->lb_kills->setGeometry(panelWidth - killsX - killsWidth, killsY, killsWidth, killsHeight);
        ui->lb_kills->setAlignment(Qt::AlignCenter | Qt::AlignVCenter);
        
        ui->lb_alive->setGeometry(panelWidth - aliveX - aliveWidth, aliveY, aliveWidth, aliveHeight);
        ui->lb_alive->setAlignment(Qt::AlignCenter | Qt::AlignVCenter);
        
        ui->lb_power->setGeometry(panelWidth - powerX - powerSize, powerY, powerSize, powerSize);
        ui->lb_power->setFixedSize(powerSize, powerSize); // 确保电量指示器大小与缩放匹配
        
        // 镜像模式下军衔标签位置
        rankLabel->setGeometry(panelWidth - rankX - rankSize, rankY, rankSize, rankSize);
    } else {
        // 正常模式(左侧玩家)，使用默认位置设置
        
        // 头像位置(左侧)
        ui->lb_avatar->setGeometry(avatarX, avatarY, avatarSize, avatarSize);
        
        // 玩家名称(居中对齐)
        ui->lb_playerName->setGeometry(nameX, nameY, nameWidth, nameHeight);
        ui->lb_playerName->setAlignment(Qt::AlignCenter | Qt::AlignVCenter);
        
        // 比分(右侧)
        ui->lb_score->setGeometry(scoreX, scoreY, scoreWidth, scoreHeight);
        ui->lb_score->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        
        // 余额(左对齐)
        ui->lb_balance->setGeometry(balanceX, balanceY, balanceWidth, balanceHeight);
        
        // 击杀数(居中对齐)
        ui->lb_kills->setGeometry(killsX, killsY, killsWidth, killsHeight);
        ui->lb_kills->setAlignment(Qt::AlignCenter | Qt::AlignVCenter);
        
        // 存活单位数(居中对齐)
        ui->lb_alive->setGeometry(aliveX, aliveY, aliveWidth, aliveHeight);
        ui->lb_alive->setAlignment(Qt::AlignCenter | Qt::AlignVCenter);
        
        // 电力状态指示器(左侧)
        ui->lb_power->setGeometry(powerX, powerY, powerSize, powerSize);
        ui->lb_power->setFixedSize(powerSize, powerSize); // 确保电量指示器大小与缩放匹配
        
        // 正常模式下军衔标签位置
        rankLabel->setGeometry(rankX, rankY, rankSize, rankSize);
    }
    
    // 设置字体
    ui->lb_balance->setFont(technoFontLarge); // 余额使用更大字体
    ui->lb_kills->setFont(technoFont);
    ui->lb_alive->setFont(technoFont);
    
    // 设置比分字体
    ui->lb_score->setFont(miSansMedium);
    
    // 设置样式
    ui->lb_playerName->setStyleSheet("color: #ffffff;");
    ui->lb_balance->setStyleSheet("color: #ffffff;"); 
    ui->lb_kills->setStyleSheet("color: #ffffff;"); 
    ui->lb_alive->setStyleSheet("color: #ffffff;"); 
    ui->lb_score->setStyleSheet("color: #ffffff;");
    
    // 确保所有标签都可见
    ui->lb_playerName->show();
    ui->lb_balance->show();
    ui->lb_kills->show();
    ui->lb_alive->show();
    ui->lb_power->show();
    ui->lb_score->show();
    ui->lb_avatar->show();
    rankLabel->show();
    
    update(); // 重新绘制
}

int PlayerInfo::getInsufficientFund(int index) {
    // 检查游戏和玩家数据是否有效
    if (!g || !g->_gameInfo.valid || index < 0 || index >= Ra2ob::MAXPLAYER || !g->_gameInfo.players[index].valid) {
        return 1; // 如果数据无效，默认返回资金充足，避免错误显示红色
    }
    
    // 获取余额
    int num = g->_gameInfo.players[index].panel.balance;
    
    // 验证余额数据是否有效（非负且在合理范围内）
    // 红警2中通常初始资金为10,000，最大可能不超过100,000
    if (num < 0 || num > 100000) {
        qDebug() << "玩家" << index << "余额数据异常:" << num << "，视为资金充足";
        return 1; // 数据异常时视为资金充足
    }
    
    // 游戏开始阶段（前100帧）不显示资金不足状态，避免初始化阶段的误判
    if (g->_gameInfo.currentFrame < 100) {
        return 1; // 游戏初期默认显示资金充足
    }
    
    // 正常判断资金是否不足
    if (num < 30) {  // 资金不足的阈值为30
        return 0;    // 资金不足
    }
    
    return 1;        // 资金充足
}

// 设置玩家头像
void PlayerInfo::setAvatar(const QPixmap &avatar) {
    // 如果传入的头像不为空，直接使用
    if (!avatar.isNull()) {
        playerAvatar = avatar;
        displayAvatar();
        return;
    }
    
    // 首先确保获取最新的游戏名称
    QString gameNameFromNickname;
    if (!currentNickname.isEmpty()) {
        gameNameFromNickname = gls->findNameByNickname(currentNickname);
        qDebug() << "玩家昵称:" << currentNickname << "对应的游戏名:" << gameNameFromNickname;
    }
    
    // 首先尝试加载本地头像
    if (!currentNickname.isEmpty()) {
        // 1. 首先尝试使用游戏名称查找本地头像
        if (!gameNameFromNickname.isEmpty()) {
            QString localGameNamePath = QCoreApplication::applicationDirPath() + "/avatars/" + gameNameFromNickname + ".png";
            if (QFile::exists(localGameNamePath)) {
                QPixmap localAvatar(localGameNamePath);
                if (!localAvatar.isNull()) {
                    qDebug() << "成功加载本地头像(游戏名):" << localGameNamePath;
                    playerAvatar = localAvatar;
                    // 标记为已成功加载头像，避免重复尝试
                    remoteAvatarLoaded = true;
                    displayAvatar();
                    return;
                }
            }
        }
        
        // 2. 如果游戏名称头像不存在，尝试使用昵称查找本地头像
        QString localAvatarPath = QCoreApplication::applicationDirPath() + "/avatars/" + currentNickname + ".png";
        if (QFile::exists(localAvatarPath)) {
            QPixmap localAvatar(localAvatarPath);
            if (!localAvatar.isNull()) {
                qDebug() << "成功加载本地头像(昵称):" << localAvatarPath;
                playerAvatar = localAvatar;
                // 标记为已成功加载头像，避免重复尝试
                remoteAvatarLoaded = true;
                displayAvatar();
                return;
            }
        }
        
        // 本地头像都不存在，尝试从远程服务器加载
        // 生成时间戳，用于避免缓存
        QString timestamp = QString::number(QDateTime::currentMSecsSinceEpoch());
        
        // 3. 优先使用游戏名称查找远程头像
        if (!gameNameFromNickname.isEmpty()) {
            // TODO: 配置头像服务器地址
        QString remoteGameNameUrl = QString("https://your-server.com/avatars/%1.png?t=%2").arg(gameNameFromNickname).arg(timestamp);
            if (checkRemoteFileExists(remoteGameNameUrl)) {
                // 远程头像存在，下载并显示
                downloadAvatar(remoteGameNameUrl);
                return;
            }
        }
        
        // 4. 如果游戏名称不存在或找不到对应头像，尝试使用昵称查找远程头像
        // TODO: 配置头像服务器地址
        QString remoteNicknameUrl = QString("https://www.ra2pvp.com/avatars/%1.png?t=%2").arg(currentNickname).arg(timestamp);
        if (checkRemoteFileExists(remoteNicknameUrl)) {
            // 远程头像存在，下载并显示
            downloadAvatar(remoteNicknameUrl);
            return;
        }
    }
    
    // 如果远程头像都不存在，标记为已完成远程加载
    // 这样避免了在每次更新状态时重复尝试加载不存在的远程头像
    remoteAvatarLoaded = true;
    
    // 使用默认的阵营头像
    useDefaultAvatar();
}

// 检查远程文件是否存在
bool PlayerInfo::checkRemoteFileExists(const QString &url) {
    QNetworkRequest request(url);
    request.setRawHeader("User-Agent", "Ra2ob Qt Client");
    
    // 添加防缓存头
    request.setAttribute(QNetworkRequest::CacheLoadControlAttribute, QNetworkRequest::AlwaysNetwork);
    request.setRawHeader("Cache-Control", "no-cache");
    request.setRawHeader("Pragma", "no-cache");
    
    // 使用HEAD请求检查文件是否存在，减少数据传输量
    QNetworkReply *reply = networkManager->head(request);
    
    // 使用事件循环等待响应完成，避免异步问题
    QEventLoop loop;
    connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();
    
    bool exists = (reply->error() == QNetworkReply::NoError);
    
    if (exists) {
        qDebug() << "远程头像存在:" << url;
    } else {
        qDebug() << "远程头像不存在:" << url << "，错误:" << reply->errorString();
    }
    
    reply->deleteLater();
    return exists;
}

// 下载头像
void PlayerInfo::downloadAvatar(const QString &url) {
    QNetworkRequest request(url);
    request.setRawHeader("User-Agent", "Ra2ob Qt Client");
    
    // 添加防缓存头
    request.setAttribute(QNetworkRequest::CacheLoadControlAttribute, QNetworkRequest::AlwaysNetwork);
    request.setRawHeader("Cache-Control", "no-cache");
    request.setRawHeader("Pragma", "no-cache");
    
    QNetworkReply *reply = networkManager->get(request);
    
    // 使用事件循环等待下载完成，避免异步问题
    QEventLoop loop;
    connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();
    
    if (reply->error() == QNetworkReply::NoError) {
        QByteArray imageData = reply->readAll();
        
        // 将图像数据加载到QPixmap
        QPixmap remoteAvatar;
        if (remoteAvatar.loadFromData(imageData)) {
            qDebug() << "成功下载头像:" << url;
            
            // 设置头像
            playerAvatar = remoteAvatar;
            
            // 标记为已成功加载远程头像
            remoteAvatarLoaded = true;
            
            displayAvatar();
        } else {
            qDebug() << "无法加载下载的头像数据";
            remoteAvatarLoaded = true; // 即使加载失败也标记为已尝试
            useDefaultAvatar();
        }
    } else {
        qDebug() << "下载头像失败:" << url << "，错误:" << reply->errorString();
        remoteAvatarLoaded = true; // 即使下载失败也标记为已尝试
        useDefaultAvatar();
    }
    
    reply->deleteLater();
}

// 使用默认阵营头像
void PlayerInfo::useDefaultAvatar() {
        QString defaultAvatarPath;
        
        // 检查当前玩家索引是否有效
        if (currentPlayerIndex >= 0 && currentPlayerIndex < Ra2ob::MAXPLAYER && 
            g->_gameInfo.valid && g->_gameInfo.players[currentPlayerIndex].valid) {
            
            // 获取玩家国家
            std::string country = g->_gameInfo.players[currentPlayerIndex].panel.country;
            QString countryStr = QString::fromStdString(country);
            
            // 根据国家选择头像 - 正确区分阵营
            if (countryStr == "Russians" || countryStr == "Cuba" || 
                countryStr == "Iraq" || countryStr == "Libya") {
                // 苏联阵营使用logo2.png
                defaultAvatarPath = QCoreApplication::applicationDirPath() + "/assets/panels/logo2.png";
                qDebug() << "使用苏联阵营头像，国家:" << countryStr;
            } else if (countryStr == "Americans" || countryStr == "British" || 
                       countryStr == "France" || countryStr == "Germans" || 
                       countryStr == "Korea") {
                // 盟军阵营使用logo.png
                defaultAvatarPath = QCoreApplication::applicationDirPath() + "/assets/panels/logo.png";
                qDebug() << "使用盟军阵营头像，国家:" << countryStr;
            } else if (countryStr == "Yuri") {
                // 尤里阵营可以使用专门的头像
                defaultAvatarPath = QCoreApplication::applicationDirPath() + "/assets/panels/logo3.png";
                qDebug() << "使用尤里阵营头像";
            }
        }
        
    // 如果没有确定特定国家头像或文件不存在，使用默认头像
        if (defaultAvatarPath.isEmpty() || !QFile::exists(defaultAvatarPath)) {
            defaultAvatarPath = QCoreApplication::applicationDirPath() + "/avatars/default.png";
            
            // 如果默认头像也不存在，使用程序图标
            if (!QFile::exists(defaultAvatarPath)) {
                playerAvatar.load(":/icon/assets/icons/icon_64.png");
                qDebug() << "使用程序图标作为默认头像";
            displayAvatar();
            return;
            }
        }
        
        qDebug() << "使用默认头像:" << defaultAvatarPath;
        playerAvatar.load(defaultAvatarPath);
    displayAvatar();
    }
    
// 显示当前加载的头像
void PlayerInfo::displayAvatar() {
    // 计算适当的头像大小（考虑分辨率）
    int avatarSize = qRound(49 * gls->l.ratio); // 与rearrange中的头像大小一致
    
    qDebug() << "显示头像，应用缩放后的尺寸:" << avatarSize << "x" << avatarSize;
    
    // 将头像设置为圆形
    QPixmap scaledAvatar = playerAvatar.scaled(avatarSize, avatarSize, 
                                            Qt::KeepAspectRatio, 
                                            Qt::SmoothTransformation);
    QPixmap roundedAvatar(scaledAvatar.size());
    roundedAvatar.fill(Qt::transparent);
    
    QPainter painter(&roundedAvatar);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    
    QPainterPath path;
    path.addEllipse(roundedAvatar.rect());
    painter.setClipPath(path);
    painter.drawPixmap(0, 0, scaledAvatar);
    
    // 设置到标签
    ui->lb_avatar->setPixmap(roundedAvatar);
    ui->lb_avatar->setFixedSize(avatarSize, avatarSize); // 确保头像标签大小与缩放匹配
    
    // 确保圆角样式与头像大小匹配
    int borderRadius = avatarSize / 2; // 圆形，所以半径是尺寸的一半
    ui->lb_avatar->setStyleSheet(QString("border-radius: %1px;").arg(borderRadius));
    
    // 确保布局正确
    rearrange();
}

// 重写事件过滤器以处理鼠标点击事件
bool PlayerInfo::eventFilter(QObject *watched, QEvent *event) {
    // 检查是否是分数标签上的鼠标事件
    if (watched == ui->lb_score) {
        if (event->type() == QEvent::MouseButtonPress) {
            QMouseEvent *mouseEvent = static_cast<QMouseEvent*>(event);
            
            // 检查Ctrl键是否按下
            if (QApplication::keyboardModifiers() & Qt::ControlModifier) {
                // 左键点击增加分数
                if (mouseEvent->button() == Qt::LeftButton) {
                    playerScore++;
                    ui->lb_score->setText(QString::number(playerScore));
                    
                    // 更新全局分数记录
                    if (!currentNickname.isEmpty()) {
                        gls->playerScores[currentNickname] = playerScore;
                        qDebug() << "玩家" << currentNickname << "分数增加到" << playerScore;
                        
                        // 发送分数变化信号
                        emit scoreChanged(currentNickname, playerScore);
                    }
                    
                    return true;
                }
                // 右键点击减少分数
                else if (mouseEvent->button() == Qt::RightButton) {
                    if (playerScore > 0) {
                        playerScore--;
                        ui->lb_score->setText(QString::number(playerScore));
                        
                        // 更新全局分数记录
                        if (!currentNickname.isEmpty()) {
                            gls->playerScores[currentNickname] = playerScore;
                            qDebug() << "玩家" << currentNickname << "分数减少到" << playerScore;
                            
                            // 发送分数变化信号
                            emit scoreChanged(currentNickname, playerScore);
                        }
                    }
                    
                    return true;
                }
            }
        }
    }
    
    return QWidget::eventFilter(watched, event);
}

// 添加电力状态动画更新函数
void PlayerInfo::updatePowerAnimation() {
    // 根据当前电力状态更新动画
    switch (powerState) {
        case PowerState::Critical: // 红色警报状态
            if (powerAnimation->state() != QAbstractAnimation::Running) {
                powerAnimation->setDuration(300); // 超快的闪烁速度，300毫秒一个周期
                powerAnimation->setStartValue(1.0);
                powerAnimation->setEndValue(0.6); // 更低的不透明度，增强对比度
                powerAnimation->start();
            }
            break;
            
        case PowerState::Warning: // 黄色警告状态 - 不闪烁
            if (powerAnimation->state() == QAbstractAnimation::Running) {
                powerAnimation->stop();
                powerEffect->setOpacity(1.0); // 恢复正常不透明度
            }
            break;
            
        default: // 正常状态或其他状态
            if (powerAnimation->state() == QAbstractAnimation::Running) {
                powerAnimation->stop();
                powerEffect->setOpacity(1.0); // 恢复正常不透明度
            }
            break;
    }
}

// 添加一个updateScaling方法，在分辨率变化时刷新所有元素
void PlayerInfo::updateScaling() {
    // 应用缩放比例到字体
    float fontRatio = gls->l.ratio;
    
    // 更新字体大小
    technoFont.setPointSize(qRound(14 * fontRatio));
    technoFontLarge.setPointSize(qRound(16 * fontRatio));
    miSansBold.setPointSize(qRound(22 * fontRatio));
    miSansMedium.setPointSize(qRound(25 * fontRatio));
    
    // 更新标签字体
    ui->lb_balance->setFont(technoFontLarge);
    ui->lb_kills->setFont(technoFont);
    ui->lb_alive->setFont(technoFont);
    ui->lb_playerName->setFont(miSansBold);
    ui->lb_score->setFont(miSansMedium);
    
    // 如果有军衔图片，重新设置以应用新的缩放
    if (!rankPixmap.isNull()) {
        int rankSize = qRound(30 * gls->l.ratio);
        QPixmap scaledRankPixmap = rankPixmap.scaled(rankSize, rankSize, 
                                                  Qt::KeepAspectRatio, 
                                                  Qt::SmoothTransformation);
        rankLabel->setPixmap(scaledRankPixmap);
    }
    
    // 如果有头像，重新设置以应用新的缩放
    if (!playerAvatar.isNull()) {
        setAvatar(playerAvatar);
    }
    
    // 更新电量指示器的大小和样式
    if (currentPlayerIndex >= 0) {
        setPowerByIndex(currentPlayerIndex); // 重新应用电力状态，这会更新大小和样式
    }
    
    // 重新排列所有元素
    rearrange();
}

// 添加resetAvatarLoadStatus方法的实现
void PlayerInfo::resetAvatarLoadStatus() {
    // 重置远程头像加载状态标志，这将使头像重新加载
    remoteAvatarLoaded = false;
    qDebug() << "定时刷新头像 - 当前玩家:" << currentNickname;
    
    // 如果有当前玩家，重新加载头像
    if (!currentNickname.isEmpty()) {
        // 尝试重新加载头像
        setAvatar(QPixmap());
    }
}
