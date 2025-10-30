#include "./producingblock.h"

#include <QFile>
#include <QPainter>
#include <QRect>
#include <QFontDatabase>
#include <QCoreApplication>
#include <QDir>
#include <QCache>
#include <QPainterPath>

#include "./layoutsetting.h"
#include "./ui_producingblock.h"

// 添加静态图像缓存
static QCache<QString, QPixmap> imageCache(50); // 缓存最多50个图片
// 添加静态字体族名称，确保字体只加载一次
static QString technoGlitchFontFamily;
static bool fontLoaded = false;

// 加载字体的静态方法，确保只执行一次
static QString loadTechnoGlitchFont() {
    if (fontLoaded) {
        return technoGlitchFontFamily;
    }
    
    QString appDir = QCoreApplication::applicationDirPath();
    QString fontPath = appDir + "/assets/fonts/LLDEtechnoGlitch-Bold0.ttf";
    int fontId = QFontDatabase::addApplicationFont(fontPath);
    
    if (fontId >= 0) {
        QStringList families = QFontDatabase::applicationFontFamilies(fontId);
        if (!families.isEmpty()) {
            technoGlitchFontFamily = families.first();
            fontLoaded = true;
            return technoGlitchFontFamily;
        }
    }
    
    // 默认字体
    fontLoaded = true;
    technoGlitchFontFamily = "Arial";
    return technoGlitchFontFamily;
}

ProducingBlock::ProducingBlock(QWidget *parent) : QWidget(parent), ui(new Ui::ProducingBlock) {
    ui->setupUi(this);

    gls = &Globalsetting::getInstance();

    // 移除 lb_status 和 lb_number 的创建，因为现在直接在 paintEvent 中绘制
    
    // 使用与 ObTeam 相同的尺寸（70×80，乘以ratio）
    float ratio = gls->l.ratio;
    int scaledWidth = qRound(70 * ratio);  // 与 ObTeam 中的 producingBlockWidth 一致
    int scaledHeight = qRound(80 * ratio); // 与 ObTeam 中的 producingBlockHeight 一致
    
    // 设置控件的默认尺寸
    this->setFixedSize(scaledWidth, scaledHeight);
    
    // 移除对 ui->lb_img 的设置，因为现在直接在 paintEvent 中绘制图像
}

ProducingBlock::~ProducingBlock() {
    clean = true;
    update();
    delete ui;
}

void ProducingBlock::paintEvent(QPaintEvent *) {
    if (isHidden()) {
        return;
    }

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);

    if (clean) {
        painter.eraseRect(this->rect());
        return;
    }

    float ratio = gls->l.ratio;
    int w = this->width();
    int h = this->height();

    // 1. 绘制圆角背景
    QPainterPath bgPath;
    bgPath.addRoundedRect(QRect(0, 0, w, h), 8 * ratio, 8 * ratio);
    QColor bgColor = blockColor;
    bgColor.setAlpha(180);
    painter.fillPath(bgPath, bgColor);

    // 2. 单位图标（带圆角，居中）
    QString appDir = QCoreApplication::applicationDirPath();
    QString img_str = appDir + "/assets/obicons/" + blockName + ".png";
    int iconWidth = qRound(60 * ratio);
    int iconHeight = qRound(60 * ratio);
    QString cacheKey = img_str + QString::number(iconWidth) + QString::number(iconHeight);
    QPixmap *iconPixmap = imageCache.object(cacheKey);
    if (!iconPixmap) {
        QFile qf(img_str);
        if (!qf.exists()) {
            QString placeholderPath = appDir + "/assets/obicons/unit_placeholder_trans.png";
            QString placeholderKey = placeholderPath + QString::number(iconWidth) + QString::number(iconHeight);
            iconPixmap = imageCache.object(placeholderKey);
            if (!iconPixmap) {
                QPixmap placeholder(placeholderPath);
                if (placeholder.isNull()) {
                    placeholder = QPixmap(iconWidth, iconHeight);
                    placeholder.fill(Qt::transparent);
                } else {
                    placeholder = placeholder.scaled(iconWidth, iconHeight, Qt::KeepAspectRatio, Qt::SmoothTransformation);
                }
                iconPixmap = new QPixmap(placeholder);
                imageCache.insert(placeholderKey, iconPixmap);
            }
        } else {
            QPixmap originalPixmap(img_str);
            QPixmap scaledPixmap = originalPixmap.scaled(iconWidth, iconHeight, Qt::KeepAspectRatio, Qt::SmoothTransformation);
            iconPixmap = new QPixmap(scaledPixmap);
            imageCache.insert(cacheKey, iconPixmap);
        }
    }
    if (iconPixmap && !iconPixmap->isNull()) {
        // 圆角处理
        QPixmap radiusIcon(iconWidth, iconHeight);
        radiusIcon.fill(Qt::transparent);
        QPainter iconPainter(&radiusIcon);
        iconPainter.setRenderHint(QPainter::Antialiasing);
        QPainterPath iconPath;
        iconPath.addRoundedRect(QRect(0, 0, iconWidth, iconHeight), 6 * ratio, 6 * ratio);
        iconPainter.setClipPath(iconPath);
        iconPainter.drawPixmap(0, 0, *iconPixmap);
        iconPainter.end();
        int iconX = (w - iconWidth) / 2;
        int iconY = qRound(5 * ratio);
        painter.drawPixmap(iconX, iconY, radiusIcon);
    }

    // 3. 绘制进度条
    int progressBarHeight = qRound(6 * ratio);
    int progressBarWidth = w - qRound(10 * ratio);
    int progressBarX = qRound(5 * ratio);
    int progressBarY = h - progressBarHeight - qRound(5 * ratio);
    // 进度条背景
    QColor progressBgColor(50, 50, 50, 250);
    painter.fillRect(QRect(progressBarX, progressBarY, progressBarWidth, progressBarHeight), progressBgColor);
    // 进度
    int maxProgress = 54; // 根据tagBuildingNode的注释，最大进度为54
    float progressRatio = static_cast<float>(blockProgress) / maxProgress;
    int filledWidth = qRound(progressBarWidth * progressRatio);
    painter.fillRect(QRect(progressBarX, progressBarY, filledWidth, progressBarHeight), blockColor);

    // 4. 状态文本（底部居中）
    QString statusText;
    if (blockStatus == "Ready") {
        statusText = "已 完 成";  // 手动增加空格以增加间距
    } else if (blockStatus == "On Hold") {
        statusText = "已 暂 停";  // 手动增加空格以增加间距
    } else if (!blockStatus.isEmpty()) {
        statusText = "建 造 中";  // 手动增加空格以增加间距
    }
    
    // 使用MiSans-Bold字体，加粗并设置字间距
    QFont miSansFont("MiSans-Bold", qRound(9 * ratio), QFont::Bold);
    miSansFont.setLetterSpacing(QFont::AbsoluteSpacing, qRound(1.2 * ratio)); // 增加字符间距
    painter.setFont(miSansFont);
    
    QFontMetrics fm(miSansFont);
    int textWidth = fm.horizontalAdvance(statusText);
    int textX = (w - textWidth) / 2;
    int textY = h - qRound(15 * ratio);
    
    // 使用白色绘制状态文本，不添加描边
    painter.save();
    painter.setPen(Qt::white);
    painter.drawText(textX, textY, statusText);
    painter.restore();
    
    // 5. 如果数量大于1，绘制数量
    if (blockNumber > 1) {
        // 设置数量文本的字体 - 使用LLDEtechnoGlitch特效字体
        QFont technoFont;
        
        // 使用保存的实际字体族名称，如果没有则尝试直接使用字体文件名
        if (!technoGlitchFontFamily.isEmpty()) {
            technoFont = QFont(technoGlitchFontFamily, qRound(14 * ratio), QFont::Bold);
        } else {
            // 如果没有保存的字体族名称，使用默认的LLDEtechnoGlitch-Bold0
            technoFont = QFont("LLDEtechnoGlitch-Bold0", qRound(14 * ratio), QFont::Bold);
        }
        
        painter.setFont(technoFont);
        
        // 绘制数量文本
        QString numberText = "x" + QString::number(blockNumber);
        QFontMetrics technoFm(technoFont);
        int numberWidth = technoFm.horizontalAdvance(numberText);
        int numberX = w - numberWidth - qRound(5 * ratio);
        int numberY = qRound(18 * ratio);
        
        // 绘制带有描边的数量文本
        painter.save();
        
        // 先绘制黑色描边
        painter.setPen(Qt::black);
        for (int dx = -1; dx <= 1; ++dx) {
            for (int dy = -1; dy <= 1; ++dy) {
                if (dx != 0 || dy != 0) {
                    painter.drawText(numberX + dx, numberY + dy, numberText);
                }
            }
        }
        
        // 再绘制白色文本
        painter.setPen(Qt::white);
        painter.drawText(numberX, numberY, numberText);
        painter.restore();
    }
}

void ProducingBlock::initBlock(QString name) {
    blockName = name;
    setImage(name);
}

void ProducingBlock::setProgress(int progress) { 
    blockProgress = progress; 
    
    // 强制重绘以确保进度条以正确的比例显示
    this->update();
}

void ProducingBlock::setStatus(int status) {
    if (status == 0) {
        blockStatus = "Building";
    } else if (status == 1) {
        blockStatus = "Ready";
    } else if (status == 2) {
        blockStatus = "On Hold";
    } else {
        blockStatus = "";
    }
    update();  // 触发重绘
}

void ProducingBlock::setNumber(int number) { 
    blockNumber = number; 
    update();  // 触发重绘
}

void ProducingBlock::setImage(QString name) {
    // 不再需要设置 ui->lb_img，因为现在直接在 paintEvent 中绘制图像
    // 但仍然需要处理缓存
    
    blockName = name;
    
    // 预加载图像到缓存
    QString appDir = QCoreApplication::applicationDirPath();
    QString img_str = appDir + "/assets/obicons/" + name + ".png";
    float ratio = gls->l.ratio;
    int iconWidth = qRound(60 * ratio);
    int iconHeight = qRound(60 * ratio);
    QString cacheKey = img_str + QString::number(iconWidth) + QString::number(iconHeight);
    
    if (!imageCache.contains(cacheKey)) {
        QFile qf(img_str);
        if (!qf.exists()) {
            QString placeholderPath = appDir + "/assets/obicons/unit_placeholder_trans.png";
            QString placeholderKey = placeholderPath + QString::number(iconWidth) + QString::number(iconHeight);
            if (!imageCache.contains(placeholderKey)) {
                QPixmap placeholder(placeholderPath);
                if (placeholder.isNull()) {
                    placeholder = QPixmap(iconWidth, iconHeight);
                    placeholder.fill(Qt::transparent);
                } else {
                    placeholder = placeholder.scaled(iconWidth, iconHeight, Qt::KeepAspectRatio, Qt::SmoothTransformation);
                }
                imageCache.insert(placeholderKey, new QPixmap(placeholder));
            }
        } else {
            QPixmap originalPixmap(img_str);
            QPixmap scaledPixmap = originalPixmap.scaled(iconWidth, iconHeight, Qt::KeepAspectRatio, Qt::SmoothTransformation);
            imageCache.insert(cacheKey, new QPixmap(scaledPixmap));
        }
    }
    
    update();  // 触发重绘
}

void ProducingBlock::setcolor(std::string color) {
    blockColor = QString::fromStdString("#" + color);
}

QColor ProducingBlock::getDarkerColor(QColor qc) {
    int red   = qc.red();
    int green = qc.green();
    int blue  = qc.blue();

    int delta = 50;

    int darkerRed   = qMax(0, red - delta);
    int darkerGreen = qMax(0, green - delta);
    int darkerBlue  = qMax(0, blue - delta);

    return QColor(darkerRed, darkerGreen, darkerBlue);
}

