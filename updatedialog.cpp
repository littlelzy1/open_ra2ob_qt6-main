#include "updatedialog.h"
#include "updatedownloader.h"
#include <QProcess>
#include <QCoreApplication>
#include <QMessageBox>
#include <QDesktopServices>
#include <QUrl>
#include <QDebug>

UpdateDialog::UpdateDialog(QWidget *parent, const QString &newVersion, const QString &changelog, const QString &downloadUrl)
    : QDialog(parent), downloadUrl(downloadUrl) {
    
    setWindowTitle("发现新版本");
    setMinimumWidth(450);
    setMinimumHeight(300);
    
    QVBoxLayout *layout = new QVBoxLayout(this);
    
    infoLabel = new QLabel(QString("当前有新版本 %1 可用").arg(newVersion), this);
    layout->addWidget(infoLabel);
    
    QLabel *changelogLabel = new QLabel("更新内容:", this);
    layout->addWidget(changelogLabel);
    
    changelogEdit = new QTextEdit(this);
    changelogEdit->setReadOnly(true);
    changelogEdit->setPlainText(changelog);
    layout->addWidget(changelogEdit);
    
    progressBar = new QProgressBar(this);
    progressBar->setVisible(false);
    layout->addWidget(progressBar);
    
    QHBoxLayout *buttonLayout = new QHBoxLayout();
    updateButton = new QPushButton("立即更新", this);
    skipButton = new QPushButton("稍后更新", this);
    
    buttonLayout->addWidget(updateButton);
    buttonLayout->addWidget(skipButton);
    layout->addLayout(buttonLayout);
    
    connect(updateButton, &QPushButton::clicked, this, &UpdateDialog::startUpdate);
    connect(skipButton, &QPushButton::clicked, this, &UpdateDialog::skipUpdate);
    
    setLayout(layout);
}

void UpdateDialog::startUpdate() {
    updateButton->setEnabled(false);
    skipButton->setEnabled(false);
    progressBar->setVisible(true);
    infoLabel->setText("正在下载更新...");
    
    UpdateDownloader *downloader = new UpdateDownloader(this);
    connect(downloader, &UpdateDownloader::downloadProgress, this, &UpdateDialog::updateProgress);
    connect(downloader, &UpdateDownloader::downloadFinished, this, &UpdateDialog::downloadFinished);
    connect(downloader, &UpdateDownloader::downloadFailed, this, &UpdateDialog::downloadFailed);
    
    downloader->downloadUpdate(downloadUrl);
}

void UpdateDialog::skipUpdate() {
    emit updateSkipped();
    reject();
}

void UpdateDialog::updateProgress(qint64 bytesReceived, qint64 bytesTotal) {
    progressBar->setMaximum(bytesTotal);
    progressBar->setValue(bytesReceived);
    
    // 显示下载百分比
    if (bytesTotal > 0) {
        int percent = static_cast<int>(bytesReceived * 100 / bytesTotal);
        infoLabel->setText(QString("正在下载更新... %1%").arg(percent));
    }
}

void UpdateDialog::downloadFinished(const QString &filePath) {
    infoLabel->setText("下载完成，即将安装更新...");
    
    // 启动安装程序
    launchInstaller(filePath);
}

void UpdateDialog::downloadFailed(const QString &errorMessage) {
    QMessageBox::critical(this, "更新失败", "下载更新失败: " + errorMessage);
    updateButton->setEnabled(true);
    skipButton->setEnabled(true);
    progressBar->setVisible(false);
    infoLabel->setText("更新下载失败，请稍后重试。");
}

void UpdateDialog::launchInstaller(const QString &installerPath) {
    qDebug() << "Launching installer:" << installerPath;
    
    // 尝试以管理员权限启动安装程序
    // 使用如下参数启动安装程序
    // /S - 静默安装
    // /D=<path> - 安装目录
    
    // 添加参数使得安装完成后自动启动程序
    QStringList arguments;
    arguments << "/S"; // 静默安装
    arguments << "/D=" + QCoreApplication::applicationDirPath(); // 安装路径
    
    if (QProcess::startDetached(installerPath, arguments)) {
        // 关闭当前应用程序
        QCoreApplication::quit();
    } else {
        // 如果无法直接启动，则打开文件让用户手动运行
        if (!QDesktopServices::openUrl(QUrl::fromLocalFile(installerPath))) {
            QMessageBox::warning(this, "安装更新", "无法自动启动安装程序，请手动运行下载的安装文件:\n" + installerPath);
        }
        accept();
    }
}

// 重写closeEvent，处理用户直接关闭窗口的情况
void UpdateDialog::closeEvent(QCloseEvent *event) {
    // 如果更新已经开始下载，则允许正常关闭
    if (!progressBar->isVisible()) {
        // 发出与点击"稍后更新"相同的信号
        emit updateSkipped();
    }
    // 调用基类的closeEvent以允许窗口关闭
    QDialog::closeEvent(event);
} 