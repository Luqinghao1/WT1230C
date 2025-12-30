/*
 * 文件名: wt_plottingwidget.cpp
 * 文件作用: 图表分析主界面实现文件 (修复线型与标签版)
 * 功能描述:
 * 1. 界面布局：左侧功能区与右侧绘图区采用 QSplitter (默认1:4)。
 * 2. 界面样式：统一黑字白底，去除多余分割线。
 * 3. 绘图逻辑修复：
 * - 尊重弹窗中的“线型”选择 (info.lineStyle)，不再强制不连线。
 * - 压力产量/导数分析：坐标轴标签恢复为标准默认值 ("Time", "Pressure" 等)。
 * - 新建曲线：坐标轴标签继续使用列名。
 * 4. 新建窗口修复：确保新建窗口中的图表也能正确显示线型和标签。
 */

#include "wt_plottingwidget.h"
#include "ui_wt_plottingwidget.h"
#include "plottingdialog1.h"
#include "plottingdialog2.h"
#include "plottingdialog3.h"
#include "plottingdialog4.h"
#include "chartwindow.h"
#include "modelparameter.h"
#include "chartsetting1.h"

#include <QMessageBox>
#include <QFileDialog>
#include <QTextStream>
#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QtMath>
#include <QDebug>
#include <QSplitter>

// ============================================================================
// 辅助函数与 CurveInfo 实现
// ============================================================================

QJsonArray vectorToJson(const QVector<double>& vec) {
    QJsonArray arr;
    for(double v : vec) arr.append(v);
    return arr;
}

QVector<double> jsonToVector(const QJsonArray& arr) {
    QVector<double> vec;
    for(const auto& val : arr) vec.append(val.toDouble());
    return vec;
}

QJsonObject CurveInfo::toJson() const {
    QJsonObject obj;
    obj["name"] = name;
    obj["legendName"] = legendName;
    obj["type"] = type;
    obj["xCol"] = xCol;
    obj["yCol"] = yCol;
    obj["xData"] = vectorToJson(xData);
    obj["yData"] = vectorToJson(yData);
    obj["pointShape"] = (int)pointShape;
    obj["pointColor"] = pointColor.name();
    obj["lineStyle"] = (int)lineStyle;
    obj["lineColor"] = lineColor.name();

    if (type == 1) {
        obj["x2Col"] = x2Col;
        obj["y2Col"] = y2Col;
        obj["x2Data"] = vectorToJson(x2Data);
        obj["y2Data"] = vectorToJson(y2Data);
        obj["prodLegendName"] = prodLegendName;
        obj["prodGraphType"] = prodGraphType;
        obj["prodColor"] = prodColor.name();
    }
    else if (type == 2) {
        obj["testType"] = testType;
        obj["initialPressure"] = initialPressure;
        obj["LSpacing"] = LSpacing;
        obj["isSmooth"] = isSmooth;
        obj["smoothFactor"] = smoothFactor;
        obj["derivData"] = vectorToJson(derivData);
        obj["derivShape"] = (int)derivShape;
        obj["derivPointColor"] = derivPointColor.name();
        obj["derivLineStyle"] = (int)derivLineStyle;
        obj["derivLineColor"] = derivLineColor.name();
        obj["prodLegendName"] = prodLegendName;
    }
    return obj;
}

CurveInfo CurveInfo::fromJson(const QJsonObject& json) {
    CurveInfo info;
    info.name = json["name"].toString();
    info.legendName = json["legendName"].toString();
    info.type = json["type"].toInt();
    info.xCol = json["xCol"].toInt(-1);
    info.yCol = json["yCol"].toInt(-1);

    info.xData = jsonToVector(json["xData"].toArray());
    info.yData = jsonToVector(json["yData"].toArray());

    info.pointShape = (QCPScatterStyle::ScatterShape)json["pointShape"].toInt();
    info.pointColor = QColor(json["pointColor"].toString());
    info.lineStyle = (Qt::PenStyle)json["lineStyle"].toInt();
    info.lineColor = QColor(json["lineColor"].toString());

    if (info.type == 1) {
        info.x2Col = json["x2Col"].toInt(-1);
        info.y2Col = json["y2Col"].toInt(-1);
        info.x2Data = jsonToVector(json["x2Data"].toArray());
        info.y2Data = jsonToVector(json["y2Data"].toArray());
        info.prodLegendName = json["prodLegendName"].toString();
        info.prodGraphType = json["prodGraphType"].toInt();
        info.prodColor = QColor(json["prodColor"].toString());
    } else if (info.type == 2) {
        info.testType = json["testType"].toInt(0);
        info.initialPressure = json["initialPressure"].toDouble(0.0);
        info.LSpacing = json["LSpacing"].toDouble();
        info.isSmooth = json["isSmooth"].toBool();
        info.smoothFactor = json["smoothFactor"].toInt();
        info.derivData = jsonToVector(json["derivData"].toArray());
        info.derivShape = (QCPScatterStyle::ScatterShape)json["derivShape"].toInt();
        info.derivPointColor = QColor(json["derivPointColor"].toString());
        info.derivLineStyle = (Qt::PenStyle)json["derivLineStyle"].toInt();
        info.derivLineColor = QColor(json["derivLineColor"].toString());
        info.prodLegendName = json["prodLegendName"].toString();
    }
    return info;
}

// ============================================================================
// WT_PlottingWidget 主类实现
// ============================================================================

WT_PlottingWidget::WT_PlottingWidget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::WT_PlottingWidget),
    m_dataModel(nullptr),
    m_isSelectingForExport(false),
    m_selectionStep(0),
    m_exportStartIndex(0),
    m_exportEndIndex(0),
    m_graphPress(nullptr),
    m_graphProd(nullptr)
{
    ui->setupUi(this);

    // 设置 QSplitter 初始比例 (左侧20% : 右侧80%)
    QList<int> sizes;
    sizes << 200 << 800;
    ui->splitter->setSizes(sizes);
    ui->splitter->setCollapsible(0, false);

    connect(ui->customPlot, &ChartWidget::exportDataTriggered, this, &WT_PlottingWidget::onExportDataTriggered);
    connect(ui->customPlot->getPlot(), &QCustomPlot::plottableClick, this, &WT_PlottingWidget::onGraphClicked);

    ui->customPlot->setChartMode(ChartWidget::Mode_Single);
    ui->customPlot->setTitle("试井分析图表");
}

WT_PlottingWidget::~WT_PlottingWidget()
{
    qDeleteAll(m_openedWindows);
    delete ui;
}

void WT_PlottingWidget::setDataModel(QStandardItemModel* model) { m_dataModel = model; }
void WT_PlottingWidget::setProjectPath(const QString& path) { m_projectPath = path; }

void WT_PlottingWidget::applyDialogStyle(QWidget* dialog) {
    if(!dialog) return;
    // 强制样式：黑字白底，清晰的边框
    QString qss =
        "QWidget { color: black; background-color: white; font-family: 'Microsoft YaHei'; }"
        "QLabel { color: black; }"
        "QGroupBox { color: black; border: 1px solid #dcdcdc; border-radius: 4px; margin-top: 10px; }"
        "QGroupBox::title { subcontrol-origin: margin; subcontrol-position: top left; padding: 0 5px; color: black; }"
        "QPushButton { color: black; background-color: #f0f0f0; border: 1px solid #bfbfbf; border-radius: 3px; padding: 4px 12px; min-width: 60px; }"
        "QPushButton:hover { background-color: #e6e6e6; }"
        "QPushButton:pressed { background-color: #d4d4d4; }"
        "QLineEdit, QSpinBox, QDoubleSpinBox, QComboBox { color: black; background-color: white; border: 1px solid #a0a0a0; padding: 2px; }"
        "QComboBox QAbstractItemView { color: black; background-color: white; selection-background-color: #0078d7; selection-color: white; }"
        "QTabWidget::pane { border: 1px solid #bfbfbf; }"
        "QTabBar::tab { background: #f0f0f0; color: black; padding: 5px 10px; border: 1px solid #bfbfbf; }"
        "QTabBar::tab:selected { background: white; border-bottom-color: white; }";
    dialog->setStyleSheet(qss);
}

void WT_PlottingWidget::loadProjectData()
{
    m_curves.clear();
    ui->listWidget_Curves->clear();
    ui->customPlot->getPlot()->clearGraphs();
    ui->customPlot->getPlot()->replot();
    m_currentDisplayedCurve.clear();

    QJsonArray plots = ModelParameter::instance()->getPlottingData();
    if (plots.isEmpty()) return;

    for (const auto& val : plots) {
        CurveInfo info = CurveInfo::fromJson(val.toObject());
        m_curves.insert(info.name, info);
        ui->listWidget_Curves->addItem(info.name);
    }

    if (ui->listWidget_Curves->count() > 0) {
        on_listWidget_Curves_itemDoubleClicked(ui->listWidget_Curves->item(0));
    }
}

void WT_PlottingWidget::saveProjectData()
{
    if (!ModelParameter::instance()->hasLoadedProject()) {
        QMessageBox msgBox(this);
        msgBox.setWindowTitle("错误");
        msgBox.setText("未加载项目，无法保存。");
        msgBox.setIcon(QMessageBox::Warning);
        msgBox.setStandardButtons(QMessageBox::Ok);
        applyDialogStyle(&msgBox);
        msgBox.exec();
        return;
    }
    QJsonArray curvesArray;
    for(auto it = m_curves.begin(); it != m_curves.end(); ++it) {
        curvesArray.append(it.value().toJson());
    }
    ModelParameter::instance()->savePlottingData(curvesArray);

    QMessageBox msgBox(this);
    msgBox.setWindowTitle("保存");
    msgBox.setText("绘图数据已保存。");
    msgBox.setIcon(QMessageBox::Information);
    msgBox.setStandardButtons(QMessageBox::Ok);
    applyDialogStyle(&msgBox);
    msgBox.exec();
}

// ---------------- 按钮与功能实现 ----------------

// 1. 新建通用曲线
void WT_PlottingWidget::on_btn_NewCurve_clicked()
{
    if(!m_dataModel) return;
    PlottingDialog1 dlg(m_dataModel, this);
    applyDialogStyle(&dlg);

    if(dlg.exec() == QDialog::Accepted) {
        CurveInfo info;
        info.name = dlg.getCurveName();
        info.legendName = dlg.getLegendName();
        info.xCol = dlg.getXColumn(); info.yCol = dlg.getYColumn();
        info.pointShape = dlg.getPointShape(); info.pointColor = dlg.getPointColor();
        info.lineStyle = dlg.getLineStyle(); info.lineColor = dlg.getLineColor();
        info.type = 0;

        // 【标签设置】新建曲线：使用列名
        QString xLabel = m_dataModel->headerData(info.xCol, Qt::Horizontal).toString();
        QString yLabel = m_dataModel->headerData(info.yCol, Qt::Horizontal).toString();

        info.xData.clear(); info.yData.clear();
        for(int i=0; i<m_dataModel->rowCount(); ++i) {
            double xVal = m_dataModel->item(i, info.xCol)->text().toDouble();
            double yVal = m_dataModel->item(i, info.yCol)->text().toDouble();
            if (xVal > 1e-9 && yVal > 1e-9) {
                info.xData.append(xVal);
                info.yData.append(yVal);
            }
        }

        m_curves.insert(info.name, info);
        ui->listWidget_Curves->addItem(info.name);

        if(dlg.isNewWindow()) {
            ChartWindow* w = new ChartWindow();
            w->setWindowTitle(info.name);
            ChartWidget* cw = w->getChartWidget();
            cw->setChartMode(ChartWidget::Mode_Single);
            cw->setTitle(info.name);

            QCPGraph* graph = cw->getPlot()->addGraph();
            graph->setName(info.legendName);
            graph->setData(info.xData, info.yData);
            graph->setScatterStyle(QCPScatterStyle(info.pointShape, info.pointColor, info.pointColor, 6));

            // 【修复】尊重弹窗选择的线型
            graph->setPen(QPen(info.lineColor, 2, info.lineStyle));
            graph->setLineStyle(info.lineStyle == Qt::NoPen ? QCPGraph::lsNone : QCPGraph::lsLine);

            cw->getPlot()->xAxis->setLabel(xLabel);
            cw->getPlot()->yAxis->setLabel(yLabel);

            cw->getPlot()->rescaleAxes();
            cw->getPlot()->replot();

            w->show();
            m_openedWindows.append(w);
        } else {
            ui->customPlot->setChartMode(ChartWidget::Mode_Single);
            ui->customPlot->getPlot()->xAxis->setLabel(xLabel);
            ui->customPlot->getPlot()->yAxis->setLabel(yLabel);

            addCurveToPlot(info);
            m_currentDisplayedCurve = info.name;
        }
    }
}

// 2. 压力产量分析 (双坐标系)
void WT_PlottingWidget::on_btn_PressureRate_clicked()
{
    if(!m_dataModel) return;
    PlottingDialog2 dlg(m_dataModel, this);
    applyDialogStyle(&dlg);

    if(dlg.exec() == QDialog::Accepted) {
        CurveInfo info;
        info.name = dlg.getChartName();
        info.legendName = dlg.getPressLegend();
        info.type = 1;
        info.xCol = dlg.getPressXCol(); info.yCol = dlg.getPressYCol();
        info.x2Col = dlg.getProdXCol(); info.y2Col = dlg.getProdYCol();

        // 【标签设置】压力产量：使用标准默认标签 (解决弹窗输入不一致问题)
        QString pressLabel = "Pressure";
        QString prodLabel = "Production";
        QString timeLabel = "Time";

        for(int i=0; i<m_dataModel->rowCount(); ++i) {
            info.xData.append(m_dataModel->item(i, info.xCol)->text().toDouble());
            info.yData.append(m_dataModel->item(i, info.yCol)->text().toDouble());
            info.x2Data.append(m_dataModel->item(i, info.x2Col)->text().toDouble());
            info.y2Data.append(m_dataModel->item(i, info.y2Col)->text().toDouble());
        }

        info.pointShape = dlg.getPressShape(); info.pointColor = dlg.getPressPointColor();
        info.lineStyle = dlg.getPressLineStyle(); info.lineColor = dlg.getPressLineColor();
        info.prodLegendName = dlg.getProdLegend();
        info.prodGraphType = dlg.getProdGraphType();
        info.prodColor = dlg.getProdColor();

        m_curves.insert(info.name, info);
        ui->listWidget_Curves->addItem(info.name);

        if(dlg.isNewWindow()) {
            ChartWindow* w = new ChartWindow();
            w->setWindowTitle(info.name);
            ChartWidget* cw = w->getChartWidget();
            cw->setChartMode(ChartWidget::Mode_Stacked);
            cw->setTitle(info.name);

            QCPAxisRect* top = cw->getTopRect();
            QCPAxisRect* bottom = cw->getBottomRect();
            MouseZoom* plot = cw->getPlot();

            if (top && bottom) {
                top->axis(QCPAxis::atLeft)->setLabel(pressLabel);
                bottom->axis(QCPAxis::atLeft)->setLabel(prodLabel);
                bottom->axis(QCPAxis::atBottom)->setLabel(timeLabel);

                QCPGraph* gPress = plot->addGraph(top->axis(QCPAxis::atBottom), top->axis(QCPAxis::atLeft));
                gPress->setData(info.xData, info.yData);
                gPress->setName(info.legendName);
                gPress->setScatterStyle(QCPScatterStyle(info.pointShape, info.pointColor, info.pointColor, 6));

                // 【修复】压力曲线：尊重弹窗线型
                gPress->setPen(QPen(info.lineColor, 2, info.lineStyle));
                gPress->setLineStyle(info.lineStyle == Qt::NoPen ? QCPGraph::lsNone : QCPGraph::lsLine);

                QCPGraph* gProd = plot->addGraph(bottom->axis(QCPAxis::atBottom), bottom->axis(QCPAxis::atLeft));
                gProd->setName(info.prodLegendName);
                if(info.prodGraphType == 0) {
                    gProd->setData(info.x2Data, info.y2Data);
                    gProd->setLineStyle(QCPGraph::lsStepLeft); // 阶梯图
                } else {
                    gProd->setData(info.x2Data, info.y2Data);
                    gProd->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssCircle, info.prodColor, info.prodColor, 6));
                    gProd->setLineStyle(QCPGraph::lsNone); // 散点图
                }
                gProd->setPen(QPen(info.prodColor, 2));

                plot->rescaleAxes();
                plot->replot();
            }
            w->show();
            m_openedWindows.append(w);

        } else {
            ui->customPlot->setChartMode(ChartWidget::Mode_Stacked);
            if (ui->customPlot->getTopRect()) ui->customPlot->getTopRect()->axis(QCPAxis::atLeft)->setLabel(pressLabel);
            if (ui->customPlot->getBottomRect()) {
                ui->customPlot->getBottomRect()->axis(QCPAxis::atLeft)->setLabel(prodLabel);
                ui->customPlot->getBottomRect()->axis(QCPAxis::atBottom)->setLabel(timeLabel);
            }

            drawStackedPlot(info);
            m_currentDisplayedCurve = info.name;
        }
    }
}

// 3. 导数分析
void WT_PlottingWidget::on_btn_Derivative_clicked()
{
    if(!m_dataModel) return;
    PlottingDialog3 dlg(m_dataModel, this);
    applyDialogStyle(&dlg);

    if(dlg.exec() == QDialog::Accepted) {
        CurveInfo info;
        info.name = dlg.getCurveName();
        info.legendName = dlg.getPressLegend();
        info.type = 2;

        info.xCol = dlg.getTimeColumn(); info.yCol = dlg.getPressureColumn();
        info.testType = (int)dlg.getTestType();
        info.initialPressure = dlg.getInitialPressure();
        info.LSpacing = dlg.getLSpacing();
        info.isSmooth = dlg.isSmoothEnabled();
        info.smoothFactor = dlg.getSmoothFactor();

        double p_shutin = 0;
        if(m_dataModel->rowCount() > 0) {
            p_shutin = m_dataModel->item(0, info.yCol)->text().toDouble();
        }

        for(int i=0; i<m_dataModel->rowCount(); ++i) {
            double t = m_dataModel->item(i, info.xCol)->text().toDouble();
            double p = m_dataModel->item(i, info.yCol)->text().toDouble();
            double dp = (info.testType == 0) ? std::abs(info.initialPressure - p) : std::abs(p - p_shutin);
            if(t > 0 && dp > 0) { info.xData.append(t); info.yData.append(dp); }
        }

        if(info.xData.size() < 3) {
            QMessageBox::warning(this, "错误", "有效数据点不足（需 > 0）");
            return;
        }

        QVector<double> derData;
        for (int i = 0; i < info.xData.size(); ++i) {
            double t = info.xData[i];
            double logT = std::log(t);
            int l=i, r=i;
            while(l>0 && std::log(info.xData[l]) > logT - info.LSpacing) l--;
            while(r<info.xData.size()-1 && std::log(info.xData[r]) < logT + info.LSpacing) r++;
            double num = info.yData[r] - info.yData[l];
            double den = std::log(info.xData[r]) - std::log(info.xData[l]);
            derData.append(std::abs(den)>1e-6 ? num/den : 0);
        }

        if(info.isSmooth && info.smoothFactor > 1) {
            QVector<double> smoothed;
            int half = info.smoothFactor/2;
            for (int i = 0; i < derData.size(); ++i) {
                double sum = 0; int cnt = 0;
                for (int j = i - half; j <= i + half; ++j) {
                    if (j >= 0 && j < derData.size()) { sum += derData[j]; cnt++; }
                }
                smoothed.append(sum / cnt);
            }
            info.derivData = smoothed;
        } else {
            info.derivData = derData;
        }

        info.pointShape = dlg.getPressShape(); info.pointColor = dlg.getPressPointColor();
        info.lineStyle = dlg.getPressLineStyle(); info.lineColor = dlg.getPressLineColor();
        info.derivShape = dlg.getDerivShape(); info.derivPointColor = dlg.getDerivPointColor();
        info.derivLineStyle = dlg.getDerivLineStyle(); info.derivLineColor = dlg.getDerivLineColor();
        info.prodLegendName = dlg.getDerivLegend();

        m_curves.insert(info.name, info);
        ui->listWidget_Curves->addItem(info.name);

        if(dlg.isNewWindow()) {
            ChartWindow* w = new ChartWindow();
            w->setWindowTitle(info.name);
            ChartWidget* cw = w->getChartWidget();
            cw->setChartMode(ChartWidget::Mode_Single);
            cw->setTitle(info.name);

            // 【标签设置】导数分析：标准默认标签
            cw->getPlot()->xAxis->setLabel("Time");
            cw->getPlot()->yAxis->setLabel("Pressure & Derivative");

            QCPGraph* g1 = cw->getPlot()->addGraph();
            g1->setData(info.xData, info.yData);
            g1->setName(info.legendName);
            g1->setScatterStyle(QCPScatterStyle(info.pointShape, info.pointColor, info.pointColor, 6));
            // 【修复】尊重弹窗线型
            g1->setPen(QPen(info.lineColor, 2, info.lineStyle));
            g1->setLineStyle(info.lineStyle == Qt::NoPen ? QCPGraph::lsNone : QCPGraph::lsLine);

            QCPGraph* g2 = cw->getPlot()->addGraph();
            g2->setData(info.xData, info.derivData);
            g2->setName(info.prodLegendName);
            g2->setScatterStyle(QCPScatterStyle(info.derivShape, info.derivPointColor, info.derivPointColor, 6));
            // 【修复】尊重弹窗线型
            g2->setPen(QPen(info.derivLineColor, 2, info.derivLineStyle));
            g2->setLineStyle(info.derivLineStyle == Qt::NoPen ? QCPGraph::lsNone : QCPGraph::lsLine);

            cw->getPlot()->rescaleAxes();
            cw->getPlot()->replot();

            w->show();
            m_openedWindows.append(w);
        } else {
            ui->customPlot->setChartMode(ChartWidget::Mode_Single);
            ui->customPlot->getPlot()->xAxis->setLabel("Time");
            ui->customPlot->getPlot()->yAxis->setLabel("Pressure & Derivative");

            drawDerivativePlot(info);
            m_currentDisplayedCurve = info.name;
        }
    }
}

// ---------------- 绘图具体实现 ----------------

void WT_PlottingWidget::addCurveToPlot(const CurveInfo& info)
{
    MouseZoom* plot = ui->customPlot->getPlot();

    QCPGraph* graph = plot->addGraph();
    graph->setName(info.legendName);
    graph->setData(info.xData, info.yData);
    graph->setScatterStyle(QCPScatterStyle(info.pointShape, info.pointColor, info.pointColor, 6));

    // 【修复】尊重弹窗中选择的线型 (info.lineStyle)
    graph->setPen(QPen(info.lineColor, 2, info.lineStyle));
    graph->setLineStyle(info.lineStyle == Qt::NoPen ? QCPGraph::lsNone : QCPGraph::lsLine);

    plot->rescaleAxes();
    plot->replot();
}

void WT_PlottingWidget::drawStackedPlot(const CurveInfo& info)
{
    QCPAxisRect* topRect = ui->customPlot->getTopRect();
    QCPAxisRect* bottomRect = ui->customPlot->getBottomRect();
    MouseZoom* plot = ui->customPlot->getPlot();

    if (!topRect || !bottomRect) return;

    m_graphPress = plot->addGraph(topRect->axis(QCPAxis::atBottom), topRect->axis(QCPAxis::atLeft));
    m_graphPress->setData(info.xData, info.yData);
    m_graphPress->setName(info.legendName);
    m_graphPress->setScatterStyle(QCPScatterStyle(info.pointShape, info.pointColor, info.pointColor, 6));

    // 【修复】压力曲线：尊重弹窗线型
    m_graphPress->setPen(QPen(info.lineColor, 2, info.lineStyle));
    m_graphPress->setLineStyle(info.lineStyle == Qt::NoPen ? QCPGraph::lsNone : QCPGraph::lsLine);

    m_graphProd = plot->addGraph(bottomRect->axis(QCPAxis::atBottom), bottomRect->axis(QCPAxis::atLeft));

    QVector<double> px, py;
    if(info.prodGraphType == 0) { // 阶梯图
        double t_cum = 0;
        if(!info.x2Data.isEmpty()) { px.append(0); py.append(info.y2Data[0]); }
        for(int i=0; i<info.x2Data.size(); ++i) {
            t_cum += info.x2Data[i];
            if(i+1 < info.y2Data.size()) { px.append(t_cum); py.append(info.y2Data[i+1]); }
            else { px.append(t_cum); py.append(info.y2Data[i]); }
        }
        m_graphProd->setLineStyle(QCPGraph::lsStepLeft); // 阶梯图保留连线
        m_graphProd->setScatterStyle(QCPScatterStyle::ssNone);
        m_graphProd->setBrush(QBrush(info.prodColor.lighter(170)));
        m_graphProd->setPen(QPen(info.prodColor, 2));
    } else { // 散点图
        px = info.x2Data; py = info.y2Data;
        m_graphProd->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssCircle, info.prodColor, info.prodColor, 6));
        m_graphProd->setBrush(Qt::NoBrush);

        // 散点图不连线 (保持默认)
        m_graphProd->setPen(QPen(info.prodColor, 2));
        m_graphProd->setLineStyle(QCPGraph::lsNone);
    }
    m_graphProd->setData(px, py);
    m_graphProd->setName(info.prodLegendName);

    m_graphPress->rescaleAxes();
    m_graphProd->rescaleAxes();
    plot->replot();
}

void WT_PlottingWidget::drawDerivativePlot(const CurveInfo& info)
{
    MouseZoom* plot = ui->customPlot->getPlot();

    QCPGraph* g1 = plot->addGraph();
    g1->setName(info.legendName);
    g1->setData(info.xData, info.yData);
    g1->setScatterStyle(QCPScatterStyle(info.pointShape, info.pointColor, info.pointColor, 6));

    QCPGraph* g2 = plot->addGraph();
    g2->setName(info.prodLegendName);
    g2->setData(info.xData, info.derivData);
    g2->setScatterStyle(QCPScatterStyle(info.derivShape, info.derivPointColor, info.derivPointColor, 6));

    // 【修复】尊重弹窗线型
    g1->setPen(QPen(info.lineColor, 2, info.lineStyle));
    g1->setLineStyle(info.lineStyle == Qt::NoPen ? QCPGraph::lsNone : QCPGraph::lsLine);

    g2->setPen(QPen(info.derivLineColor, 2, info.derivLineStyle));
    g2->setLineStyle(info.derivLineStyle == Qt::NoPen ? QCPGraph::lsNone : QCPGraph::lsLine);

    plot->rescaleAxes();
    plot->replot();
}

void WT_PlottingWidget::on_listWidget_Curves_itemDoubleClicked(QListWidgetItem *item)
{
    QString name = item->text();
    if(!m_curves.contains(name)) return;
    CurveInfo info = m_curves[name];
    m_currentDisplayedCurve = name;
    ui->customPlot->setTitle(name);

    if (info.type == 1) {
        ui->customPlot->setChartMode(ChartWidget::Mode_Stacked);
        // 回显时使用标准默认标签
        ui->customPlot->getTopRect()->axis(QCPAxis::atLeft)->setLabel("Pressure");
        ui->customPlot->getBottomRect()->axis(QCPAxis::atLeft)->setLabel("Production");
        ui->customPlot->getBottomRect()->axis(QCPAxis::atBottom)->setLabel("Time");

        drawStackedPlot(info);
    }
    else if (info.type == 2) {
        ui->customPlot->setChartMode(ChartWidget::Mode_Single);
        ui->customPlot->getPlot()->xAxis->setLabel("Time");
        ui->customPlot->getPlot()->yAxis->setLabel("Pressure & Derivative");
        drawDerivativePlot(info);
    }
    else {
        ui->customPlot->setChartMode(ChartWidget::Mode_Single);
        if(m_dataModel && info.xCol >=0 && info.xCol < m_dataModel->columnCount())
            ui->customPlot->getPlot()->xAxis->setLabel(m_dataModel->headerData(info.xCol, Qt::Horizontal).toString());
        if(m_dataModel && info.yCol >=0 && info.yCol < m_dataModel->columnCount())
            ui->customPlot->getPlot()->yAxis->setLabel(m_dataModel->headerData(info.yCol, Qt::Horizontal).toString());

        addCurveToPlot(info);
    }
}

void WT_PlottingWidget::onExportDataTriggered()
{
    if(m_currentDisplayedCurve.isEmpty()) return;

    QMessageBox msgBox(this);
    msgBox.setWindowTitle("导出数据");
    msgBox.setText("请选择导出范围：");
    msgBox.setIcon(QMessageBox::Question);
    QPushButton* btnAll = msgBox.addButton("全部数据", QMessageBox::ActionRole);
    QPushButton* btnPart = msgBox.addButton("部分数据", QMessageBox::ActionRole);
    msgBox.addButton("取消", QMessageBox::RejectRole);
    applyDialogStyle(&msgBox);
    msgBox.exec();

    if(msgBox.clickedButton() == btnAll) {
        executeExport(true);
    }
    else if(msgBox.clickedButton() == btnPart) {
        m_isSelectingForExport = true;
        m_selectionStep = 1;
        ui->customPlot->getPlot()->setCursor(Qt::CrossCursor);
        QMessageBox msg(this);
        msg.setWindowTitle("提示");
        msg.setText("请在曲线上点击起始点。");
        msg.setIcon(QMessageBox::Information);
        msg.setStandardButtons(QMessageBox::Ok);
        applyDialogStyle(&msg);
        msg.exec();
    }
}

void WT_PlottingWidget::onGraphClicked(QCPAbstractPlottable *plottable, int dataIndex, QMouseEvent *event)
{
    Q_UNUSED(event);
    if(!m_isSelectingForExport) return;

    QCPGraph* graph = qobject_cast<QCPGraph*>(plottable);
    if(!graph) return;

    double key = graph->dataMainKey(dataIndex);

    if(m_selectionStep == 1) {
        m_exportStartIndex = key;
        m_selectionStep = 2;
        QMessageBox msg(this);
        msg.setWindowTitle("提示");
        msg.setText("请点击结束点。");
        msg.setIcon(QMessageBox::Information);
        msg.setStandardButtons(QMessageBox::Ok);
        applyDialogStyle(&msg);
        msg.exec();
    } else {
        m_exportEndIndex = key;
        if(m_exportStartIndex > m_exportEndIndex) std::swap(m_exportStartIndex, m_exportEndIndex);

        m_isSelectingForExport = false;
        ui->customPlot->getPlot()->setCursor(Qt::ArrowCursor);

        executeExport(false, m_exportStartIndex, m_exportEndIndex);
    }
}

void WT_PlottingWidget::executeExport(bool fullRange, double start, double end)
{
    QString name = m_projectPath + "/export.csv";
    QString file = QFileDialog::getSaveFileName(this, "保存", name, "CSV Files (*.csv);;Excel Files (*.xls);;Text Files (*.txt)");
    if(file.isEmpty()) return;
    QFile f(file);
    if(!f.open(QIODevice::WriteOnly | QIODevice::Text)) return;
    QTextStream out(&f);
    QString sep = ",";
    if(file.endsWith(".txt") || file.endsWith(".xls")) sep = "\t";

    CurveInfo& info = m_curves[m_currentDisplayedCurve];

    if(ui->customPlot->getChartMode() == ChartWidget::Mode_Stacked) {
        out << (fullRange ? "Time,P,Q\n" : "AdjTime,P,Q,OrigTime\n");
        for(int i=0; i<info.xData.size(); ++i) {
            double t = info.xData[i];
            if(!fullRange && (t < start || t > end)) continue;
            double p = info.yData[i];
            double q = getProductionValueAt(t, info);
            if(fullRange) out << t << sep << p << sep << q << "\n";
            else out << (t-start) << sep << p << sep << q << sep << t << "\n";
        }
    } else {
        out << (fullRange ? "Time,Value\n" : "AdjTime,Value,OrigTime\n");
        for(int i=0; i<info.xData.size(); ++i) {
            double t = info.xData[i];
            if(!fullRange && (t < start || t > end)) continue;
            double val = info.yData[i];
            if(fullRange) out << t << sep << val << "\n";
            else out << (t-start) << sep << val << sep << t << "\n";
        }
    }
    f.close();
    QMessageBox msg(this);
    msg.setWindowTitle("成功");
    msg.setText("导出完成。");
    msg.setIcon(QMessageBox::Information);
    msg.setStandardButtons(QMessageBox::Ok);
    applyDialogStyle(&msg);
    msg.exec();
}

double WT_PlottingWidget::getProductionValueAt(double t, const CurveInfo& info) {
    if(info.y2Data.isEmpty()) return 0;
    Q_UNUSED(t);
    return info.y2Data.last();
}

void WT_PlottingWidget::on_btn_Manage_clicked() {
    QListWidgetItem* item = getCurrentSelectedItem();
    if(!item) return;
    QString name = item->text();
    CurveInfo& info = m_curves[name];

    PlottingDialog4 dlg(m_dataModel, this);
    applyDialogStyle(&dlg);

    bool hasSecond = (info.type == 1 || info.type == 2);
    QString name2 = "";
    QCPScatterStyle::ScatterShape shape2 = QCPScatterStyle::ssNone;
    QColor c2 = Qt::black, lc2 = Qt::black;
    Qt::PenStyle ls2 = Qt::SolidLine;

    if (info.type == 1) {
        name2 = info.prodLegendName;
        shape2 = (info.prodGraphType == 1) ? QCPScatterStyle::ssCircle : QCPScatterStyle::ssNone;
        c2 = info.prodColor;
        lc2 = info.prodColor;
    } else if (info.type == 2) {
        name2 = info.prodLegendName;
        shape2 = info.derivShape;
        c2 = info.derivPointColor;
        ls2 = info.derivLineStyle;
        lc2 = info.derivLineColor;
    }

    dlg.setInitialData(hasSecond,
                       info.legendName, info.xCol, info.yCol,
                       info.pointShape, info.pointColor, info.lineStyle, info.lineColor,
                       name2, shape2, c2, ls2, lc2);

    if(dlg.exec() == QDialog::Accepted) {
        info.legendName = dlg.getLegendName1();
        info.xCol = dlg.getXColumn(); info.yCol = dlg.getYColumn();
        info.pointShape = dlg.getPointShape1(); info.pointColor = dlg.getPointColor1();
        info.lineStyle = dlg.getLineStyle1(); info.lineColor = dlg.getLineColor1();

        if(info.type == 0) {
            info.xData.clear(); info.yData.clear();
            for(int i=0; i<m_dataModel->rowCount(); ++i) {
                double xVal = m_dataModel->item(i, info.xCol)->text().toDouble();
                double yVal = m_dataModel->item(i, info.yCol)->text().toDouble();
                if (xVal > 1e-9 && yVal > 1e-9) {
                    info.xData.append(xVal);
                    info.yData.append(yVal);
                }
            }
        }

        if (hasSecond) {
            if (info.type == 1) {
                info.prodLegendName = dlg.getLegendName2();
                info.prodColor = dlg.getPointColor2();
            } else if (info.type == 2) {
                info.prodLegendName = dlg.getLegendName2();
                info.derivShape = dlg.getPointShape2();
                info.derivPointColor = dlg.getPointColor2();
                info.derivLineStyle = dlg.getLineStyle2();
                info.derivLineColor = dlg.getLineColor2();
            }
        }
        if(m_currentDisplayedCurve == name) on_listWidget_Curves_itemDoubleClicked(item);
    }
}

void WT_PlottingWidget::on_btn_Delete_clicked() {
    QListWidgetItem* item = getCurrentSelectedItem();
    if(!item) return;
    QString name = item->text();
    QMessageBox msgBox(this);
    msgBox.setWindowTitle("确认删除");
    msgBox.setText("确定要删除曲线 \"" + name + "\" 吗？");
    msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
    msgBox.setIcon(QMessageBox::Question);
    applyDialogStyle(&msgBox);

    if(msgBox.exec() == QMessageBox::Yes) {
        m_curves.remove(name);
        delete item;
        if(m_currentDisplayedCurve == name) {
            ui->customPlot->getPlot()->clearGraphs();
            ui->customPlot->getPlot()->replot();
            m_currentDisplayedCurve.clear();
        }
    }
}

void WT_PlottingWidget::on_btn_Save_clicked() { saveProjectData(); }

void WT_PlottingWidget::clearAllPlots()
{
    m_curves.clear();
    m_currentDisplayedCurve.clear();
    ui->listWidget_Curves->clear();
    qDeleteAll(m_openedWindows);
    m_openedWindows.clear();
    ui->customPlot->getPlot()->clearGraphs();
    ui->customPlot->setChartMode(ChartWidget::Mode_Single);
}

QListWidgetItem* WT_PlottingWidget::getCurrentSelectedItem() {
    return ui->listWidget_Curves->currentItem();
}
