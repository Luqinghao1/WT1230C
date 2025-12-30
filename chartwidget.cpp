/*
 * chartwidget.cpp
 * 文件作用: 通用图表组件实现文件 (修复优化版)
 * 功能描述:
 * 1. 实现了图表布局的核心逻辑 (setChartMode)，接管了原 PlottingWidget 的布局工作。
 * 2. 实现了设置对话框的分发逻辑。
 * 3. [修改] 强制所有图表元素（标题、坐标轴、图例）颜色为黑色。
 * 4. [修改] 单图模式下开启四周边框（Top/Right轴显示但不显示数值）。
 * 5. [修改] 坐标轴数字格式设为 "g"，只保留有效数字。
 */

#include "chartwidget.h"
#include "ui_chartwidget.h"
#include "chartsetting1.h"
#include "chartsetting2.h"
#include <QFileDialog>
#include <QMessageBox>
#include <QDebug>

ChartWidget::ChartWidget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::ChartWidget),
    m_titleElement(nullptr),
    m_currentMode(Mode_Single),
    m_topRect(nullptr),
    m_bottomRect(nullptr)
{
    ui->setupUi(this);

    // 初始化默认为单图模式
    setChartMode(Mode_Single);
}

ChartWidget::~ChartWidget()
{
    delete ui;
}

MouseZoom* ChartWidget::getPlot()
{
    return ui->chartArea;
}

void ChartWidget::setTitle(const QString& title)
{
    if (m_titleElement) {
        m_titleElement->setText(title);
        // 确保标题颜色为黑色
        m_titleElement->setTextColor(Qt::black);
    }
}

// 辅助函数：统一设置坐标轴样式（黑色、数字格式、有效数字）
void setupAxisStyle(QCPAxis* axis) {
    // 颜色设置
    axis->setBasePen(QPen(Qt::black));
    axis->setTickPen(QPen(Qt::black));
    axis->setSubTickPen(QPen(Qt::black));
    axis->setLabelColor(Qt::black);
    axis->setTickLabelColor(Qt::black);

    // 数字格式设置：使用 "g" 格式，自动去除无效零，保留有效数字
    // "g" 格式根据数值大小自动选择常规或科学计数，通常最符合“有效数字”需求
    axis->setNumberFormat("g");
    axis->setNumberPrecision(5); // 设置有效数字位数

    // 开启子网格
    axis->grid()->setSubGridVisible(true);
}

// 核心函数：根据模式重建图表布局
void ChartWidget::setChartMode(ChartMode mode)
{
    m_currentMode = mode;
    MouseZoom* plot = ui->chartArea;

    // 1. 清除现有的图表布局
    plot->plotLayout()->clear();
    plot->clearGraphs();

    m_topRect = nullptr;
    m_bottomRect = nullptr;
    m_titleElement = nullptr;

    // 2. 重建标题行 (Row 0)
    plot->plotLayout()->insertRow(0);
    m_titleElement = new QCPTextElement(plot, "Chart Title", QFont("Microsoft YaHei", 12, QFont::Bold));
    m_titleElement->setTextColor(Qt::black); // 标题黑色
    plot->plotLayout()->addElement(0, 0, m_titleElement);

    if (mode == Mode_Single) {
        // --- 单图模式 (Mode_Single) ---
        QCPAxisRect* rect = new QCPAxisRect(plot);
        plot->plotLayout()->addElement(1, 0, rect);

        // 应用样式到 Bottom 和 Left
        setupAxisStyle(rect->axis(QCPAxis::atBottom));
        setupAxisStyle(rect->axis(QCPAxis::atLeft));

        // [修改] 实现四周边框效果
        // 启用 Top 和 Right 轴，但不显示数值，只显示刻度线（作为边框）
        QCPAxis* top = rect->axis(QCPAxis::atTop);
        QCPAxis* right = rect->axis(QCPAxis::atRight);

        top->setVisible(true);
        top->setTickLabels(false); // 不显示数字
        setupAxisStyle(top);       // 设置为黑色

        right->setVisible(true);
        right->setTickLabels(false);
        setupAxisStyle(right);

        // 连接信号，实现 Top/Right 轴跟随 Bottom/Left 变化
        connect(rect->axis(QCPAxis::atBottom), SIGNAL(rangeChanged(QCPRange)), top, SLOT(setRange(QCPRange)));
        connect(rect->axis(QCPAxis::atLeft), SIGNAL(rangeChanged(QCPRange)), right, SLOT(setRange(QCPRange)));

        // 创建图例
        QCPLegend *newLegend = new QCPLegend;
        rect->insetLayout()->addElement(newLegend, Qt::AlignRight | Qt::AlignTop);
        newLegend->setLayer("legend");
        newLegend->setTextColor(Qt::black); // 图例文字黑色
        newLegend->setBorderPen(QPen(Qt::black)); // 图例边框黑色

        plot->legend = newLegend;
        plot->legend->setVisible(true);
        plot->legend->setFont(QFont("Microsoft YaHei", 9));

        // 默认设置为对数坐标（可由外部修改）
        QSharedPointer<QCPAxisTickerLog> logTicker(new QCPAxisTickerLog);
        rect->axis(QCPAxis::atBottom)->setScaleType(QCPAxis::stLogarithmic);
        rect->axis(QCPAxis::atBottom)->setTicker(logTicker);
        rect->axis(QCPAxis::atLeft)->setScaleType(QCPAxis::stLogarithmic);
        rect->axis(QCPAxis::atLeft)->setTicker(logTicker);

        // 注意：对数坐标下通常使用 'eb' 格式，但为了遵循“只保留有效数字”的要求，
        // 如果不需要强制指数显示，可以使用 'g'。如果诊断图必须用科学计数，可单独设置。
        // 这里根据要求，保持 'g' (在 setupAxisStyle 中设置)，如果数值很大或很小会自动转为指数。

    } else if (mode == Mode_Stacked) {
        // --- 堆叠模式 (Mode_Stacked) ---
        m_topRect = new QCPAxisRect(plot);
        m_bottomRect = new QCPAxisRect(plot);

        plot->plotLayout()->addElement(1, 0, m_topRect);
        plot->plotLayout()->addElement(2, 0, m_bottomRect);

        QCPMarginGroup *group = new QCPMarginGroup(plot);
        m_topRect->setMarginGroup(QCP::msLeft | QCP::msRight, group);
        m_bottomRect->setMarginGroup(QCP::msLeft | QCP::msRight, group);

        // 联动
        connect(m_topRect->axis(QCPAxis::atBottom), SIGNAL(rangeChanged(QCPRange)),
                m_bottomRect->axis(QCPAxis::atBottom), SLOT(setRange(QCPRange)));
        connect(m_bottomRect->axis(QCPAxis::atBottom), SIGNAL(rangeChanged(QCPRange)),
                m_topRect->axis(QCPAxis::atBottom), SLOT(setRange(QCPRange)));

        // 应用黑色样式和数字格式
        foreach(QCPAxisRect* r, QList<QCPAxisRect*>() << m_topRect << m_bottomRect) {
            foreach(QCPAxis* ax, r->axes()) {
                setupAxisStyle(ax);
            }
        }

        // 堆叠图通常上方图表不需要显示 X 轴数值
        m_topRect->axis(QCPAxis::atBottom)->setTickLabels(false);

        // 图例
        QCPLegend *newLegend = new QCPLegend;
        m_topRect->insetLayout()->addElement(newLegend, Qt::AlignRight | Qt::AlignTop);
        newLegend->setLayer("legend");
        newLegend->setTextColor(Qt::black);
        newLegend->setBorderPen(QPen(Qt::black));

        plot->legend = newLegend;
        plot->legend->setVisible(true);
        plot->legend->setFont(QFont("Microsoft YaHei", 9));
    }

    plot->setInteractions(QCP::iRangeDrag | QCP::iRangeZoom | QCP::iSelectPlottables);
    plot->replot();
}

ChartWidget::ChartMode ChartWidget::getChartMode() const
{
    return m_currentMode;
}

QCPAxisRect* ChartWidget::getTopRect() const
{
    return m_topRect;
}

QCPAxisRect* ChartWidget::getBottomRect() const
{
    return m_bottomRect;
}

void ChartWidget::on_btnExportImg_clicked()
{
    QString filter = "PNG Image (*.png);;JPEG Image (*.jpg);;PDF Document (*.pdf)";
    QString fileName = QFileDialog::getSaveFileName(this, "导出图片", "", filter);
    if (fileName.isEmpty()) return;

    bool success = false;
    if (fileName.endsWith(".png", Qt::CaseInsensitive))
        success = ui->chartArea->savePng(fileName);
    else if (fileName.endsWith(".jpg", Qt::CaseInsensitive))
        success = ui->chartArea->saveJpg(fileName);
    else if (fileName.endsWith(".pdf", Qt::CaseInsensitive))
        success = ui->chartArea->savePdf(fileName);
    else
        success = ui->chartArea->savePng(fileName + ".png");

    if (!success) {
        QMessageBox::critical(this, "错误", "导出图片失败！");
    } else {
        QMessageBox::information(this, "成功", "图片导出成功。");
    }
}

void ChartWidget::on_btnChartSettings_clicked()
{
    if (m_currentMode == Mode_Stacked && m_topRect && m_bottomRect) {
        ChartSetting2 dlg(ui->chartArea, m_topRect, m_bottomRect, m_titleElement, this);
        dlg.exec();
    } else {
        ChartSetting1 dlg(ui->chartArea, m_titleElement, this);
        dlg.exec();
    }
}

void ChartWidget::on_btnExportData_clicked()
{
    emit exportDataTriggered();
}

void ChartWidget::on_btnResetView_clicked()
{
    ui->chartArea->rescaleAxes();
    // 简单修正
    if(ui->chartArea->xAxis->range().lower <= 0) ui->chartArea->xAxis->setRangeLower(1e-3);
    if(ui->chartArea->yAxis->range().lower <= 0) ui->chartArea->yAxis->setRangeLower(1e-3);
    ui->chartArea->replot();
}
