#pragma once

#include <QDialog>
#include <QLabel>
#include <QPushButton>
#include <QProgressBar>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTextEdit>
#include <QCloseEvent>

class UpdateDialog : public QDialog {
    Q_OBJECT
public:
    UpdateDialog(QWidget *parent, const QString &newVersion, const QString &changelog, const QString &downloadUrl);
    
signals:
    void updateSkipped();
    
private slots:
    void startUpdate();
    void skipUpdate();
    void updateProgress(qint64 bytesReceived, qint64 bytesTotal);
    void downloadFinished(const QString &filePath);
    void downloadFailed(const QString &errorMessage);
    
private:
    QString downloadUrl;
    QLabel *infoLabel;
    QTextEdit *changelogEdit;
    QProgressBar *progressBar;
    QPushButton *updateButton;
    QPushButton *skipButton;
    
    void launchInstaller(const QString &installerPath);
    
    // 重写closeEvent以捕获窗口关闭事件
    void closeEvent(QCloseEvent *event) override;
}; 