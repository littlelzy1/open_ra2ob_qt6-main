#include "./unitblock.h"

#include <QFile>
#include <QFontMetrics>
#include <QPainter>
#include <QPainterPath>
#include <QFontDatabase>
#include <QCoreApplication>
#include <QDir>

#include "./layoutsetting.h"
#include "./ui_unitblock.h"

// 添加静态字体族变量
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

Unitblock::Unitblock(QWidget *parent) : QWidget(parent), ui(new Ui::Unitblock) {
    ui->setupUi(this);
    lb_num = new QOutlineLabel(this);

    gls = &Globalsetting::getInstance();

    rearrange();
}

Unitblock::~Unitblock() {
    delete lb_num;
    delete ui;
}

void Unitblock::initUnit(QString name) {
    unit_name = name;
    setImage(name);
}

void Unitblock::paintEvent(QPaintEvent *) {
    QPainter painter(this);

 

    painter.end();
}

void Unitblock::setName(QString name) {
    unit_name = name;
    setImage(name);
}

void Unitblock::setImage(QString name) {
    QString appDir = QCoreApplication::applicationDirPath();
    QString img_str = appDir + "/assets/obicons/" + name + ".png";

    QFile qf(img_str);

    int w = gls->l.unit_w;
    int h = gls->l.unit_h;

    if (!qf.exists()) {
        QPixmap p = QPixmap(appDir + "/assets/obicons/unit_placeholder_trans.png");
        if (p.isNull()) {
            // 创建一个空白图片
            p = QPixmap(w, h);
            p.fill(Qt::transparent);
        }
        p = p.scaled(w, h);
        ui->img->setPixmap(p);
        ui->img->setGeometry(0, 0, w, h);
        return;
    }

    QPixmap p = QPixmap(img_str);

    p = p.scaled(w, h);
    ui->img->setPixmap(getRadius(p, 8 * gls->l.ratio));
    ui->img->setGeometry(0, 0, w, h);
}

QPixmap Unitblock::getRadius(QPixmap src, int radius) {
    int w = src.width();
    int h = src.height();

    QPixmap dest(w, h);
    dest.fill(Qt::transparent);

    QPainter painter(&dest);

    QPainterPath path;
    QRect rect(0, 0, w, h);
    path.addRoundedRect(rect, radius, radius);
    painter.setClipPath(path);
    painter.drawPixmap(0, 0, w, h, src);

    painter.end();

    return dest;
}

void Unitblock::setNumber(int n) {
    QFont font;
    
    // 使用静态方法加载字体，确保只加载一次
    font.setFamily(loadTechnoGlitchFont());
    
    font.setPointSize(14 * gls->l.ratio); // 稍微减小字体大小，因为特效字体通常看起来更大

    lb_num->setFont(font);
    lb_num->setOutline(Qt::white, QColor(30, 27, 24), 2, true);
    lb_num->setText(QString::number(n));
    lb_num->adjustSize();
    int cX = (this->width() - lb_num->width()) / 2;
    int cY = gls->l.unit_bg_y + gls->l.unit_bg_h - lb_num->height();

    lb_num->setGeometry(cX, cY - 1, lb_num->width(), lb_num->height());
    lb_num->show();

    ui->bg->show();
}

void Unitblock::setColor(std::string color) {
    QString q_color = QString::fromStdString("#" + color);

    QString rad = QString::number(8 * gls->l.ratio);

    ui->bg->setStyleSheet("background-color: " + q_color + ";" + "border-bottom-left-radius:" +
                          rad + "px;border-bottom-right-radius:" + rad + "px;");
    ui->border->setStyleSheet("border: 1px solid " + q_color + ";" + "border-radius: " + rad +
                              "px;");
}

void Unitblock::setEmpty() {
    unit_name = "";
    QString appDir = QCoreApplication::applicationDirPath();
    QString img_str = appDir + "/assets/obicons/unit_placeholder_trans.png";
    QPixmap p(img_str);
    if (p.isNull()) {
        // 创建一个空白图片
        p = QPixmap(gls->l.unit_w, gls->l.unit_h);
        p.fill(Qt::transparent);
    }
    ui->img->setPixmap(p);

    lb_num->hide();
    ui->bg->hide();
}

void Unitblock::rearrange() {
    ui->bg->setGeometry(0, gls->l.unit_bg_y, gls->l.unit_w, gls->l.unit_bg_h);
    ui->border->setGeometry(0, 0, gls->l.unit_w, gls->l.unit_h);
    ui->img->setGeometry(0, 0, gls->l.unit_w, gls->l.unit_h);
}
