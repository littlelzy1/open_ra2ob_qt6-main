#include "authmanager.h"
#include "membermanager.h"
#include <QVBoxLayout>
#include <QLabel>
#include <QApplication>
#include <QClipboard>
#include <QTimer>
#include <QJsonObject>
#include <QJsonArray>
#include <QEventLoop>
#include <QNetworkReply>
#include <QDebug>
#include <QProcess>
#include <QCryptographicHash>
#include <QSysInfo>
#include <QNetworkInterface>

#ifdef Q_OS_WIN
#include <Windows.h>
#include <intrin.h>
#include <wbemidl.h>
#include <comdef.h>
#include <Oleauto.h>
#pragma comment(lib, "wbemuuid.lib")
#pragma comment(lib, "oleaut32.lib")
#pragma comment(lib, "ole32.lib")
#endif

AuthManager::AuthManager(QObject *parent) : QObject(parent) {
    manager = new QNetworkAccessManager(this);
}

QString AuthManager::getMacAddress() {
    QString macAddress;
    
    // 获取所有网络接口
    QList<QNetworkInterface> interfaces = QNetworkInterface::allInterfaces();
    
    // 查找一个有效的物理网卡MAC地址
    for (const QNetworkInterface &iface : interfaces) {
        // 过滤掉虚拟接口和回环接口
        if (!(iface.flags() & QNetworkInterface::IsLoopBack) && 
             (iface.flags() & QNetworkInterface::IsRunning) &&
             (iface.hardwareAddress() != "00:00:00:00:00:00")) {
            macAddress = iface.hardwareAddress();
            // 优先使用物理接口
            if (!(iface.flags() & QNetworkInterface::IsPointToPoint) && 
                macAddress.contains(':')) {
                break;
            }
        }
    }
    
    // 移除MAC地址中的冒号，保留纯十六进制字符
    macAddress = macAddress.remove(":");
    
    qDebug() << "获取MAC地址:" << macAddress;
    return macAddress;
}

QString AuthManager::getCpuId() {
    // 尝试多种方式获取CPU ID
    QString cpuId = getCpuIdViaWMI();
    
    // 如果WMI方式失败，尝试通过CPUID指令获取
    if (cpuId.isEmpty()) {
        cpuId = getCpuIdViaIntrinsic();
    }
    
    // 如果上述方法都失败，使用wmic命令行获取
    if (cpuId.isEmpty()) {
        cpuId = getCpuIdViaWmic();
    }
    
    // 最后，如果所有方法都失败，使用系统信息生成唯一ID
    if (cpuId.isEmpty()) {
        cpuId = generateFallbackId();
    }
    
    // 获取MAC地址
    QString macAddress = getMacAddress();
    
    // 组合CPU ID和MAC地址，添加前缀 -
    QString uniqueId = "-" + cpuId;
    if (!macAddress.isEmpty()) {
        uniqueId += "-" + macAddress;
    }
    
    qDebug() << "最终唯一ID:" << uniqueId;
    return uniqueId;
}

QString AuthManager::getCpuIdViaWMI() {
    QString cpuId;
#ifdef Q_OS_WIN
    // 初始化COM
    HRESULT hr = CoInitializeEx(0, COINIT_MULTITHREADED);
    if (FAILED(hr)) {
        qDebug() << "CoInitializeEx失败: " << hr;
        return cpuId;
    }
    
    // 创建WbemLocator对象
    IWbemLocator *pLoc = nullptr;
    hr = CoCreateInstance(CLSID_WbemLocator, 0, CLSCTX_INPROC_SERVER, IID_IWbemLocator, (LPVOID*)&pLoc);
    if (FAILED(hr)) {
        qDebug() << "CoCreateInstance失败: " << hr;
        CoUninitialize();
        return cpuId;
    }
    
    // 连接到WMI
    IWbemServices *pSvc = nullptr;
    BSTR wmiNamespace = SysAllocString(L"ROOT\\CIMV2");
    hr = pLoc->ConnectServer(wmiNamespace, NULL, NULL, 0, NULL, 0, 0, &pSvc);
    SysFreeString(wmiNamespace);
    
    if (FAILED(hr)) {
        qDebug() << "ConnectServer失败: " << hr;
        pLoc->Release();
        CoUninitialize();
        return cpuId;
    }
    
    // 设置安全级别
    hr = CoSetProxyBlanket(pSvc, RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE, NULL, 
                          RPC_C_AUTHN_LEVEL_CALL, RPC_C_IMP_LEVEL_IMPERSONATE, 
                          NULL, EOAC_NONE);
    if (FAILED(hr)) {
        qDebug() << "CoSetProxyBlanket失败: " << hr;
        pSvc->Release();
        pLoc->Release();
        CoUninitialize();
        return cpuId;
    }
    
    // 执行WQL查询
    IEnumWbemClassObject *pEnumerator = nullptr;
    BSTR wql = SysAllocString(L"WQL");
    BSTR query = SysAllocString(L"SELECT ProcessorId FROM Win32_Processor");
    
    hr = pSvc->ExecQuery(wql, query, WBEM_FLAG_FORWARD_ONLY, NULL, &pEnumerator);
    
    SysFreeString(wql);
    SysFreeString(query);
    
    if (FAILED(hr)) {
        qDebug() << "ExecQuery失败: " << hr;
        pSvc->Release();
        pLoc->Release();
        CoUninitialize();
        return cpuId;
    }
    
    // 获取查询结果
    IWbemClassObject *pclsObj = nullptr;
    ULONG uReturn = 0;
    
    while (pEnumerator) {
        hr = pEnumerator->Next(WBEM_INFINITE, 1, &pclsObj, &uReturn);
        if (0 == uReturn) break;
        
        VARIANT vtProp;
        VariantInit(&vtProp);
        
        BSTR processorId = SysAllocString(L"ProcessorId");
        hr = pclsObj->Get(processorId, 0, &vtProp, 0, 0);
        SysFreeString(processorId);
        
        if (SUCCEEDED(hr)) {
            if (vtProp.vt == VT_BSTR) {
                cpuId = QString::fromWCharArray(vtProp.bstrVal);
            }
            VariantClear(&vtProp);
        }
        
        pclsObj->Release();
    }
    
    // 清理资源
    pEnumerator->Release();
    pSvc->Release();
    pLoc->Release();
    CoUninitialize();
#endif
    qDebug() << "WMI方式获取CPU ID:" << cpuId;
    return cpuId;
}

QString AuthManager::getCpuIdViaIntrinsic() {
    QString cpuId;
#ifdef Q_OS_WIN
    // 使用CPUID指令获取CPU信息
    int cpuInfo[4] = {0};
    __cpuid(cpuInfo, 1);
    
    // 组合处理器信息生成唯一ID，移除前缀
    QString processorID = QString("%1%2")
                             .arg(cpuInfo[3], 8, 16, QLatin1Char('0'))
                             .arg(cpuInfo[0], 8, 16, QLatin1Char('0'));
    cpuId = processorID.toUpper();
    
    // 移除可能的前缀字符
    if (cpuId.startsWith("-")) {
        cpuId = cpuId.mid(1);
    }
#endif
    qDebug() << "CPUID方式获取CPU ID:" << cpuId;
    return cpuId;
}

QString AuthManager::getCpuIdViaWmic() {
    QString cpuId;
#ifdef Q_OS_WIN
    QProcess process;
    process.start("wmic", QStringList() << "cpu" << "get" << "processorid");
    process.waitForFinished(3000);
    
    QString output = process.readAllStandardOutput();
    QStringList lines = output.split("\n", Qt::SkipEmptyParts);
    
    if (lines.size() >= 2) {
        cpuId = lines[1].trimmed();
    }
#endif
    qDebug() << "WMIC方式获取CPU ID:" << cpuId;
    return cpuId;
}

QString AuthManager::generateFallbackId() {
    // 使用系统信息生成唯一ID
    QString systemInfo = QSysInfo::machineHostName() + 
                         QSysInfo::machineUniqueId() +
                         QSysInfo::currentCpuArchitecture() +
                         QSysInfo::productType() + 
                         QSysInfo::productVersion();
    
    // 计算哈希值作为唯一ID
    QByteArray hash = QCryptographicHash::hash(systemInfo.toUtf8(), 
                                             QCryptographicHash::Sha256);
    QString fallbackId = hash.toHex().left(16).toUpper();
    
    qDebug() << "系统信息生成唯一ID:" << fallbackId;
    return fallbackId;
}

void AuthManager::checkAuthorization() {
    QString cpuId = getCpuId();
    if (cpuId.isEmpty()) {
        qDebug() << "无法获取CPU ID";
        emit authorizationFailed("无法获取CPU ID");
        return;
    }
    
    // TODO: 配置你的授权服务器URL
    QUrl url("https://your-server.com/api/authorized_cpus.json");
    QNetworkRequest request(url);
    QNetworkReply *reply = manager->get(request);
    
    qDebug() << "检查CPU ID授权：" << cpuId << "，URL：" << url.toString();
    
    connect(reply, &QNetworkReply::finished, [this, reply, cpuId]() {
        if (reply->error() == QNetworkReply::NoError) {
            parseAuthResponse(reply->readAll());
        } else {
            qDebug() << "网络错误:" << reply->errorString();
            // 修改: 网络错误时默认拒绝访问
            emit authorizationFailed(cpuId + " (网络请求失败)");
        }
        reply->deleteLater();
    });
}

void AuthManager::parseAuthResponse(const QByteArray &data) {
    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);
    
    if (parseError.error != QJsonParseError::NoError) {
        qDebug() << "JSON解析错误:" << parseError.errorString();
        emit authorizationFailed("服务器响应格式错误");
        return;
    }

    QJsonObject rootObj = doc.object();
    QJsonArray authorizedArray = rootObj["cpuIds"].toArray();
    
    QString currentCpuId = getCpuId();
    bool authorized = false;
    
    for (const QJsonValue &value : authorizedArray) {
        if (value.toString() == currentCpuId) {
            authorized = true;
            break;
        }
    }
    
    if (authorized) {
        qDebug() << "授权验证成功";
        
        // 初始化会员管理器
        initializeMemberManager();
        
        emit authorizationSuccess();
    } else {
        qDebug() << "授权验证失败，CPU ID未在授权列表中";
        emit authorizationFailed(currentCpuId);
    }
}

void AuthManager::initializeMemberManager() {
    // 获取CPU ID和MAC地址
    QString cpuId = getCpuIdViaWMI();
    if (cpuId.isEmpty()) {
        cpuId = getCpuIdViaIntrinsic();
    }
    if (cpuId.isEmpty()) {
        cpuId = getCpuIdViaWmic();
    }
    if (cpuId.isEmpty()) {
        cpuId = generateFallbackId();
    }
    
    QString macAddress = getMacAddress();
    
    // 初始化会员管理器
    MemberManager& memberManager = MemberManager::getInstance();
    memberManager.initialize(cpuId, macAddress);
    
    qDebug() << "[AuthManager] 会员管理器初始化完成";
}

// 授权失败对话框实现
AuthDialog::AuthDialog(const QString& cpuId, QWidget *parent) 
    : QDialog(parent), m_cpuId(cpuId) {
    setWindowTitle(tr("授权验证失败"));
    setFixedSize(500, 200);  // 将宽度从400增加到500
    
    QVBoxLayout *layout = new QVBoxLayout(this);
    
    QLabel *infoLabel = new QLabel(tr("插件权限获取请访问www.ra2pvp.com，其它/OB插件权限，提交下列ID即可。"), this);
    infoLabel->setWordWrap(true);  // 允许标签文本自动换行
    
    m_cpuIdEdit = new QLineEdit(cpuId, this);
    m_cpuIdEdit->setReadOnly(true);
    
    m_copyButton = new QPushButton(tr("复制ID"), this);
    connect(m_copyButton, &QPushButton::clicked, this, &AuthDialog::onCopyButtonClicked);
    
    QPushButton *closeButton = new QPushButton(tr("关闭"), this);
    connect(closeButton, &QPushButton::clicked, this, &QDialog::reject);
    
    layout->addWidget(infoLabel);
    layout->addWidget(m_cpuIdEdit);
    layout->addWidget(m_copyButton);
    layout->addWidget(closeButton);
    
    setLayout(layout);
}

void AuthDialog::onCopyButtonClicked() {
    QClipboard *clipboard = QApplication::clipboard();
    clipboard->setText(m_cpuId);
    m_copyButton->setText(tr("已复制"));
    QTimer::singleShot(1500, [this]() {
        m_copyButton->setText(tr("复制ID"));  // 修改为与按钮初始文本一致
    });
}