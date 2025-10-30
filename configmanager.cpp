#include "./configmanager.h"

#include <QCoreApplication>
#include <QFile>
#include <QJsonDocument>
#include <QDir>
#include <functional>

#include "./messagebox.h"

ConfigManager::ConfigManager(QObject *parent) : QObject(parent) {
    _config_path = "./config/config.json";
    gls          = &Globalsetting::getInstance();
    
    // 确保配置目录存在
    ensureConfigDirectoryExists();
}

bool ConfigManager::checkConfig() {
    QFile config_file(_config_path);

    if (config_file.exists()) {
        if (config_file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QByteArray config_data = config_file.readAll();
            config_file.close();

            QJsonDocument json_doc = QJsonDocument::fromJson(config_data);
            if (json_doc.isObject()) {
                _config_json = json_doc.object();

                // 检查是否存在头像服务器URL配置
                if (!_config_json.contains("avatar")) {
                    QJsonObject avatarObj;
                    _config_json["avatar"] = avatarObj;
                }

                return true;
            }
            MessageBox::errorQuit("Initialization", "Read config file error: Not a json file.");
            return false;
        }
        MessageBox::errorQuit("Initialization", "Failed to open config file for reading.");
        return false;
    }
    
    // 如果配置文件不存在，创建一个默认的配置文件
    _config_json = QJsonObject();
    _config_json["Players"] = QJsonArray();
    
    // 创建背景设置
    QJsonObject backgroundSettings;
    backgroundSettings["path"] = "";
    _config_json["background"] = backgroundSettings;
    
    // 创建侧边栏背景设置
    QJsonObject sidebarBackgroundSettings;
    sidebarBackgroundSettings["path"] = "";
    _config_json["sidebar_background"] = sidebarBackgroundSettings;
    
    // 写入默认配置
    bool success = writeConfig(_config_json);
    if (!success) {
        MessageBox::errorQuit("Initialization", "Failed to create default config file.");
        return false;
    }
    
    return true;
}

bool ConfigManager::verifyConfig() { return true; }

bool ConfigManager::writeConfig(const QJsonObject &jsonobj) {
    // 确保配置目录存在
    ensureConfigDirectoryExists();
    
    QJsonDocument new_json_doc(jsonobj);  // 使用传入的jsonobj参数，而不是成员变量
    QByteArray new_data = new_json_doc.toJson(QJsonDocument::Indented);

    QFile config_file(_config_path);

    if (!config_file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        MessageBox::errorQuit("Config", "Failed to open config.json for writing.");
        return false;
    }

    config_file.write(new_data);
    config_file.close();
    return true;
}

// 确保配置目录存在
void ConfigManager::ensureConfigDirectoryExists() {
    QFileInfo fileInfo(_config_path);
    QDir directory = fileInfo.dir();
    
    if (!directory.exists()) {
        directory.mkpath(".");
    }
}

/*
 * Must call ConfigManager::checkConfig() first!
 */
bool ConfigManager::setLanguage(QString language) {
    // 语言设置功能已移除
    return true;
}

QString ConfigManager::getLanguage() {
    // 固定返回中文
    return QString("zh_CN");
}

QJsonArray ConfigManager::getPlayernameList() {
    if (_config_json.contains("Players")) {
        QJsonValue player_json = _config_json["Players"];

        if (player_json.type() == QJsonValue::Array) {
            QJsonArray player_list = player_json.toArray();

            return player_list;
        }
    }
    return QJsonArray();
}

QString ConfigManager::findNameByNickname(QString nickname) {
    QString name;

    if (nickname.isEmpty()) {
        return name;
    }

    QJsonArray player_list = getPlayernameList();
    for (auto it : player_list) {
        if (it.type() == QJsonValue::Object) {
            QJsonObject p       = it.toObject();
            QString jnickname   = p.value("nickname").toString();
            QString jplayername = p.value("playername").toString();

            if (QString::compare(nickname, jnickname) == 0) {
                name = jplayername;
            }
        }
    }
    return name;
}

void ConfigManager::giveValueToGlobalsetting() {
    // 加载玩家列表
    gls->player_list = getPlayernameList();
    
    // 清空现有的玩家映射缓存，强制重新加载
    gls->clearPlayerMappings();
    
    // 重新加载所有玩家映射到缓存
    QJsonArray playerList = getPlayernameList();
    for (const auto &player : playerList) {
        if (player.isObject()) {
            QJsonObject obj = player.toObject();
            QString nickname = obj["nickname"].toString();
            QString playername = obj["playername"].toString();
            if (!nickname.isEmpty() && !playername.isEmpty()) {
                gls->addPlayerMapping(nickname, playername);
            }
        }
    }
    
    // 添加PMB图片显示设置
    gls->s.show_pmb_image = value("show_pmb_image", true).toBool();
    
    qDebug() << "[ConfigManager] giveValueToGlobalsetting已调用，重新加载了" << playerList.size() << "个玩家映射";
}

bool ConfigManager::insertPlayer(QString nickname, QString playername) {
    QJsonArray player_list = getPlayernameList();
    QJsonObject p;

    p.insert("nickname", nickname);
    p.insert("playername", playername);

    player_list.append(p);
    _config_json["Players"] = player_list;
    return writeConfig(_config_json);
}

bool ConfigManager::overwritePlayer(QString nickname, QString playername) {
    QJsonArray player_list = getPlayernameList();
    QJsonObject p;

    p.insert("nickname", nickname);
    p.insert("playername", playername);

    for (int i = 0; i < player_list.size(); ++i) {
        QJsonObject obj   = player_list[i].toObject();
        QString jnickname = obj.value("nickname").toString();
        if (QString::compare(jnickname, nickname) == 0) {
            player_list.removeAt(i);
            player_list.insert(i, p);
            break;
        }
    }

    _config_json["Players"] = player_list;
    return writeConfig(_config_json);
}

bool ConfigManager::updatePlayer(QString nickname, QString playername) {
    QString found = findNameByNickname(nickname);
    
    bool result;
    if (found.isEmpty()) {
        result = insertPlayer(nickname, playername);
    } else {
        result = overwritePlayer(nickname, playername);
    }
    
    // 确保更新后立即同步到全局设置
    if (result) {
        gls->addPlayerMapping(nickname, playername);
        qDebug() << "[ConfigManager] updatePlayer成功更新映射:" << nickname << "->" << playername;
    }
    
    return result;
}

QVariant ConfigManager::value(const QString &key, const QVariant &defaultValue) {
    // 按照键路径解析JSON对象
    QStringList parts = key.split('/');
    QJsonObject obj = _config_json;
    
    // 处理除最后一个部分外的所有部分
    for (int i = 0; i < parts.size() - 1; ++i) {
        if (!obj.contains(parts[i]) || !obj[parts[i]].isObject()) {
            return defaultValue;
        }
        obj = obj[parts[i]].toObject();
    }
    
    // 处理最后一个部分
    QString lastPart = parts.last();
    if (!obj.contains(lastPart)) {
        return defaultValue;
    }
    
    QJsonValue val = obj[lastPart];
    
    // 根据值类型返回适当的QVariant
    if (val.isString()) {
        return val.toString();
    } else if (val.isBool()) {
        return val.toBool();
    } else if (val.isDouble()) {
        return val.toDouble();
    } else if (val.isArray()) {
        return QVariant::fromValue(val.toArray());
    } else if (val.isObject()) {
        return QVariant::fromValue(val.toObject());
    }
    
    return defaultValue;
}

void ConfigManager::setValue(const QString &key, const QVariant &value) {
    // 分割键路径
    QStringList parts = key.split('/');
    if (parts.isEmpty()) {
        return;
    }
    
    // 对于简单的非嵌套情况，直接处理
    if (parts.size() == 1) {
        if (value.typeId() == QMetaType::QString) {
            _config_json[parts[0]] = value.toString();
        } else if (value.typeId() == QMetaType::Bool) {
            _config_json[parts[0]] = value.toBool();
        } else if (value.typeId() == QMetaType::Int) {
            _config_json[parts[0]] = value.toInt();
        } else if (value.typeId() == QMetaType::Double) {
            _config_json[parts[0]] = value.toDouble();
        } else {
            _config_json[parts[0]] = value.toString();
        }
        return;
    }
    
    // 递归辅助函数需要使用std::function来处理递归
    std::function<void(QJsonObject&, const QStringList&, int, const QVariant&)> setValueRecursive;
    
    setValueRecursive = [&setValueRecursive](QJsonObject &obj, const QStringList &parts, int index, const QVariant &value) {
        if (index >= parts.size() - 1) {
            // 到达最后一个部分，设置值
            QString lastPart = parts.at(index);
            
            if (value.typeId() == QMetaType::QString) {
                obj[lastPart] = value.toString();
            } else if (value.typeId() == QMetaType::Bool) {
                obj[lastPart] = value.toBool();
            } else if (value.typeId() == QMetaType::Int) {
                obj[lastPart] = value.toInt();
            } else if (value.typeId() == QMetaType::Double) {
                obj[lastPart] = value.toDouble();
            } else {
                obj[lastPart] = value.toString();
            }
            return;
        }
        
        // 处理当前部分
        QString currentPart = parts.at(index);
        
        // 如果当前部分不存在或不是对象，则创建它
        if (!obj.contains(currentPart) || !obj[currentPart].isObject()) {
            obj[currentPart] = QJsonObject();
        }
        
        // 获取当前部分的对象，修改它，然后保存回去
        QJsonObject childObj = obj[currentPart].toObject();
        setValueRecursive(childObj, parts, index + 1, value);
        obj[currentPart] = childObj;
    };
    
    // 启动递归
    setValueRecursive(_config_json, parts, 0, value);
}
