#include <shlobj.h>

#ifdef _WIN32
#include <Windows.h>
#include <io.h>
#include <fcntl.h>
#endif

#include <QApplication>
#include <QFontDatabase>
#include <QLocale>
#include <QMessageBox>
#include <QObject>
#include <QProcess>
#include <QSharedMemory>
#include <QString>
#include <QTranslator>
#include <QEventLoop>
#include <QDateTime>
#include <iostream>

#include "./globalsetting.h"
#include "./hotkeymanager.h"
#include "./mainwindow.h"
#include "updatechecker.h"
#include "updatedialog.h"
#include "authmanager.h"
#include "membermanager.h"
#include "buildingdetector.h"

// 开启控制台窗口的辅助函数
void enableConsoleWindow() {
#ifdef _WIN32
    // 分配新的控制台
    if (AllocConsole()) {
        // 重定向标准输出到控制台
        freopen("CONOUT$", "w", stdout);
        freopen("CONOUT$", "w", stderr);
        freopen("CONIN$", "r", stdin);
        
        // 设置C++的iostream同步
        std::ios::sync_with_stdio(true);
        
        // 设置控制台标题
        SetConsoleTitle(L"RA2 OB Debug Console");
        
        // 调整控制台缓冲区大小
        HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
        CONSOLE_SCREEN_BUFFER_INFO csbi;
        GetConsoleScreenBufferInfo(hConsole, &csbi);
        csbi.dwSize.Y = 5000; // 增加缓冲区行数
        SetConsoleScreenBufferSize(hConsole, csbi.dwSize);
        
        // 输出启动信息
        std::cout << "Debug console initialized." << std::endl;
        std::cout << "Application started at: " << QDateTime::currentDateTime().toString().toStdString() << std::endl;
        std::cout << "----------------------------------------" << std::endl;
    }
#endif
}

int main(int argc, char *argv[]) {
    // 开启控制台窗口
  //  enableConsoleWindow();  // 启用控制台显示间谍渗透状态
    
    QApplication a(argc, argv);
    
    // 设置应用程序版本
    QCoreApplication::setApplicationVersion("2.4.63");

    NEFilter filter;
    a.installNativeEventFilter(&filter);

    HotkeyManager hm;
    hm.registerHotkey(filter);

    Globalsetting &gls = Globalsetting::getInstance();

    ConfigManager *cfgm = new ConfigManager();
    if (!cfgm->checkConfig()) {
        return 1;
    }

    // 加载中文翻译
    QTranslator translator;
        if (translator.load(":/i18n/open_ra2ob_qt6_zh_CN")) {
            a.installTranslator(&translator);
    }

    BOOL isAdmin = IsUserAnAdmin();
    if (!isAdmin) {
        QMessageBox::information(NULL, QObject::tr("tips"),
                                 QObject::tr("Please run this program as admin."));
        qApp->quit();
        return -1;
    }

    static QSharedMemory *singleApp = new QSharedMemory("SingleApp");
    if (!singleApp->create(1)) {
        QMessageBox::information(NULL, QObject::tr("tips"),
                                 QObject::tr("The program is already running."));
        qApp->quit();
        return -1;
    }
    
    // =============== 授权验证 ===============
    AuthManager *authManager = new AuthManager(&a);
    QEventLoop authLoop;
    bool authSuccess = false;
    
    QObject::connect(authManager, &AuthManager::authorizationSuccess, [&]() {
        authSuccess = true;
        authLoop.quit();
    });
    
    QObject::connect(authManager, &AuthManager::authorizationFailed, [&](QString cpuId) {
        AuthDialog dialog(cpuId);
        dialog.exec();
        authSuccess = false;
        authLoop.quit();
    });
    
    // 开始授权检查
    authManager->checkAuthorization();
    authLoop.exec(); // 等待授权检查完成
    
    if (!authSuccess) {
        delete singleApp;
        a.removeNativeEventFilter(&filter);
        hm.releaseHotkey();
        return -1;
    }
    // =============== 授权验证结束 ===============
    
    // 添加更新检查
    UpdateChecker *updateChecker = new UpdateChecker(&a);
    QEventLoop loop;
    bool checkUpdateDone = false;
    
    QObject::connect(updateChecker, &UpdateChecker::updateAvailable, 
                     [&](const QString &newVersion, const QString &changelog, const QString &downloadUrl) {
        checkUpdateDone = true;
        loop.quit();
        
        UpdateDialog dialog(nullptr, newVersion, changelog, downloadUrl);
        
        // 连接跳过更新信号，如果用户选择不更新，则直接退出程序
        QObject::connect(&dialog, &UpdateDialog::updateSkipped, [&]() {
            qApp->quit();
            std::exit(0); // 强制退出程序
        });
        
        dialog.exec();
    });
    
    // 添加处理程序禁用的情况
    QObject::connect(updateChecker, &UpdateChecker::programDisabled, 
                     [&](const QString &message) {
        checkUpdateDone = true;
        loop.quit();
        
        QMessageBox::critical(nullptr, "程序已禁用", message);
        qApp->quit();
        std::exit(0); // 强制退出程序
    });
    
    QObject::connect(updateChecker, &UpdateChecker::noUpdateAvailable, 
                     [&]() {
        checkUpdateDone = true;
        loop.quit(); 
    });
    
    QObject::connect(updateChecker, &UpdateChecker::checkFailed, 
                     [&](const QString &errorMessage) {
        qDebug() << "Update check failed:" << errorMessage;
        checkUpdateDone = true;
        loop.quit(); 
    });
    
    // 检查更新
    updateChecker->checkForUpdates();
    
    // 等待更新检查完成
    if (!checkUpdateDone) {
        loop.exec();
    }

    MainWindow w(nullptr, cfgm);
    w.show();

    Ra2ob::Game &g = Ra2ob::Game::getInstance();
    g.startLoop();

    // 创建并启动建筑检测器
    BuildingDetector *buildingDetector = new BuildingDetector(&a);
    buildingDetector->startDetection(500); // 每0.5秒检测一次
    
    qDebug() << "Building detector started in main application";

    int ret = a.exec();

    delete singleApp;
    a.removeNativeEventFilter(&filter);
    hm.releaseHotkey();
    delete authManager;

    return ret;
}
