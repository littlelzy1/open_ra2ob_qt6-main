#ifndef CONFIGMANAGER_H
#define CONFIGMANAGER_H

#include <QJsonArray>
#include <QJsonObject>
#include <QList>
#include <QObject>
#include <QString>
#include <QVariant>

#include "./globalsetting.h"

class ConfigManager : public QObject {
    Q_OBJECT

public:
    explicit ConfigManager(QObject *parent = nullptr);
    ~ConfigManager() {}

    QJsonObject _config_json;
    QString _config_path;
    Globalsetting *gls;

    bool checkConfig();
    bool verifyConfig();
    bool writeConfig(const QJsonObject &jsonobj);
    bool setLanguage(QString language);
    QString getLanguage();
    bool setOpacity(float opacity);
    float getOpacity();
    QJsonArray getPlayernameList();
    bool updatePlayer(QString nickname, QString playername);
    void giveValueToGlobalsetting();
    QString findNameByNickname(QString nickname);
    
    // 添加通用设置方法
    QVariant value(const QString &key, const QVariant &defaultValue = QVariant());
    void setValue(const QString &key, const QVariant &value);
    bool save() { return writeConfig(_config_json); }
    void ensureConfigDirectoryExists();

private:
    bool insertPlayer(QString nickname, QString playername);
    bool overwritePlayer(QString nickname, QString playername);
};

#endif  // CONFIGMANAGER_H
