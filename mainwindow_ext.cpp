#include "./mainwindow.h"
#include "./ui_mainwindow.h"
#include <QMessageBox>
#include <QFileDialog>
#include <QDebug>
#include <QCoreApplication>
#include <QFile>
#include <QLabel>
#include <QPixmap>

// 这是mainwindow.cpp的扩展文件
// 可以在这里继续实现MainWindow类的方法

// 主题切换功能已移除，仅保留默认主题

// 1V1科技主题相关槽函数实现
void MainWindow::onBtnUpdatePlayerTechClicked() {
    // 获取科技主题tab的玩家信息
    QString p1Nickname = ui->le_p1nickname_tech->text();
    QString p2Nickname = ui->le_p2nickname_tech->text();
    QString p1Playername = ui->le_p1playername_tech->text();
    QString p2Playername = ui->le_p2playername_tech->text();
    
    // 获取段位信息
    QString p1_rank = ui->cb_p1_rank_tech->currentText();
    QString p2_rank = ui->cb_p2_rank_tech->currentText();
    
    // 获取主播名字和赛事名
    QString streamerName = ui->le_streamer_name_tech->text();
    QString eventName = ui->le_event_name_tech->text();
    
    // 保存到全局设置
    gls->sb.streamer_name = streamerName;
    gls->sb.event_name = eventName;
    
    // 保存到config.json文件
    _cfgm->setValue("streamer_name", streamerName);
    _cfgm->setValue("event_name", eventName);
    _cfgm->save(); // 保存到config.json文件

    // 更新玩家信息到远程服务器（与1V1模式保持一致）
    if (!p1Nickname.isEmpty() && !p1Playername.isEmpty()) {
        // 更新远程玩家名称映射
        updateRemotePlayerName(p1Nickname, p1Playername);
        
        // 同时更新本地缓存
        gls->addPlayerMapping(p1Nickname, p1Playername);
        
        // 保存玩家名称映射到配置文件（关键：实现持久化）
        _cfgm->updatePlayer(p1Nickname, p1Playername);
        
        // 段位信息仍然保存在本地
        _cfgm->setValue("rank/nickname/" + p1Nickname, p1_rank);
    }
    if (!p2Nickname.isEmpty() && !p2Playername.isEmpty()) {
        // 更新远程玩家名称映射
        updateRemotePlayerName(p2Nickname, p2Playername);
        
        // 同时更新本地缓存
        gls->addPlayerMapping(p2Nickname, p2Playername);
        
        // 保存玩家名称映射到配置文件（关键：实现持久化）
        _cfgm->updatePlayer(p2Nickname, p2Playername);
        
        // 段位信息仍然保存在本地
        _cfgm->setValue("rank/nickname/" + p2Nickname, p2_rank);
    }

    // 保存本地设置（仅包括段位等其他本地信息）
    _cfgm->save();
    
    // 更新玩家头像显示（不上传到服务器）
    if (ob && (currentMode == 0 || currentMode == 1)) {
        ob->updatePlayerAvatars();
    }
    
    // 如果是1V1科技主题模式，主动调用ob1的forceRefreshPlayerNames方法和头像更新
    if (ob1 && currentMode == 1) {
        ob1->forceRefreshPlayerNames();
        ob1->updatePlayerAvatars();
        qDebug() << "[MainWindow] 主动调用ob1->forceRefreshPlayerNames()和updatePlayerAvatars()更新玩家名称和头像显示";
    }
    
    // 发出玩家映射更新信号
    emit playerMappingUpdated();
}

void MainWindow::onBtnUploadAvatarP1TechClicked() {
    QString nickname = ui->le_p1nickname_tech->text();
    QString gameName = ui->le_p1playername_tech->text();
    
    if (nickname.isEmpty()) {
        QMessageBox::warning(this, tr("警告"), tr("请先输入玩家昵称"));
        return;
    }
    
    QString fileName = QFileDialog::getOpenFileName(this, tr("选择头像"), "", tr("图片文件 (*.png *.jpg *.jpeg *.bmp)"));
    if (!fileName.isEmpty()) {
        saveAvatarLocally(fileName, nickname, gameName);
        loadAvatarTech(1);
        
        // 自动上传到远程服务器
        QString localAvatarPath = QCoreApplication::applicationDirPath() + "/avatars/" + (!gameName.isEmpty() ? gameName : nickname) + ".png";
        uploadAvatarToServer(1, nickname, gameName, localAvatarPath);
        
        // 通知OB界面更新头像
        if (ob && (currentMode == 0 || currentMode == 1)) {
            ob->updatePlayerAvatars();
        }
        
        // 如果是1V1科技主题模式，也需要通知ob1更新头像
        if (ob1 && currentMode == 1) {
            ob1->updatePlayerAvatars();
        }
    }
}

// 1V1科技主题模式下的比分更新方法
void MainWindow::updateTechScore() {
    // 获取当前科技主题tab的玩家昵称
    QString p1_nickname = ui->le_p1nickname_tech->text();
    QString p2_nickname = ui->le_p2nickname_tech->text();
    
    // 从globalsetting获取比分并更新UI
    if (!p1_nickname.isEmpty() && gls->playerScores.contains(p1_nickname)) {
        int currentP1Score = gls->playerScores[p1_nickname];
        if (ui->spinBox_p1_score_tech->value() != currentP1Score) {
            ui->spinBox_p1_score_tech->setValue(currentP1Score);
            qDebug() << "[Tech] 从GlobalSetting更新玩家1(" << p1_nickname << ")分数为:" << currentP1Score;
        }
    }
    
    if (!p2_nickname.isEmpty() && gls->playerScores.contains(p2_nickname)) {
        int currentP2Score = gls->playerScores[p2_nickname];
        if (ui->spinBox_p2_score_tech->value() != currentP2Score) {
            ui->spinBox_p2_score_tech->setValue(currentP2Score);
            qDebug() << "[Tech] 从GlobalSetting更新玩家2(" << p2_nickname << ")分数为:" << currentP2Score;
        }
    }
    
    // 更新比分标签显示
    QString p1_playername = ui->le_p1playername_tech->text();
    QString p2_playername = ui->le_p2playername_tech->text();
    
    if (!p1_nickname.isEmpty()) {
        ui->lb_p1_score_tech->setText(p1_playername.isEmpty() ? p1_nickname : p1_playername);
    } else {
        ui->lb_p1_score_tech->setText(tr("玩家1"));
    }
    
    if (!p2_nickname.isEmpty()) {
        ui->lb_p2_score_tech->setText(p2_playername.isEmpty() ? p2_nickname : p2_playername);
    } else {
        ui->lb_p2_score_tech->setText(tr("玩家2"));
    }
}

void MainWindow::onBtnUploadAvatarP2TechClicked() {
    QString nickname = ui->le_p2nickname_tech->text();
    QString gameName = ui->le_p2playername_tech->text();
    
    if (nickname.isEmpty()) {
        QMessageBox::warning(this, tr("警告"), tr("请先输入玩家昵称"));
        return;
    }
    
    QString fileName = QFileDialog::getOpenFileName(this, tr("选择头像"), "", tr("图片文件 (*.png *.jpg *.jpeg *.bmp)"));
    if (!fileName.isEmpty()) {
        saveAvatarLocally(fileName, nickname, gameName);
        loadAvatarTech(2);
        
        // 自动上传到远程服务器
        QString localAvatarPath = QCoreApplication::applicationDirPath() + "/avatars/" + (!gameName.isEmpty() ? gameName : nickname) + ".png";
        uploadAvatarToServer(2, nickname, gameName, localAvatarPath);
        
        // 通知OB界面更新头像
        if (ob && (currentMode == 0 || currentMode == 1)) {
            ob->updatePlayerAvatars();
        }
        
        // 如果是1V1科技主题模式，也需要通知ob1更新头像
        if (ob1 && currentMode == 1) {
            ob1->updatePlayerAvatars();
        }
    }
}

void MainWindow::onBtnUpdateScoreTechClicked() {
    // 获取科技主题tab的比分
    int p1Score = ui->spinBox_p1_score_tech->value();
    int p2Score = ui->spinBox_p2_score_tech->value();
    
    // 更新全局比分
    this->p1Score = p1Score;
    this->p2Score = p2Score;
    
    // 获取当前BO数
    boNumber = ui->spinBox_bo_number_tech->value();
    _cfgm->setValue("bo_number", boNumber);
    
    // 更新OB界面上的BO数显示
    if (ob && (currentMode == 0 || currentMode == 1)) {
        ob->setBONumber(boNumber);
    }
    
    // 获取当前玩家昵称
    QString p1_nickname = ui->le_p1nickname_tech->text();
    QString p2_nickname = ui->le_p2nickname_tech->text();
    
    // 获取玩家显示名称
    QString p1_playername = ui->le_p1playername_tech->text();
    QString p2_playername = ui->le_p2playername_tech->text();
    
    // 更新比分标签显示当前玩家昵称
    if (!p1_nickname.isEmpty()) {
        ui->lb_p1_score_tech->setText(p1_playername.isEmpty() ? p1_nickname : p1_playername);
    } else {
        ui->lb_p1_score_tech->setText(tr("玩家1"));
    }
    
    if (!p2_nickname.isEmpty()) {
        ui->lb_p2_score_tech->setText(p2_playername.isEmpty() ? p2_nickname : p2_playername);
    } else {
        ui->lb_p2_score_tech->setText(tr("玩家2"));
    }
    
    // 将比分与玩家昵称关联存储
    if (!p1_nickname.isEmpty()) {
        gls->playerScores[p1_nickname] = p1Score;
    }
    if (!p2_nickname.isEmpty()) {
        gls->playerScores[p2_nickname] = p2Score;
    }
    
    // 保存到配置
    _cfgm->setValue("score/p1", p1Score);
    _cfgm->setValue("score/p2", p2Score);
    _cfgm->save();
    
    // 使用ob更新比分显示
    if (ob && (currentMode == 0 || currentMode == 1)) {
        ob->updateScores();
    }
    
    qDebug() << "科技主题模式比分已更新: P1=" << p1Score << ", P2=" << p2Score;
}

void MainWindow::onP1ScoreTechSpinBoxValueChanged(int value) {
    // 获取当前玩家1昵称
    QString p1_nickname = ui->le_p1nickname_tech->text();
    
    // 实时更新玩家1比分
    this->p1Score = value;
    _cfgm->setValue("score/p1", value);
    
    // 将比分与玩家昵称关联存储
    if (!p1_nickname.isEmpty()) {
        gls->playerScores[p1_nickname] = value;
        qDebug() << "通过spinbox更新玩家1(" << p1_nickname << ")分数为:" << value;
    }
    
    // 保存分数设置
    saveAllSettings();
    
    // 实时更新比分显示
    if (ob && (currentMode == 0 || currentMode == 1)) {
        ob->updateScores();
    }
}

void MainWindow::onP2ScoreTechSpinBoxValueChanged(int value) {
    // 获取当前玩家2昵称
    QString p2_nickname = ui->le_p2nickname_tech->text();
    
    // 实时更新玩家2比分
    this->p2Score = value;
    _cfgm->setValue("score/p2", value);
    
    // 将比分与玩家昵称关联存储
    if (!p2_nickname.isEmpty()) {
        gls->playerScores[p2_nickname] = value;
        qDebug() << "通过spinbox更新玩家2(" << p2_nickname << ")分数为:" << value;
    }
    
    // 保存分数设置
    saveAllSettings();
    
    // 实时更新比分显示
    if (ob && (currentMode == 0 || currentMode == 1)) {
        ob->updateScores();
    }
}

// 1V1科技主题模式下的BO数更新方法
void MainWindow::onBONumberTechSpinBoxValueChanged(int value) {
    boNumber = value;
    _cfgm->setValue("bo_number", boNumber);
    
    // 同步更新其他模式的BO数spinbox值
    ui->spinBox_bo_number->setValue(boNumber);
    ui->spinBox_bo_number_2v2->setValue(boNumber);
    ui->spinBox_bo_number_3v3->setValue(boNumber);
    
    // 更新BO数 - 1V1科技模式应该使用ob1对象
    if (ob1 && currentMode == 1) {
        ob1->setBONumber(boNumber);
    }
    
    // 科技主题模式BO数已更新
}

// 1V1科技主题模式下的玩家昵称更新方法
void MainWindow::updatePlayernameTech() {
 
    
    // 获取当前游戏中的玩家昵称（从全局设置）
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
    if (!p1_nickname.isEmpty() && p1_nickname != ui->le_p1nickname_tech->text()) {
      
        ui->le_p1nickname_tech->setText(p1_nickname);
        p1NicknameChanged = true;
    } else {
      
    }
    
    if (!p2_nickname.isEmpty() && p2_nickname != ui->le_p2nickname_tech->text()) {
      
        ui->le_p2nickname_tech->setText(p2_nickname);
        p2NicknameChanged = true;
    } else {
       
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
        
        ui->le_p1playername_tech->setText(p1_playername);
        
        // 更新比分标签显示
        if (!p1_nickname.isEmpty()) {
            ui->lb_p1_score_tech->setText(p1_playername.isEmpty() ? p1_nickname : p1_playername);
        } else {
            ui->lb_p1_score_tech->setText(tr("玩家1"));
        }
        
        // 更新段位显示
        updatePlayerRankTech(1, p1_nickname);
        
        // 玩家名称变化时，更新头像
        loadAvatarTech(1);
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
        
        ui->le_p2playername_tech->setText(p2_playername);
        
        // 更新比分标签显示
        if (!p2_nickname.isEmpty()) {
            ui->lb_p2_score_tech->setText(p2_playername.isEmpty() ? p2_nickname : p2_playername);
        } else {
            ui->lb_p2_score_tech->setText(tr("玩家2"));
        }
        
        // 更新段位显示
        updatePlayerRankTech(2, p2_nickname);
        
        // 玩家名称变化时，更新头像
        loadAvatarTech(2);
    }
    
    // 如果有昵称变化且是1V1科技主题模式，主动调用ob1的forceRefreshPlayerNames方法和头像更新
    if ((p1NicknameChanged || p2NicknameChanged) && ob1 && currentMode == 1) {
        ob1->forceRefreshPlayerNames();
        ob1->updatePlayerAvatars();
        // 发出玩家映射更新信号
        emit playerMappingUpdated();
        qDebug() << "[MainWindow] updatePlayernameTech中主动调用ob1->forceRefreshPlayerNames()和updatePlayerAvatars()更新玩家名称和头像显示";
    }
}

// 1V1科技主题模式下的段位更新方法
void MainWindow::updatePlayerRankTech(int playerIndex, const QString &nickname) {
    if (nickname.isEmpty()) return;
    
    QString rankKey = "rank/nickname/" + nickname;
    QString savedRank = _cfgm->value(rankKey, "").toString();
    
    if (!savedRank.isEmpty()) {
        if (playerIndex == 1) {
            // 在下拉框中查找对应的段位
            int index = ui->cb_p1_rank_tech->findText(savedRank);
            if (index != -1) {
                ui->cb_p1_rank_tech->setCurrentIndex(index);
            }
        } else if (playerIndex == 2) {
            int index = ui->cb_p2_rank_tech->findText(savedRank);
            if (index != -1) {
                ui->cb_p2_rank_tech->setCurrentIndex(index);
            }
        }
    }
}

// 1V1科技主题模式下的头像加载方法
void MainWindow::loadAvatarTech(int playerIndex) {
    QString nickname;
    QString playername;
    QLabel* avatarLabel = nullptr;
    
    if (playerIndex == 1) {
        nickname = ui->le_p1nickname_tech->text();
        playername = ui->le_p1playername_tech->text();
        avatarLabel = ui->lb_avatar_p1_tech;
    } else if (playerIndex == 2) {
        nickname = ui->le_p2nickname_tech->text();
        playername = ui->le_p2playername_tech->text();
        avatarLabel = ui->lb_avatar_p2_tech;
    }
    
    if (nickname.isEmpty() || !avatarLabel) return;
    
    // 构建头像文件路径（优先使用游戏名称，其次是昵称）
    QString avatarPath;
    QString basePath = QCoreApplication::applicationDirPath() + "/avatars/";
    
    // 优先尝试使用游戏名称
    if (!playername.isEmpty()) {
        avatarPath = basePath + playername + ".png";
        if (!QFile::exists(avatarPath)) {
            avatarPath = basePath + playername + ".jpg";
        }
    }
    
    // 如果游戏名称对应的头像不存在，尝试使用昵称
    if (avatarPath.isEmpty() || !QFile::exists(avatarPath)) {
        avatarPath = basePath + nickname + ".png";
        if (!QFile::exists(avatarPath)) {
            avatarPath = basePath + nickname + ".jpg";
        }
    }
    
    // 加载头像
    if (QFile::exists(avatarPath)) {
        QPixmap avatar(avatarPath);
        if (!avatar.isNull()) {
            avatarLabel->setPixmap(avatar.scaled(40, 40, Qt::KeepAspectRatio, Qt::SmoothTransformation));
            return;
        }
    }
    
    // 如果本地头像不存在，设置默认头像
    avatarLabel->clear();
    avatarLabel->setText("无头像");
}

// 加载主播名字和赛事名
void MainWindow::loadStreamerAndEventInfo() {
    // 从config.json加载主播名字和赛事名
    QString streamerName = _cfgm->value("streamer_name", "").toString();
    QString eventName = _cfgm->value("event_name", "").toString();
    
    // 设置到界面控件
    ui->le_streamer_name_tech->setText(streamerName);
    ui->le_event_name_tech->setText(eventName);
    
    // 同时更新到全局设置
    gls->sb.streamer_name = streamerName;
    gls->sb.event_name = eventName;
    
    qDebug() << "加载主播和赛事信息 - 主播:" << streamerName << ", 赛事:" << eventName;
}

// 1V1科技模式滚动文字相关槽函数
void MainWindow::onHideDateCheckBoxToggledTech(bool checked) {
    // 保存隐藏日期设置到config.json
    _cfgm->setValue("hide_date_tech", checked);
    _cfgm->save();
    
    // 获取滚动文字内容
    QString scrollText = ui->le_scroll_text_tech->text();
    
    if (checked && !scrollText.isEmpty()) {
        // 如果勾选了隐藏日期且有滚动文字内容，则设置滚动文字
        if (ob) {
            ob->setScrollText(scrollText);
        }
        if (ob1) {
            ob1->setScrollText(scrollText);
        }
    } else {
        // 如果取消勾选或没有滚动文字内容，则清空滚动文字
        if (ob) {
            ob->setScrollText("");
        }
        if (ob1) {
            ob1->setScrollText("");
        }
    }
    
    qDebug() << "[Tech] 隐藏日期复选框状态:" << checked << ", 滚动文字:" << scrollText;
}

void MainWindow::onBtnUpdateScrollTechClicked() {
    // 获取滚动文字内容
    QString scrollText = ui->le_scroll_text_tech->text();
    
    // 保存滚动文字内容到config.json
    _cfgm->setValue("scroll_text_tech", scrollText);
    _cfgm->save();
    
    // 如果勾选了隐藏日期，则更新滚动文字
    if (ui->cb_hide_date_tech->isChecked()) {
        if (ob) {
            ob->setScrollText(scrollText);
        }
        if (ob1) {
            ob1->setScrollText(scrollText);
        }
    }
    
    qDebug() << "[Tech] 更新滚动文字:" << scrollText;
}

// 加载1V1科技模式滚动文字设置
void MainWindow::loadScrollTextSettingsTech() {
    // 从config.json加载设置
    bool hideDate = _cfgm->value("hide_date_tech", false).toBool();
    QString scrollText = _cfgm->value("scroll_text_tech", "").toString();
    
    // 设置到界面控件
    ui->cb_hide_date_tech->setChecked(hideDate);
    ui->le_scroll_text_tech->setText(scrollText);
    
    // 应用设置到ob1对象（如果存在且勾选了隐藏日期）
    if (hideDate && !scrollText.isEmpty()) {
        if (ob) {
            ob->setScrollText(scrollText);
        }
        if (ob1) {
            ob1->setScrollText(scrollText);
        }
    } else {
        // 如果取消勾选或没有滚动文字内容，则清空滚动文字
        if (ob) {
            ob->setScrollText("");
        }
        if (ob1) {
            ob1->setScrollText("");
        }
    }
    
    qDebug() << "[Tech] 加载滚动文字设置 - 隐藏日期:" << hideDate << ", 滚动文字:" << scrollText;
}