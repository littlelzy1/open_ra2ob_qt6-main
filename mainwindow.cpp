#include "./mainwindow.h"

#include <QCloseEvent>
#include <QHideEvent>
#include <QPainter>
#include <QProcess>
#include <QTimer>
#include <QLayout>
#include <QFileDialog>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>
#include <QDir>
#include <QCoreApplication>
#include <QRegularExpression>
#include <QDebug>
#include <QDesktopServices>
#include <QFile>
#include <QFontDialog>
#include <QFontMetrics>
#include <QMenu>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPixmap>
#include <QScreen>
#include <QSettings>
#include <QUrl>
#include <QWindow>
#include <QLabel>
#include <QSpinBox>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QPushButton>
#include <QCheckBox>
#include <QGroupBox>
#include <QHttpMultiPart>
#include <QMimeDatabase>
#include <QUrlQuery>
#include <QLineEdit>
#include <QFormLayout>
#include <QTime>
#include <QTabWidget>

#include "./configmanager.h"
#include "./ui_mainwindow.h"
#include "./ob1.h"

// 定义API密钥常量
// TODO: 配置API密钥 - 请在配置文件中设置您的API密钥
const QString API_KEY = "YOUR_API_KEY_HERE";

// 获取设备唯一标识符的静态方法
QString getDeviceIdentifier() {
    // 创建一个临时的AuthManager实例
    AuthManager authManager;
    // 获取CPU ID和MAC地址组合的唯一标识
    return authManager.getCpuId();
}

MainWindow::MainWindow(QWidget *parent, ConfigManager *cfgm)
    : QMainWindow(parent), ui(new Ui::MainWindow) {
    ui->setupUi(this);
    
    // 获取用户电脑当前的分辨率
    QScreen *screen = QGuiApplication::primaryScreen();
    if (screen) {
        QRect screenGeometry = screen->geometry();
        int screenWidth = screenGeometry.width();
        int screenHeight = screenGeometry.height();
        
        qDebug() << "当前屏幕分辨率:" << screenWidth << "x" << screenHeight;
        
        // 将分辨率信息保存到全局设置中，供其他模块使用
        Globalsetting &gls = Globalsetting::getInstance();
        gls.currentScreenWidth = screenWidth;
        gls.currentScreenHeight = screenHeight;
    }

    // 设置所有昵称输入框为只读模式 - 禁止用户输入
    // 但保持显示名称可编辑
    // 1V1模式
    ui->le_p1nickname->setReadOnly(true);
    ui->le_p2nickname->setReadOnly(true);
    
    // 1V1科技主题模式
    ui->le_p1nickname_tech->setReadOnly(true);
    ui->le_p2nickname_tech->setReadOnly(true);
    
    // 2V2模式
    ui->le_team1_p1_nickname->setReadOnly(true);
    ui->le_team1_p2_nickname->setReadOnly(true);
    ui->le_team2_p1_nickname->setReadOnly(true);
    ui->le_team2_p2_nickname->setReadOnly(true);
    
    // 3V3模式
    ui->le_team1_p1_nickname_3v3->setReadOnly(true);
    ui->le_team1_p2_nickname_3v3->setReadOnly(true);
    ui->le_team1_p3_nickname_3v3->setReadOnly(true);
    ui->le_team2_p1_nickname_3v3->setReadOnly(true);
    ui->le_team2_p2_nickname_3v3->setReadOnly(true);
    ui->le_team2_p3_nickname_3v3->setReadOnly(true);

    ui->mode1v1->installEventFilter(this);

    this->setWindowIcon(QIcon(":/icon/assets/icons/icon_16.png"));
    this->setWindowTitle(tr("Ra2ob免费插件"));
    this->setWindowFlags(Qt::MSWindowsFixedSizeDialogHint);

    detectShortcutStatus();

    gls = &Globalsetting::getInstance();

    // 初始化网络管理器
    networkManager = new QNetworkAccessManager(this);

    // 直接使用固定服务器URL
    // TODO: 配置头像上传服务器地址
    serverUrl = "https://your-server.com/api/upload_avatar.php";

    _cfgm = cfgm;
    if (cfgm == nullptr) {
        _cfgm = new ConfigManager();
        _cfgm->checkConfig();
    }

    _cfgm->giveValueToGlobalsetting();
    
    // 程序启动时获取远程玩家名称映射
    fetchRemotePlayerNames();
    
    // 初始化游戏状态监控
    lastGameStatus = false; // 初始假设游戏未运行
    lastConfigUpdateTime = QTime::currentTime(); // 初始化更新时间
    
    gameStatusTimer = new QTimer(this);
    connect(gameStatusTimer, &QTimer::timeout, this, &MainWindow::monitorGameStatus);
    gameStatusTimer->start(3000); // 每3秒检查一次游戏状态
    
    connect(ui->btn_updateplayer, &QPushButton::clicked, this,
            &MainWindow::onBtnUpdatePlayerClicked);
    
    // 连接头像上传按钮信号
    connect(ui->btn_upload_avatar_p1, &QPushButton::clicked, this, &MainWindow::onBtnUploadAvatarP1Clicked);
    connect(ui->btn_upload_avatar_p2, &QPushButton::clicked, this, &MainWindow::onBtnUploadAvatarP2Clicked);
    
    // 连接1V1科技主题tab的信号
    connect(ui->btn_updateplayer_tech, &QPushButton::clicked, this, &MainWindow::onBtnUpdatePlayerTechClicked);
    connect(ui->btn_upload_avatar_p1_tech, &QPushButton::clicked, this, &MainWindow::onBtnUploadAvatarP1TechClicked);
    connect(ui->btn_upload_avatar_p2_tech, &QPushButton::clicked, this, &MainWindow::onBtnUploadAvatarP2TechClicked);
    connect(ui->btn_update_score_tech, &QPushButton::clicked, this, &MainWindow::onBtnUpdateScoreTechClicked);
    connect(ui->spinBox_bo_number_tech, QOverload<int>::of(&QSpinBox::valueChanged), this, &MainWindow::onBONumberTechSpinBoxValueChanged);
    connect(ui->spinBox_p1_score_tech, QOverload<int>::of(&QSpinBox::valueChanged), this, &MainWindow::onP1ScoreTechSpinBoxValueChanged);
    connect(ui->spinBox_p2_score_tech, QOverload<int>::of(&QSpinBox::valueChanged), this, &MainWindow::onP2ScoreTechSpinBoxValueChanged);
    
    // 连接1V1科技模式滚动文字相关信号
    connect(ui->cb_hide_date_tech, &QCheckBox::toggled, this, &MainWindow::onHideDateCheckBoxToggledTech);
    connect(ui->btn_update_scroll_tech, &QPushButton::clicked, this, &MainWindow::onBtnUpdateScrollTechClicked);

    QFont f(layout::OPPO_M, 10);

    // 连接标签页切换信号
    connect(ui->tabWidget, &QTabWidget::currentChanged, this, &MainWindow::onTabIndexChanged);
    
    // 设置初始模式为1v1，初始化指针
    currentMode = ui->tabWidget->currentIndex();
    ob = nullptr;
    ob1 = nullptr;
    obTeam = nullptr;
    ob3 = nullptr;
    
    if (currentMode == 0) { // 1v1模式
        // 使用默认主题
        currentTheme = 0;
        
        // 创建默认主题实例
        if (currentTheme == 0) { // 默认主题
            ob = new Ob();
            connect(ob, &Ob::playernameNeedsUpdate, this, &MainWindow::updatePlayername);
            // 连接ob中PlayerInfo的分数变化信号
            connect(ob, &Ob::scoreChanged, this, &MainWindow::onScoreChanged);
            
            // 设置BO数
            ob->setBONumber(boNumber);
            
            // 应用PMB位置设置
            int pmbX = _cfgm->value("pmb_position_x", 0).toInt();
            int pmbY = _cfgm->value("pmb_position_y", 0).toInt();
            ob->setPmbPosition(pmbX, pmbY);
        } else { // 替代主题
            // 替代主题(ob1)已删除
            QMessageBox::information(this, tr("提示"), tr("替代主题已删除，将使用默认主题"));
            currentTheme = 0;
            
            // 创建默认主题实例
            ob = new Ob();
            connect(ob, &Ob::playernameNeedsUpdate, this, &MainWindow::updatePlayername);
            connect(ob, &Ob::scoreChanged, this, &MainWindow::onScoreChanged);
            
            // 设置BO数
            ob->setBONumber(boNumber);
            
            // 应用PMB位置设置
            int pmbX = _cfgm->value("pmb_position_x", 0).toInt();
            int pmbY = _cfgm->value("pmb_position_y", 0).toInt();
            ob->setPmbPosition(pmbX, pmbY);
        }
    } else if (currentMode == 1) { // 1V1科技主题模式
        // 创建科技主题实例
        ob1 = new Ob1();
        
        // 连接ob1的playernameNeedsUpdate信号到updatePlayernameTech方法
        bool connection1 = connect(ob1, &Ob1::playernameNeedsUpdate, this, &MainWindow::updatePlayernameTech, Qt::QueuedConnection);
        // 连接MainWindow的playerMappingUpdated信号到ob1的forceRefreshPlayerNames槽
        bool connection2 = connect(this, &MainWindow::playerMappingUpdated, ob1, &Ob1::forceRefreshPlayerNames, Qt::QueuedConnection);
        qDebug() << "[MainWindow] ob1信号连接已建立 - playernameNeedsUpdate:" << connection1 << ", playerMappingUpdated:" << connection2;
        
        // 设置BO数
        ob1->setBONumber(boNumber);
        
        // 应用PMB位置设置
        int pmbX = _cfgm->value("pmb_position_x", 0).toInt();
        int pmbY = _cfgm->value("pmb_position_y", 0).toInt();
        ob1->setPmbPosition(pmbX, pmbY);
        
        // 应用PMB大小设置
        int pmbWidth = _cfgm->value("pmb_size_width", 137).toInt();
        int pmbHeight = _cfgm->value("pmb_size_height", 220).toInt();
        ob1->setPmbSize(pmbWidth, pmbHeight);
        
        // 加载1V1科技模式滚动文字设置（在ob1对象创建后）
        loadScrollTextSettingsTech();

    } else if (currentMode == 2) { // 2v2模式
        obTeam = new ObTeam(nullptr); // 不传入this作为parent参数
        
        // 创建定时器，定期更新2v2模式的玩家名称映射
        QTimer *teamNameUpdateTimer = new QTimer(this);
        connect(teamNameUpdateTimer, &QTimer::timeout, this, &MainWindow::updateTeamPlayername);
        teamNameUpdateTimer->start(8000); // 每8秒更新一次
        
        // 立即更新一次
        QTimer::singleShot(1000, this, &MainWindow::updateTeamPlayername);
    } else if (currentMode == 3) { // 3v3模式
        ob3 = new Ob3(nullptr); // 不传入this作为parent参数
        
        // 创建定时器，定期更新3v3模式的玩家名称映射
        QTimer *team3v3NameUpdateTimer = new QTimer(this);
        connect(team3v3NameUpdateTimer, &QTimer::timeout, this, &MainWindow::updateTeam3v3Playername);
        team3v3NameUpdateTimer->start(8000); // 每8秒更新一次
        
        // 立即更新一次
        QTimer::singleShot(1000, this, &MainWindow::updateTeam3v3Playername);
    }
    
    ui->le_p1nickname->setPlaceholderText(tr("Match not started."));
    ui->le_p2nickname->setPlaceholderText(tr("Match not started."));
    
    // 初始化比分标签
    ui->lb_p1_score->setText(tr("玩家1"));
    ui->lb_p2_score->setText(tr("玩家2"));
    
    // 初始化BO数
    boNumber = _cfgm->value("bo_number", 5).toInt(); // 从配置中读取BO数，默认为5
    ui->spinBox_bo_number->setValue(boNumber);
    ui->spinBox_bo_number_tech->setValue(boNumber); // 同步设置1V1科技主题模式下的BO数
    ui->spinBox_bo_number_2v2->setValue(boNumber); // 同步设置2v2模式下的BO数
    ui->spinBox_bo_number_3v3->setValue(boNumber); // 同步设置3v3模式下的BO数
    
    // 连接BO数变化信号
    connect(ui->spinBox_bo_number, QOverload<int>::of(&QSpinBox::valueChanged), this, &MainWindow::onBONumberSpinBoxValueChanged);
    connect(ui->spinBox_bo_number_2v2, QOverload<int>::of(&QSpinBox::valueChanged), this, &MainWindow::onBONumberSpinBoxValueChanged);
    
    // 设置初始BO数
    if (ob && (currentMode == 0 || currentMode == 1)) {
        ob->setBONumber(boNumber);
    } else if (obTeam && currentMode == 2) {
        obTeam->setBONumber(boNumber);
    } else if (ob3 && currentMode == 3) {
        ob3->setBONumber(boNumber);
    }

    // 添加定时更新玩家名称的定时器
    QTimer *nameUpdateTimer = new QTimer();
    connect(nameUpdateTimer, SIGNAL(timeout()), this, SLOT(updatePlayername()));
    nameUpdateTimer->setInterval(5000); // 每5秒更新一次
    nameUpdateTimer->start();

    // 清空所有比分记录
    gls->playerScores.clear();
    _cfgm->setValue("score/p1", 0);
    _cfgm->setValue("score/p2", 0);
    _cfgm->setValue("score/playerScores", "{}");
    _cfgm->save();
    
    qDebug() << "程序启动时已清空所有比分记录";
    
    // 设置比分显示为0
    p1Score = 0;
    p2Score = 0;
    ui->spinBox_p1_score->setValue(0);
    ui->spinBox_p2_score->setValue(0);
    
    // 连接比分相关信号
    connect(ui->btn_update_score, &QPushButton::clicked, this, &MainWindow::onBtnUpdateScoreClicked);
    connect(ui->spinBox_p1_score, QOverload<int>::of(&QSpinBox::valueChanged), this, &MainWindow::onP1ScoreSpinBoxValueChanged);
    connect(ui->spinBox_p2_score, QOverload<int>::of(&QSpinBox::valueChanged), this, &MainWindow::onP2ScoreSpinBoxValueChanged);
    

    
    // 每次启动时直接使用默认头像
    p1AvatarPath = "";
    p2AvatarPath = "";
    
    // 加载默认头像
    loadAvatar(1);
    loadAvatar(2);
    
    // 加载玩家段位
    loadPlayerRanks();
    
    // 设置产能区块始终显示
    gls->s.show_producing = true;
    
    // 初始化PMB图片控制界面
    setupPmbControls();

    // 连接2v2模式下的更新玩家映射按钮信号
    connect(ui->btn_updateplayer_2v2, &QPushButton::clicked, this, &MainWindow::onBtnUpdatePlayer2v2Clicked);
    
    // 连接比分SpinBox信号
    connect(ui->spinBox_p1_score, QOverload<int>::of(&QSpinBox::valueChanged), this, &MainWindow::onP1ScoreSpinBoxValueChanged);
    connect(ui->spinBox_p2_score, QOverload<int>::of(&QSpinBox::valueChanged), this, &MainWindow::onP2ScoreSpinBoxValueChanged);
    
    // 连接2V2队伍比分SpinBox信号
    connect(ui->spinBox_team1_score, QOverload<int>::of(&QSpinBox::valueChanged), this, &MainWindow::onTeam1ScoreSpinBoxValueChanged);
    connect(ui->spinBox_team2_score, QOverload<int>::of(&QSpinBox::valueChanged), this, &MainWindow::onTeam2ScoreSpinBoxValueChanged);
    
    // 连接日期显示开关和滚动文字相关信号
    connect(ui->cb_hide_date, &QCheckBox::toggled, this, &MainWindow::onHideDateCheckBoxToggled);
    connect(ui->btn_update_scroll, &QPushButton::clicked, this, &MainWindow::onBtnUpdateScrollClicked);

    // 加载玩家比分映射
    QByteArray scoreMapJson = _cfgm->value("score/playerScores", "{}").toString().toUtf8();
    QJsonObject scoreMap = QJsonDocument::fromJson(scoreMapJson).object();
    
    QJsonObject::iterator i;
    for (i = scoreMap.begin(); i != scoreMap.end(); ++i) {
        QString nickname = i.key();
        int score = i.value().toInt();
        gls->playerScores[nickname] = score;
    }
    
    // 加载队伍比分映射
    QByteArray teamScoreMapJson = _cfgm->value("score/teamScores", "{}").toString().toUtf8();
    QJsonObject teamScoreMap = QJsonDocument::fromJson(teamScoreMapJson).object();
    
    QJsonObject::iterator j;
    for (j = teamScoreMap.begin(); j != teamScoreMap.end(); ++j) {
        QString teamId = j.key();
        int score = j.value().toInt();
        gls->teamScores[teamId] = score;
    }

    // 连接2v2模式相关槽
    connect(ui->btn_updateplayer_2v2, &QPushButton::clicked, this, &MainWindow::onBtnUpdatePlayer2v2Clicked);
    connect(ui->spinBox_team1_score, QOverload<int>::of(&QSpinBox::valueChanged), this, &MainWindow::onTeam1ScoreSpinBoxValueChanged);
    connect(ui->spinBox_team2_score, QOverload<int>::of(&QSpinBox::valueChanged), this, &MainWindow::onTeam2ScoreSpinBoxValueChanged);
    
    // 连接3v3模式相关槽
    connect(ui->btn_updateplayer_3v3, &QPushButton::clicked, this, &MainWindow::onBtnUpdatePlayer3v3Clicked);
    connect(ui->spinBox_team1_score_3v3, QOverload<int>::of(&QSpinBox::valueChanged), this, &MainWindow::onTeam1Score3v3SpinBoxValueChanged);
    connect(ui->spinBox_team2_score_3v3, QOverload<int>::of(&QSpinBox::valueChanged), this, &MainWindow::onTeam2Score3v3SpinBoxValueChanged);
    connect(ui->spinBox_bo_number_3v3, QOverload<int>::of(&QSpinBox::valueChanged), this, &MainWindow::onBONumberSpinBoxValueChanged);
    
    // 连接1V1科技主题模式相关槽
    connect(ui->btn_update_score_tech, &QPushButton::clicked, this, &MainWindow::onBtnUpdateScoreTechClicked);
    connect(ui->spinBox_p1_score_tech, QOverload<int>::of(&QSpinBox::valueChanged), this, &MainWindow::onP1ScoreTechSpinBoxValueChanged);
    connect(ui->spinBox_p2_score_tech, QOverload<int>::of(&QSpinBox::valueChanged), this, &MainWindow::onP2ScoreTechSpinBoxValueChanged);
    connect(ui->spinBox_bo_number_tech, QOverload<int>::of(&QSpinBox::valueChanged), this, &MainWindow::onBONumberTechSpinBoxValueChanged);
    
    // 创建1V1科技主题模式的比分更新定时器
    QTimer *techScoreUpdateTimer = new QTimer(this);
    connect(techScoreUpdateTimer, &QTimer::timeout, this, &MainWindow::updateTechScore);
    techScoreUpdateTimer->start(5000); // 每5秒更新一次比分
    
    // 创建1V1科技主题模式的玩家名称更新定时器
    QTimer *techNameUpdateTimer = new QTimer(this);
    connect(techNameUpdateTimer, &QTimer::timeout, this, &MainWindow::updatePlayernameTech);
    techNameUpdateTimer->start(5000); // 每5秒更新一次玩家名称
    
    // 加载主播名字和赛事名
    loadStreamerAndEventInfo();
    
    // 初始化会员状态显示
    updateMembershipStatus();
    
    // 连接会员状态变化信号
    connect(&MemberManager::getInstance(), &MemberManager::membershipStatusChanged, 
            this, &MainWindow::updateMembershipStatus);
    
    // 加载日期显示开关和滚动文字设置
    if (_cfgm) {
        bool hideDate = _cfgm->value("ui/hide_date", false).toBool();
        QString scrollText = _cfgm->value("ui/scroll_text", "").toString();
        
        ui->cb_hide_date->setChecked(hideDate);
        ui->le_scroll_text->setText(scrollText);
        
        // 应用设置到ob窗口
        if (ob) {
            ob->setHideDate(hideDate);
            ob->setScrollText(scrollText);
        }
    }

}

void MainWindow::detectShortcutStatus() {
    Globalsetting &gls = Globalsetting::getInstance();

    ui->lb_status_1->setText(tr("Normal"));
    ui->lb_status_1->setStyleSheet("color:green;");

    ui->lb_status_4->setText(tr("Normal"));
    ui->lb_status_4->setStyleSheet("color:green;");

    if (!gls.s.sc_ctrl_alt_h) {
        ui->lb_status_1->setText(tr("Occupied"));
        ui->lb_status_1->setStyleSheet("color:red;");
    }
    if (!gls.s.sc_ctrl_alt_j) {
        ui->lb_status_4->setText(tr("Occupied"));
        ui->lb_status_4->setStyleSheet("color:red;");
    }
}

void MainWindow::drawPreview(QWidget *widget) {
    // 空实现，不再绘制任何预览
}

bool MainWindow::eventFilter(QObject *watched, QEvent *event) {
    return QMainWindow::eventFilter(watched, event);
}

void MainWindow::onBtnUpdatePlayerClicked() {
    QString p1_nickname   = ui->le_p1nickname->text();
    QString p1_playername = ui->le_p1playername->text();
    QString p2_nickname   = ui->le_p2nickname->text();
    QString p2_playername = ui->le_p2playername->text();
    
    // 获取段位信息
    QString p1_rank = ui->cb_p1_rank->currentText();
    QString p2_rank = ui->cb_p2_rank->currentText();

    // 更新玩家信息到远程服务器
    if (!p1_nickname.isEmpty() && !p1_playername.isEmpty()) {
        // 更新远程玩家名称映射
        updateRemotePlayerName(p1_nickname, p1_playername);
        
        // 同时更新本地缓存
        gls->addPlayerMapping(p1_nickname, p1_playername);
        
        // 段位信息仍然保存在本地
        _cfgm->setValue("rank/nickname/" + p1_nickname, p1_rank);
    }
    if (!p2_nickname.isEmpty() && !p2_playername.isEmpty()) {
        // 更新远程玩家名称映射
        updateRemotePlayerName(p2_nickname, p2_playername);
        
        // 同时更新本地缓存
        gls->addPlayerMapping(p2_nickname, p2_playername);
        
        // 段位信息仍然保存在本地
        _cfgm->setValue("rank/nickname/" + p2_nickname, p2_rank);
    }

    // 保存本地设置（仅包括段位等其他本地信息）
    _cfgm->save();
    
    // 更新ob界面上的玩家头像，需要检查ob是否存在
    if (ob) {
        ob->updatePlayerAvatars();
    }
}

// 玩家1头像上传按钮点击处理
void MainWindow::onBtnUploadAvatarP1Clicked() {
    QString p1_nickname = ui->le_p1nickname->text();
    QString p1_playername = ui->le_p1playername->text();
    
    if (p1_nickname.isEmpty()) {
        QMessageBox::warning(this, tr("提示"), tr("请先输入玩家1昵称"));
        return;
    }
    
    // 获取实际游戏名称，如果没有填写则使用昵称
    QString actualName = !p1_playername.isEmpty() ? p1_playername : p1_nickname;
    
    uploadAvatar(1, p1_nickname, actualName);
}

// 玩家2头像上传按钮点击处理
void MainWindow::onBtnUploadAvatarP2Clicked() {
    QString p2_nickname = ui->le_p2nickname->text();
    QString p2_playername = ui->le_p2playername->text();
    
    if (p2_nickname.isEmpty()) {
        QMessageBox::warning(this, tr("提示"), tr("请先输入玩家2昵称"));
        return;
    }
    
    // 获取实际游戏名称，如果没有填写则使用昵称
    QString actualName = !p2_playername.isEmpty() ? p2_playername : p2_nickname;
    
    uploadAvatar(2, p2_nickname, actualName);
}

// 上传头像的辅助方法
void MainWindow::uploadAvatar(int playerIndex, const QString &nickname, const QString &gameName) {
    QString dialogTitle = (playerIndex == 1) ? tr("选择玩家1头像") : tr("选择玩家2头像");
    QString filePath = QFileDialog::getOpenFileName(this, dialogTitle, 
                                                  QStandardPaths::writableLocation(QStandardPaths::PicturesLocation),
                                                  tr("图片文件 (*.png *.jpg *.jpeg *.bmp)"));
    if (filePath.isEmpty()) {
        return;
    }
    
    QPixmap avatar(filePath);
    if (avatar.isNull()) {
        QMessageBox::warning(this, tr("错误"), tr("无法加载选择的图片"));
        return;
    }
    
    // 显示在界面上
    if (playerIndex == 1) {
        ui->lb_avatar_p1->setPixmap(avatar.scaled(ui->lb_avatar_p1->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
        p1AvatarPath = filePath; // 临时保存路径用于显示
    } else {
        ui->lb_avatar_p2->setPixmap(avatar.scaled(ui->lb_avatar_p2->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
        p2AvatarPath = filePath; // 临时保存路径用于显示
    }
    
    // 保存头像到本地
    saveAvatarLocally(filePath, nickname, gameName);
    
    // 本地保存完成后立即通知ob1更新头像显示
    if (ob1) {
        ob1->updatePlayerAvatars();
        qDebug() << "[MainWindow] 本地头像保存完成后通知ob1更新头像显示";
    }
    
    // 上传到服务器
    uploadAvatarToServer(playerIndex, nickname, gameName, filePath);
}

// 保存头像到本地
void MainWindow::saveAvatarLocally(const QString &sourceFilePath, const QString &nickname, const QString &gameName) {
    // 确保avatars目录存在
    QString avatarsDir = QCoreApplication::applicationDirPath() + "/avatars";
    QDir dir(avatarsDir);
    if (!dir.exists()) {
        dir.mkpath(".");
    }
    
    // 打开源文件
    QFile sourceFile(sourceFilePath);
    if (!sourceFile.open(QIODevice::ReadOnly)) {
        qDebug() << "无法打开源文件:" << sourceFilePath;
        return;
    }
    
    QByteArray imageData = sourceFile.readAll();
    sourceFile.close();
    
    // 将图像统一转换为PNG格式
    QImage image;
    if (!image.loadFromData(imageData)) {
        qDebug() << "无法加载图像数据";
        return;
    }
    
    // 1. 优先以游戏名保存（如果有）
    if (!gameName.isEmpty()) {
        QString gameNameFilePath = avatarsDir + "/" + gameName + ".png";
        if (!image.save(gameNameFilePath, "PNG")) {
            qDebug() << "保存游戏名头像失败:" << gameNameFilePath;
    } else {
            qDebug() << "成功保存游戏名头像:" << gameNameFilePath;
        }
    }
    
    // 2. 如果昵称和游戏名不同，也以昵称保存
    if (!nickname.isEmpty() && (gameName.isEmpty() || nickname != gameName)) {
        QString nicknameFilePath = avatarsDir + "/" + nickname + ".png";
        if (!image.save(nicknameFilePath, "PNG")) {
            qDebug() << "保存昵称头像失败:" << nicknameFilePath;
        } else {
            qDebug() << "成功保存昵称头像:" << nicknameFilePath;
        }
    }
}

// 上传头像到服务器的方法
void MainWindow::uploadAvatarToServer(int playerIndex, const QString &nickname, const QString &gameName, const QString &localFilePath) {
    // 检查网络管理器是否初始化
    if (!networkManager) {
        qDebug() << "网络管理器未初始化!";
        return;
    }
    
    // 同时上传两个文件，一个用玩家名称，一个用玩家昵称
    QFile *file = new QFile(localFilePath);
    if (!file->open(QIODevice::ReadOnly)) {
        qDebug() << "无法打开文件进行上传:" << localFilePath;
        QMessageBox::warning(this, tr("错误"), tr("无法打开文件进行上传"));
        file->deleteLater();
        return;
    }
    
    // 创建multipart请求
    QHttpMultiPart *multiPart = new QHttpMultiPart(QHttpMultiPart::FormDataType);
    
    // 设置正确的MIME类型
    QMimeDatabase db;
    QMimeType mime = db.mimeTypeForFile(localFilePath);
    
    // 1. 添加以游戏名称命名的头像文件部分
    QHttpPart gameNameImagePart;
    gameNameImagePart.setHeader(QNetworkRequest::ContentTypeHeader, QVariant(mime.name()));
    gameNameImagePart.setHeader(QNetworkRequest::ContentDispositionHeader, 
                       QVariant("form-data; name=\"avatar_gamename\"; filename=\"" + 
                               gameName + ".png\""));
    
    // 读取文件内容到内存中
    QByteArray fileData = file->readAll();
    file->seek(0); // 重置文件指针，以便后续再次读取
    
    gameNameImagePart.setBody(fileData);
    multiPart->append(gameNameImagePart);
    
    // 2. 添加以昵称命名的头像文件部分（如果昵称和游戏名称不同）
    if (nickname != gameName) {
        QHttpPart nicknameImagePart;
        nicknameImagePart.setHeader(QNetworkRequest::ContentTypeHeader, QVariant(mime.name()));
        nicknameImagePart.setHeader(QNetworkRequest::ContentDispositionHeader, 
                           QVariant("form-data; name=\"avatar_nickname\"; filename=\"" + 
                                   nickname + ".png\""));
        
        nicknameImagePart.setBody(fileData);
        multiPart->append(nicknameImagePart);
    }
    
    // 添加玩家昵称
    QHttpPart nicknamePart;
    nicknamePart.setHeader(QNetworkRequest::ContentDispositionHeader, 
                          QVariant("form-data; name=\"nickname\""));
    nicknamePart.setBody(nickname.toUtf8());
    multiPart->append(nicknamePart);
    
    // 添加玩家游戏名称
    QHttpPart gameNamePart;
    gameNamePart.setHeader(QNetworkRequest::ContentDispositionHeader, 
                          QVariant("form-data; name=\"game_name\""));
    gameNamePart.setBody(gameName.toUtf8());
    multiPart->append(gameNamePart);
    
    // 添加玩家索引
    QHttpPart playerIndexPart;
    playerIndexPart.setHeader(QNetworkRequest::ContentDispositionHeader, 
                             QVariant("form-data; name=\"player_index\""));
    playerIndexPart.setBody(QByteArray::number(playerIndex));
    multiPart->append(playerIndexPart);
    
    // 添加上传模式标志，告诉服务器这是双文件上传
    QHttpPart uploadModePart;
    uploadModePart.setHeader(QNetworkRequest::ContentDispositionHeader, 
                            QVariant("form-data; name=\"upload_mode\""));
    uploadModePart.setBody("dual_names");
    multiPart->append(uploadModePart);
    
    // 添加设备唯一标识符
    QHttpPart deviceIdPart;
    deviceIdPart.setHeader(QNetworkRequest::ContentDispositionHeader, 
                         QVariant("form-data; name=\"device_id\""));
    deviceIdPart.setBody(getDeviceIdentifier().toUtf8());
    multiPart->append(deviceIdPart);
    
    // 添加API密钥
    QHttpPart apiKeyPart;
    apiKeyPart.setHeader(QNetworkRequest::ContentDispositionHeader, 
                        QVariant("form-data; name=\"api_key\""));
    apiKeyPart.setBody(API_KEY.toUtf8());
    multiPart->append(apiKeyPart);
    
    // 创建请求
    QUrl url(serverUrl);
    QNetworkRequest request(url);
    
    // 发送请求
    QNetworkReply *reply = networkManager->post(request, multiPart);
    multiPart->setParent(reply); // reply接管multiPart的生命周期
    file->deleteLater();
    
    // 显示上传中消息
    QMessageBox *uploadingMsg = new QMessageBox(QMessageBox::Information, 
                                               tr("上传中"), 
                                               tr("头像正在上传到服务器..."), 
                                               QMessageBox::NoButton, 
                                               this);
    uploadingMsg->setStandardButtons(QMessageBox::NoButton);
    
    // 连接完成信号
    connect(reply, &QNetworkReply::finished, this, [=]() {
        uploadingMsg->close();
        uploadingMsg->deleteLater();
        
        if (reply->error() == QNetworkReply::NoError) {
            // 读取响应
            QByteArray responseData = reply->readAll();
            
            // 尝试解析JSON响应
            QJsonDocument doc = QJsonDocument::fromJson(responseData);
            QJsonObject json = doc.object();
            
            if (json["success"].toBool()) {
                // 上传成功
                QStringList fileUrls;
                
                // 检查是否有多个URL返回
                if (json.contains("file_urls") && json["file_urls"].isArray()) {
                    QJsonArray urlArray = json["file_urls"].toArray();
                    for (const QJsonValue &value : urlArray) {
                        fileUrls.append(value.toString());
    }
    
                    qDebug() << "上传了多个头像文件，URL列表:" << fileUrls;
                } else if (json.contains("file_url")) {
                    QString fileUrl = json["file_url"].toString();
                    fileUrls.append(fileUrl);
                }
                
                // 上传成功不显示提示消息
                qDebug() << "玩家" << playerIndex << "(" << nickname << ") 的头像已成功上传到服务器和保存到本地";
                
                // 如果OB窗口存在，立即更新头像显示
                if (ob) {
                    ob->updatePlayerAvatars();
                }
                
                // 如果OB1窗口存在，也要更新头像显示
                if (ob1) {
                    ob1->updatePlayerAvatars();
                    qDebug() << "[MainWindow] 头像上传完成后通知ob1更新头像显示";
                }
            } else {
                // 服务器返回错误
                QString errorMsg = json.contains("message") ? json["message"].toString() : tr("未知错误");
                QMessageBox::warning(this, tr("服务器上传失败"), 
                                   tr("头像已保存到本地，但服务器上传失败: %1").arg(errorMsg));
                
                qDebug() << "服务器上传失败:" << errorMsg;
                
                // 即使服务器上传失败，仍然刷新本地头像
                if (ob) {
                    ob->updatePlayerAvatars();
                }
                
                // 如果OB1窗口存在，也要更新头像显示
                if (ob1) {
                    ob1->updatePlayerAvatars();
                    qDebug() << "[MainWindow] 服务器上传失败但本地保存成功，通知ob1更新头像显示";
                }
            }
        } else {
            // 网络错误
            QMessageBox::warning(this, tr("网络错误"), 
                               tr("头像已保存到本地，但无法连接到服务器: %1").arg(reply->errorString()));
            
            qDebug() << "网络错误:" << reply->errorString();
            
            // 即使网络错误，仍然刷新本地头像
            if (ob) {
                ob->updatePlayerAvatars();
            }
            
            // 如果OB1窗口存在，也要更新头像显示
            if (ob1) {
                ob1->updatePlayerAvatars();
                qDebug() << "[MainWindow] 网络错误但本地保存成功，通知ob1更新头像显示";
            }
        }
        
        reply->deleteLater();
    });
    
    // 显示上传中消息框
    QTimer::singleShot(0, uploadingMsg, &QMessageBox::exec);
}

// 更新比分按钮点击处理
void MainWindow::onBtnUpdateScoreClicked() {
    // 获取当前比分
    p1Score = ui->spinBox_p1_score->value();
    p2Score = ui->spinBox_p2_score->value();
    
    // 获取当前BO数
    boNumber = ui->spinBox_bo_number->value();
    _cfgm->setValue("bo_number", boNumber);
    
    // 更新OB界面上的BO数显示，需要检查ob、obTeam或ob3是否存在
    if (ob && currentMode == 0) {
        ob->setBONumber(boNumber);
    } else if (obTeam && currentMode == 1) {
        obTeam->setBONumber(boNumber);
    } else if (ob3 && currentMode == 2) {
        ob3->setBONumber(boNumber);
    }
    
    // 获取当前玩家昵称
    QString p1_nickname = ui->le_p1nickname->text();
    QString p2_nickname = ui->le_p2nickname->text();
    
    // 获取玩家显示名称
    QString p1_playername = ui->le_p1playername->text();
    QString p2_playername = ui->le_p2playername->text();
    
    // 更新比分标签显示当前玩家昵称
    if (!p1_nickname.isEmpty()) {
        ui->lb_p1_score->setText(p1_playername.isEmpty() ? p1_nickname : p1_playername);
    } else {
        ui->lb_p1_score->setText(tr("玩家1"));
    }
    
    if (!p2_nickname.isEmpty()) {
        ui->lb_p2_score->setText(p2_playername.isEmpty() ? p2_nickname : p2_playername);
    } else {
        ui->lb_p2_score->setText(tr("玩家2"));
    }
    
    // 将比分与玩家昵称关联存储
    if (!p1_nickname.isEmpty()) {
        gls->playerScores[p1_nickname] = p1Score;
    }
    
    if (!p2_nickname.isEmpty()) {
        gls->playerScores[p2_nickname] = p2Score;
    }
    
    // 更新OB界面上的比分显示，需要检查ob是否存在
    if (ob && currentMode == 0) {
        ob->updateScores();
    }
    
    // 保存比分设置
    saveAllSettings();
    
    qDebug() << "比分已更新 - 玩家1(" << p1_nickname << "):" << p1Score 
             << ", 玩家2(" << p2_nickname << "):" << p2Score
             << ", BO数:" << boNumber;
}

// 玩家1比分变化处理
void MainWindow::onP1ScoreSpinBoxValueChanged(int value) {
    p1Score = value;
}

// 玩家2比分变化处理
void MainWindow::onP2ScoreSpinBoxValueChanged(int value) {
    p2Score = value;
}

void MainWindow::updatePlayername() {
    // 获取当前游戏中的玩家昵称
    QString p1_nickname, p2_nickname;
    
    // 如果没有运行中的游戏，但有缓存数据，则使用缓存数据
    if (gls->sb.p1_nickname.isEmpty() && !gls->sb.p1_nickname_cache.isEmpty()) {
        p1_nickname = gls->sb.p1_nickname_cache;
    } else {
        p1_nickname = gls->sb.p1_nickname;
    }
    
    if (gls->sb.p2_nickname.isEmpty() && !gls->sb.p2_nickname_cache.isEmpty()) {
        p2_nickname = gls->sb.p2_nickname_cache;
    } else {
        p2_nickname = gls->sb.p2_nickname;
    }
    
    // 昵称是否变化的标志
    bool p1NicknameChanged = false;
    bool p2NicknameChanged = false;
    
    // 只有当昵称不为空且与当前显示不同时才更新界面
    if (!p1_nickname.isEmpty() && p1_nickname != ui->le_p1nickname->text()) {
    ui->le_p1nickname->setText(p1_nickname);
        p1NicknameChanged = true;
    }
    
    if (!p2_nickname.isEmpty() && p2_nickname != ui->le_p2nickname->text()) {
    ui->le_p2nickname->setText(p2_nickname);
        p2NicknameChanged = true;
    }
 
    // 只有当昵称变化时才更新玩家名称
    if (p1NicknameChanged) {
        QString p1_playername;
        
        // 优先使用缓存的完整名称
        if (!gls->sb.p1_playername_cache.isEmpty()) {
            p1_playername = gls->sb.p1_playername_cache;
        } else {
            // 先尝试从本地获取
            p1_playername = gls->findNameByNickname(p1_nickname);
            
            // 如果本地找不到，尝试从远程获取（异步请求）
            if (p1_playername.isEmpty()) {
                fetchSinglePlayerName(p1_nickname);
            }
        }
        
    ui->le_p1playername->setText(p1_playername);
    
        // 更新比分标签显示
    if (!p1_nickname.isEmpty()) {
            ui->lb_p1_score->setText(p1_playername.isEmpty() ? p1_nickname : p1_playername);
    } else {
            ui->lb_p1_score->setText(tr("玩家1"));
        }
        
        // 更新段位显示
        updatePlayerRank(1, p1_nickname);
        
        // 玩家名称变化时，更新头像
        loadAvatar(1);
    }
    
    if (p2NicknameChanged) {
        QString p2_playername;
        
        // 优先使用缓存的完整名称
        if (!gls->sb.p2_playername_cache.isEmpty()) {
            p2_playername = gls->sb.p2_playername_cache;
        } else {
            // 先尝试从本地获取
            p2_playername = gls->findNameByNickname(p2_nickname);
            
            // 如果本地找不到，尝试从远程获取（异步请求）
            if (p2_playername.isEmpty()) {
                fetchSinglePlayerName(p2_nickname);
            }
        }
        
        ui->le_p2playername->setText(p2_playername);
        
        // 更新比分标签显示
    if (!p2_nickname.isEmpty()) {
            ui->lb_p2_score->setText(p2_playername.isEmpty() ? p2_nickname : p2_playername);
    } else {
            ui->lb_p2_score->setText(tr("玩家2"));
        }
        
        // 更新段位显示
        updatePlayerRank(2, p2_nickname);
    
        // 玩家名称变化时，更新头像
            loadAvatar(2);
    }
    
    // 更新比分显示 - 这部分仍然每5秒更新一次
    updateScore();
}

void MainWindow::hideEvent(QHideEvent *event) {
    // 正常处理隐藏事件
    QMainWindow::hideEvent(event);
}

void MainWindow::closeEvent(QCloseEvent *event) {
    // 显示确认对话框，询问用户是否确认退出程序
    QMessageBox::StandardButton reply = QMessageBox::question(
        this, 
        tr("确认退出"), 
        tr("确定要退出程序吗？"),
        QMessageBox::Yes | QMessageBox::No, 
        QMessageBox::No
    );
    
    // 如果用户点击"是"，则退出程序；否则忽略关闭事件
    if (reply == QMessageBox::Yes) {
        // 保存所有用户设置
        saveAllSettings();
        
        // 强制立即写入
        if (_cfgm) {
            _cfgm->save();
        }
        
        // 释放ob资源
        if (ob) {
            delete ob;
            ob = nullptr;
        }
        
        // 释放obTeam资源
        if (obTeam) {
            delete obTeam;
            obTeam = nullptr;
        }
        
        // 释放ob3资源
        if (ob3) {
            delete ob3;
            ob3 = nullptr;
        }
        
        // 真正退出程序
        qApp->quit();
        event->accept();
    } else {
        // 用户选择不退出，忽略关闭事件
        event->ignore();
    }
}

void MainWindow::saveAllSettings() {
    if (_cfgm) {
        // 保存比分设置
        _cfgm->setValue("score/p1", p1Score);
        _cfgm->setValue("score/p2", p2Score);
        _cfgm->setValue("bo_number", boNumber); // 保存BO数
        
        // 保存玩家昵称和比分映射
        QJsonObject scoreMap;
        QMapIterator<QString, int> i(gls->playerScores);
        while (i.hasNext()) {
            i.next();
            scoreMap[i.key()] = i.value();
        }
        _cfgm->setValue("score/playerScores", QJsonDocument(scoreMap).toJson(QJsonDocument::Compact));
        
        // 保存队伍比分映射
        QJsonObject teamScoreMap;
        QMapIterator<QString, int> j(gls->teamScores);
        while (j.hasNext()) {
            j.next();
            teamScoreMap[j.key()] = j.value();
        }
        _cfgm->setValue("score/teamScores", QJsonDocument(teamScoreMap).toJson(QJsonDocument::Compact));
        
        // 保存玩家昵称和实际名称对应关系
        QString p1_nickname   = ui->le_p1nickname->text();
        QString p1_playername = ui->le_p1playername->text();
        QString p2_nickname   = ui->le_p2nickname->text();
        QString p2_playername = ui->le_p2playername->text();

        if (!p1_nickname.isEmpty() && !p1_playername.isEmpty()) {
            _cfgm->updatePlayer(p1_nickname, p1_playername);
        }
        if (!p2_nickname.isEmpty() && !p2_playername.isEmpty()) {
            _cfgm->updatePlayer(p2_nickname, p2_playername);
        }
        
        // 保存1V1科技模式的玩家昵称和实际名称对应关系
        QString p1_nickname_tech   = ui->le_p1nickname_tech->text();
        QString p1_playername_tech = ui->le_p1playername_tech->text();
        QString p2_nickname_tech   = ui->le_p2nickname_tech->text();
        QString p2_playername_tech = ui->le_p2playername_tech->text();

        if (!p1_nickname_tech.isEmpty() && !p1_playername_tech.isEmpty()) {
            _cfgm->updatePlayer(p1_nickname_tech, p1_playername_tech);
        }
        if (!p2_nickname_tech.isEmpty() && !p2_playername_tech.isEmpty()) {
            _cfgm->updatePlayer(p2_nickname_tech, p2_playername_tech);
        }
        
        // 保存段位信息
        if (!p1_nickname.isEmpty()) {
            _cfgm->setValue("rank/nickname/" + p1_nickname, ui->cb_p1_rank->currentText());
        }
        if (!p2_nickname.isEmpty()) {
            _cfgm->setValue("rank/nickname/" + p2_nickname, ui->cb_p2_rank->currentText());
        }
        
        // 保存1V1科技模式的段位信息
        if (!p1_nickname_tech.isEmpty()) {
            _cfgm->setValue("rank/nickname/" + p1_nickname_tech, ui->cb_p1_rank_tech->currentText());
        }
        if (!p2_nickname_tech.isEmpty()) {
            _cfgm->setValue("rank/nickname/" + p2_nickname_tech, ui->cb_p2_rank_tech->currentText());
        }
        
        // 保存日期显示开关和滚动文字设置
        _cfgm->setValue("ui/hide_date", ui->cb_hide_date->isChecked());
        _cfgm->setValue("ui/scroll_text", ui->le_scroll_text->text());
        
        // 保存到文件
        _cfgm->save();
    }
}

void MainWindow::loadAvatar(int playerIndex) {
    QString nickname;
    QLabel* avatarLabel;
    
    if (playerIndex == 1) {
        nickname = ui->le_p1nickname->text();
        avatarLabel = ui->lb_avatar_p1;
    } else {
        nickname = ui->le_p2nickname->text();
        avatarLabel = ui->lb_avatar_p2;
    }
    
    // 如果昵称为空，使用默认头像
    if (nickname.isEmpty()) {
        // 使用程序目录下avatars文件夹中的default.png作为默认头像
        QString defaultAvatarPath = QCoreApplication::applicationDirPath() + "/avatars/default.png";
        QPixmap defaultAvatar;
        if (QFile::exists(defaultAvatarPath)) {
            defaultAvatar.load(defaultAvatarPath);
        } else {
            // 如果默认头像文件不存在，使用程序图标作为备用
            defaultAvatar.load(":/icon/assets/icons/icon_64.png");
            if (defaultAvatar.isNull()) {
                // 如果图标也加载失败，创建一个简单的默认头像
                defaultAvatar = QPixmap(64, 64);
                defaultAvatar.fill(Qt::lightGray);
            }
        }
        
        // 缩放头像并显示
        defaultAvatar = defaultAvatar.scaled(avatarLabel->width(), avatarLabel->height(), Qt::KeepAspectRatio, Qt::SmoothTransformation);
        avatarLabel->setPixmap(defaultAvatar);
        
        // 更新本地路径变量
        if (playerIndex == 1) {
            p1AvatarPath = "";
        } else {
            p2AvatarPath = "";
        }
        
        return;
    }
    
    // 确保获取最新的游戏名称
    QString gameName;
    if (playerIndex == 1) {
        // 优先使用输入框中的游戏名，如果为空则尝试从全局设置中查找
        gameName = ui->le_p1playername->text();
        if (gameName.isEmpty()) {
            gameName = gls->findNameByNickname(nickname);
            // 如果从全局设置中找到了游戏名，更新到输入框
            if (!gameName.isEmpty()) {
                ui->le_p1playername->setText(gameName);
            } else {
                // 如果全局设置中也没有，则使用昵称作为游戏名
                gameName = nickname;
            }
        }
    } else {
        // 优先使用输入框中的游戏名，如果为空则尝试从全局设置中查找
        gameName = ui->le_p2playername->text();
        if (gameName.isEmpty()) {
            gameName = gls->findNameByNickname(nickname);
            // 如果从全局设置中找到了游戏名，更新到输入框
            if (!gameName.isEmpty()) {
                ui->le_p2playername->setText(gameName);
            } else {
                // 如果全局设置中也没有，则使用昵称作为游戏名
                gameName = nickname;
            }
        }
    }
    
    qDebug() << "玩家" << playerIndex << "昵称:" << nickname << "游戏名:" << gameName;
    
    // 首先尝试加载本地头像
    QPixmap localAvatar;
    
    // 1. 优先尝试加载以游戏名称命名的本地头像
    QString localGameNamePath = QCoreApplication::applicationDirPath() + "/avatars/" + gameName + ".png";
    if (QFile::exists(localGameNamePath)) {
        localAvatar.load(localGameNamePath);
        if (!localAvatar.isNull()) {
            // 本地头像加载成功
            localAvatar = localAvatar.scaled(avatarLabel->width(), avatarLabel->height(), Qt::KeepAspectRatio, Qt::SmoothTransformation);
            avatarLabel->setPixmap(localAvatar);
            
            // 更新本地路径变量
            if (playerIndex == 1) {
                p1AvatarPath = localGameNamePath;
            } else {
                p2AvatarPath = localGameNamePath;
            }
            
            qDebug() << "从本地加载玩家" << playerIndex << "的头像(游戏名):" << localGameNamePath;
            return;
        }
    }
    
    // 2. 如果游戏名头像不存在，尝试加载以昵称命名的本地头像
    QString localAvatarPath = QCoreApplication::applicationDirPath() + "/avatars/" + nickname + ".png";
    if (gameName != nickname && QFile::exists(localAvatarPath)) {
        localAvatar.load(localAvatarPath);
        if (!localAvatar.isNull()) {
            // 本地头像加载成功
            localAvatar = localAvatar.scaled(avatarLabel->width(), avatarLabel->height(), Qt::KeepAspectRatio, Qt::SmoothTransformation);
            avatarLabel->setPixmap(localAvatar);
            
            // 更新本地路径变量
            if (playerIndex == 1) {
                p1AvatarPath = localAvatarPath;
            } else {
                p2AvatarPath = localAvatarPath;
            }
            
            qDebug() << "从本地加载玩家" << playerIndex << "的头像(昵称):" << localAvatarPath;
            return;
        }
    }
    
    // 如果本地头像加载失败，尝试从远程服务器加载
    // 创建请求，添加时间戳防止缓存问题
    QString timestamp = QString::number(QDateTime::currentMSecsSinceEpoch());
    // TODO: 配置头像下载地址
        QUrl avatarUrl("https://your-server.com/avatars/" + gameName + ".png");
    QUrlQuery query;
    query.addQueryItem("t", timestamp);
    query.addQueryItem("api_key", API_KEY);
    avatarUrl.setQuery(query);
    
    QNetworkRequest request(avatarUrl);
    request.setAttribute(QNetworkRequest::CacheLoadControlAttribute, QNetworkRequest::AlwaysNetwork);
    request.setRawHeader("Cache-Control", "no-cache, no-store, must-revalidate");
    request.setRawHeader("Pragma", "no-cache");
    request.setRawHeader("Expires", "0");
    
    // 显示加载中的提示
    avatarLabel->setText(tr("加载中..."));
    
    // 发送请求
    QNetworkReply *reply = networkManager->get(request);
    
    // 使用lambda处理响应
    connect(reply, &QNetworkReply::finished, this, [=]() {
        reply->deleteLater();
        
        if (reply->error() == QNetworkReply::NoError) {
            // 远程加载成功
    QPixmap avatar;
            avatar.loadFromData(reply->readAll());
            
            if (!avatar.isNull()) {
                // 缩放头像并显示
                avatar = avatar.scaled(avatarLabel->width(), avatarLabel->height(), Qt::KeepAspectRatio, Qt::SmoothTransformation);
                avatarLabel->setPixmap(avatar);
                
                // 更新本地路径变量（虽然使用的是远程图片，但为了保持变量有值）
                if (playerIndex == 1) {
                    p1AvatarPath = "remote://" + avatarUrl.toString();
    } else {
                    p2AvatarPath = "remote://" + avatarUrl.toString();
                }
                
                // 同时保存到本地，以便下次可以从本地加载
                QDir avatarsDir(QCoreApplication::applicationDirPath() + "/avatars");
                if (!avatarsDir.exists()) {
                    avatarsDir.mkpath(".");
                }
                
                QString localSavePath = QCoreApplication::applicationDirPath() + "/avatars/" + gameName + ".png";
                if (avatar.save(localSavePath, "PNG")) {
                    qDebug() << "远程头像已保存到本地:" << localSavePath;
                }
                
                // 如果昵称和游戏名不同，也保存以昵称命名的版本
                if (nickname != gameName) {
                    QString nicknameLocalPath = QCoreApplication::applicationDirPath() + "/avatars/" + nickname + ".png";
                    if (avatar.save(nicknameLocalPath, "PNG")) {
                        qDebug() << "远程头像已保存到本地(昵称):" << nicknameLocalPath;
                    }
                }
                
                qDebug() << "从远程服务器成功加载玩家" << playerIndex << "的头像";
                return;
            }
        }
        
        // 远程加载失败，直接尝试使用阵营头像
        qDebug() << "远程加载失败，尝试使用阵营头像, 错误:" << reply->errorString();
        
        // 准备使用阵营头像
        QPixmap avatar;
        QString country;
        int playerIdx = -1;
            
        // 从全局设置获取玩家阵营信息
        Ra2ob::Game &g = Ra2ob::Game::getInstance();
        
        if (g._gameInfo.valid) {
            // 尝试找到对应的玩家索引
            for (int i = 0; i < Ra2ob::MAXPLAYER; i++) {
                if (g._gameInfo.players[i].valid) {
                    QString playerName = QString::fromStdString(g._gameInfo.players[i].panel.playerNameUtf);
                    if (playerName == nickname) {
                        playerIdx = i;
                        break;
                    }
                }
            }
            
            // 如果找到对应玩家，获取其阵营
            if (playerIdx >= 0) {
                country = QString::fromStdString(g._gameInfo.players[playerIdx].panel.country);
            }
        }
        
        // 如果没有游戏中的阵营信息，尝试使用默认阵营
        if (country.isEmpty()) {
            // 默认使用Americans作为阵营
            country = "Americans";
        }
        
        // 如果找到阵营信息，加载对应的阵营图标
        if (!country.isEmpty()) {
            QString appDir = QCoreApplication::applicationDirPath();
            
            // 确保首字母大写，其余小写
            country = country.at(0).toUpper() + country.mid(1).toLower();
            QString countryAvatarPath = appDir + "/assets/countries/" + country + ".png";
            
            if (QFile::exists(countryAvatarPath)) {
                avatar.load(countryAvatarPath);
                qDebug() << "使用阵营图标作为头像:" << country;
                
                // 更新本地路径变量
                if (playerIndex == 1) {
                    p1AvatarPath = "country://" + country;
                } else {
                    p2AvatarPath = "country://" + country;
                }
            }
        }
        
        // 如果阵营图标加载失败，使用默认头像
        if (avatar.isNull()) {
        QString defaultAvatarPath = QCoreApplication::applicationDirPath() + "/avatars/default.png";
        if (QFile::exists(defaultAvatarPath)) {
            avatar.load(defaultAvatarPath);
        } else {
            // 如果默认头像文件不存在，使用程序图标作为备用
            avatar.load(":/icon/assets/icons/icon_64.png");
            if (avatar.isNull()) {
                // 如果图标也加载失败，创建一个简单的默认头像
                avatar = QPixmap(64, 64);
                avatar.fill(Qt::lightGray);
            }
        }
            
            // 更新本地路径变量
            if (playerIndex == 1) {
                p1AvatarPath = "";
            } else {
                p2AvatarPath = "";
            }
    }
    
    // 缩放头像并显示
    avatar = avatar.scaled(avatarLabel->width(), avatarLabel->height(), Qt::KeepAspectRatio, Qt::SmoothTransformation);
    avatarLabel->setPixmap(avatar);
    });
}

// 处理分数变化的槽函数
void MainWindow::onScoreChanged(const QString &nickname, int newScore) {
    // 更新全局分数记录
    gls->playerScores[nickname] = newScore;
    
    // 检查是哪个玩家的分数变化
    QString p1_nickname = ui->le_p1nickname->text();
    QString p2_nickname = ui->le_p2nickname->text();
    
    // 更新界面上的分数显示
    if (nickname == p1_nickname) {
        p1Score = newScore;
        ui->spinBox_p1_score->setValue(newScore);
        qDebug() << "通过点击更新玩家1(" << nickname << ")分数为:" << newScore;
    } else if (nickname == p2_nickname) {
        p2Score = newScore;
        ui->spinBox_p2_score->setValue(newScore);
        qDebug() << "通过点击更新玩家2(" << nickname << ")分数为:" << newScore;
    }
    
    // 保存分数设置
    saveAllSettings();
    
    // 更新OB界面上的比分显示，需要检查ob是否存在
    if (ob) {
        ob->updateScores();
    }
}

// 加载玩家段位
void MainWindow::loadPlayerRanks() {
    // 检查是否有当前玩家的段位信息
    QString p1_nickname = ui->le_p1nickname->text();
    QString p2_nickname = ui->le_p2nickname->text();
    
    updatePlayerRank(1, p1_nickname);
    updatePlayerRank(2, p2_nickname);
}

// 更新玩家段位显示
void MainWindow::updatePlayerRank(int playerIndex, const QString &nickname) {
    if (nickname.isEmpty()) {
        return;
    }
    
    // 从配置中获取段位信息
    QString rank = _cfgm->value("rank/nickname/" + nickname, "无").toString();
    
    // 更新下拉框显示
    QComboBox* rankComboBox = (playerIndex == 1) ? ui->cb_p1_rank : ui->cb_p2_rank;
    int index = rankComboBox->findText(rank);
    if (index != -1) {
        rankComboBox->setCurrentIndex(index);
    } else {
        rankComboBox->setCurrentIndex(0); // 默认为"无"
    }
}

void MainWindow::onBONumberSpinBoxValueChanged(int value) {
    boNumber = value;
    _cfgm->setValue("bo_number", boNumber);
    
    // 同步更新所有模式的BO数spinbox值
    ui->spinBox_bo_number->setValue(boNumber);
    ui->spinBox_bo_number_tech->setValue(boNumber);
    ui->spinBox_bo_number_2v2->setValue(boNumber);
    ui->spinBox_bo_number_3v3->setValue(boNumber);
    
    // 添加检查，确保ob、ob1、obTeam或ob3存在时才设置BO数
    if (ob && currentMode == 0) {
        ob->setBONumber(boNumber);
    } else if (ob1 && currentMode == 1) {
        ob1->setBONumber(boNumber);
    } else if (obTeam && currentMode == 2) {
        obTeam->setBONumber(boNumber);
    } else if (ob3 && currentMode == 3) {
        ob3->setBONumber(boNumber);
    }
}

// 处理PlayerStatusTracker发送的分数变化信号
void MainWindow::onPlayerScoreChanged(const QString &nickname, int newScore) {
    // 更新全局分数记录
    gls->playerScores[nickname] = newScore;
    
    // 检查是哪个玩家的分数变化
    QString p1_nickname = ui->le_p1nickname->text();
    QString p2_nickname = ui->le_p2nickname->text();
    
    // 更新界面上的分数显示
    if (nickname == p1_nickname) {
        p1Score = newScore;
        ui->spinBox_p1_score->setValue(newScore);
        qDebug() << "来自状态跟踪的玩家分数更新：玩家1(" << nickname << ")分数为:" << newScore;
    } else if (nickname == p2_nickname) {
        p2Score = newScore;
        ui->spinBox_p2_score->setValue(newScore);
        qDebug() << "来自状态跟踪的玩家分数更新：玩家2(" << nickname << ")分数为:" << newScore;
    }
    
    // 保存分数设置
    saveAllSettings();
    
    // 更新OB界面上的比分显示，需要检查ob是否存在
    if (ob && currentMode == 0) {
        ob->updateScores();
    }
}

MainWindow::~MainWindow() { 
    // 最后一次保存所有设置，确保不丢失数据
    saveAllSettings();
    
    // 强制立即写入
    if (_cfgm) {
        _cfgm->save();
    }
    
    // 释放ob资源
    if (ob) {
        delete ob;
        ob = nullptr;
    }
    
    // 释放ob1资源
    if (ob1) {
        delete ob1;
        ob1 = nullptr;
    }
    
    // 释放obTeam资源
    if (obTeam) {
        delete obTeam;
        obTeam = nullptr;
    }
    
    // 释放ob3资源
    if (ob3) {
        delete ob3;
        ob3 = nullptr;
    }
    
    delete ui; 
}

// 设置PMB图片控制界面
void MainWindow::setupPmbControls() {
    // 连接按钮信号到槽函数
    connect(ui->btn_change_pmb, &QPushButton::clicked, this, &MainWindow::onBtnChangePmbClicked);
    connect(ui->btn_reset_pmb, &QPushButton::clicked, this, &MainWindow::onBtnResetPmbClicked);
    
    // 从全局设置加载PMB图片显示状态
    ui->chk_show_pmb->setChecked(gls->s.show_pmb_image);
    connect(ui->chk_show_pmb, &QCheckBox::toggled, this, &MainWindow::onChkShowPmbToggled);
    
    // 从配置文件加载PMB位置设置
    int pmbX = _cfgm->value("pmb_position_x", 0).toInt();
    int pmbY = _cfgm->value("pmb_position_y", 0).toInt();
    ui->spinBox_pmb_x->setValue(pmbX);
    ui->spinBox_pmb_y->setValue(pmbY);
    
    // 连接PMB位置控件的信号
    connect(ui->btn_apply_pmb_position, &QPushButton::clicked, this, &MainWindow::onBtnApplyPmbPositionClicked);
    
    // 从配置文件加载PMB大小设置
    int pmbWidth = _cfgm->value("pmb_size_width", 137).toInt();
    int pmbHeight = _cfgm->value("pmb_size_height", 220).toInt();
    ui->spinBox_pmb_width->setValue(pmbWidth);
    ui->spinBox_pmb_height->setValue(pmbHeight);
    
    // 连接PMB大小控件的信号
    connect(ui->btn_apply_pmb_size, &QPushButton::clicked, this, &MainWindow::onBtnApplyPmbSizeClicked);
}

// 添加新方法：处理应用PMB位置按钮点击
void MainWindow::onBtnApplyPmbPositionClicked() {
    // 获取X和Y位置
    int pmbX = ui->spinBox_pmb_x->value();
    int pmbY = ui->spinBox_pmb_y->value();
    
    // 根据当前模式更新位置设置
    if (ob && currentMode == 0) {
        ob->setPmbPosition(pmbX, pmbY);
    } else if (ob1 && currentMode == 1) {
        ob1->setPmbPosition(pmbX, pmbY);
    } else {
        QMessageBox::warning(this, tr("设置失败"), tr("当前模式不支持PMB图片设置。"));
        return;
    }
    
    // 保存位置设置到配置文件
    _cfgm->setValue("pmb_position_x", pmbX);
    _cfgm->setValue("pmb_position_y", pmbY);
    _cfgm->save();
    
    QMessageBox::information(this, tr("设置成功"), tr("广告图片位置已更新。"));
}

// 添加新方法：处理应用PMB大小按钮点击
void MainWindow::onBtnApplyPmbSizeClicked() {
    // 获取宽度和高度
    int pmbWidth = ui->spinBox_pmb_width->value();
    int pmbHeight = ui->spinBox_pmb_height->value();
    
    // 根据当前模式更新大小设置
    if (ob && currentMode == 0) {
        ob->setPmbSize(pmbWidth, pmbHeight);
    } else if (ob1 && currentMode == 1) {
        ob1->setPmbSize(pmbWidth, pmbHeight);
    } else {
        QMessageBox::warning(this, tr("设置失败"), tr("当前模式不支持PMB图片设置。"));
        return;
    }
    
    // 保存大小设置到配置文件
    _cfgm->setValue("pmb_size_width", pmbWidth);
    _cfgm->setValue("pmb_size_height", pmbHeight);
    _cfgm->save();
    
    QMessageBox::information(this, tr("设置成功"), tr("广告图片大小已更新。"));
}

void MainWindow::onBtnChangePmbClicked() {
    QString filePath = QFileDialog::getOpenFileName(this, tr("选择PMB图片"),
                                                  QStandardPaths::writableLocation(QStandardPaths::PicturesLocation),
                                                  tr("图片文件 (*.png *.jpg *.jpeg *.bmp)"));
    if (filePath.isEmpty()) {
        return;
    }
    
    bool success = false;
    // 根据当前模式设置PMB图片
    if (ob && currentMode == 0) {
        ob->setPmbImage(filePath);
        success = true;
    } else if (ob1 && currentMode == 1) {
        ob1->setPmbImage(filePath);
        success = true;
    } else {
        QMessageBox::warning(this, tr("设置失败"), tr("当前模式不支持PMB图片设置。"));
        return;
    }
    
    if (!success) {
        QMessageBox::warning(this, tr("设置失败"), tr("无法设置新的PMB图片，请确认图片格式正确。"));
    } else {
        QMessageBox::information(this, tr("设置成功"), tr("PMB图片已成功更新。"));
    }
}

void MainWindow::onBtnResetPmbClicked() {
    // 弹出确认对话框
    QMessageBox::StandardButton reply = QMessageBox::question(
        this, tr("确认重置"), tr("是否确定将PMB图片重置为默认图片？"),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    
    if (reply == QMessageBox::Yes) {
        // 根据当前模式重置PMB图片
        if (ob && currentMode == 0) {
            ob->resetPmbImage();
        } else if (ob1 && currentMode == 1) {
            ob1->resetPmbImage();
        } else {
            QMessageBox::warning(this, tr("重置失败"), tr("当前模式不支持PMB图片设置。"));
            return;
        }
        
        QMessageBox::information(this, tr("重置成功"), tr("PMB图片已重置为默认图片。"));
    }
}

void MainWindow::onChkShowPmbToggled(bool checked) {
    // 根据当前模式设置PMB图片显示状态
    if (ob && currentMode == 0) {
        ob->setShowPmbImage(checked);
    } else if (ob1 && currentMode == 1) {
        ob1->setShowPmbImage(checked);
    }
    
    // 保存设置到配置文件
    gls->s.show_pmb_image = checked;
    if (_cfgm) {
        _cfgm->setValue("show_pmb_image", checked);
        _cfgm->save();
    }
}



// 修改远程玩家名称更新函数，同时更新本地和远程
void MainWindow::updateRemotePlayerName(const QString &nickname, const QString &playername) {
    // 首先更新本地config.json
    _cfgm->updatePlayer(nickname, playername);
    _cfgm->save();
    
    // 同时更新远程配置
    // 构建请求URL
    // TODO: 配置玩家名称更新API地址
    QUrl url("https://your-server.com/api/update_player_name.php");
    QNetworkRequest request(url);
    
    // 创建multipart请求
    QHttpMultiPart *multiPart = new QHttpMultiPart(QHttpMultiPart::FormDataType);
    
    // 添加昵称参数
    QHttpPart nicknamePart;
    nicknamePart.setHeader(QNetworkRequest::ContentDispositionHeader, 
                         QVariant("form-data; name=\"nickname\""));
    nicknamePart.setBody(nickname.toUtf8());
    multiPart->append(nicknamePart);
    
    // 添加游戏名称参数
    QHttpPart playernamePart;
    playernamePart.setHeader(QNetworkRequest::ContentDispositionHeader, 
                           QVariant("form-data; name=\"playername\""));
    playernamePart.setBody(playername.toUtf8());
    multiPart->append(playernamePart);
    
    // 添加设备唯一标识符
    QHttpPart deviceIdPart;
    deviceIdPart.setHeader(QNetworkRequest::ContentDispositionHeader, 
                         QVariant("form-data; name=\"device_id\""));
    deviceIdPart.setBody(getDeviceIdentifier().toUtf8());
    multiPart->append(deviceIdPart);
    
    // 添加API密钥
    QHttpPart apiKeyPart;
    apiKeyPart.setHeader(QNetworkRequest::ContentDispositionHeader, 
                        QVariant("form-data; name=\"api_key\""));
    apiKeyPart.setBody(API_KEY.toUtf8());
    multiPart->append(apiKeyPart);
    
    // 发送请求
    QNetworkReply *reply = networkManager->post(request, multiPart);
    multiPart->setParent(reply); // reply接管multiPart的生命周期
    
    // 处理响应
    connect(reply, &QNetworkReply::finished, this, [=]() {
        reply->deleteLater();
        
        if (reply->error() == QNetworkReply::NoError) {
            // 读取响应
            QByteArray responseData = reply->readAll();
            
            // 尝试解析JSON响应
            QJsonDocument doc = QJsonDocument::fromJson(responseData);
            QJsonObject json = doc.object();
            
            if (json["success"].toBool()) {
                qDebug() << "玩家" << nickname << "的名称映射已成功更新到远程服务器";
                
                // 名称映射更新后，尝试更新头像
                // 查找当前是玩家1还是玩家2
                if (ui->le_p1nickname->text() == nickname) {
                    loadAvatar(1);
                } else if (ui->le_p2nickname->text() == nickname) {
                    loadAvatar(2);
                }
                
                // 如果OB窗口存在，更新其头像显示
                if (ob) {
                    ob->updatePlayerAvatars();
                }
            } else {
                qDebug() << "更新玩家名称映射到远程失败:" << json["message"].toString();
            }
        } else {
            qDebug() << "远程更新网络请求失败:" << reply->errorString();
        }
    });
}

// 检查本地是否存在某个玩家的映射
bool MainWindow::isPlayerExistLocally(const QString &nickname) {
    QString playername = gls->findNameByNickname(nickname);
    return !playername.isEmpty();
}

// 实现从远程获取单个玩家名称映射
void MainWindow::fetchSinglePlayerName(const QString &nickname) {
    // 如果昵称为空，直接返回
    if (nickname.isEmpty()) {
        return;
    }
    
    // 构建请求URL
    // TODO: 配置获取玩家名称API地址
    QUrl url("https://your-server.com/api/get_player_name.php");
    QUrlQuery query;
    query.addQueryItem("nickname", nickname);
    query.addQueryItem("api_key", API_KEY);
    query.addQueryItem("device_id", getDeviceIdentifier());
    url.setQuery(query);
    
    // 创建请求
    QNetworkRequest request(url);
    
    // 发送GET请求
    QNetworkReply *reply = networkManager->get(request);
    
    // 连接完成信号
    connect(reply, &QNetworkReply::finished, this, [=]() {
        reply->deleteLater();
        
        if (reply->error() == QNetworkReply::NoError) {
            // 读取响应数据
            QByteArray response = reply->readAll();
            QString responseString = QString::fromUtf8(response);
            
            // 解析JSON响应
            QJsonDocument jsonDoc = QJsonDocument::fromJson(response);
            if (!jsonDoc.isNull() && jsonDoc.isObject()) {
                QJsonObject jsonObj = jsonDoc.object();
                
                // 检查是否成功获取玩家名称
                if (jsonObj.contains("success") && jsonObj["success"].toBool() && 
                    jsonObj.contains("player_name")) {
                    QString playerName = jsonObj["player_name"].toString();
                    
                    // 更新全局设置中的玩家名称映射
                    if (!playerName.isEmpty()) {
                        // 更新配置
                        _cfgm->updatePlayer(nickname, playerName);
                        _cfgm->save();
                        
                        // 发出玩家映射更新信号
                        emit playerMappingUpdated();
                        
                        qDebug() << "从远程获取玩家名称成功:" << nickname << "->" << playerName;
                        
                        // 更新1v1模式UI
                        if (currentMode == 0 && ob) {
                            // 检查是否是玩家1或玩家2的昵称
                            if (nickname == ui->le_p1nickname->text()) {
                                ui->le_p1playername->setText(playerName);
                                ui->lb_p1_score->setText(playerName);
                            } else if (nickname == ui->le_p2nickname->text()) {
                                ui->le_p2playername->setText(playerName);
                                ui->lb_p2_score->setText(playerName);
                            }
                        }
                        // 更新1v1科技主题模式UI
                        else if (currentMode == 1 && ob1) {
                            // 检查是否是玩家1或玩家2的昵称
                            if (nickname == ui->le_p1nickname_tech->text()) {
                                // 只有当科技模式的显示名称为空时才自动填充
                                if (ui->le_p1playername_tech->text().isEmpty()) {
                                    ui->le_p1playername_tech->setText(playerName);
                                    ui->lb_p1_score_tech->setText(playerName);
                                }
                            } else if (nickname == ui->le_p2nickname_tech->text()) {
                                // 只有当科技模式的显示名称为空时才自动填充
                                if (ui->le_p2playername_tech->text().isEmpty()) {
                                    ui->le_p2playername_tech->setText(playerName);
                                    ui->lb_p2_score_tech->setText(playerName);
                                }
                            }
                            
                            // 触发ob1窗口刷新玩家名称
                            ob1->forceRefreshPlayerNames();
                        }
                        // 更新2v2模式UI
                        else if (currentMode == 2 && obTeam) {
                            // 检查是否是任何队伍玩家的昵称并更新
                            if (nickname == ui->le_team1_p1_nickname->text()) {
                                ui->le_team1_p1_playername->setText(playerName);
                            } else if (nickname == ui->le_team1_p2_nickname->text()) {
                                ui->le_team1_p2_playername->setText(playerName);
                            } else if (nickname == ui->le_team2_p1_nickname->text()) {
                                ui->le_team2_p1_playername->setText(playerName);
                            } else if (nickname == ui->le_team2_p2_nickname->text()) {
                                ui->le_team2_p2_playername->setText(playerName);
                            }
                            
                            // 触发ObTeam窗口重绘
                            obTeam->update();
                        }
                    }
                }
            }
        } else {
            qDebug() << "获取玩家名称失败:" << reply->errorString();
        }
    });
}

// 实现从远程获取玩家名称映射（修改逻辑为备份服务，只在必要时请求）
void MainWindow::fetchRemotePlayerNames() {
    // TODO: 配置批量获取玩家名称API地址
    QUrl url("https://your-server.com/api/get_player_names.php");
    QUrlQuery query;
    query.addQueryItem("api_key", API_KEY);
    query.addQueryItem("device_id", getDeviceIdentifier());
    url.setQuery(query);
    
    QNetworkRequest request(url);
    
    QNetworkReply *reply = networkManager->get(request);
    
    connect(reply, &QNetworkReply::finished, this, [=]() {
        reply->deleteLater();
        
        if (reply->error() == QNetworkReply::NoError) {
            // 读取响应
            QByteArray responseData = reply->readAll();
            
            // 尝试解析JSON响应
            QJsonDocument doc = QJsonDocument::fromJson(responseData);
            
            if (doc.isArray()) {
                QJsonArray playersArray = doc.array();
                
                for (const QJsonValue &value : playersArray) {
                    QJsonObject player = value.toObject();
                    QString nickname = player["nickname"].toString();
                    QString playername = player["playername"].toString();
                    
                    // 仅当本地没有这个玩家映射时才添加
                    if (!isPlayerExistLocally(nickname)) {
                        // 更新到本地配置文件
                        _cfgm->updatePlayer(nickname, playername);
                        
                        // 同时更新到内存缓存
                        gls->addPlayerMapping(nickname, playername);
                        
                        qDebug() << "添加远程玩家映射到本地:" << nickname << "->" << playername;
                    }
                }
                
                // 完成后保存配置
                _cfgm->save();
                
                qDebug() << "已从远程服务器获取缺失的玩家名称映射数据";
            } else {
                QJsonObject json = doc.object();
                QString errorMsg = json["message"].toString();
                qDebug() << "获取玩家名称映射失败:" << errorMsg;
            }
        } else {
            qDebug() << "获取玩家名称映射网络请求失败:" << reply->errorString();
        }
    });
}

// 实现游戏进程状态监控函数
void MainWindow::monitorGameStatus() {
    // 获取游戏有效性状态
    bool currentGameStatus = false;
    
    // 通过Ra2ob::Game获取当前游戏状态，需要检查ob是否存在
    if (ob && ob->getGamePtr()) {
        currentGameStatus = ob->getGamePtr()->_gameInfo.valid;
    } else {
        // 如果ob不存在，可以直接从Ra2ob::Game获取状态
        Ra2ob::Game &g = Ra2ob::Game::getInstance();
        currentGameStatus = g._gameInfo.valid;
    }
    
    // 检测游戏进程从无到有的转变
    if (currentGameStatus && !lastGameStatus) {
        qDebug() << "检测到游戏进程启动，检查是否需要更新玩家映射...";
        
        // 检查当前玩家是否在本地有映射，如果没有才从远程获取
        QString p1_nickname, p2_nickname;
        
        if (ob && ob->getGamePtr()) {
            // 获取当前游戏中的玩家
            for (int i = 0; i < 8; i++) {  // 假设最多8个玩家
                if (ob->getGamePtr()->_gameInfo.players[i].valid) {
                    // 如果是第一个有效玩家
                    if (p1_nickname.isEmpty()) {
                        p1_nickname = QString::fromStdString(ob->getGamePtr()->_gameInfo.players[i].panel.playerNameUtf);
                        
                        // 检查是否需要从远程获取
                        if (!isPlayerExistLocally(p1_nickname)) {
                            fetchSinglePlayerName(p1_nickname);
                        }
                    }
                    // 如果是第二个有效玩家
                    else if (p2_nickname.isEmpty()) {
                        p2_nickname = QString::fromStdString(ob->getGamePtr()->_gameInfo.players[i].panel.playerNameUtf);
                        
                        // 检查是否需要从远程获取
                        if (!isPlayerExistLocally(p2_nickname)) {
                            fetchSinglePlayerName(p2_nickname);
                        }
                    }
                }
            }
        }
        
        // 更新配置刷新时间
        lastConfigUpdateTime = QTime::currentTime();
    }
    
    // 如果游戏正在运行，检查是否需要定期更新配置
    if (currentGameStatus) {
        // 计算距离上次更新的秒数
        int secondsSinceLastUpdate = lastConfigUpdateTime.secsTo(QTime::currentTime());
        
        // 如果已经过了50秒，检查当前玩家是否需要更新
    if (secondsSinceLastUpdate >= 50) {
        qDebug() << "游戏运行中，获取所有远程玩家映射更新（50秒间隔）...";
            
            // 获取所有远程玩家映射（与程序启动时一样）
            fetchRemotePlayerNames();
            
            // 更新配置刷新时间
            lastConfigUpdateTime = QTime::currentTime();
        }
    }
    
    // 更新上次状态
    lastGameStatus = currentGameStatus;
}

// 更新比分显示
void MainWindow::updateScore() {
    QString p1_nickname = ui->le_p1nickname->text();
    QString p2_nickname = ui->le_p2nickname->text();
    
    // 更新比分显示
    if (!p1_nickname.isEmpty() && gls->playerScores.contains(p1_nickname)) {
        ui->spinBox_p1_score->setValue(gls->playerScores[p1_nickname]);
    } else {
        ui->spinBox_p1_score->setValue(0);
    }
    
    if (!p2_nickname.isEmpty() && gls->playerScores.contains(p2_nickname)) {
        ui->spinBox_p2_score->setValue(gls->playerScores[p2_nickname]);
    } else {
        ui->spinBox_p2_score->setValue(0);
    }
    
    // 仅在1v1模式(ob存在)时更新ob窗口上的比分
    if (ob && currentMode == 0) {
        ob->updateScores();
    }
}

// 更新标签切换处理函数
void MainWindow::onTabIndexChanged(int index) {
    // 处理1v1模式(索引0)、1v1科技主题(索引1)、2v2模式(索引2)和3v3模式(索引3)之间的切换
    if (index > 3) {
        // 对于其他标签页（帮助、广告logo、关于等），不改变游戏模式
        return;
    }
    
    // 保存旧模式
    int oldMode = currentMode;
    currentMode = index;
    
    // 模式切换处理
    if (oldMode != currentMode) {
        qDebug() << "模式切换:" << (currentMode == 0 ? "1v1模式" : (currentMode == 1 ? "1v1科技主题" : (currentMode == 2 ? "2v2模式" : "3v3模式")));
        
        // 同步所有标签页的BO数值
        if (currentMode == 0) {
            // 从其他模式切换到1v1，更新1v1的BO数
            ui->spinBox_bo_number->setValue(boNumber);
        } else if (currentMode == 1) {
            // 从其他模式切换到1v1科技主题，更新1v1科技主题的BO数
            ui->spinBox_bo_number_tech->setValue(boNumber);
        } else if (currentMode == 2) {
            // 从其他模式切换到2v2，更新2v2的BO数
            ui->spinBox_bo_number_2v2->setValue(boNumber);
        } else {
            // 从其他模式切换到3v3，更新3v3的BO数
            ui->spinBox_bo_number_3v3->setValue(boNumber);
        }
        
        // 处理模式切换
        if (currentMode == 0) { // 1v1模式
            // 释放其他模式资源
            if (ob1) {
                delete ob1;
                ob1 = nullptr;
            }
            if (obTeam) {
                delete obTeam;
                obTeam = nullptr;
            }
            if (ob3) {
                delete ob3;
                ob3 = nullptr;
            }
            
            // 创建1v1模式实例
            if (!ob) {
                ob = new Ob();
                connect(ob, &Ob::playernameNeedsUpdate, this, &MainWindow::updatePlayername);
                connect(ob, &Ob::scoreChanged, this, &MainWindow::onScoreChanged);
                
                // 设置BO数
                ob->setBONumber(boNumber);
                
                // 应用PMB位置设置
                int pmbX = _cfgm->value("pmb_position_x", 0).toInt();
                int pmbY = _cfgm->value("pmb_position_y", 0).toInt();
                ob->setPmbPosition(pmbX, pmbY);
                
                // 应用PMB大小设置
                int pmbWidth = _cfgm->value("pmb_size_width", 137).toInt();
                int pmbHeight = _cfgm->value("pmb_size_height", 220).toInt();
                ob->setPmbSize(pmbWidth, pmbHeight);
            }
        } 
        else if (currentMode == 1) { // 1v1科技主题模式
            // 释放其他模式资源
            if (ob) {
                delete ob;
                ob = nullptr;
            }
            if (obTeam) {
                delete obTeam;
                obTeam = nullptr;
            }
            if (ob3) {
                delete ob3;
                ob3 = nullptr;
            }
            
            // 创建科技主题实例
            if (!ob1) {
                ob1 = new Ob1();
                
                // 连接ob1的playernameNeedsUpdate信号到updatePlayernameTech方法
                bool connection1 = connect(ob1, &Ob1::playernameNeedsUpdate, this, &MainWindow::updatePlayernameTech, Qt::QueuedConnection);
                // 连接MainWindow的playerMappingUpdated信号到ob1的forceRefreshPlayerNames槽
                bool connection2 = connect(this, &MainWindow::playerMappingUpdated, ob1, &Ob1::forceRefreshPlayerNames, Qt::QueuedConnection);
                qDebug() << "[MainWindow] 科技主题ob1实例已创建，信号连接已建立 - playernameNeedsUpdate:" << connection1 << ", playerMappingUpdated:" << connection2;
                
                // 设置BO数
                ob1->setBONumber(boNumber);
                
                // 应用PMB位置设置
                int pmbX = _cfgm->value("pmb_position_x", 0).toInt();
                int pmbY = _cfgm->value("pmb_position_y", 0).toInt();
                ob1->setPmbPosition(pmbX, pmbY);
                
                // 应用PMB大小设置
                int pmbWidth = _cfgm->value("pmb_size_width", 137).toInt();
                int pmbHeight = _cfgm->value("pmb_size_height", 220).toInt();
                ob1->setPmbSize(pmbWidth, pmbHeight);
                
                // 加载1V1科技模式滚动文字设置
                loadScrollTextSettingsTech();
            }
        }
        else if (currentMode == 2) { // 2v2模式
            // 释放其他模式资源
            if (ob) {
                delete ob;
                ob = nullptr;
            }
            if (ob1) {
                delete ob1;
                ob1 = nullptr;
            }
            if (ob3) {
                delete ob3;
                ob3 = nullptr;
            }
            
            // 创建2v2模式实例
            if (!obTeam) {
                obTeam = new ObTeam(nullptr);
                
                // 设置BO数
                obTeam->setBONumber(boNumber);
                
                // 先同步更新一次玩家映射（不使用延迟）
                updateTeamPlayername();
                
                // 现在显示ObTeam窗口
                obTeam->showObTeam();
                
                // 连接定时器，定期更新2v2模式的玩家名称映射
                QTimer *teamNameUpdateTimer = new QTimer(this);
                connect(teamNameUpdateTimer, &QTimer::timeout, this, &MainWindow::updateTeamPlayername);
                teamNameUpdateTimer->start(9000); // 每9秒更新一次
            }
        }
        else { // 3v3模式
            // 释放其他模式资源
            if (ob) {
                delete ob;
                ob = nullptr;
            }
            if (ob1) {
                delete ob1;
                ob1 = nullptr;
            }
            if (obTeam) {
                delete obTeam;
                obTeam = nullptr;
            }
            
            // 创建3v3模式实例
            if (!ob3) {
                ob3 = new Ob3(nullptr);
                
                // 设置BO数
                ob3->setBONumber(boNumber);
                
                // 先同步更新一次玩家映射（不使用延迟）
                updateTeam3v3Playername();
                
                // 现在显示Ob3窗口
                ob3->showOb3();
                
                // 连接定时器，定期更新3v3模式的玩家名称映射
                QTimer *team3v3NameUpdateTimer = new QTimer(this);
                connect(team3v3NameUpdateTimer, &QTimer::timeout, this, &MainWindow::updateTeam3v3Playername);
                team3v3NameUpdateTimer->start(9000); // 每9秒更新一次
            }
        }
        
        // 保存当前模式到配置
        _cfgm->setValue("mode", currentMode);
    }
}

// 更新2v2模式的玩家名称映射
void MainWindow::updateTeamPlayername() {
    if (!obTeam) return;
    
    // 获取游戏实例
    Ra2ob::Game* game = obTeam->getGamePtr();
    if (!game) return;
    
    // 跟踪是否有任何UI变更，只在有变化时重绘ObTeam窗口
    bool anyUiChange = false;
    
    // 临时存储队伍玩家信息，用于填充UI
    QMap<int, QList<int>> teamPlayers;
    QList<int> uniqueTeams;
    
    // 检测队伍关系
    for (int i = 0; i < Ra2ob::MAXPLAYER; i++) {
        if (game->_gameInfo.players[i].valid) {
            // 使用status.teamNumber代替不存在的team成员
            int teamId = game->_gameInfo.players[i].status.teamNumber;
            if (!teamPlayers.contains(teamId)) {
                teamPlayers[teamId] = QList<int>();
                uniqueTeams.append(teamId);
            }
            teamPlayers[teamId].append(i);
        }
    }
    
    // 只处理有两个队伍的情况
    if (uniqueTeams.size() >= 2) {
        int team1Id = uniqueTeams[0];
        int team2Id = uniqueTeams[1];
        
        QList<int> team1Players = teamPlayers[team1Id];
        QList<int> team2Players = teamPlayers[team2Id];
        
        // 更新队伍1玩家信息
        if (team1Players.size() >= 1) {
            int playerIdx = team1Players[0];
            QString nickname = QString::fromUtf8(game->_gameInfo.players[playerIdx].panel.playerNameUtf);
            // 只有当昵称不为空且与当前显示不同时才更新界面
            if (!nickname.isEmpty() && nickname != ui->le_team1_p1_nickname->text()) {
                ui->le_team1_p1_nickname->setText(nickname);
                anyUiChange = true;
                
                QString playername = getPlayerNameFromConfig(nickname);
                // 只有当实际名称不同时才更新
                if (playername != ui->le_team1_p1_playername->text()) {
                    ui->le_team1_p1_playername->setText(playername);
                    anyUiChange = true;
                }
                
                // 如果配置文件中找不到，尝试从远程获取（异步请求）
                if (playername.isEmpty()) {
                    fetchSinglePlayerName(nickname);
                }
            }
        }
        
        if (team1Players.size() >= 2) {
            int playerIdx = team1Players[1];
            QString nickname = QString::fromUtf8(game->_gameInfo.players[playerIdx].panel.playerNameUtf);
            // 只有当昵称不为空且与当前显示不同时才更新界面
            if (!nickname.isEmpty() && nickname != ui->le_team1_p2_nickname->text()) {
                ui->le_team1_p2_nickname->setText(nickname);
                anyUiChange = true;
                
                QString playername = getPlayerNameFromConfig(nickname);
                // 只有当实际名称不同时才更新
                if (playername != ui->le_team1_p2_playername->text()) {
                    ui->le_team1_p2_playername->setText(playername);
                    anyUiChange = true;
                }
                
                // 如果配置文件中找不到，尝试从远程获取（异步请求）
                if (playername.isEmpty()) {
                    fetchSinglePlayerName(nickname);
                }
            }
        }
        
        // 更新队伍2玩家信息
        if (team2Players.size() >= 1) {
            int playerIdx = team2Players[0];
            QString nickname = QString::fromUtf8(game->_gameInfo.players[playerIdx].panel.playerNameUtf);
            // 只有当昵称不为空且与当前显示不同时才更新界面
            if (!nickname.isEmpty() && nickname != ui->le_team2_p1_nickname->text()) {
                ui->le_team2_p1_nickname->setText(nickname);
                anyUiChange = true;
                
                QString playername = getPlayerNameFromConfig(nickname);
                // 只有当实际名称不同时才更新
                if (playername != ui->le_team2_p1_playername->text()) {
                    ui->le_team2_p1_playername->setText(playername);
                    anyUiChange = true;
                }
                
                // 如果配置文件中找不到，尝试从远程获取（异步请求）
                if (playername.isEmpty()) {
                    fetchSinglePlayerName(nickname);
                }
            }
        }
        
        if (team2Players.size() >= 2) {
            int playerIdx = team2Players[1];
            QString nickname = QString::fromUtf8(game->_gameInfo.players[playerIdx].panel.playerNameUtf);
            // 只有当昵称不为空且与当前显示不同时才更新界面
            if (!nickname.isEmpty() && nickname != ui->le_team2_p2_nickname->text()) {
                ui->le_team2_p2_nickname->setText(nickname);
                anyUiChange = true;
                
                QString playername = getPlayerNameFromConfig(nickname);
                // 只有当实际名称不同时才更新
                if (playername != ui->le_team2_p2_playername->text()) {
                    ui->le_team2_p2_playername->setText(playername);
                    anyUiChange = true;
                }
                
                // 如果配置文件中找不到，尝试从远程获取（异步请求）
                if (playername.isEmpty()) {
                    fetchSinglePlayerName(nickname);
                }
            }
        }
    }
    
    // 遍历所有玩家，确保所有玩家的昵称都有对应的映射
    for (int i = 0; i < Ra2ob::MAXPLAYER; i++) {
        if (game->_gameInfo.players[i].valid) {
            // 获取玩家昵称
            QString nickname = QString::fromUtf8(game->_gameInfo.players[i].panel.playerNameUtf);
            
            // 如果昵称不为空
            if (!nickname.isEmpty()) {
                // 直接从配置文件获取玩家名称
                QString playername = getPlayerNameFromConfig(nickname);
                
                // 如果配置文件中找不到，尝试从远程获取（异步请求）
                if (playername.isEmpty()) {
                    fetchSinglePlayerName(nickname);
                }
            }
        }
    }
    
    // 只有当UI有变化时才触发ObTeam窗口重绘
    if (anyUiChange && obTeam->isObTeamVisible()) {
        obTeam->update();
    }
    
    // 更新队伍比分 - 只在队伍ID变化时才更新
    updateTeamScores();
}

// 添加2v2模式更新玩家映射的槽函数
void MainWindow::onBtnUpdatePlayer2v2Clicked() {
    bool hasUpdates = false;
    QStringList updatedPlayers;
    
    // 获取队伍1玩家1的映射
    QString team1_p1_nickname = ui->le_team1_p1_nickname->text().trimmed();
    QString team1_p1_playername = ui->le_team1_p1_playername->text().trimmed();
    if (!team1_p1_nickname.isEmpty() && !team1_p1_playername.isEmpty()) {
        _cfgm->updatePlayer(team1_p1_nickname, team1_p1_playername);
        qDebug() << "更新队伍1玩家1映射:" << team1_p1_nickname << "->" << team1_p1_playername;
        // 更新到远程服务器
        updateRemotePlayerName(team1_p1_nickname, team1_p1_playername);
        hasUpdates = true;
        updatedPlayers.append(team1_p1_nickname);
    }
    
    // 获取队伍1玩家2的映射
    QString team1_p2_nickname = ui->le_team1_p2_nickname->text().trimmed();
    QString team1_p2_playername = ui->le_team1_p2_playername->text().trimmed();
    if (!team1_p2_nickname.isEmpty() && !team1_p2_playername.isEmpty()) {
        _cfgm->updatePlayer(team1_p2_nickname, team1_p2_playername);
        qDebug() << "更新队伍1玩家2映射:" << team1_p2_nickname << "->" << team1_p2_playername;
        // 更新到远程服务器
        updateRemotePlayerName(team1_p2_nickname, team1_p2_playername);
        hasUpdates = true;
        updatedPlayers.append(team1_p2_nickname);
    }
    
    // 获取队伍2玩家1的映射
    QString team2_p1_nickname = ui->le_team2_p1_nickname->text().trimmed();
    QString team2_p1_playername = ui->le_team2_p1_playername->text().trimmed();
    if (!team2_p1_nickname.isEmpty() && !team2_p1_playername.isEmpty()) {
        _cfgm->updatePlayer(team2_p1_nickname, team2_p1_playername);
        qDebug() << "更新队伍2玩家1映射:" << team2_p1_nickname << "->" << team2_p1_playername;
        // 更新到远程服务器
        updateRemotePlayerName(team2_p1_nickname, team2_p1_playername);
        hasUpdates = true;
        updatedPlayers.append(team2_p1_nickname);
    }
    
    // 获取队伍2玩家2的映射
    QString team2_p2_nickname = ui->le_team2_p2_nickname->text().trimmed();
    QString team2_p2_playername = ui->le_team2_p2_playername->text().trimmed();
    if (!team2_p2_nickname.isEmpty() && !team2_p2_playername.isEmpty()) {
        _cfgm->updatePlayer(team2_p2_nickname, team2_p2_playername);
        qDebug() << "更新队伍2玩家2映射:" << team2_p2_nickname << "->" << team2_p2_playername;
        // 更新到远程服务器
        updateRemotePlayerName(team2_p2_nickname, team2_p2_playername);
        hasUpdates = true;
        updatedPlayers.append(team2_p2_nickname);
    }
    
    // 保存配置
    _cfgm->save();
    
    // 如果有更新，发出玩家映射更新信号
    if (hasUpdates) {
        emit playerMappingUpdated();
        qDebug() << "[MainWindow] onBtnUpdatePlayer2v2Clicked发出playerMappingUpdated信号";
    }
    
    // 立即更新2v2模式的玩家名称显示
    updateTeamPlayername();
}

// 队伍1比分变化处理
void MainWindow::onTeam1ScoreSpinBoxValueChanged(int value) {
    // 获取当前队伍1的玩家
    QString team1_p1 = ui->le_team1_p1_nickname->text().trimmed();
    QString team1_p2 = ui->le_team1_p2_nickname->text().trimmed();
    
    // 计算队伍ID (排序后的玩家名称组合)
    QStringList team1Players;
    if (!team1_p1.isEmpty()) team1Players << team1_p1;
    if (!team1_p2.isEmpty()) team1Players << team1_p2;
    team1Players.sort(); // 排序确保A+B和B+A被识别为同一队伍
    
    QString team1Id = team1Players.join("+");
    
    // 只有当队伍ID有效时才更新比分
    if (!team1Id.isEmpty()) {
        // 保存到GlobalSetting中
        gls->teamScores[team1Id] = value;
        
        // 更新ObTeam中的显示
        if (obTeam) {
            obTeam->setTeamInfo(team1Id, obTeam->getTeam2Id());
        }
        
        // 更新队伍显示名称
        updateTeamScoreDisplayLabels();
    }
}

// 队伍2比分变化处理
void MainWindow::onTeam2ScoreSpinBoxValueChanged(int value) {
    // 获取当前队伍2的玩家
    QString team2_p1 = ui->le_team2_p1_nickname->text().trimmed();
    QString team2_p2 = ui->le_team2_p2_nickname->text().trimmed();
    
    // 计算队伍ID (排序后的玩家名称组合)
    QStringList team2Players;
    if (!team2_p1.isEmpty()) team2Players << team2_p1;
    if (!team2_p2.isEmpty()) team2Players << team2_p2;
    team2Players.sort(); // 排序确保C+D和D+C被识别为同一队伍
    
    QString team2Id = team2Players.join("+");
    
    // 只有当队伍ID有效时才更新比分
    if (!team2Id.isEmpty()) {
        // 保存到GlobalSetting中
        gls->teamScores[team2Id] = value;
        
        // 更新ObTeam中的显示
        if (obTeam) {
            obTeam->setTeamInfo(obTeam->getTeam1Id(), team2Id);
        }
        
        // 更新队伍显示名称
        updateTeamScoreDisplayLabels();
    }
}

// 更新队伍比分显示标签
void MainWindow::updateTeamScoreDisplayLabels() {
    // 获取当前队伍1的玩家
    QString team1_p1 = ui->le_team1_p1_nickname->text().trimmed();
    QString team1_p2 = ui->le_team1_p2_nickname->text().trimmed();
    QString team1_p1_name = ui->le_team1_p1_playername->text().trimmed();
    QString team1_p2_name = ui->le_team1_p2_playername->text().trimmed();
    
    // 获取当前队伍2的玩家
    QString team2_p1 = ui->le_team2_p1_nickname->text().trimmed();
    QString team2_p2 = ui->le_team2_p2_nickname->text().trimmed();
    QString team2_p1_name = ui->le_team2_p1_playername->text().trimmed();
    QString team2_p2_name = ui->le_team2_p2_playername->text().trimmed();
    
    // 使用显示名称(如果有)或昵称
    QString team1_p1_display = team1_p1_name.isEmpty() ? team1_p1 : team1_p1_name;
    QString team1_p2_display = team1_p2_name.isEmpty() ? team1_p2 : team1_p2_name;
    QString team2_p1_display = team2_p1_name.isEmpty() ? team2_p1 : team2_p1_name;
    QString team2_p2_display = team2_p2_name.isEmpty() ? team2_p2 : team2_p2_name;
    
    // 生成队伍显示文本
    QString team1Display = "队伍1比分: ";
    if (!team1_p1_display.isEmpty()) {
        team1Display += team1_p1_display;
        if (!team1_p2_display.isEmpty()) {
            team1Display += " + " + team1_p2_display;
        }
    }
    
    QString team2Display = "队伍2比分: ";
    if (!team2_p1_display.isEmpty()) {
        team2Display += team2_p1_display;
        if (!team2_p2_display.isEmpty()) {
            team2Display += " + " + team2_p2_display;
        }
    }
    
    // 更新显示标签
    ui->lb_team1_score_display->setText(team1Display);
    ui->lb_team2_score_display->setText(team2Display);
}

// 更新2v2队伍比分
void MainWindow::updateTeamScores() {
    if (!obTeam) return;
    
    // 获取当前队伍的玩家
    QString team1_p1 = ui->le_team1_p1_nickname->text().trimmed();
    QString team1_p2 = ui->le_team1_p2_nickname->text().trimmed();
    QString team2_p1 = ui->le_team2_p1_nickname->text().trimmed();
    QString team2_p2 = ui->le_team2_p2_nickname->text().trimmed();
    
    // 计算队伍ID
    QStringList team1Players;
    if (!team1_p1.isEmpty()) team1Players << team1_p1;
    if (!team1_p2.isEmpty()) team1Players << team1_p2;
    team1Players.sort();
    
    QStringList team2Players;
    if (!team2_p1.isEmpty()) team2Players << team2_p1;
    if (!team2_p2.isEmpty()) team2Players << team2_p2;
    team2Players.sort();
    
    QString team1Id = team1Players.join("+");
    QString team2Id = team2Players.join("+");
    
    // 设置ObTeam中的队伍信息
    if (!team1Id.isEmpty() || !team2Id.isEmpty()) {
        obTeam->setTeamInfo(team1Id, team2Id);
    }
    
    // 更新UI中的比分显示
    if (!team1Id.isEmpty() && gls->teamScores.contains(team1Id)) {
        ui->spinBox_team1_score->setValue(gls->teamScores[team1Id]);
    } else {
        ui->spinBox_team1_score->setValue(0);
    }
    
    if (!team2Id.isEmpty() && gls->teamScores.contains(team2Id)) {
        ui->spinBox_team2_score->setValue(gls->teamScores[team2Id]);
    } else {
        ui->spinBox_team2_score->setValue(0);
    }
    
    // 更新队伍显示名称
    updateTeamScoreDisplayLabels();
}

// 新增函数：直接从配置文件读取玩家名称
QString MainWindow::getPlayerNameFromConfig(const QString &nickname) {
    // 如果nickname为空直接返回空字符串
    if (nickname.isEmpty()) return QString();
    
    // 直接从配置文件读取
    QFile configFile(QCoreApplication::applicationDirPath() + "/config/config.json");
    if (!configFile.open(QIODevice::ReadOnly)) {
        qDebug() << "无法打开配置文件";
        return QString();
    }
    
    QByteArray jsonData = configFile.readAll();
    configFile.close();
    
    QJsonDocument doc = QJsonDocument::fromJson(jsonData);
    if (doc.isNull() || !doc.isObject()) {
        qDebug() << "配置文件格式不正确";
        return QString();
    }
    
    QJsonObject config = doc.object();
    
    // 实际的结构是"Players"键下的数组，每个元素是包含nickname和playername的对象
    if (config.contains("Players") && config["Players"].isArray()) {
        QJsonArray players = config["Players"].toArray();
        
        // 遍历数组查找匹配的nickname
        for (const QJsonValue &playerValue : players) {
            QJsonObject player = playerValue.toObject();
            if (player["nickname"].toString() == nickname) {
                QString playername = player["playername"].toString();
                
                // 仍然更新内存缓存以确保其他依赖于它的部分能正常工作
                gls->addPlayerMapping(nickname, playername);
                
                qDebug() << "从配置文件找到玩家映射:" << nickname << "->" << playername;
                return playername;
            }
        }
    }
    
    qDebug() << "配置文件中未找到玩家:" << nickname;
    // 配置文件中也没有找到
    return QString();
}

// 更新3v3模式的玩家名称映射
void MainWindow::updateTeam3v3Playername() {
    if (!ob3) return;
    
    // 获取游戏实例
    Ra2ob::Game* game = ob3->getGamePtr();
    if (!game) return;
    
    // 跟踪是否有任何UI变更，只在有变化时重绘Ob3窗口
    bool anyUiChange = false;
    
    // 临时存储队伍玩家信息，用于填充UI
    QMap<int, QList<int>> teamPlayers;
    QList<int> uniqueTeams;
    
    // 检测队伍关系
    for (int i = 0; i < Ra2ob::MAXPLAYER; i++) {
        if (game->_gameInfo.players[i].valid) {
            // 使用status.teamNumber代替不存在的team成员
            int teamId = game->_gameInfo.players[i].status.teamNumber;
            if (!teamPlayers.contains(teamId)) {
                teamPlayers[teamId] = QList<int>();
                uniqueTeams.append(teamId);
            }
            teamPlayers[teamId].append(i);
        }
    }
    
    // 只处理有两个队伍的情况
    if (uniqueTeams.size() >= 2) {
        int team1Id = uniqueTeams[0];
        int team2Id = uniqueTeams[1];
        
        QList<int> team1Players = teamPlayers[team1Id];
        QList<int> team2Players = teamPlayers[team2Id];
        
        // 更新队伍1玩家信息 - 支持最多3个玩家
        for (int i = 0; i < qMin(3, team1Players.size()); i++) {
            int playerIdx = team1Players[i];
            QString nickname = QString::fromUtf8(game->_gameInfo.players[playerIdx].panel.playerNameUtf);
            
            // 构建动态控件名称
            QString leNicknameObjName = QString("le_team1_p%1_nickname_3v3").arg(i+1);
            QString lePlayernameObjName = QString("le_team1_p%1_playername_3v3").arg(i+1);
            
            // 获取QLineEdit控件指针
            QLineEdit *leNickname = ui->centralwidget->findChild<QLineEdit*>(leNicknameObjName);
            QLineEdit *lePlayername = ui->centralwidget->findChild<QLineEdit*>(lePlayernameObjName);
            
            if (leNickname && !nickname.isEmpty() && nickname != leNickname->text()) {
                leNickname->setText(nickname);
                anyUiChange = true;
                
                // 更新玩家实际名称
                if (lePlayername) {
                    QString playername = getPlayerNameFromConfig(nickname);
                    if (playername != lePlayername->text()) {
                        lePlayername->setText(playername);
                        anyUiChange = true;
                    }
                    
                    // 如果配置文件中找不到，尝试从远程获取（异步请求）
                    if (playername.isEmpty()) {
                        fetchSinglePlayerName(nickname);
                    }
                }
            }
        }
        
        // 更新队伍2玩家信息 - 支持最多3个玩家
        for (int i = 0; i < qMin(3, team2Players.size()); i++) {
            int playerIdx = team2Players[i];
            QString nickname = QString::fromUtf8(game->_gameInfo.players[playerIdx].panel.playerNameUtf);
            
            // 构建动态控件名称
            QString leNicknameObjName = QString("le_team2_p%1_nickname_3v3").arg(i+1);
            QString lePlayernameObjName = QString("le_team2_p%1_playername_3v3").arg(i+1);
            
            // 获取QLineEdit控件指针
            QLineEdit *leNickname = ui->centralwidget->findChild<QLineEdit*>(leNicknameObjName);
            QLineEdit *lePlayername = ui->centralwidget->findChild<QLineEdit*>(lePlayernameObjName);
            
            if (leNickname && !nickname.isEmpty() && nickname != leNickname->text()) {
                leNickname->setText(nickname);
                anyUiChange = true;
                
                // 更新玩家实际名称
                if (lePlayername) {
                    QString playername = getPlayerNameFromConfig(nickname);
                    if (playername != lePlayername->text()) {
                        lePlayername->setText(playername);
                        anyUiChange = true;
                    }
                    
                    // 如果配置文件中找不到，尝试从远程获取（异步请求）
                    if (playername.isEmpty()) {
                        fetchSinglePlayerName(nickname);
                    }
                }
            }
        }
        
        // 更新团队ID - 修复逻辑错误，避免分别调用setTeamInfo导致队伍ID被清空
        QString finalTeam1Id, finalTeam2Id;
        
        if (team1Players.size() > 0) {
            QStringList team1Names;
            for (int i = 0; i < qMin(3, team1Players.size()); i++) {
                int playerIdx = team1Players[i];
                QString nickname = QString::fromUtf8(game->_gameInfo.players[playerIdx].panel.playerNameUtf);
                QString playername = getPlayerNameFromConfig(nickname);
                if (!playername.isEmpty()) {
                    team1Names.append(playername);
                } else {
                    team1Names.append(nickname);
                }
            }
            
            if (!team1Names.isEmpty()) {
                team1Names.sort(); // 排序确保队伍ID一致性
                finalTeam1Id = team1Names.join("+");
                anyUiChange = true;
            }
        }
        
        if (team2Players.size() > 0) {
            QStringList team2Names;
            for (int i = 0; i < qMin(3, team2Players.size()); i++) {
                int playerIdx = team2Players[i];
                QString nickname = QString::fromUtf8(game->_gameInfo.players[playerIdx].panel.playerNameUtf);
                QString playername = getPlayerNameFromConfig(nickname);
                if (!playername.isEmpty()) {
                    team2Names.append(playername);
                } else {
                    team2Names.append(nickname);
                }
            }
            
            if (!team2Names.isEmpty()) {
                team2Names.sort(); // 排序确保队伍ID一致性
                finalTeam2Id = team2Names.join("+");
                anyUiChange = true;
            }
        }
        
        // 一次性设置两个队伍的ID，避免分别调用导致的问题
        if (!finalTeam1Id.isEmpty() || !finalTeam2Id.isEmpty()) {
            // 如果某个队伍ID为空，保持原有的ID不变
            QString currentTeam1Id = finalTeam1Id.isEmpty() ? ob3->getTeam1Id() : finalTeam1Id;
            QString currentTeam2Id = finalTeam2Id.isEmpty() ? ob3->getTeam2Id() : finalTeam2Id;
            ob3->setTeamInfo(currentTeam1Id, currentTeam2Id);
        }
        
        // 如果有UI变更，可能需要更新显示
        if (anyUiChange) {
            // 更新团队比分显示
            updateTeam3v3ScoreDisplayLabels();
        }
        
        // 更新队伍比分 - 确保比分正确同步
        updateTeam3v3Scores();
    }
}

// 更新3V3团队比分显示标签
void MainWindow::updateTeam3v3ScoreDisplayLabels() {
    if (!ob3) return;
    
    // 从ob3获取队伍ID
    QString team1Id = ob3->getTeam1Id();
    QString team2Id = ob3->getTeam2Id();
    
    // 设置队伍标签文本
    if (!team1Id.isEmpty()) {
        ui->lb_team1_3v3->setText(team1Id);
    } else {
        ui->lb_team1_3v3->setText(tr("队伍1"));
    }
    
    if (!team2Id.isEmpty()) {
        ui->lb_team2_3v3->setText(team2Id);
    } else {
        ui->lb_team2_3v3->setText(tr("队伍2"));
    }
    
    // 更新分数显示
    updateTeam3v3Scores();
}

// 3v3模式更新玩家按钮点击处理
void MainWindow::onBtnUpdatePlayer3v3Clicked() {
    if (!ob3) return;
    
    // 更新团队1玩家信息
    for (int i = 1; i <= 3; i++) {
        // 构建动态控件名称
        QString leNicknameObjName = QString("le_team1_p%1_nickname_3v3").arg(i);
        QString lePlayernameObjName = QString("le_team1_p%1_playername_3v3").arg(i);
        
        // 获取QLineEdit控件指针
        QLineEdit *leNickname = ui->centralwidget->findChild<QLineEdit*>(leNicknameObjName);
        QLineEdit *lePlayername = ui->centralwidget->findChild<QLineEdit*>(lePlayernameObjName);
        
        if (leNickname && lePlayername) {
            QString nickname = leNickname->text().trimmed();
            QString playername = lePlayername->text().trimmed();
            
            // 只处理有昵称且有实际名称的情况
            if (!nickname.isEmpty() && !playername.isEmpty()) {
                updateRemotePlayerName(nickname, playername);
            }
        }
    }
    
    // 更新团队2玩家信息
    for (int i = 1; i <= 3; i++) {
        // 构建动态控件名称
        QString leNicknameObjName = QString("le_team2_p%1_nickname_3v3").arg(i);
        QString lePlayernameObjName = QString("le_team2_p%1_playername_3v3").arg(i);
        
        // 获取QLineEdit控件指针
        QLineEdit *leNickname = ui->centralwidget->findChild<QLineEdit*>(leNicknameObjName);
        QLineEdit *lePlayername = ui->centralwidget->findChild<QLineEdit*>(lePlayernameObjName);
        
        if (leNickname && lePlayername) {
            QString nickname = leNickname->text().trimmed();
            QString playername = lePlayername->text().trimmed();
            
            // 只处理有昵称且有实际名称的情况
            if (!nickname.isEmpty() && !playername.isEmpty()) {
                updateRemotePlayerName(nickname, playername);
            }
        }
    }
    
    // 强制保存配置
    if (_cfgm) {
        _cfgm->save();
    }
    
    // 发出玩家映射更新信号
    emit playerMappingUpdated();
    qDebug() << "[MainWindow] onBtnUpdatePlayer3v3Clicked发出playerMappingUpdated信号";
    
    // 刷新3v3团队信息
    updateTeam3v3Playername();
}

// 更新3V3团队比分
void MainWindow::updateTeam3v3Scores() {
    if (!ob3) return;
    
    // 从ob3获取队伍ID
    QString team1Id = ob3->getTeam1Id();
    QString team2Id = ob3->getTeam2Id();
    
    // 获取当前全局比分
    int team1Score = 0;
    int team2Score = 0;
    
    if (!team1Id.isEmpty() && gls->teamScores.contains(team1Id)) {
        team1Score = gls->teamScores[team1Id];
    }
    
    if (!team2Id.isEmpty() && gls->teamScores.contains(team2Id)) {
        team2Score = gls->teamScores[team2Id];
    }
    
    // 更新界面控件
    if (ui->spinBox_team1_score_3v3) {
        ui->spinBox_team1_score_3v3->setValue(team1Score);
    }
    
    if (ui->spinBox_team2_score_3v3) {
        ui->spinBox_team2_score_3v3->setValue(team2Score);
    }
    
    // 同步ob3比分
    ob3->updateTeamScores();
}

// 处理3v3模式队伍1比分改变
void MainWindow::onTeam1Score3v3SpinBoxValueChanged(int value) {
    if (!ob3) return;
    
    // 获取队伍ID
    QString team1Id = ob3->getTeam1Id();
    
    // 只有在队伍ID有效时才更新比分
    if (!team1Id.isEmpty()) {
        // 更新全局比分记录
        gls->teamScores[team1Id] = value;
        
        // 保存设置
        _cfgm->setValue("team_scores/" + team1Id, value);
        _cfgm->save();
        
        // 更新ob3显示
        ob3->updateTeamScores();
    }
}



// 处理3v3模式队伍2比分改变
void MainWindow::onTeam2Score3v3SpinBoxValueChanged(int value) {
    if (!ob3) return;
    
    // 获取队伍ID
    QString team2Id = ob3->getTeam2Id();
    
    // 只有在队伍ID有效时才更新比分
    if (!team2Id.isEmpty()) {
        // 更新全局比分记录
        gls->teamScores[team2Id] = value;
        
        // 保存设置
        _cfgm->setValue("team_scores/" + team2Id, value);
        _cfgm->save();
        
        // 更新ob3显示
        ob3->updateTeamScores();
    }
}

// 处理日期显示开关的槽函数
void MainWindow::onHideDateCheckBoxToggled(bool checked) {
    if (ob) {
        ob->setHideDate(checked);
        // 如果勾选隐藏日期且有滚动文字内容，立即应用
        if (checked && ui->le_scroll_text && !ui->le_scroll_text->text().isEmpty()) {
            ob->setScrollText(ui->le_scroll_text->text());
        }
    }
    
    // 1V1技术模式下也需要设置滚动文字
    if (ob1 && ui->le_scroll_text && !ui->le_scroll_text->text().isEmpty()) {
        ob1->setScrollText(ui->le_scroll_text->text());
    }
    
    // 保存设置
    saveAllSettings();
}

// 处理滚动文字更新按钮的槽函数
void MainWindow::onBtnUpdateScrollClicked() {
    if (ob && ui->le_scroll_text) {
        QString scrollText = ui->le_scroll_text->text();
        ob->setScrollText(scrollText);
    }
    
    // 1V1技术模式下也需要设置滚动文字
    if (ob1 && ui->le_scroll_text) {
        QString scrollText = ui->le_scroll_text->text();
        ob1->setScrollText(scrollText);
    }
    
    // 保存设置
    saveAllSettings();
}

// 会员功能相关方法实现
void MainWindow::updateMembershipStatus() {
    QString statusText = getMembershipStatusText();
    
    // 更新UI中的会员状态标签
    if (ui->lb_membership_status) {
        ui->lb_membership_status->setText(statusText);
    }
}

QString MainWindow::getMembershipStatusText() {
    MemberManager& memberManager = MemberManager::getInstance();
    
    if (memberManager.isMember()) {
        return "当前用户: 会员用户";
    } else {
        return "当前用户: 免费用户";
    }
}
