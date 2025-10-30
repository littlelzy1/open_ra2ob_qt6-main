#include "playerinfocard.h"
#include <QPainter>
#include <QPainterPath>
#include <QFontDatabase>
#include <QDir>
#include <QCoreApplication>
#include <QApplication>
#include "globalsetting.h"
#include "layoutsetting.h"
#include <QDebug>

PlayerInfoCard* PlayerInfoCard::sampleCard = nullptr;

PlayerInfoCard::PlayerInfoCard(QWidget *parent)
    : QWidget(parent), playerKills(0), playerUnits(0), playerBalance(0), isDefeated(false),
      hasBackgroundImage(false), debugLogFile(nullptr)
{
    if (!sampleCard) {
        sampleCard = this;
    }
    
    Globalsetting &gls = Globalsetting::getInstance();
    
    int cardWidth = 159;
    int cardHeight = 99;
    if (gls.l.ratio > 0) {
        cardWidth = qRound(cardWidth * gls.l.ratio);
        cardHeight = qRound(cardHeight * gls.l.ratio);
    }
    setFixedSize(cardWidth, cardHeight);
    
    nameLabel = new QOutlineLabel(this);
    killsLabel = new QOutlineLabel(this);
    unitsLabel = new QOutlineLabel(this);
    econLabel = new QOutlineLabel(this);
    flagLabel = new QLabel(this);
    
    nameLabel->setOutline(Qt::white, QColor(30, 27, 24), 1, true);
    killsLabel->setOutline(Qt::white, QColor(30, 27, 24), 1, true);
    unitsLabel->setOutline(Qt::white, QColor(30, 27, 24), 1, true);
    econLabel->setOutline(Qt::white, QColor(30, 27, 24), 1, true);
    
    nameLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    killsLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    unitsLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    econLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    
    int nameX = qRound(25 * gls.l.ratio);
    int nameY = qRound(10 * gls.l.ratio);
    int nameW = qRound(140 * gls.l.ratio);
    int nameH = qRound(20 * gls.l.ratio);
    
    int killsX = qRound(45 * gls.l.ratio);
    int unitsX = qRound(102 * gls.l.ratio);
    int statsY = qRound(45 * gls.l.ratio);
    int statsW = qRound(30 * gls.l.ratio);
    int statsH = qRound(16 * gls.l.ratio);
    
    int econX = qRound(45 * gls.l.ratio);
    int econY = qRound(70 * gls.l.ratio);
    int econW = qRound(60 * gls.l.ratio);
    int econH = qRound(16 * gls.l.ratio);
    
    int flagX = qRound(120 * gls.l.ratio);
    int flagY = qRound(70 * gls.l.ratio);
    int flagW = qRound(25 * gls.l.ratio);
    int flagH = qRound(15 * gls.l.ratio);
    
    nameLabel->setGeometry(nameX, nameY, nameW, nameH);
    killsLabel->setGeometry(killsX, statsY, statsW, statsH);
    unitsLabel->setGeometry(unitsX, statsY, statsW, statsH);
    econLabel->setGeometry(econX, econY, econW, econH);
    flagLabel->setGeometry(flagX, flagY, flagW, flagH);
    
    setupFonts();
    
    nameLabel->hide();
    killsLabel->hide();
    unitsLabel->hide();
    econLabel->hide();
    flagLabel->hide();
    
    QString appDir = QCoreApplication::applicationDirPath();
    QPixmap testImage(appDir + "/assets/panels/player_bg_red.png");
    
    QStringList resourceList;
    resourceList << appDir + "/assets/panels/player_bg_red.png"
                << appDir + "/assets/panels/player_bg_blue.png"
                << appDir + "/assets/panels/player_bg_green.png"
                << appDir + "/assets/panels/player_bg_yellow.png"
                << appDir + "/assets/panels/player_bg_orange.png"
                << appDir + "/assets/panels/player_bg_pink.png"
                << appDir + "/assets/panels/player_bg_purple.png"
                << appDir + "/assets/panels/player_bg_skyblue.png";
}

PlayerInfoCard::~PlayerInfoCard()
{
}

void PlayerInfoCard::setupFonts()
{
    Globalsetting &gls = Globalsetting::getInstance();
    float ratio = gls.l.ratio;
    
    int baseFontSizeName = qRound(11 * ratio);
    int baseFontSizeData = qRound(13 * ratio);
    
    QFont nameFont("Arial", baseFontSizeName, QFont::Bold);
    QFont dataFont("Arial", baseFontSizeData, QFont::Bold);
    
    QString appDir = QCoreApplication::applicationDirPath();
    QString miSansPath = appDir + "/assets/fonts/MiSans-Bold.ttf";
    QString technoPath = appDir + "/assets/fonts/LLDEtechnoGlitch-Bold0.ttf";
    
    int fontIdMiSans = QFontDatabase::addApplicationFont(miSansPath);
    int fontIdTechno = QFontDatabase::addApplicationFont(technoPath);
    
    if (fontIdMiSans >= 0) {
        QStringList families = QFontDatabase::applicationFontFamilies(fontIdMiSans);
        if (!families.isEmpty()) {
            nameFont = QFont(families.first(), baseFontSizeName, QFont::Bold);
        }
    }
    
    if (fontIdTechno >= 0) {
        QStringList families = QFontDatabase::applicationFontFamilies(fontIdTechno);
        if (!families.isEmpty()) {
            dataFont = QFont(families.first(), baseFontSizeData, QFont::Bold);
        }
    }
    
    nameFont.setStyleStrategy(QFont::PreferAntialias);
    dataFont.setStyleStrategy(QFont::PreferAntialias);
    
    nameLabel->setFont(nameFont);
    killsLabel->setFont(dataFont);
    unitsLabel->setFont(dataFont);
    econLabel->setFont(dataFont);
}

void PlayerInfoCard::setPlayerInfo(const QString& name, const QString& country, int kills, int units, int balance, const std::string& color)
{
    QString realPlayerName = name;
    Globalsetting &gls = Globalsetting::getInstance();
    QString mappedName = gls.findNameByNickname(name);
    
    if (!mappedName.isEmpty()) {
        realPlayerName = mappedName;
    }
    
    playerName = realPlayerName;
    
    if (!country.isEmpty()) {
        playerCountry = country.at(0).toUpper() + country.mid(1).toLower();
    } else {
        playerCountry = "Americans";
    }
    
    playerKills = kills;
    playerUnits = units;
    playerBalance = balance;
    playerColor = color;
    
    backgroundImage = getBackgroundForColor(color);
    hasBackgroundImage = !backgroundImage.isNull();
    
    if (hasBackgroundImage) {
        if (backgroundImage.width() != width() || backgroundImage.height() != height()) {
            backgroundImage = backgroundImage.scaled(width(), height(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
        }
    }
    
    updateDisplay();
}

void PlayerInfoCard::setDefeated(bool defeated) {
    if (isDefeated != defeated) {
        isDefeated = defeated;
        
        QColor textColor = isDefeated ? QColor(128, 128, 128) : Qt::white;
        
        nameLabel->setFillColor(textColor);
        killsLabel->setFillColor(textColor);
        unitsLabel->setFillColor(textColor);
        econLabel->setFillColor(textColor);
        
        backgroundImage = getBackgroundForColor(playerColor);
        hasBackgroundImage = !backgroundImage.isNull();
        
        if (hasBackgroundImage) {
            if (backgroundImage.width() != width() || backgroundImage.height() != height()) {
                backgroundImage = backgroundImage.scaled(width(), height(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
            }
        }
        
        updatePlayerIDVisibility();
        
        update();
    }
}

void PlayerInfoCard::updateDisplay()
{
    QPixmap flagPixmap = getCountryFlag(playerCountry);
    if (!flagPixmap.isNull()) {
        QSize flagSize = flagLabel->size();
        flagLabel->setPixmap(flagPixmap.scaled(flagSize, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        flagLabel->setScaledContents(false);
        flagLabel->show();
    } else {
        flagLabel->hide();
    }
    
    QString killsText = QString::number(playerKills);
    QString unitsText = QString::number(playerUnits);
    QString econText = QString::number(playerBalance);
    
    nameLabel->setText(playerName);
    
    QColor textColor = isDefeated ? QColor(128, 128, 128) : Qt::white;
    nameLabel->setFillColor(textColor);
    killsLabel->setFillColor(textColor);
    unitsLabel->setFillColor(textColor);
    econLabel->setFillColor(textColor);
    
    killsLabel->setText(killsText);
    unitsLabel->setText(unitsText);
    econLabel->setText(econText);
    
    updatePlayerIDVisibility();
    
    killsLabel->show();
    unitsLabel->show();
    econLabel->show();
    
    update();
}

void PlayerInfoCard::updatePlayerIDVisibility()
{
    // 始终显示真实玩家名称
    nameLabel->setText(playerName);
    nameLabel->show();
}

void PlayerInfoCard::paintEvent(QPaintEvent *event)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    
    if (hasBackgroundImage) {
        painter.drawPixmap(rect(), backgroundImage);
    } else {
        QColor bgColor = QColor("#" + QString::fromStdString(playerColor));
        
        QLinearGradient gradient(0, 0, width(), height());
        gradient.setColorAt(0, bgColor.lighter(120));
        gradient.setColorAt(1, bgColor.darker(120));
        
        QBrush brush(gradient);
        painter.setBrush(brush);
        
        QPainterPath path;
        path.addRoundedRect(rect(), 8, 8);
        painter.fillPath(path, brush);
        
        QPen pen(bgColor.darker(150));
        pen.setWidth(1);
        painter.setPen(pen);
        painter.drawPath(path);
    }
    
    QWidget::paintEvent(event);
}

QPixmap PlayerInfoCard::getBackgroundForColor(const std::string& color)
{
    QString originalColor = QString::fromStdString(color);
    
    bool ok;
    unsigned int colorHex = QString::fromStdString(color).toUInt(&ok, 16);
    
    if (!ok) {
        return QPixmap();
    }
    
    QString appDir = QCoreApplication::applicationDirPath();
    QString imagePath;
    
    if (isDefeated) {
        imagePath = appDir + "/assets/panels/player_bg_defeat.png";
        QPixmap defeatImage(imagePath);
        if (!defeatImage.isNull()) {
            return defeatImage;
        }
    }
    
    int r = (colorHex >> 16) & 0xFF;
    int g = (colorHex >> 8) & 0xFF;
    int b = colorHex & 0xFF;
    
    if (color == "e0d838") {
        imagePath = appDir + "/assets/panels/player_bg_yellow.png";
    } else if (color == "f84c48") {
        imagePath = appDir + "/assets/panels/player_bg_red.png";
    } else if (color == "58cc50") {
        imagePath = appDir + "/assets/panels/player_bg_green.png";
    } else if (color == "487cc8") {
        imagePath = appDir + "/assets/panels/player_bg_blue.png";
    } else if (color == "58d4e0") {
        imagePath = appDir + "/assets/panels/player_bg_skyblue.png";
    } else if (color == "9848b8") {
        imagePath = appDir + "/assets/panels/player_bg_purple.png";
    } else if (color == "f8ace8") {
        imagePath = appDir + "/assets/panels/player_bg_pink.png";
    } else if (color == "f8b048") {
        imagePath = appDir + "/assets/panels/player_bg_orange.png";
    } 
    else if (r > 200 && r > g*1.5 && r > b*1.5) {
        imagePath = appDir + "/assets/panels/player_bg_red.png";
    } else if (b > 150 && b > r*1.5 && b > g*1.5) {
        imagePath = appDir + "/assets/panels/player_bg_blue.png";
    } else if (g > 150 && g > r*1.2 && g > b*1.5) {
        imagePath = appDir + "/assets/panels/player_bg_green.png";
    } else if (r > 180 && g > 180 && b < 100) {
        imagePath = appDir + "/assets/panels/player_bg_yellow.png";
    } else if (r > 200 && g > 100 && g < 180 && b < 100) {
        imagePath = appDir + "/assets/panels/player_bg_orange.png";
    } else if (r > 200 && b > 150 && g < r*0.8) {
        imagePath = appDir + "/assets/panels/player_bg_pink.png";
    } else if (r > 100 && b > 100 && g < r*0.7 && g < b*0.7) {
        imagePath = appDir + "/assets/panels/player_bg_purple.png";
    } else if (b > 150 && g > 150 && r < b*0.8) {
        imagePath = appDir + "/assets/panels/player_bg_skyblue.png";
    } else {
        return QPixmap();
    }
    
    QPixmap bgImage(imagePath);
    
    if (bgImage.isNull()) {
        return QPixmap();
    }
    
    return bgImage;
}

QPixmap PlayerInfoCard::getCountryFlag(const QString& country)
{
    QString appDir = QCoreApplication::applicationDirPath();
    QString flagPath = appDir + "/assets/countries/" + country + ".png";
    QPixmap flag(flagPath);
    
    if (flag.isNull()) {
        flagPath = appDir + "/assets/countries/Americans.png";
        flag = QPixmap(flagPath);
        
        if (flag.isNull()) {
            flag = QPixmap(25, 15);
            flag.fill(Qt::transparent);
        }
    }
    
    return flag;
} 