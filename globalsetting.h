#ifndef GLOBALSETTING_H
#define GLOBALSETTING_H

#include <QColor>
#include <QJsonArray>
#include <QJsonObject>
#include <QMap>

struct Layout {
    float ratio;
    int w;
    int h;
    int m;
    int right_x;
    int right_w;
    int right_header_h;
    int right_leftborder_w;
    int right_rightborder_w;
    int right_rightborder_x;
    int right_border_h;
    int right_mapname_h;
    int map_x;
    int map_y;
    int map_w;
    int map_h;
    int right_bottom_h;
    int top_m;
    int top_w;
    int top_h;
    int top_playername_y;
    int top_country_x;
    int top_country_y;
    int top_country_w;
    int top_country_h;
    int top_i_x;
    int top_i_wh;
    int top_ibalance_y;
    int top_ipower_y;
    int top_balance_x;
    int top_balance_y;
    int top_balance_w;
    int top_balance_h;
    int top_power_x;
    int top_power_y;
    int top_power_w;
    int top_power_h;
    int if_bar_h;
    int unit_x;
    int unit_y;
    int unit_w;
    int unit_ws;
    int unit_h;
    int unit_hs;
    int unit_bg_y;
    int unit_bg_h;
    int unitblocks;
    int unitblock_y;
    int icon_side;
    int bottom_h;
    int bottom_y;
    int bottom_y1;
    int bottom_y2;
    int bottom_hs;
    int bottom_fill_y1;
    int bottom_fill_y2;
    int bottom_fill_h;
    int bottom_credit_x;
    int bottom_credit_y1;
    int bottom_credit_y2;
    int bottom_credit_w;
    int bottom_credit_h;
    int producingblock_x;
    int producingblock_w;
    int producingblock_h;
    int producingblock_ws;
    int producingblock_hs;
    int producingblock_y1;
    int producingblock_y2;
    int producingblock_space;
    int producing_img_x;
    int producing_img_y;
    int producing_img_w;
    int producing_img_h;
    int producing_status_y;
    int producing_number_y;
    int producing_progress_x;
    int producing_progress_y;
    int producing_progress_w;
    int producing_progress_h;
};

struct Colors {
    QColor producing_stripe_color = QColor(30, 30, 30);
    QColor preview_color          = QColor("red");
};

struct Status {
    bool show_all_panel    = true;
    bool show_top_panel    = true;
    bool show_bottom_panel = true;
    bool show_producing    = true;
    bool show_gametime      = true;
    bool force_16_9          = false;
    bool show_pmb_image      = true;
    bool show_spy_infiltration = true;  // 间谍渗透显示开关

    bool sc_ctrl_alt_h        = true;
    bool sc_ctrl_alt_j        = true;
    bool sc_ctrl_s            = true;
};

struct ScoreBoard {
    QString p1_nickname, p1_playername;
    QString p2_nickname, p2_playername;
    
    // 缓存变量，用于在游戏结束后保持玩家信息
    QString p1_nickname_cache, p1_playername_cache;
    QString p2_nickname_cache, p2_playername_cache;
    QString p1_avatar_path_cache, p2_avatar_path_cache;
    
    // 主播和赛事信息
    QString streamer_name;  // 主播名字
    QString event_name;     // 赛事名
};

struct GameLog {
    int game_index;
    QString p1_nickname, p2_nickname;
    int game_duration;
    int game_start_time;
};

class Globalsetting {
public:
    static Globalsetting &getInstance();

    Globalsetting(const Globalsetting &)            = delete;
    Globalsetting &operator=(const Globalsetting &) = delete;

    void loadLayoutSetting(int width = 0, int height = 0, float ratio = 1);
    QString findGlobalColorRGB(QString name);
    QString findGlobalColorHex(QString name);
    QString findNameByNickname(const QString &nickname);

    Layout l;
    Colors c;
    Status s;
    ScoreBoard sb;
    QJsonArray player_list;
    int game_numbers = 0;
    bool game_start  = false;
    bool game_end    = false;
    QList<GameLog> game_log;
    QString buildingQueuePosition = "Left"; // 建造栏位置，始终为"Left"
    
    // 存储玩家比分的映射
    QMap<QString, int> playerScores;
    
    // 存储队伍比分的映射 (键为"玩家1+玩家2"格式的字符串)
    QMap<QString, int> teamScores;
    
    // 当前屏幕分辨率
    int currentScreenWidth = 0;
    int currentScreenHeight = 0;

    // 玩家名称映射管理函数
    void clearPlayerMappings() {
        playerMapping.clear();
    }
    
    void addPlayerMapping(const QString &nickname, const QString &playername) {
        playerMapping[nickname] = playername;
    }

protected:
    Globalsetting()  = default;
    ~Globalsetting() = default;

    QMap<QString, QString> playerMapping; // 昵称到游戏名的映射
};

#endif  // GLOBALSETTING_H
