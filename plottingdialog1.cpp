/*
 * 文件名: plottingdialog1.cpp
 * 文件作用: 新建单一曲线配置对话框实现
 * 功能描述:
 * 1. 实现数据列加载。
 * 2. 实现点击按钮弹出 QColorDialog 进行颜色选择的逻辑。
 * 3. 实现点形状的完整列表填充。
 */

#include "plottingdialog1.h"
#include "ui_plottingdialog1.h"
#include <QColorDialog>

// 初始化静态计数器
int PlottingDialog1::s_curveCounter = 1;

PlottingDialog1::PlottingDialog1(QStandardItemModel* model, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::PlottingDialog1),
    m_dataModel(model),
    m_pointColor(Qt::red), // 默认红色
    m_lineColor(Qt::red)
{
    ui->setupUi(this);

    // 1. 设置默认名称 (曲线 1, 曲线 2...)
    ui->lineEdit_Name->setText(QString("曲线 %1").arg(s_curveCounter++));

    // 2. 初始化UI内容
    populateComboBoxes();
    setupStyleOptions();

    // 3. 信号连接
    connect(ui->combo_XCol, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &PlottingDialog1::onXColumnChanged);
    connect(ui->combo_YCol, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &PlottingDialog1::onYColumnChanged);

    // 颜色按钮点击事件
    connect(ui->btn_PointColor, &QPushButton::clicked, this, &PlottingDialog1::selectPointColor);
    connect(ui->btn_LineColor, &QPushButton::clicked, this, &PlottingDialog1::selectLineColor);

    // 4. 初始触发一次以填充默认标签
    if (ui->combo_XCol->count() > 0) onXColumnChanged(ui->combo_XCol->currentIndex());
    if (ui->combo_YCol->count() > 0) onYColumnChanged(ui->combo_YCol->currentIndex());
}

PlottingDialog1::~PlottingDialog1()
{
    delete ui;
}

void PlottingDialog1::populateComboBoxes()
{
    if (!m_dataModel) return;
    QStringList headers;
    for(int i=0; i<m_dataModel->columnCount(); ++i) {
        QStandardItem* item = m_dataModel->horizontalHeaderItem(i);
        headers << (item ? item->text() : QString("列 %1").arg(i+1));
    }
    ui->combo_XCol->addItems(headers);
    ui->combo_YCol->addItems(headers);
}

void PlottingDialog1::setupStyleOptions()
{
    // 填充点形状 (统一全集)
    ui->combo_PointShape->addItem("实心圆 (Disc)", (int)QCPScatterStyle::ssDisc);
    ui->combo_PointShape->addItem("空心圆 (Circle)", (int)QCPScatterStyle::ssCircle);
    ui->combo_PointShape->addItem("正方形 (Square)", (int)QCPScatterStyle::ssSquare);
    ui->combo_PointShape->addItem("菱形 (Diamond)", (int)QCPScatterStyle::ssDiamond);
    ui->combo_PointShape->addItem("三角形 (Triangle)", (int)QCPScatterStyle::ssTriangle);
    ui->combo_PointShape->addItem("倒三角 (InvTriangle)", (int)QCPScatterStyle::ssTriangleInverted);
    ui->combo_PointShape->addItem("十字 (Cross)", (int)QCPScatterStyle::ssCross);
    ui->combo_PointShape->addItem("加号 (Plus)", (int)QCPScatterStyle::ssPlus);
    ui->combo_PointShape->addItem("星形 (Star)", (int)QCPScatterStyle::ssStar);
    ui->combo_PointShape->addItem("无 (None)", (int)QCPScatterStyle::ssNone);

    // 填充线型
    ui->combo_LineStyle->addItem("实线 (Solid)", (int)Qt::SolidLine);
    ui->combo_LineStyle->addItem("虚线 (Dash)", (int)Qt::DashLine);
    ui->combo_LineStyle->addItem("点线 (Dot)", (int)Qt::DotLine);
    ui->combo_LineStyle->addItem("点划线 (DashDot)", (int)Qt::DashDotLine);
    ui->combo_LineStyle->addItem("无 (None)", (int)Qt::NoPen);

    // 初始化颜色按钮显示
    updateColorButton(ui->btn_PointColor, m_pointColor);
    updateColorButton(ui->btn_LineColor, m_lineColor);
}

// 辅助：更新按钮背景色以显示当前选择
void PlottingDialog1::updateColorButton(QPushButton* btn, const QColor& color)
{
    QString qss = QString("background-color: %1; border: 1px solid #555; border-radius: 3px;").arg(color.name());
    btn->setStyleSheet(qss);
}

// 弹出调色盘：点颜色
void PlottingDialog1::selectPointColor()
{
    QColor c = QColorDialog::getColor(m_pointColor, this, "选择点颜色");
    if (c.isValid()) {
        m_pointColor = c;
        updateColorButton(ui->btn_PointColor, m_pointColor);
        // 默认线颜色跟随点颜色，除非用户已特意修改（此处简化为跟随）
        m_lineColor = c;
        updateColorButton(ui->btn_LineColor, m_lineColor);
    }
}

// 弹出调色盘：线颜色
void PlottingDialog1::selectLineColor()
{
    QColor c = QColorDialog::getColor(m_lineColor, this, "选择线颜色");
    if (c.isValid()) {
        m_lineColor = c;
        updateColorButton(ui->btn_LineColor, m_lineColor);
    }
}

void PlottingDialog1::onXColumnChanged(int index)
{
    if (index < 0) return;
    ui->lineEdit_XLabel->setText(ui->combo_XCol->itemText(index));
}

void PlottingDialog1::onYColumnChanged(int index)
{
    if (index < 0) return;
    QString colName = ui->combo_YCol->itemText(index);
    ui->lineEdit_YLabel->setText(colName);
    ui->lineEdit_Legend->setText(colName); // 图例默认跟随Y轴列名
}

// Getters
QString PlottingDialog1::getCurveName() const { return ui->lineEdit_Name->text(); }
QString PlottingDialog1::getLegendName() const { return ui->lineEdit_Legend->text(); }
int PlottingDialog1::getXColumn() const { return ui->combo_XCol->currentIndex(); }
int PlottingDialog1::getYColumn() const { return ui->combo_YCol->currentIndex(); }
QString PlottingDialog1::getXLabel() const { return ui->lineEdit_XLabel->text(); }
QString PlottingDialog1::getYLabel() const { return ui->lineEdit_YLabel->text(); }
bool PlottingDialog1::isNewWindow() const { return ui->check_NewWindow->isChecked(); }

QCPScatterStyle::ScatterShape PlottingDialog1::getPointShape() const {
    return (QCPScatterStyle::ScatterShape)ui->combo_PointShape->currentData().toInt();
}
QColor PlottingDialog1::getPointColor() const { return m_pointColor; }
Qt::PenStyle PlottingDialog1::getLineStyle() const {
    return (Qt::PenStyle)ui->combo_LineStyle->currentData().toInt();
}
QColor PlottingDialog1::getLineColor() const { return m_lineColor; }
