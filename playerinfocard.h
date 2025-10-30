#ifndef PLAYERINFOCARD_H
#define PLAYERINFOCARD_H

#include <QWidget>
#include <QLabel>
#include <QPaintEvent>
#include <QPixmap>
#include <QDebug>
#include <QFile>
#include <QTextStream>
#include <QDir>
#include <QDateTime>
#include <QPropertyAnimation>
#include "qoutlinelabel.h"
#include "Ra2ob/Ra2ob/src/Datatypes.hpp"

class QOutlineLabel;

class PlayerInfoCard : public QWidget
{
    Q_OBJECT
    
    Q_PROPERTY(QPoint pos READ pos WRITE move)

public:
    explicit PlayerInfoCard(QWidget *parent = nullptr);
    ~PlayerInfoCard();

    void setPlayerInfo(const QString& name, const QString& country, int kills, int units, int balance, const std::string& color);
    
    void setDefeated(bool defeated);
    
    void updateDisplay();
    
    int getKills() const { return playerKills; }
    int getUnits() const { return playerUnits; }
    int getBalance() const { return playerBalance; }
    QString getPlayerName() const { return playerName; }
    
    void updatePlayerIDVisibility();
    
    QOutlineLabel *nameLabel;
    QOutlineLabel *killsLabel;
    QOutlineLabel *unitsLabel;
    QOutlineLabel *econLabel;
    QLabel *flagLabel;
    
    static PlayerInfoCard* sampleCard;

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    void writeDebugToFile(const QString& message);
    
    QPixmap getBackgroundForColor(const std::string& color);
    
    QPixmap getCountryFlag(const QString& country);
    
    QString playerName;
    QString playerCountry;
    int playerKills;
    int playerUnits;
    int playerBalance;
    std::string playerColor;
    bool isDefeated;
    
    QPixmap backgroundImage;
    bool hasBackgroundImage;

    void setupFonts();
    
    QFile* debugLogFile;
};

#endif