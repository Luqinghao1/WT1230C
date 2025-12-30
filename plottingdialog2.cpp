/*
 * 文件名: plottingdialog2.cpp
 * 文件作用: 压力产量分析配置对话框实现文件
 * 功能描述:
 * 1. 初始化界面控件，加载数据模型中的列名。
 * 2. 初始化点形状、线型等样式选项，设置颜色选择逻辑。
 * 3. 实现数据获取接口，返回用户配置的各项参数。
 */

#include "plottingdialog2.h"
#include "ui_plottingdialog2.h"
#include <QColorDialog>

int PlottingDialog2::s_counter = 1;

PlottingDialog2::PlottingDialog2(QStandardItemModel* model, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::PlottingDialog2),
    m_dataModel(model),
    m_pressPointColor(Qt::red),
    m_pressLineColor(Qt::red),
    m_prodColor(Qt::blue)
{
    ui->setupUi(this);

    // 设置默认的图表名称和坐标轴标签
    ui->lineChartName->setText(QString("压力产量分析 %1").arg(s_counter++));
    ui->lineXLabel->setText("Time (h)");
    ui->linePLabel->setText("Pressure (MPa)");
    ui->lineQLabel->setText("Production (m3/d)");

    populateComboBoxes();
    setupStyleOptions();

    // 连接信号槽
    connect(ui->comboPressY, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &PlottingDialog2::onPressYColChanged);
    connect(ui->comboProdY, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &PlottingDialog2::onProdYColChanged);

    connect(ui->btnPressPointColor, &QPushButton::clicked, this, &PlottingDialog2::selectPressPointColor);
    connect(ui->btnPressLineColor, &QPushButton::clicked, this, &PlottingDialog2::selectPressLineColor);
    connect(ui->btnProdColor, &QPushButton::clicked, this, &PlottingDialog2::selectProdColor);

    // 初始触发一次图例更新
    if(ui->comboPressY->count()>0) onPressYColChanged(ui->comboPressY->currentIndex());
    if(ui->comboProdY->count()>0) onProdYColChanged(ui->comboProdY->currentIndex());
}

PlottingDialog2::~PlottingDialog2()
{
    delete ui;
}

void PlottingDialog2::populateComboBoxes()
{
    if (!m_dataModel) return;
    QStringList headers;
    for(int i=0; i<m_dataModel->columnCount(); ++i) {
        QStandardItem* item = m_dataModel->horizontalHeaderItem(i);
        headers << (item ? item->text() : QString("列 %1").arg(i+1));
    }
    ui->comboPressX->addItems(headers);
    ui->comboPressY->addItems(headers);
    ui->comboProdX->addItems(headers);
    ui->comboProdY->addItems(headers);
}

void PlottingDialog2::setupStyleOptions()
{
    // 添加点形状选项
    auto addShapes = [this](QComboBox* box) {
        box->addItem("实心圆 (Disc)", (int)QCPScatterStyle::ssDisc);
        box->addItem("空心圆 (Circle)", (int)QCPScatterStyle::ssCircle);
        box->addItem("正方形 (Square)", (int)QCPScatterStyle::ssSquare);
        box->addItem("三角形 (Triangle)", (int)QCPScatterStyle::ssTriangle);
        box->addItem("无 (None)", (int)QCPScatterStyle::ssNone);
    };
    addShapes(ui->comboPressShape);

    // 添加线型选项
    auto addLines = [this](QComboBox* box) {
        box->addItem("实线 (Solid)", (int)Qt::SolidLine);
        box->addItem("虚线 (Dash)", (int)Qt::DashLine);
        box->addItem("点线 (Dot)", (int)Qt::DotLine);
        box->addItem("无 (None)", (int)Qt::NoPen);
    };
    addLines(ui->comboPressLine);

    // 产量绘图类型选项
    ui->comboProdType->addItem("阶梯图 (Step Chart)", 0);
    ui->comboProdType->addItem("散点图 (Scatter)", 1);
    ui->comboProdType->addItem("折线图 (Line)", 2);

    // 初始化颜色按钮
    updateColorButton(ui->btnPressPointColor, m_pressPointColor);
    updateColorButton(ui->btnPressLineColor, m_pressLineColor);
    updateColorButton(ui->btnProdColor, m_prodColor);

    ui->comboPressLine->setCurrentIndex(3); // 默认压力无连线
}

void PlottingDialog2::updateColorButton(QPushButton* btn, const QColor& color)
{
    btn->setStyleSheet(QString("background-color: %1; border: 1px solid #555; border-radius: 3px;").arg(color.name()));
}

void PlottingDialog2::selectPressPointColor() {
    QColor c = QColorDialog::getColor(m_pressPointColor, this);
    if(c.isValid()) { m_pressPointColor = c; updateColorButton(ui->btnPressPointColor, c); }
}
void PlottingDialog2::selectPressLineColor() {
    QColor c = QColorDialog::getColor(m_pressLineColor, this);
    if(c.isValid()) { m_pressLineColor = c; updateColorButton(ui->btnPressLineColor, c); }
}
void PlottingDialog2::selectProdColor() {
    QColor c = QColorDialog::getColor(m_prodColor, this);
    if(c.isValid()) { m_prodColor = c; updateColorButton(ui->btnProdColor, c); }
}

void PlottingDialog2::onPressYColChanged(int index) {
    if(index>=0) ui->linePressLegend->setText(ui->comboPressY->itemText(index));
}
void PlottingDialog2::onProdYColChanged(int index) {
    if(index>=0) ui->lineProdLegend->setText(ui->comboProdY->itemText(index));
}

// 获取用户配置
QString PlottingDialog2::getChartName() const { return ui->lineChartName->text(); }
QString PlottingDialog2::getPressLegend() const { return ui->linePressLegend->text(); }
int PlottingDialog2::getPressXCol() const { return ui->comboPressX->currentIndex(); }
int PlottingDialog2::getPressYCol() const { return ui->comboPressY->currentIndex(); }
QCPScatterStyle::ScatterShape PlottingDialog2::getPressShape() const { return (QCPScatterStyle::ScatterShape)ui->comboPressShape->currentData().toInt(); }
QColor PlottingDialog2::getPressPointColor() const { return m_pressPointColor; }
Qt::PenStyle PlottingDialog2::getPressLineStyle() const { return (Qt::PenStyle)ui->comboPressLine->currentData().toInt(); }
QColor PlottingDialog2::getPressLineColor() const { return m_pressLineColor; }

QString PlottingDialog2::getProdLegend() const { return ui->lineProdLegend->text(); }
int PlottingDialog2::getProdXCol() const { return ui->comboProdX->currentIndex(); }
int PlottingDialog2::getProdYCol() const { return ui->comboProdY->currentIndex(); }
int PlottingDialog2::getProdGraphType() const { return ui->comboProdType->currentData().toInt(); }
QColor PlottingDialog2::getProdColor() const { return m_prodColor; }

QString PlottingDialog2::getXLabel() const { return ui->lineXLabel->text(); }
QString PlottingDialog2::getPLabel() const { return ui->linePLabel->text(); }
QString PlottingDialog2::getQLabel() const { return ui->lineQLabel->text(); }

bool PlottingDialog2::isNewWindow() const { return ui->checkNewWindow->isChecked(); }
