/*
 * 文件名: plottingdialog4.cpp
 * 文件作用: 曲线样式管理/编辑对话框实现
 * 功能描述:
 * 1. 实现了界面控件的初始化（下拉框填充、颜色按钮更新）。
 * 2. 实现了根据是否包含第二条曲线来显示或隐藏配置区域。
 * 3. 提供了颜色选择交互逻辑。
 */

#include "plottingdialog4.h"
#include "ui_plottingdialog4.h"
#include <QColorDialog>

PlottingDialog4::PlottingDialog4(QStandardItemModel* model, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::PlottingDialog4),
    m_dataModel(model)
{
    ui->setupUi(this);
    populateComboBoxes();

    connect(ui->btnColor1, &QPushButton::clicked, this, &PlottingDialog4::onBtnColor1Clicked);
    connect(ui->btnLineColor1, &QPushButton::clicked, this, &PlottingDialog4::onBtnLineColor1Clicked);
    connect(ui->btnColor2, &QPushButton::clicked, this, &PlottingDialog4::onBtnColor2Clicked);
    connect(ui->btnLineColor2, &QPushButton::clicked, this, &PlottingDialog4::onBtnLineColor2Clicked);
}

PlottingDialog4::~PlottingDialog4()
{
    delete ui;
}

void PlottingDialog4::populateComboBoxes()
{
    // 填充列选择
    if(m_dataModel) {
        QStringList headers;
        for(int i=0; i<m_dataModel->columnCount(); ++i) {
            headers << m_dataModel->headerData(i, Qt::Horizontal).toString();
        }
        ui->comboX1->addItems(headers);
        ui->comboY1->addItems(headers);
    }

    // 填充点形
    auto addShapes = [](QComboBox* box){
        box->addItem("实心圆", (int)QCPScatterStyle::ssDisc);
        box->addItem("空心圆", (int)QCPScatterStyle::ssCircle);
        box->addItem("三角形", (int)QCPScatterStyle::ssTriangle);
        box->addItem("菱形", (int)QCPScatterStyle::ssDiamond);
        box->addItem("正方形", (int)QCPScatterStyle::ssSquare);
        box->addItem("无", (int)QCPScatterStyle::ssNone);
    };
    addShapes(ui->comboShape1);
    addShapes(ui->comboShape2);

    // 填充线型
    auto addLines = [](QComboBox* box){
        box->addItem("实线", (int)Qt::SolidLine);
        box->addItem("虚线", (int)Qt::DashLine);
        box->addItem("点线", (int)Qt::DotLine);
        box->addItem("无", (int)Qt::NoPen);
    };
    addLines(ui->comboLineStyle1);
    addLines(ui->comboLineStyle2);
}

void PlottingDialog4::setInitialData(bool hasSecondCurve,
                                     QString name1, int xCol, int yCol,
                                     QCPScatterStyle::ScatterShape shape1, QColor c1, Qt::PenStyle ls1, QColor lc1,
                                     QString name2,
                                     QCPScatterStyle::ScatterShape shape2, QColor c2,
                                     Qt::PenStyle ls2, QColor lc2)
{
    // 设置主曲线
    ui->lineLegend1->setText(name1);
    ui->comboX1->setCurrentIndex(xCol);
    ui->comboY1->setCurrentIndex(yCol);

    int shapeIdx1 = ui->comboShape1->findData((int)shape1);
    if(shapeIdx1 >= 0) ui->comboShape1->setCurrentIndex(shapeIdx1);

    int lineIdx1 = ui->comboLineStyle1->findData((int)ls1);
    if(lineIdx1 >= 0) ui->comboLineStyle1->setCurrentIndex(lineIdx1);

    m_color1 = c1; updateColorButton(ui->btnColor1, m_color1);
    m_lineColor1 = lc1; updateColorButton(ui->btnLineColor1, m_lineColor1);

    // 设置副曲线
    if(hasSecondCurve) {
        ui->groupCurve2->setVisible(true);
        ui->lineLegend2->setText(name2);

        int shapeIdx2 = ui->comboShape2->findData((int)shape2);
        if(shapeIdx2 >= 0) ui->comboShape2->setCurrentIndex(shapeIdx2);

        int lineIdx2 = ui->comboLineStyle2->findData((int)ls2);
        if(lineIdx2 >= 0) ui->comboLineStyle2->setCurrentIndex(lineIdx2);

        m_color2 = c2; updateColorButton(ui->btnColor2, m_color2);
        m_lineColor2 = lc2; updateColorButton(ui->btnLineColor2, m_lineColor2);
    } else {
        ui->groupCurve2->setVisible(false);
    }
}

void PlottingDialog4::updateColorButton(QPushButton* btn, const QColor& c) {
    btn->setStyleSheet(QString("background-color: %1; border: 1px solid #555; border-radius: 3px;").arg(c.name()));
}

void PlottingDialog4::onBtnColor1Clicked() {
    QColor c = QColorDialog::getColor(m_color1, this);
    if(c.isValid()) { m_color1 = c; updateColorButton(ui->btnColor1, c); }
}
void PlottingDialog4::onBtnLineColor1Clicked() {
    QColor c = QColorDialog::getColor(m_lineColor1, this);
    if(c.isValid()) { m_lineColor1 = c; updateColorButton(ui->btnLineColor1, c); }
}
void PlottingDialog4::onBtnColor2Clicked() {
    QColor c = QColorDialog::getColor(m_color2, this);
    if(c.isValid()) { m_color2 = c; updateColorButton(ui->btnColor2, c); }
}
void PlottingDialog4::onBtnLineColor2Clicked() {
    QColor c = QColorDialog::getColor(m_lineColor2, this);
    if(c.isValid()) { m_lineColor2 = c; updateColorButton(ui->btnLineColor2, c); }
}

// Getters
QString PlottingDialog4::getLegendName1() const { return ui->lineLegend1->text(); }
int PlottingDialog4::getXColumn() const { return ui->comboX1->currentIndex(); }
int PlottingDialog4::getYColumn() const { return ui->comboY1->currentIndex(); }
QCPScatterStyle::ScatterShape PlottingDialog4::getPointShape1() const { return (QCPScatterStyle::ScatterShape)ui->comboShape1->currentData().toInt(); }
QColor PlottingDialog4::getPointColor1() const { return m_color1; }
Qt::PenStyle PlottingDialog4::getLineStyle1() const { return (Qt::PenStyle)ui->comboLineStyle1->currentData().toInt(); }
QColor PlottingDialog4::getLineColor1() const { return m_lineColor1; }

QString PlottingDialog4::getLegendName2() const { return ui->lineLegend2->text(); }
QCPScatterStyle::ScatterShape PlottingDialog4::getPointShape2() const { return (QCPScatterStyle::ScatterShape)ui->comboShape2->currentData().toInt(); }
QColor PlottingDialog4::getPointColor2() const { return m_color2; }
Qt::PenStyle PlottingDialog4::getLineStyle2() const { return (Qt::PenStyle)ui->comboLineStyle2->currentData().toInt(); }
QColor PlottingDialog4::getLineColor2() const { return m_lineColor2; }
