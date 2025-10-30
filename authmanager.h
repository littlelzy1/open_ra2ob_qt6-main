#ifndef AUTHMANAGER_H
#define AUTHMANAGER_H

#include <QObject>
#include <QNetworkAccessManager>
#include <QJsonDocument>
#include <QDialog>
#include <QPushButton>
#include <QLineEdit>

class AuthManager : public QObject {
    Q_OBJECT
public:
    explicit AuthManager(QObject *parent = nullptr);
    void checkAuthorization();
    static QString getCpuId();
    
    // 会员管理相关方法
    void initializeMemberManager();
    static QString getMacAddress();

signals:
    void authorizationSuccess();
    void authorizationFailed(QString cpuId);

private slots:
    void parseAuthResponse(const QByteArray &data);

private:
    QNetworkAccessManager *manager;
    
    // 多种获取CPU ID的方法
    static QString getCpuIdViaWMI();
    static QString getCpuIdViaIntrinsic();
    static QString getCpuIdViaWmic();
    static QString generateFallbackId();
};

class AuthDialog : public QDialog {
    Q_OBJECT
public:
    explicit AuthDialog(const QString& cpuId, QWidget *parent = nullptr);

private slots:
    void onCopyButtonClicked();

private:
    QString m_cpuId;
    QPushButton *m_copyButton;
    QLineEdit *m_cpuIdEdit;
};

#endif // AUTHMANAGER_H