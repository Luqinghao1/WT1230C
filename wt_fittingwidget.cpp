/*
 * 文件名: wt_fittingwidget.cpp
 * 文件作用: 试井拟合分析主界面类的实现文件
 * 功能描述:
 * 1. 初始化拟合分析界面，配置图表控件 (QCustomPlot) 和参数表格。
 * 2. 实现观测数据的加载逻辑，支持根据试井类型（降落/恢复）计算压差 (Delta P)。
 * 3. 核心算法实现：完整实现了 Levenberg-Marquardt (LM) 非线性最小二乘拟合算法。
 * 4. 提供丰富的交互功能：手动调整参数、权重滑块、模型选择、图表视图控制。
 * 5. 提供结果输出功能：导出拟合参数、导出图表图片、生成 HTML 分析报告。
 */

#include "wt_fittingwidget.h"
#include "ui_wt_fittingwidget.h"
#include "modelparameter.h"
#include "modelselect.h"
#include "fittingdatadialog.h"
#include "pressurederivativecalculator.h"
#include "pressurederivativecalculator1.h"

#include <QtConcurrent>
#include <QMessageBox>
#include <QDebug>
#include <cmath>
#include <QFileDialog>
#include <QFile>
#include <QTextStream>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QPushButton>
#include <QLabel>
#include <QComboBox>
#include <QJsonObject>
#include <QJsonArray>
#include <QDateTime>
#include <QBuffer>
#include <Eigen/Dense>

// ===========================================================================
// 构造与析构
// ===========================================================================

/**
 * @brief 构造函数：初始化界面和核心组件
 * @param parent 父窗口指针
 */
FittingWidget::FittingWidget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::FittingWidget),
    m_modelManager(nullptr),
    m_projectModel(nullptr),
    m_plotTitle(nullptr),
    m_currentModelType(ModelManager::Model_1),
    m_isFitting(false)
{
    // 加载 UI 布局
    ui->setupUi(this);

    // 设置主界面分割器的初始比例：左侧参数栏 380 像素，右侧图表区 720 像素
    ui->splitter->setSizes(QList<int>{380, 720});
    // 禁止折叠左侧面板
    ui->splitter->setCollapsible(0, false);

    // 初始化参数表格管理模块，负责参数的显示和交互
    m_paramChart = new FittingParameterChart(ui->tableParams, this);

    // 初始化绘图控件 (支持鼠标缩放交互)，并将其添加到界面的布局容器中
    m_plot = new MouseZoom(this);
    ui->plotContainer->layout()->addWidget(m_plot);
    // 配置图表外观（坐标轴、网格、标题等）
    setupPlot();

    // 注册自定义数据类型，以便在跨线程信号槽中传递这些类型的数据
    qRegisterMetaType<QMap<QString,double>>("QMap<QString,double>");
    qRegisterMetaType<ModelManager::ModelType>("ModelManager::ModelType");
    qRegisterMetaType<QVector<double>>("QVector<double>");

    // 连接内部信号槽：
    // 1. 迭代更新信号 -> 更新界面显示（使用 QueuedConnection 确保在主线程执行）
    connect(this, &FittingWidget::sigIterationUpdated, this, &FittingWidget::onIterationUpdate, Qt::QueuedConnection);
    // 2. 进度信号 -> 更新进度条
    connect(this, &FittingWidget::sigProgress, ui->progressBar, &QProgressBar::setValue);
    // 3. 异步任务监视器完成信号 -> 处理拟合结束
    connect(&m_watcher, &QFutureWatcher<void>::finished, this, &FittingWidget::onFitFinished);

    // 连接权重滑块变化信号 -> 更新权重数值标签
    connect(ui->sliderWeight, &QSlider::valueChanged, this, &FittingWidget::onSliderWeightChanged);

    // 初始化权重滑块，默认为 50%（压力和导数拟合权重各占一半）
    ui->sliderWeight->setRange(0, 100);
    ui->sliderWeight->setValue(50);
    onSliderWeightChanged(50);
}

/**
 * @brief 析构函数：释放 UI 资源
 */
FittingWidget::~FittingWidget()
{
    delete ui;
}

// ===========================================================================
// 初始化与配置
// ===========================================================================

/**
 * @brief 设置模型管理器
 * @param m 模型管理器指针
 * 说明：模型管理器用于提供理论模型的计算服务。
 */
void FittingWidget::setModelManager(ModelManager *m)
{
    m_modelManager = m;
    m_paramChart->setModelManager(m);
    // 设置默认使用的解释模型
    initializeDefaultModel();
}

/**
 * @brief 设置项目数据模型指针
 * @param model 项目表格数据模型
 * 说明：用于在加载数据弹窗中直接读取项目中的数据。
 */
void FittingWidget::setProjectDataModel(QStandardItemModel *model)
{
    m_projectModel = model;
}

/**
 * @brief 更新基础参数（预留接口）
 * 说明：当项目的基础物性参数（如孔隙度、厚度）发生变化时，可调用此函数同步更新。
 */
void FittingWidget::updateBasicParameters()
{
    // 目前暂无特殊处理，留作扩展接口
}

/**
 * @brief 初始化默认的模型状态
 * 说明：默认选中模型1，并重置参数。
 */
void FittingWidget::initializeDefaultModel()
{
    if(!m_modelManager) return;
    m_currentModelType = ModelManager::Model_1;
    ui->btn_modelSelect->setText("当前: 压裂水平井复合页岩油模型1");
    // 重置参数表为模型1的默认值
    on_btnResetParams_clicked();
}

/**
 * @brief 初始化绘图控件属性
 * 说明：设置坐标轴为对数坐标，设置网格线、图例、曲线样式等。
 */
void FittingWidget::setupPlot() {
    // 启用鼠标拖拽和平移、缩放功能
    m_plot->setInteractions(QCP::iRangeDrag | QCP::iRangeZoom);
    m_plot->setBackground(Qt::white);
    m_plot->axisRect()->setBackground(Qt::white);

    // 在图表顶部插入一行用于显示标题
    m_plot->plotLayout()->insertRow(0);
    m_plotTitle = new QCPTextElement(m_plot, "试井解释拟合", QFont("SimHei", 14, QFont::Bold));
    m_plot->plotLayout()->addElement(0, 0, m_plotTitle);

    // 设置 X 轴和 Y 轴为对数坐标 (Logarithmic)
    QSharedPointer<QCPAxisTickerLog> logTicker(new QCPAxisTickerLog);
    m_plot->xAxis->setScaleType(QCPAxis::stLogarithmic); m_plot->xAxis->setTicker(logTicker);
    m_plot->yAxis->setScaleType(QCPAxis::stLogarithmic); m_plot->yAxis->setTicker(logTicker);

    // 设置数字格式为科学计数法 (1e0, 1e1...)
    m_plot->xAxis->setNumberFormat("eb"); m_plot->xAxis->setNumberPrecision(0);
    m_plot->yAxis->setNumberFormat("eb"); m_plot->yAxis->setNumberPrecision(0);

    // 设置轴标签字体和内容
    QFont labelFont("Arial", 12, QFont::Bold); QFont tickFont("Arial", 12);
    m_plot->xAxis->setLabel("时间 Time (h)");
    // [修改处]：明确Y轴显示的是压差和导数
    m_plot->yAxis->setLabel("压差 & 导数 Delta P & Derivative (MPa)");
    m_plot->xAxis->setLabelFont(labelFont); m_plot->yAxis->setLabelFont(labelFont);
    m_plot->xAxis->setTickLabelFont(tickFont); m_plot->yAxis->setTickLabelFont(tickFont);

    // 设置顶部和右侧刻度可见，但不显示数值（为了美观）
    m_plot->xAxis2->setVisible(true); m_plot->yAxis2->setVisible(true);
    m_plot->xAxis2->setTickLabels(false); m_plot->yAxis2->setTickLabels(false);
    // 确保顶部和右侧轴也跟随主轴的变化
    connect(m_plot->xAxis, SIGNAL(rangeChanged(QCPRange)), m_plot->xAxis2, SLOT(setRange(QCPRange)));
    connect(m_plot->yAxis, SIGNAL(rangeChanged(QCPRange)), m_plot->yAxis2, SLOT(setRange(QCPRange)));
    m_plot->xAxis2->setScaleType(QCPAxis::stLogarithmic); m_plot->yAxis2->setScaleType(QCPAxis::stLogarithmic);
    m_plot->xAxis2->setTicker(logTicker); m_plot->yAxis2->setTicker(logTicker);

    // 启用网格线和子网格线
    m_plot->xAxis->grid()->setVisible(true); m_plot->yAxis->grid()->setVisible(true);
    m_plot->xAxis->grid()->setSubGridVisible(true); m_plot->yAxis->grid()->setSubGridVisible(true);
    m_plot->xAxis->grid()->setPen(QPen(QColor(220, 220, 220), 1, Qt::SolidLine));
    m_plot->yAxis->grid()->setPen(QPen(QColor(220, 220, 220), 1, Qt::SolidLine));
    m_plot->xAxis->grid()->setSubGridPen(QPen(QColor(240, 240, 240), 1, Qt::DotLine));
    m_plot->yAxis->grid()->setSubGridPen(QPen(QColor(240, 240, 240), 1, Qt::DotLine));

    // 设置初始视图范围
    m_plot->xAxis->setRange(1e-3, 1e3); m_plot->yAxis->setRange(1e-3, 1e2);

    // 添加 4 条曲线对象：
    // Graph 0: 实测压差 (绿色圆点，无连线)
    m_plot->addGraph(); m_plot->graph(0)->setPen(Qt::NoPen);
    m_plot->graph(0)->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssCircle, QColor(0, 100, 0), 6));
    // [修改处]：图例明确为实测压差
    m_plot->graph(0)->setName("实测压差");

    // Graph 1: 实测导数 (洋红三角，无连线)
    m_plot->addGraph(); m_plot->graph(1)->setPen(Qt::NoPen);
    m_plot->graph(1)->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssTriangle, Qt::magenta, 6));
    m_plot->graph(1)->setName("实测导数");

    // Graph 2: 理论压差 (红色实线)
    m_plot->addGraph(); m_plot->graph(2)->setPen(QPen(Qt::red, 2));
    m_plot->graph(2)->setName("理论压差");

    // Graph 3: 理论导数 (蓝色实线)
    m_plot->addGraph(); m_plot->graph(3)->setPen(QPen(Qt::blue, 2));
    m_plot->graph(3)->setName("理论导数");

    // 显示图例
    m_plot->legend->setVisible(true);
    m_plot->legend->setFont(QFont("Arial", 9));
    m_plot->legend->setBrush(QBrush(QColor(255, 255, 255, 200)));
}

// ===========================================================================
// 数据加载与处理
// ===========================================================================

/**
 * @brief 按钮槽函数：加载观测数据
 * 说明：弹出配置对话框，读取用户选择的数据列，进行压差计算和导数处理，最后绘制到图表上。
 */
void FittingWidget::on_btnLoadData_clicked() {
    // 1. 创建并弹出数据加载配置对话框，传入当前项目数据模型供预览
    FittingDataDialog dlg(m_projectModel, this);
    if (dlg.exec() != QDialog::Accepted) {
        return; // 用户取消
    }

    // 2. 获取用户在弹窗中配置的参数（列索引、平滑设置等）
    FittingDataSettings settings = dlg.getSettings();
    // 获取预览模型（其中包含了实际的数据内容，无论是来自项目还是文件）
    QStandardItemModel* sourceModel = dlg.getPreviewModel();

    if (!sourceModel || sourceModel->rowCount() == 0) {
        QMessageBox::warning(this, "警告", "所选数据源为空，无法加载！");
        return;
    }

    // 3. 提取基础数据（时间和原始压力）
    QVector<double> rawTime, rawPressureData, finalDeriv;

    // 获取需要跳过的首行数
    int skip = settings.skipRows;
    int rows = sourceModel->rowCount();

    for (int i = skip; i < rows; ++i) {
        // 根据列索引读取数据项
        QStandardItem* itemT = sourceModel->item(i, settings.timeColIndex);
        QStandardItem* itemP = sourceModel->item(i, settings.pressureColIndex);

        if (itemT && itemP) {
            bool okT, okP;
            double t = itemT->text().toDouble(&okT);
            double p = itemP->text().toDouble(&okP);

            // 过滤无效数据：双对数坐标图要求时间必须大于0
            if (okT && okP && t > 0) {
                rawTime.append(t);
                rawPressureData.append(p);

                // 如果用户选择了具体的导数列（索引 >= 0），则同时提取导数
                // 如果选择的是“自动计算”（索引 == -1），则此处暂不处理
                if (settings.derivColIndex >= 0) {
                    QStandardItem* itemD = sourceModel->item(i, settings.derivColIndex);
                    if (itemD) {
                        finalDeriv.append(itemD->text().toDouble());
                    } else {
                        finalDeriv.append(0.0);
                    }
                }
            }
        }
    }

    if (rawTime.isEmpty()) {
        QMessageBox::warning(this, "警告", "未能提取到有效数据，请检查列映射或跳过行数设置。");
        return;
    }

    // 4. 计算压差 (Delta P)
    // 根据试井类型和初始参数，将原始压力转换为压差
    QVector<double> finalDeltaP;

    // 获取压力恢复试井的关井流压 Pwf(delta_t=0)，假设为数据的第一点
    // 注意：实际应用中可能需要更复杂的逻辑判定，这里简化处理
    double p_shutin = rawPressureData.first();

    for (double p : rawPressureData) {
        double deltaP = 0.0;

        if (settings.testType == Test_Drawdown) {
            // 压力降落试井: Delta P = |Pi - P(t)|
            // Pi 由用户在弹窗中输入
            deltaP = std::abs(settings.initialPressure - p);
        } else {
            // 压力恢复试井: Delta P = |P(t) - Pwf(dt=0)|
            deltaP = std::abs(p - p_shutin);
        }
        finalDeltaP.append(deltaP);
    }

    // 5. 处理导数数据
    if (settings.derivColIndex == -1) {
        // 情况A: 用户选择“自动计算”
        // 调用静态方法计算 Bourdet 导数 (默认 L-Spacing = 0.15)
        // 注意：传入压差数据进行计算
        finalDeriv = PressureDerivativeCalculator::calculateBourdetDerivative(rawTime, finalDeltaP, 0.15);

        // 如果开启了平滑，调用静态平滑方法处理计算结果
        if (settings.enableSmoothing) {
            finalDeriv = PressureDerivativeCalculator1::smoothData(finalDeriv, settings.smoothingSpan);
        }
    } else {
        // 情况B: 用户选择了已有导数列
        // 如果用户在已有导数的基础上仍要求平滑，则进行平滑处理
        if (settings.enableSmoothing) {
            finalDeriv = PressureDerivativeCalculator1::smoothData(finalDeriv, settings.smoothingSpan);
        }

        // 确保导数向量长度与时间向量一致
        if (finalDeriv.size() != rawTime.size()) {
            finalDeriv.resize(rawTime.size());
        }
    }

    // 6. 将处理好的数据设置到界面成员变量，并刷新绘图
    setObservedData(rawTime, finalDeltaP, finalDeriv);

    QMessageBox::information(this, "成功", "观测数据已成功加载。");
}

/**
 * @brief 设置观测数据并更新绘图
 * @param t 时间向量
 * @param deltaP 压差向量
 * @param d 导数向量
 */
void FittingWidget::setObservedData(const QVector<double>& t, const QVector<double>& deltaP, const QVector<double>& d) {
    m_obsTime = t;
    m_obsDeltaP = deltaP;
    m_obsDerivative = d;

    // 准备绘图数据（过滤掉非正值，因为对数坐标无法显示 <= 0 的点）
    QVector<double> vt, vp, vd;
    for(int i=0; i<t.size(); ++i) {
        if(t[i]>1e-8 && deltaP[i]>1e-8) {
            vt << t[i];
            vp << deltaP[i];
            // 导数允许为0（不显示），为了数据对齐补一个极小值或仅添加有效值
            if(i < d.size() && d[i] > 1e-8) {
                vd << d[i];
            } else {
                vd << 1e-10; // 极小值，在对数图上不可见或位于底部
            }
        }
    }

    // 更新 Graph 0 (压差) 和 Graph 1 (导数)
    m_plot->graph(0)->setData(vt, vp);
    m_plot->graph(1)->setData(vt, vd);

    // 自动缩放坐标轴以适应新数据
    m_plot->rescaleAxes();
    // 修正下限，防止对数轴崩溃
    if(m_plot->xAxis->range().lower <= 0) m_plot->xAxis->setRangeLower(1e-3);
    if(m_plot->yAxis->range().lower <= 0) m_plot->yAxis->setRangeLower(1e-3);

    m_plot->replot();
}

// ===========================================================================
// 交互逻辑槽函数
// ===========================================================================

/**
 * @brief 权重滑块变化处理
 */
void FittingWidget::onSliderWeightChanged(int value)
{
    double wPressure = value / 100.0;
    double wDerivative = 1.0 - wPressure;
    ui->label_ValDerivative->setText(QString("导数权重: %1").arg(wDerivative, 0, 'f', 2));
    ui->label_ValPressure->setText(QString("压差权重: %1").arg(wPressure, 0, 'f', 2));
}

/**
 * @brief 打开参数选择对话框
 */
void FittingWidget::on_btnSelectParams_clicked()
{
    // 同步表格数据
    m_paramChart->updateParamsFromTable();
    QList<FitParameter> currentParams = m_paramChart->getParameters();

    ParamSelectDialog dlg(currentParams, this);
    if(dlg.exec() == QDialog::Accepted) {
        // 更新参数的选中状态（是否参与拟合）
        m_paramChart->setParameters(dlg.getUpdatedParams());
        // 刷新曲线
        updateModelCurve();
    }
}

/**
 * @brief 开始拟合按钮点击
 */
void FittingWidget::on_btnRunFit_clicked() {
    if(m_isFitting) return; // 防止重复点击
    if(m_obsTime.isEmpty()) {
        QMessageBox::warning(this,"错误","请先加载观测数据。");
        return;
    }

    // 同步参数并禁用按钮
    m_paramChart->updateParamsFromTable();
    m_isFitting = true;
    m_stopRequested = false;
    ui->btnRunFit->setEnabled(false);

    ModelManager::ModelType modelType = m_currentModelType;
    QList<FitParameter> paramsCopy = m_paramChart->getParameters();
    double w = ui->sliderWeight->value() / 100.0;

    // 使用 QtConcurrent 在后台线程运行拟合优化任务，避免阻塞 UI 主线程
    (void)QtConcurrent::run([this, modelType, paramsCopy, w](){
        runOptimizationTask(modelType, paramsCopy, w);
    });
}

/**
 * @brief 停止拟合按钮点击
 */
void FittingWidget::on_btnStop_clicked() {
    m_stopRequested = true;
}

/**
 * @brief 刷新曲线按钮点击
 */
void FittingWidget::on_btnImportModel_clicked() {
    updateModelCurve();
}

/**
 * @brief 重置参数按钮点击
 */
void FittingWidget::on_btnResetParams_clicked() {
    if(!m_modelManager) return;
    m_paramChart->resetParams(m_currentModelType);
    updateModelCurve();
}

/**
 * @brief 复位视图按钮点击
 */
void FittingWidget::on_btnResetView_clicked() {
    if(m_plot->graph(0)->dataCount() > 0) {
        m_plot->rescaleAxes();
        if(m_plot->xAxis->range().lower<=0) m_plot->xAxis->setRangeLower(1e-3);
        if(m_plot->yAxis->range().lower<=0) m_plot->yAxis->setRangeLower(1e-3);
    } else {
        m_plot->xAxis->setRange(1e-3, 1e3); m_plot->yAxis->setRange(1e-3, 1e2);
    }
    m_plot->replot();
}

/**
 * @brief 图表设置按钮点击
 */
void FittingWidget::on_btnChartSettings_clicked() {
    ChartSetting1 dlg(m_plot, m_plotTitle, this);
    dlg.exec();
}

/**
 * @brief 选择模型按钮点击
 */
void FittingWidget::on_btn_modelSelect_clicked() {
    ModelSelect dlg(this);
    if (dlg.exec() == QDialog::Accepted) {
        QString code = dlg.getSelectedModelCode();
        QString name = dlg.getSelectedModelName();

        bool found = false;
        ModelManager::ModelType newType = ModelManager::Model_1;

        // 模型代码映射
        if (code == "modelwidget1") newType = ModelManager::Model_1;
        else if (code == "modelwidget2") newType = ModelManager::Model_2;
        else if (code == "modelwidget3") newType = ModelManager::Model_3;
        else if (code == "modelwidget4") newType = ModelManager::Model_4;
        else if (code == "modelwidget5") newType = ModelManager::Model_5;
        else if (code == "modelwidget6") newType = ModelManager::Model_6;
        else if (!code.isEmpty()) found = true;

        if (code.startsWith("modelwidget")) found = true;

        if (found) {
            m_paramChart->switchModel(newType);
            m_currentModelType = newType;
            ui->btn_modelSelect->setText("当前: " + name);
            updateModelCurve();
        } else {
            QMessageBox::warning(this, "提示", "所选组合暂无对应的模型。\nCode: " + code);
        }
    }
}

/**
 * @brief 导出参数数据
 */
void FittingWidget::on_btnExportData_clicked() {
    m_paramChart->updateParamsFromTable();
    QList<FitParameter> params = m_paramChart->getParameters();

    QString defaultDir = ModelParameter::instance()->getProjectPath();
    if(defaultDir.isEmpty()) defaultDir = ".";

    QString fileName = QFileDialog::getSaveFileName(this, "导出拟合参数", defaultDir + "/FittingParameters.csv", "CSV Files (*.csv);;Text Files (*.txt)");
    if (fileName.isEmpty()) return;

    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) return;
    QTextStream out(&file);

    // CSV 格式带 BOM 头
    if(fileName.endsWith(".csv", Qt::CaseInsensitive)) {
        file.write("\xEF\xBB\xBF");
        out << QString("参数中文名,参数英文名,拟合值,单位\n");
        for(const auto& param : params) {
            QString htmlSym, uniSym, unitStr, dummyName;
            FittingParameterChart::getParamDisplayInfo(param.name, dummyName, htmlSym, uniSym, unitStr);
            if(unitStr == "无因次" || unitStr == "小数") unitStr = "";
            out << QString("%1,%2,%3,%4\n").arg(param.displayName).arg(uniSym).arg(param.value, 0, 'g', 10).arg(unitStr);
        }
    } else {
        // 纯文本格式
        for(const auto& param : params) {
            QString htmlSym, uniSym, unitStr, dummyName;
            FittingParameterChart::getParamDisplayInfo(param.name, dummyName, htmlSym, uniSym, unitStr);
            if(unitStr == "无因次" || unitStr == "小数") unitStr = "";
            QString lineStr = QString("%1 (%2): %3 %4").arg(param.displayName).arg(uniSym).arg(param.value, 0, 'g', 10).arg(unitStr);
            out << lineStr.trimmed() << "\n";
        }
    }
    file.close();
    QMessageBox::information(this, "完成", "参数数据已成功导出。");
}

/**
 * @brief 导出图表为图片或PDF
 */
void FittingWidget::on_btnExportChart_clicked() {
    QString defaultDir = ModelParameter::instance()->getProjectPath();
    if(defaultDir.isEmpty()) defaultDir = ".";

    QString fileName = QFileDialog::getSaveFileName(this, "导出图表", defaultDir + "/FittingChart.png", "PNG Image (*.png);;JPEG Image (*.jpg);;PDF Document (*.pdf)");
    if (fileName.isEmpty()) return;

    bool success = false;
    if (fileName.endsWith(".png", Qt::CaseInsensitive)) success = m_plot->savePng(fileName);
    else if (fileName.endsWith(".jpg", Qt::CaseInsensitive)) success = m_plot->saveJpg(fileName);
    else if (fileName.endsWith(".pdf", Qt::CaseInsensitive)) success = m_plot->savePdf(fileName);
    else success = m_plot->savePng(fileName + ".png");

    if (success) QMessageBox::information(this, "完成", "图表已成功导出。");
    else QMessageBox::critical(this, "错误", "导出图表失败。");
}

// ===========================================================================
// 拟合算法核心实现 (Levenberg-Marquardt)
// ===========================================================================

/**
 * @brief 运行优化任务的入口函数
 */
void FittingWidget::runOptimizationTask(ModelManager::ModelType modelType, QList<FitParameter> fitParams, double weight) {
    runLevenbergMarquardtOptimization(modelType, fitParams, weight);
}

/**
 * @brief Levenberg-Marquardt 算法具体实现
 * @param modelType 模型类型
 * @param params 参数列表
 * @param weight 权重 (0~1)
 */
void FittingWidget::runLevenbergMarquardtOptimization(ModelManager::ModelType modelType, QList<FitParameter> params, double weight) {
    // 设置模型计算为低精度模式以提高迭代速度
    if(m_modelManager) m_modelManager->setHighPrecision(false);

    // 1. 确定需要拟合的参数索引
    QVector<int> fitIndices;
    for(int i=0; i<params.size(); ++i) {
        if(params[i].isFit) fitIndices.append(i);
    }
    int nParams = fitIndices.size();

    // 如果没有勾选任何拟合参数，直接结束
    if(nParams == 0) {
        QMetaObject::invokeMethod(this, "onFitFinished");
        return;
    }

    // 2. 初始化算法参数
    double lambda = 0.01;      // 阻尼因子 (initial damping factor)
    int maxIter = 50;          // 最大迭代次数
    double currentSSE = 1e15;  // 当前误差平方和 (Sum Squared Error)

    // 构建参数映射表
    QMap<QString, double> currentParamMap;
    for(const auto& p : params) currentParamMap.insert(p.name, p.value);

    // 初始参数联动处理 (LfD = Lf / L)
    if(currentParamMap.contains("L") && currentParamMap.contains("Lf") && currentParamMap["L"] > 1e-9)
        currentParamMap["LfD"] = currentParamMap["Lf"] / currentParamMap["L"];

    // 3. 计算初始状态的残差和误差
    QVector<double> residuals = calculateResiduals(currentParamMap, modelType, weight);
    currentSSE = calculateSumSquaredError(residuals);

    // 通知界面更新初始状态
    ModelCurveData curve = m_modelManager->calculateTheoreticalCurve(modelType, currentParamMap);
    emit sigIterationUpdated(currentSSE/residuals.size(), currentParamMap, std::get<0>(curve), std::get<1>(curve), std::get<2>(curve));

    // 4. 迭代主循环
    for(int iter = 0; iter < maxIter; ++iter) {
        if(m_stopRequested) break; // 响应用户停止请求

        // 收敛判据：如果均方误差足够小，提前结束
        if (!residuals.isEmpty() && (currentSSE / residuals.size()) < 3e-3) break;

        emit sigProgress(iter * 100 / maxIter);

        // 计算雅可比矩阵 J (size: nResiduals x nParams)
        QVector<QVector<double>> J = computeJacobian(currentParamMap, residuals, fitIndices, modelType, params, weight);
        int nRes = residuals.size();

        // 构造正规方程的近似 Hessian 矩阵 H = J^T * J 和 梯度向量 g = J^T * r
        QVector<QVector<double>> H(nParams, QVector<double>(nParams, 0.0));
        QVector<double> g(nParams, 0.0);

        for(int k=0; k<nRes; ++k) {
            for(int i=0; i<nParams; ++i) {
                // 计算梯度 g
                g[i] += J[k][i] * residuals[k];
                // 计算 Hessian 的下三角部分
                for(int j=0; j<=i; ++j) {
                    H[i][j] += J[k][i] * J[k][j];
                }
            }
        }
        // 填充 Hessian 的对称部分 (上三角)
        for(int i=0; i<nParams; ++i) {
            for(int j=i+1; j<nParams; ++j) {
                H[i][j] = H[j][i];
            }
        }

        bool stepAccepted = false;

        // 5. 内部循环：尝试更新步长 (Levenberg-Marquardt 核心步骤)
        // 如果新误差变大，则增大阻尼因子 lambda 并重试
        for(int tryIter=0; tryIter<5; ++tryIter) {
            QVector<QVector<double>> H_lm = H;

            // 将阻尼因子加入 Hessian 对角线: H_ii = H_ii + lambda * (1 + |H_ii|) 或 simplified: H_ii + lambda
            // 这里采用 Marquardt 建议的方式
            for(int i=0; i<nParams; ++i) {
                H_lm[i][i] += lambda * (1.0 + std::abs(H[i][i]));
            }

            QVector<double> negG(nParams);
            for(int i=0;i<nParams;++i) negG[i] = -g[i];

            // 求解线性方程组 (H_lm * delta = -g) 得到参数更新量 delta
            QVector<double> delta = solveLinearSystem(H_lm, negG);

            // 计算试探性新参数
            QMap<QString, double> trialMap = currentParamMap;
            for(int i=0; i<nParams; ++i) {
                int pIdx = fitIndices[i];
                QString pName = params[pIdx].name;
                double oldVal = currentParamMap[pName];

                // 判断参数是否需要在对数域更新 (大部分试井参数如 k, C, S 为对数敏感，但 S 和 nf 除外)
                bool isLog = (oldVal > 1e-12 && pName != "S" && pName != "nf");
                double newVal;

                if(isLog) {
                    // 对数域更新: new = 10^(log(old) + delta)
                    double logVal = log10(oldVal) + delta[i];
                    newVal = pow(10.0, logVal);
                } else {
                    // 线性域更新: new = old + delta
                    newVal = oldVal + delta[i];
                }

                // 强制约束参数范围 (Min/Max)
                newVal = qMax(params[pIdx].min, qMin(newVal, params[pIdx].max));
                trialMap[pName] = newVal;
            }

            // 参数联动更新
            if(trialMap.contains("L") && trialMap.contains("Lf") && trialMap["L"] > 1e-9)
                trialMap["LfD"] = trialMap["Lf"] / trialMap["L"];

            // 计算新参数下的残差和误差
            QVector<double> newRes = calculateResiduals(trialMap, modelType, weight);
            double newSSE = calculateSumSquaredError(newRes);

            // 6. 评估更新结果
            if(newSSE < currentSSE) {
                // 成功：接受新参数，减小阻尼因子，进入下一次迭代
                currentSSE = newSSE;
                currentParamMap = trialMap;
                residuals = newRes;
                lambda /= 10.0;
                stepAccepted = true;

                // 刷新界面曲线
                ModelCurveData iterCurve = m_modelManager->calculateTheoreticalCurve(modelType, currentParamMap);
                emit sigIterationUpdated(currentSSE/nRes, currentParamMap, std::get<0>(iterCurve), std::get<1>(iterCurve), std::get<2>(iterCurve));
                break;
            } else {
                // 失败：误差增加，拒绝更新，增大阻尼因子重试
                lambda *= 10.0;
            }
        }

        // 如果 lambda 过大仍无法下降，认为已陷入局部极小值，终止
        if(!stepAccepted && lambda > 1e10) break;
    }

    // 7. 拟合结束处理
    // 恢复高精度模式并计算最终曲线
    if(m_modelManager) m_modelManager->setHighPrecision(true);

    if(currentParamMap.contains("L") && currentParamMap.contains("Lf") && currentParamMap["L"] > 1e-9)
        currentParamMap["LfD"] = currentParamMap["Lf"] / currentParamMap["L"];

    ModelCurveData finalCurve = m_modelManager->calculateTheoreticalCurve(modelType, currentParamMap);
    emit sigIterationUpdated(currentSSE/residuals.size(), currentParamMap, std::get<0>(finalCurve), std::get<1>(finalCurve), std::get<2>(finalCurve));

    // 通知主线程完成
    QMetaObject::invokeMethod(this, "onFitFinished");
}

/**
 * @brief 计算残差向量
 * @return 包含压差残差和导数残差的向量
 */
QVector<double> FittingWidget::calculateResiduals(const QMap<QString, double>& params, ModelManager::ModelType modelType, double weight) {
    if(!m_modelManager || m_obsTime.isEmpty()) return QVector<double>();

    // 调用模型管理器计算理论曲线
    ModelCurveData res = m_modelManager->calculateTheoreticalCurve(modelType, params, m_obsTime);
    const QVector<double>& pCal = std::get<1>(res);
    const QVector<double>& dpCal = std::get<2>(res);

    QVector<double> r;
    double wp = weight;
    double wd = 1.0 - weight;

    // 计算压差残差 (基于对数差，更符合试井双对数图的拟合需求)
    // 注意：m_obsDeltaP 已经是压差
    int count = qMin(m_obsDeltaP.size(), pCal.size());
    for(int i=0; i<count; ++i) {
        if(m_obsDeltaP[i] > 1e-10 && pCal[i] > 1e-10)
            r.append( (log(m_obsDeltaP[i]) - log(pCal[i])) * wp );
        else
            r.append(0.0);
    }

    // 计算导数残差
    int dCount = qMin(m_obsDerivative.size(), dpCal.size());
    dCount = qMin(dCount, count);
    for(int i=0; i<dCount; ++i) {
        if(m_obsDerivative[i] > 1e-10 && dpCal[i] > 1e-10)
            r.append( (log(m_obsDerivative[i]) - log(dpCal[i])) * wd );
        else
            r.append(0.0);
    }
    return r;
}

/**
 * @brief 计算雅可比矩阵 (数值微分法)
 * @return J 矩阵
 */
QVector<QVector<double>> FittingWidget::computeJacobian(const QMap<QString, double>& params, const QVector<double>& baseResiduals, const QVector<int>& fitIndices, ModelManager::ModelType modelType, const QList<FitParameter>& currentFitParams, double weight) {
    int nRes = baseResiduals.size();
    int nParams = fitIndices.size();
    QVector<QVector<double>> J(nRes, QVector<double>(nParams));

    for(int j = 0; j < nParams; ++j) {
        int idx = fitIndices[j];
        QString pName = currentFitParams[idx].name;
        double val = params.value(pName);
        bool isLog = (val > 1e-12 && pName != "S" && pName != "nf");

        // 计算中心差分步长 h
        double h;
        QMap<QString, double> pPlus = params;
        QMap<QString, double> pMinus = params;

        if(isLog) {
            h = 0.01; // 对数域步长
            double valLog = log10(val);
            pPlus[pName] = pow(10.0, valLog + h);
            pMinus[pName] = pow(10.0, valLog - h);
        } else {
            h = 1e-4; // 线性域步长
            pPlus[pName] = val + h;
            pMinus[pName] = val - h;
        }

        // 联动更新
        auto updateDeps = [](QMap<QString,double>& map) { if(map.contains("L") && map.contains("Lf") && map["L"] > 1e-9) map["LfD"] = map["Lf"] / map["L"]; };
        if(pName == "L" || pName == "Lf") { updateDeps(pPlus); updateDeps(pMinus); }

        // 分别计算正向扰动和负向扰动的残差
        QVector<double> rPlus = calculateResiduals(pPlus, modelType, weight);
        QVector<double> rMinus = calculateResiduals(pMinus, modelType, weight);

        // 中心差分公式: df/dx = (f(x+h) - f(x-h)) / 2h
        if(rPlus.size() == nRes && rMinus.size() == nRes) {
            for(int i=0; i<nRes; ++i) {
                J[i][j] = (rPlus[i] - rMinus[i]) / (2.0 * h);
            }
        }
    }
    return J;
}

/**
 * @brief 求解线性方程组 Ax = b
 * 说明：使用 Eigen 库的 LDLT 分解求解对称正定矩阵，稳定性好。
 */
QVector<double> FittingWidget::solveLinearSystem(const QVector<QVector<double>>& A, const QVector<double>& b) {
    int n = b.size();
    if (n == 0) return QVector<double>();

    Eigen::MatrixXd matA(n, n);
    Eigen::VectorXd vecB(n);

    // 将 QVector 转换为 Eigen 数据格式
    for (int i = 0; i < n; ++i) {
        vecB(i) = b[i];
        for (int j = 0; j < n; ++j) {
            matA(i, j) = A[i][j];
        }
    }

    // 求解
    Eigen::VectorXd x = matA.ldlt().solve(vecB);

    // 转换回 QVector
    QVector<double> res(n);
    for (int i = 0; i < n; ++i) res[i] = x(i);
    return res;
}

/**
 * @brief 计算误差平方和 (SSE)
 */
double FittingWidget::calculateSumSquaredError(const QVector<double>& residuals) {
    double sse = 0.0;
    for(double v : residuals) sse += v*v;
    return sse;
}

// ===========================================================================
// 其他辅助逻辑
// ===========================================================================

/**
 * @brief 更新理论模型曲线（非拟合状态下手动调用）
 */
void FittingWidget::updateModelCurve() {
    if(!m_modelManager) {
        QMessageBox::critical(this, "错误", "ModelManager 未初始化！");
        return;
    }
    ui->tableParams->clearFocus();

    m_paramChart->updateParamsFromTable();
    QList<FitParameter> params = m_paramChart->getParameters();

    QMap<QString,double> currentParams;
    for(const auto& p : params) currentParams.insert(p.name, p.value);

    // 联动处理
    if(currentParams.contains("L") && currentParams.contains("Lf") && currentParams["L"] > 1e-9)
        currentParams["LfD"] = currentParams["Lf"] / currentParams["L"];
    else
        currentParams["LfD"] = 0.0;

    ModelManager::ModelType type = m_currentModelType;
    QVector<double> targetT = m_obsTime;
    // 如果没有观测数据，使用默认的时间序列绘制预览曲线
    if(targetT.isEmpty()) {
        for(double e = -4; e <= 4; e += 0.1) targetT.append(pow(10, e));
    }

    ModelCurveData res = m_modelManager->calculateTheoreticalCurve(type, currentParams, targetT);
    // 直接复用 onIterationUpdate 来刷新界面
    onIterationUpdate(0, currentParams, std::get<0>(res), std::get<1>(res), std::get<2>(res));
}

/**
 * @brief 界面刷新槽函数（处理迭代更新）
 */
void FittingWidget::onIterationUpdate(double err, const QMap<QString,double>& p,
                                      const QVector<double>& t, const QVector<double>& p_curve, const QVector<double>& d_curve) {
    // 更新误差标签
    ui->label_Error->setText(QString("误差(MSE): %1").arg(err, 0, 'e', 3));

    // 更新参数表中的数值
    ui->tableParams->blockSignals(true);
    for(int i=0; i<ui->tableParams->rowCount(); ++i) {
        QString key = ui->tableParams->item(i, 1)->data(Qt::UserRole).toString();
        if(p.contains(key)) {
            double val = p[key];
            ui->tableParams->item(i, 2)->setText(QString::number(val, 'g', 5));
        }
    }
    ui->tableParams->blockSignals(false);

    // 绘制曲线
    plotCurves(t, p_curve, d_curve, true);
}

/**
 * @brief 拟合完成槽函数
 */
void FittingWidget::onFitFinished() {
    m_isFitting = false;
    ui->btnRunFit->setEnabled(true);
    QMessageBox::information(this, "完成", "拟合完成。");
}

/**
 * @brief 绘制图表曲线
 */
void FittingWidget::plotCurves(const QVector<double>& t, const QVector<double>& p, const QVector<double>& d, bool isModel) {
    QVector<double> vt, vp, vd;
    // 过滤绘图数据
    for(int i=0; i<t.size(); ++i) {
        if(t[i]>1e-8 && p[i]>1e-8) {
            vt<<t[i];
            vp<<p[i];
            if(i<d.size() && d[i]>1e-8) vd<<d[i]; else vd<<1e-10;
        }
    }
    if(isModel) {
        m_plot->graph(2)->setData(vt, vp);
        m_plot->graph(3)->setData(vt, vd);

        // 如果没有观测数据，自动缩放以显示模型曲线
        if (m_obsTime.isEmpty() && !vt.isEmpty()) {
            m_plot->rescaleAxes();
            if(m_plot->xAxis->range().lower<=0) m_plot->xAxis->setRangeLower(1e-3);
            if(m_plot->yAxis->range().lower<=0) m_plot->yAxis->setRangeLower(1e-3);
        }
        m_plot->replot();
    }
}

/**
 * @brief 导出 HTML 报告槽函数
 */
void FittingWidget::on_btnExportReport_clicked()
{
    m_paramChart->updateParamsFromTable();
    QList<FitParameter> params = m_paramChart->getParameters();

    QString defaultDir = ModelParameter::instance()->getProjectPath();
    if(defaultDir.isEmpty()) defaultDir = ".";
    QString fileName = QFileDialog::getSaveFileName(this, "导出试井分析报告",
                                                    defaultDir + "/WellTestReport.doc",
                                                    "Word 文档 (*.doc);;HTML 文件 (*.html)");
    if(fileName.isEmpty()) return;

    ModelParameter* mp = ModelParameter::instance();

    // 构建 HTML 内容
    QString html = "<html><head><style>";
    html += "body { font-family: 'Times New Roman', 'SimSun', serif; }";
    html += "h1 { text-align: center; font-size: 24px; font-weight: bold; margin-bottom: 20px; }";
    html += "h2 { font-size: 18px; font-weight: bold; background-color: #f2f2f2; padding: 5px; border-left: 5px solid #2d89ef; margin-top: 20px; }";
    html += "table { width: 100%; border-collapse: collapse; margin-bottom: 15px; font-size: 14px; }";
    html += "td, th { border: 1px solid #888; padding: 6px; text-align: center; }";
    html += "th { background-color: #e0e0e0; font-weight: bold; }";
    html += ".param-table td { text-align: left; padding-left: 10px; }";
    html += "</style></head><body>";

    html += "<h1>试井解释分析报告</h1>";
    html += "<p style='text-align:right;'>生成日期: " + QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm") + "</p>";

    html += "<h2>1. 基础信息</h2>";
    html += "<table class='param-table'>";
    html += "<tr><td width='30%'>项目路径</td><td>" + mp->getProjectPath() + "</td></tr>";
    html += "<tr><td>测试产量 (q)</td><td>" + QString::number(mp->getQ()) + " m³/d</td></tr>";
    html += "<tr><td>有效厚度 (h)</td><td>" + QString::number(mp->getH()) + " m</td></tr>";
    html += "<tr><td>孔隙度 (φ)</td><td>" + QString::number(mp->getPhi()) + "</td></tr>";
    html += "<tr><td>井筒半径 (rw)</td><td>" + QString::number(mp->getRw()) + " m</td></tr>";
    html += "</table>";

    html += "<h2>2. 流体高压物性 (PVT)</h2>";
    html += "<table class='param-table'>";
    html += "<tr><td width='30%'>原油粘度 (μ)</td><td>" + QString::number(mp->getMu()) + " mPa·s</td></tr>";
    html += "<tr><td>体积系数 (B)</td><td>" + QString::number(mp->getB()) + "</td></tr>";
    html += "<tr><td>综合压缩系数 (Ct)</td><td>" + QString::number(mp->getCt()) + " MPa⁻¹</td></tr>";
    html += "</table>";

    html += "<h2>3. 解释模型选择</h2>";
    html += "<p><strong>当前模型:</strong> " + ModelManager::getModelTypeName(m_currentModelType) + "</p>";

    html += "<h2>4. 拟合结果参数</h2>";
    html += "<table>";
    html += "<tr><th>参数名称</th><th>符号</th><th>拟合结果</th><th>单位</th></tr>";
    for(const auto& p : params) {
        QString dummy, symbol, uniSym, unit;
        FittingParameterChart::getParamDisplayInfo(p.name, dummy, symbol, uniSym, unit);
        if(unit == "无因次" || unit == "小数") unit = "-";

        html += "<tr>";
        html += "<td>" + p.displayName + "</td>";
        html += "<td>" + uniSym + "</td>";
        if(p.isFit)
            html += "<td><strong>" + QString::number(p.value, 'g', 6) + "</strong></td>";
        else
            html += "<td>" + QString::number(p.value, 'g', 6) + "</td>";
        html += "<td>" + unit + "</td>";
        html += "</tr>";
    }
    html += "</table>";

    html += "<h2>5. 拟合曲线图</h2>";
    QString imgBase64 = getPlotImageBase64();
    if(!imgBase64.isEmpty()) {
        html += "<div style='text-align:center;'><img src='data:image/png;base64," + imgBase64 + "' width='600' /></div>";
    } else {
        html += "<p>图像导出失败。</p>";
    }

    html += "</body></html>";

    QFile file(fileName);
    if(file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&file);
        out.setEncoding(QStringConverter::Utf8);
        out << html;
        file.close();
        QMessageBox::information(this, "导出成功", "报告已保存至:\n" + fileName);
    } else {
        QMessageBox::critical(this, "错误", "无法写入文件，请检查权限或文件是否被占用。");
    }
}

/**
 * @brief 获取图表图像的 Base64 编码 (用于嵌入报告)
 */
QString FittingWidget::getPlotImageBase64()
{
    if(!m_plot) return "";
    QPixmap pixmap = m_plot->toPixmap(800, 600);
    QByteArray byteArray;
    QBuffer buffer(&byteArray);
    buffer.open(QIODevice::WriteOnly);
    pixmap.save(&buffer, "PNG");
    return QString::fromLatin1(byteArray.toBase64().data());
}

// 响应保存请求信号
void FittingWidget::on_btnSaveFit_clicked()
{
    emit sigRequestSave();
}

/**
 * @brief 获取当前界面状态（序列化为 JSON）
 */
QJsonObject FittingWidget::getJsonState() const
{
    const_cast<FittingWidget*>(this)->m_paramChart->updateParamsFromTable();
    QList<FitParameter> params = m_paramChart->getParameters();

    QJsonObject root;
    root["modelType"] = (int)m_currentModelType;
    root["modelName"] = ModelManager::getModelTypeName(m_currentModelType);
    root["fitWeightVal"] = ui->sliderWeight->value();

    QJsonObject plotRange;
    plotRange["xMin"] = m_plot->xAxis->range().lower;
    plotRange["xMax"] = m_plot->xAxis->range().upper;
    plotRange["yMin"] = m_plot->yAxis->range().lower;
    plotRange["yMax"] = m_plot->yAxis->range().upper;
    root["plotView"] = plotRange;

    QJsonArray paramsArray;
    for(const auto& p : params) {
        QJsonObject pObj;
        pObj["name"] = p.name;
        pObj["value"] = p.value;
        pObj["isFit"] = p.isFit;
        pObj["min"] = p.min;
        pObj["max"] = p.max;
        pObj["isVisible"] = p.isVisible;
        paramsArray.append(pObj);
    }
    root["parameters"] = paramsArray;

    QJsonArray timeArr, pressArr, derivArr;
    for(double v : m_obsTime) timeArr.append(v);
    for(double v : m_obsDeltaP) pressArr.append(v);
    for(double v : m_obsDerivative) derivArr.append(v);
    QJsonObject obsData;
    obsData["time"] = timeArr;
    obsData["pressure"] = pressArr;
    obsData["derivative"] = derivArr;
    root["observedData"] = obsData;

    return root;
}

/**
 * @brief 从 JSON 恢复界面状态
 */
void FittingWidget::loadFittingState(const QJsonObject& root)
{
    if (root.isEmpty()) return;

    if (root.contains("modelType")) {
        int type = root["modelType"].toInt();
        m_currentModelType = (ModelManager::ModelType)type;
        ui->btn_modelSelect->setText("当前: " + ModelManager::getModelTypeName(m_currentModelType));
    }

    m_paramChart->resetParams(m_currentModelType);

    if (root.contains("parameters")) {
        QJsonArray arr = root["parameters"].toArray();
        QList<FitParameter> currentParams = m_paramChart->getParameters();

        for(int i=0; i<arr.size(); ++i) {
            QJsonObject pObj = arr[i].toObject();
            QString name = pObj["name"].toString();

            for(auto& p : currentParams) {
                if(p.name == name) {
                    p.value = pObj["value"].toDouble();
                    p.isFit = pObj["isFit"].toBool();
                    p.min = pObj["min"].toDouble();
                    p.max = pObj["max"].toDouble();
                    if(pObj.contains("isVisible")) {
                        p.isVisible = pObj["isVisible"].toBool();
                    } else {
                        p.isVisible = true;
                    }
                    break;
                }
            }
        }
        m_paramChart->setParameters(currentParams);
    }

    if (root.contains("fitWeightVal")) {
        int val = root["fitWeightVal"].toInt();
        ui->sliderWeight->setValue(val);
    } else if (root.contains("fitWeight")) {
        double w = root["fitWeight"].toDouble();
        ui->sliderWeight->setValue((int)(w * 100));
    }

    if (root.contains("observedData")) {
        QJsonObject obs = root["observedData"].toObject();
        QJsonArray tArr = obs["time"].toArray();
        QJsonArray pArr = obs["pressure"].toArray();
        QJsonArray dArr = obs["derivative"].toArray();

        QVector<double> t, p, d;
        for(auto v : tArr) t.append(v.toDouble());
        for(auto v : pArr) p.append(v.toDouble());
        for(auto v : dArr) d.append(v.toDouble());

        setObservedData(t, p, d);
    }

    updateModelCurve();

    if (root.contains("plotView")) {
        QJsonObject range = root["plotView"].toObject();
        if (range.contains("xMin") && range.contains("xMax")) {
            double xMin = range["xMin"].toDouble();
            double xMax = range["xMax"].toDouble();
            double yMin = range["yMin"].toDouble();
            double yMax = range["yMax"].toDouble();
            if (xMax > xMin && yMax > yMin && xMin > 0 && yMin > 0) {
                m_plot->xAxis->setRange(xMin, xMax);
                m_plot->yAxis->setRange(yMin, yMax);
                m_plot->replot();
            }
        }
    }
}

