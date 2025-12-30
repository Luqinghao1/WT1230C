/*
 * 文件名: plottingdialog3.cpp
 * 文件作用: 试井分析曲线配置对话框实现
 * 功能描述:
 * 1. 实现了对话框的初始化，设置默认值为标准的双对数曲线配置（压差 & 导数）。
 * 2. 实现了试井类型（降落/恢复）的逻辑切换。
 * 3. 提供了从UI控件获取试井参数（Pi, TestType）的具体实现。
 */

#include "plottingdialog3.h"
#include "ui_plottingdialog3.h"
#include <QColorDialog>

// 初始化静态计数器
int PlottingDialog3::s_counter = 1;

// 构造函数实现
PlottingDialog3::PlottingDialog3(QStandardItemModel* model, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::PlottingDialog3),
    m_dataModel(model),
    m_pressPointColor(Qt::red),    // 默认压差点颜色：红
    m_pressLineColor(Qt::red),     // 默认压差线颜色：红
    m_derivPointColor(Qt::blue),   // 默认导数点颜色：蓝
    m_derivLineColor(Qt::blue)     // 默认导数线颜色：蓝
{
    ui->setupUi(this);

    // 设置默认值
    ui->lineName->setText(QString("试井分析 %1").arg(s_counter++));

    // 默认图例名称
    ui->linePressLegend->setText("Delta P");
    ui->lineDerivLegend->setText("Derivative");

    // 设置坐标轴默认标签
    ui->lineXLabel->setText("dt (h)");
    ui->lineYLabel->setText("Delta P / Derivative (MPa)");

    // 初始化下拉框内容（列名）
    populateComboBoxes();
    // 初始化样式选项
    setupStyleOptions();

    // 默认选中“压力降落”试井
    ui->radioDrawdown->setChecked(true);

    // 连接信号与槽

    // 1. 平滑复选框切换
    connect(ui->checkSmooth, &QCheckBox::toggled, this, &PlottingDialog3::onSmoothToggled);
    onSmoothToggled(ui->checkSmooth->isChecked()); // 初始化状态

    // 2. 试井类型切换（控制地层压力输入框）
    connect(ui->radioDrawdown, &QRadioButton::toggled, this, &PlottingDialog3::onTestTypeChanged);
    connect(ui->radioBuildup, &QRadioButton::toggled, this, &PlottingDialog3::onTestTypeChanged);
    onTestTypeChanged(); // 初始化状态

    // 3. 颜色按钮点击信号
    connect(ui->btnPressPointColor, &QPushButton::clicked, this, &PlottingDialog3::selectPressPointColor);
    connect(ui->btnPressLineColor, &QPushButton::clicked, this, &PlottingDialog3::selectPressLineColor);
    connect(ui->btnDerivPointColor, &QPushButton::clicked, this, &PlottingDialog3::selectDerivPointColor);
    connect(ui->btnDerivLineColor, &QPushButton::clicked, this, &PlottingDialog3::selectDerivLineColor);
}

// 析构函数实现
PlottingDialog3::~PlottingDialog3()
{
    delete ui;
}

// 填充列选择下拉框
void PlottingDialog3::populateComboBoxes()
{
    if (!m_dataModel) return;
    QStringList headers;
    // 遍历模型的水平表头，获取列名
    for(int i=0; i<m_dataModel->columnCount(); ++i) {
        QStandardItem* item = m_dataModel->horizontalHeaderItem(i);
        headers << (item ? item->text() : QString("列 %1").arg(i+1));
    }
    // 将列名添加到下拉框中
    ui->comboTime->addItems(headers);
    ui->comboPress->addItems(headers);
}

// 设置样式选项（点形、线型）
void PlottingDialog3::setupStyleOptions()
{
    // Lambda函数：添加点形状选项
    auto addShapes = [](QComboBox* box) {
        box->addItem("实心圆 (Disc)", (int)QCPScatterStyle::ssDisc);
        box->addItem("空心圆 (Circle)", (int)QCPScatterStyle::ssCircle);
        box->addItem("三角形 (Triangle)", (int)QCPScatterStyle::ssTriangle);
        box->addItem("菱形 (Diamond)", (int)QCPScatterStyle::ssDiamond);
        box->addItem("无 (None)", (int)QCPScatterStyle::ssNone);
    };
    // Lambda函数：添加线型选项
    auto addLines = [](QComboBox* box) {
        box->addItem("实线 (Solid)", (int)Qt::SolidLine);
        box->addItem("虚线 (Dash)", (int)Qt::DashLine);
        box->addItem("无 (None)", (int)Qt::NoPen);
    };

    // 为各个下拉框添加选项
    addShapes(ui->comboPressShape);
    addLines(ui->comboPressLine);
    addShapes(ui->comboDerivShape);
    addLines(ui->comboDerivLine);

    // 设置默认选中项
    ui->comboPressLine->setCurrentIndex(2); // 压力默认无连线 (NoPen)
    ui->comboDerivShape->setCurrentIndex(2); // 导数默认三角形 (Triangle)
    ui->comboDerivLine->setCurrentIndex(2); // 导数默认无连线 (NoPen)

    // 更新颜色按钮的初始颜色显示
    updateColorButton(ui->btnPressPointColor, m_pressPointColor);
    updateColorButton(ui->btnPressLineColor, m_pressLineColor);
    updateColorButton(ui->btnDerivPointColor, m_derivPointColor);
    updateColorButton(ui->btnDerivLineColor, m_derivLineColor);
}

// 平滑选项切换槽函数
void PlottingDialog3::onSmoothToggled(bool checked)
{
    ui->spinSmooth->setEnabled(checked);
}

// 试井类型切换槽函数
void PlottingDialog3::onTestTypeChanged()
{
    // 只有在“压力降落”试井时，才需要用户输入地层初始压力 Pi
    bool isDrawdown = ui->radioDrawdown->isChecked();
    ui->spinPi->setEnabled(isDrawdown);
    ui->labelPi->setEnabled(isDrawdown);
}

// 更新颜色按钮样式表
void PlottingDialog3::updateColorButton(QPushButton* btn, const QColor& color) {
    btn->setStyleSheet(QString("background-color: %1; border: 1px solid #555; border-radius: 3px;").arg(color.name()));
}

// 颜色选择槽函数实现
void PlottingDialog3::selectPressPointColor() {
    QColor c = QColorDialog::getColor(m_pressPointColor, this);
    if(c.isValid()) { m_pressPointColor = c; updateColorButton(ui->btnPressPointColor, c); }
}
void PlottingDialog3::selectPressLineColor() {
    QColor c = QColorDialog::getColor(m_pressLineColor, this);
    if(c.isValid()) { m_pressLineColor = c; updateColorButton(ui->btnPressLineColor, c); }
}
void PlottingDialog3::selectDerivPointColor() {
    QColor c = QColorDialog::getColor(m_derivPointColor, this);
    if(c.isValid()) { m_derivPointColor = c; updateColorButton(ui->btnDerivPointColor, c); }
}
void PlottingDialog3::selectDerivLineColor() {
    QColor c = QColorDialog::getColor(m_derivLineColor, this);
    if(c.isValid()) { m_derivLineColor = c; updateColorButton(ui->btnDerivLineColor, c); }
}

// --- Getters: 获取界面控件的值 ---
QString PlottingDialog3::getCurveName() const { return ui->lineName->text(); }
QString PlottingDialog3::getPressLegend() const { return ui->linePressLegend->text(); }
QString PlottingDialog3::getDerivLegend() const { return ui->lineDerivLegend->text(); }
int PlottingDialog3::getTimeColumn() const { return ui->comboTime->currentIndex(); }
int PlottingDialog3::getPressureColumn() const { return ui->comboPress->currentIndex(); }

// 获取试井类型
PlottingDialog3::TestType PlottingDialog3::getTestType() const {
    if (ui->radioDrawdown->isChecked()) return Drawdown;
    return Buildup;
}

// 获取地层初始压力
double PlottingDialog3::getInitialPressure() const {
    return ui->spinPi->value();
}

double PlottingDialog3::getLSpacing() const { return ui->spinL->value(); }
bool PlottingDialog3::isSmoothEnabled() const { return ui->checkSmooth->isChecked(); }
int PlottingDialog3::getSmoothFactor() const { return ui->spinSmooth->value(); }
QString PlottingDialog3::getXLabel() const { return ui->lineXLabel->text(); }
QString PlottingDialog3::getYLabel() const { return ui->lineYLabel->text(); }

QCPScatterStyle::ScatterShape PlottingDialog3::getPressShape() const { return (QCPScatterStyle::ScatterShape)ui->comboPressShape->currentData().toInt(); }
QColor PlottingDialog3::getPressPointColor() const { return m_pressPointColor; }
Qt::PenStyle PlottingDialog3::getPressLineStyle() const { return (Qt::PenStyle)ui->comboPressLine->currentData().toInt(); }
QColor PlottingDialog3::getPressLineColor() const { return m_pressLineColor; }

QCPScatterStyle::ScatterShape PlottingDialog3::getDerivShape() const { return (QCPScatterStyle::ScatterShape)ui->comboDerivShape->currentData().toInt(); }
QColor PlottingDialog3::getDerivPointColor() const { return m_derivPointColor; }
Qt::PenStyle PlottingDialog3::getDerivLineStyle() const { return (Qt::PenStyle)ui->comboDerivLine->currentData().toInt(); }
QColor PlottingDialog3::getDerivLineColor() const { return m_derivLineColor; }

bool PlottingDialog3::isNewWindow() const { return ui->checkNewWindow->isChecked(); }
