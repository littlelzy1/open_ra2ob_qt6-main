#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QColorDialog>
#include <QMainWindow>
#include <QFileDialog>
#include <QPixmap>
#include <QMap>
#include <QPoint>
#include <QSize>
#include <QTimer>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QTime>

#include "./configmanager.h"
#include "./globalsetting.h"
#include "./ob.h"
#include "./ob1.h"
#include "obteam.h"
#include "ob3.h"
#include "./authmanager.h"
#include "./membermanager.h"

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr, ConfigManager *cfgm = nullptr);
    ~MainWindow();

protected:
    void hideEvent(QHideEvent *event) override;
    void closeEvent(QCloseEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

signals:
    // 玩家映射更新信号
    void playerMappingUpdated();

public slots:
    // 处理PlayerStatusTracker发送的分数变化信号
    void onPlayerScoreChanged(const QString &nickname, int newScore);

private slots:
    void onBtnUpdatePlayerClicked();
    void onBtnUploadAvatarP1Clicked();
    void onBtnUploadAvatarP2Clicked();
    void onBtnUpdateScoreClicked();
    void onP1ScoreSpinBoxValueChanged(int value);
    void onP2ScoreSpinBoxValueChanged(int value);
    void onBONumberSpinBoxValueChanged(int value);
    
    // 1V1科技主题相关槽函数
    void onBtnUpdatePlayerTechClicked();
    void onBtnUploadAvatarP1TechClicked();
    void onBtnUploadAvatarP2TechClicked();
    void onBtnUpdateScoreTechClicked();
    void onP1ScoreTechSpinBoxValueChanged(int value);
    void onP2ScoreTechSpinBoxValueChanged(int value);
    void onBONumberTechSpinBoxValueChanged(int value);
    void updatePlayername();
    void updatePlayernameTech(); // 1V1科技主题模式下的玩家昵称更新
    void updateTechScore(); // 1V1科技主题模式下的比分更新
    
    // 1V1科技模式滚动文字相关槽函数
    void onHideDateCheckBoxToggledTech(bool checked);
    void onBtnUpdateScrollTechClicked();
    void loadScrollTextSettingsTech();
    void onScoreChanged(const QString &nickname, int newScore);
    
    // 添加PMB图片管理相关槽
    void onBtnChangePmbClicked();
    void onBtnResetPmbClicked();
    void onChkShowPmbToggled(bool checked);
    void onBtnApplyPmbPositionClicked();
    void onBtnApplyPmbSizeClicked();

    // 添加标签切换处理槽
    void onTabIndexChanged(int index);

    // 更新2v2模式玩家名称
    void updateTeamPlayername();
    
    // 2v2模式更新玩家按钮点击
    void onBtnUpdatePlayer2v2Clicked();

    // 更新2V2团队比分
    void updateTeamScores();

    // 2V2模式队伍比分处理
    void onTeam1ScoreSpinBoxValueChanged(int value);
    void onTeam2ScoreSpinBoxValueChanged(int value);
    void updateTeamScoreDisplayLabels();
    
    // 更新3v3模式玩家名称
    void updateTeam3v3Playername();
    
    // 3v3模式更新玩家按钮点击
    void onBtnUpdatePlayer3v3Clicked();

    // 更新3V3团队比分
    void updateTeam3v3Scores();

    // 3V3模式队伍比分处理
    void onTeam1Score3v3SpinBoxValueChanged(int value);
    void onTeam2Score3v3SpinBoxValueChanged(int value);
    void updateTeam3v3ScoreDisplayLabels();
    
    // 日期显示开关和滚动文字相关槽函数
    void onHideDateCheckBoxToggled(bool checked);
    void onBtnUpdateScrollClicked();

private:
    Ui::MainWindow *ui;
    Ob *ob;
    Ob1 *ob1;
    ObTeam *obTeam;
    Ob3 *ob3;
    ConfigManager *_cfgm;
    Globalsetting *gls;
    
    QString p1AvatarPath;
    QString p2AvatarPath;
    
    int p1Score = 0;
    int p2Score = 0;
    int boNumber = 5;
    
    // 主题相关变量
    int currentTheme = 0; // 0: 默认主题, 1: 替代主题

    void drawPreview(QWidget *widget);
    void detectShortcutStatus();
    void uploadAvatar(int playerIndex, const QString &nickname, const QString &gameName);
    void loadAvatar(int playerIndex);
    void saveAllSettings();
    
    void loadPlayerRanks();
    void updatePlayerRank(int playerIndex, const QString &nickname);
    
    // 1V1科技主题模式相关方法
    void updatePlayerRankTech(int playerIndex, const QString &nickname);
    void loadAvatarTech(int playerIndex);
    void loadStreamerAndEventInfo(); // 加载主播名字和赛事名

    QMap<QString, int> _previousScores;
    
    void setupPmbControls(); // 初始化PMB图片控制界面

    QNetworkAccessManager *networkManager;
    QString serverUrl;
    
    void uploadAvatarToServer(int playerIndex, const QString &nickname, const QString &gameName, const QString &localFilePath);
    void saveAvatarLocally(const QString &sourceFilePath, const QString &nickname, const QString &gameName);
    void handleUploadReply(QNetworkReply *reply, int playerIndex, const QString &nickname);
    
    // 远程玩家名称管理函数
    void updateRemotePlayerName(const QString &nickname, const QString &playername);
    void fetchRemotePlayerNames();
    bool isPlayerExistLocally(const QString &nickname);
    void fetchSinglePlayerName(const QString &nickname);
    
    // 游戏进程状态监控
    void monitorGameStatus();
    bool lastGameStatus;  // 保存上次检查时的游戏状态
    QTime lastConfigUpdateTime;  // 记录上次配置更新的时间
    
    QTimer *gameStatusTimer;  // 用于定期检查游戏状态的定时器
    
    // 更新比分显示
    void updateScore();

    // 标记当前模式
    int currentMode = 0; // 0: 1v1, 1: 2v2, 2: 3v3

    // 新增函数：直接从配置文件读取玩家名称
    QString getPlayerNameFromConfig(const QString &nickname);
    
    // 会员功能相关方法
    void updateMembershipStatus();
    QString getMembershipStatusText();
    

};

#endif  // MAINWINDOW_H
