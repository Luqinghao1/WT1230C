/*
 * ModelWidget01-06.cpp
 * 文件作用: 压裂水平井复合页岩油模型计算实现
 * 功能描述:
 * 1. 包含6种不同边界和井储条件组合的页岩油模型。
 * 2. 界面布局：左侧参数(20%) + 右侧图表(80%)，支持拖拽调节。
 * 3. [修复] 修正了初始化时因控件未显示导致默认参数无法赋值的问题。
 */

#include "modelwidget01-06.h"
#include "ui_modelwidget01-06.h"
#include "modelmanager.h"
#include "pressurederivativecalculator.h"
#include "modelparameter.h"

#include <Eigen/Dense>
#include <boost/math/special_functions/bessel.hpp>

#include <cmath>
#include <algorithm>
#include <QDebug>
#include <QMessageBox>
#include <QFileDialog>
#include <QTextStream>
#include <QDateTime>
#include <QCoreApplication>
#include <QSplitter>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

ModelWidget01_06::ModelWidget01_06(ModelType type, QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::ModelWidget01_06)
    , m_type(type)
    , m_highPrecision(true)
{
    ui->setupUi(this);
    m_colorList = { Qt::red, Qt::blue, QColor(0,180,0), Qt::magenta, QColor(255,140,0), Qt::cyan };

    // [布局] 设置 Splitter 初始比例 (左 20% : 右 80%)
    QList<int> sizes;
    sizes << 240 << 960;
    ui->splitter->setSizes(sizes);
    ui->splitter->setCollapsible(0, false); // 左侧不可折叠

    // [界面] 设置当前模型名称到选择按钮
    ui->btnSelectModel->setText(getModelName() + "  (点击切换)");

    initUi();
    initChart();
    setupConnections();
    onResetParameters();
}

ModelWidget01_06::~ModelWidget01_06() { delete ui; }

QString ModelWidget01_06::getModelName() const {
    switch(m_type) {
    case Model_1: return "模型1: 变井储+无限大边界";
    case Model_2: return "模型2: 恒定井储+无限大边界";
    case Model_3: return "模型3: 变井储+封闭边界";
    case Model_4: return "模型4: 恒定井储+封闭边界";
    case Model_5: return "模型5: 变井储+定压边界";
    case Model_6: return "模型6: 恒定井储+定压边界";
    default: return "未知模型";
    }
}

void ModelWidget01_06::initUi() {
    if (m_type == Model_1 || m_type == Model_2) {
        ui->label_reD->setVisible(false);
        ui->reDEdit->setVisible(false);
    } else {
        ui->label_reD->setVisible(true);
        ui->reDEdit->setVisible(true);
    }

    bool hasStorage = (m_type == Model_1 || m_type == Model_3 || m_type == Model_5);
    ui->label_cD->setVisible(hasStorage);
    ui->cDEdit->setVisible(hasStorage);
    ui->label_s->setVisible(hasStorage);
    ui->sEdit->setVisible(hasStorage);
}

void ModelWidget01_06::initChart() {
    MouseZoom* plot = ui->chartWidget->getPlot();

    plot->setBackground(Qt::white);
    plot->axisRect()->setBackground(Qt::white);

    QSharedPointer<QCPAxisTickerLog> logTicker(new QCPAxisTickerLog);
    plot->xAxis->setScaleType(QCPAxis::stLogarithmic); plot->xAxis->setTicker(logTicker);
    plot->yAxis->setScaleType(QCPAxis::stLogarithmic); plot->yAxis->setTicker(logTicker);
    plot->xAxis->setNumberFormat("eb"); plot->xAxis->setNumberPrecision(0);
    plot->yAxis->setNumberFormat("eb"); plot->yAxis->setNumberPrecision(0);

    QFont labelFont("Microsoft YaHei", 10, QFont::Bold);
    QFont tickFont("Microsoft YaHei", 9);
    plot->xAxis->setLabel("时间 Time (h)");
    plot->yAxis->setLabel("压力 & 导数 Pressure & Derivative (MPa)");
    plot->xAxis->setLabelFont(labelFont); plot->yAxis->setLabelFont(labelFont);
    plot->xAxis->setTickLabelFont(tickFont); plot->yAxis->setTickLabelFont(tickFont);

    plot->xAxis2->setVisible(true); plot->yAxis2->setVisible(true);
    plot->xAxis2->setTickLabels(false); plot->yAxis2->setTickLabels(false);
    connect(plot->xAxis, SIGNAL(rangeChanged(QCPRange)), plot->xAxis2, SLOT(setRange(QCPRange)));
    connect(plot->yAxis, SIGNAL(rangeChanged(QCPRange)), plot->yAxis2, SLOT(setRange(QCPRange)));
    plot->xAxis2->setScaleType(QCPAxis::stLogarithmic); plot->yAxis2->setScaleType(QCPAxis::stLogarithmic);
    plot->xAxis2->setTicker(logTicker); plot->yAxis2->setTicker(logTicker);

    plot->xAxis->grid()->setVisible(true); plot->yAxis->grid()->setVisible(true);
    plot->xAxis->grid()->setSubGridVisible(true); plot->yAxis->grid()->setSubGridVisible(true);
    plot->xAxis->grid()->setPen(QPen(QColor(220, 220, 220), 1, Qt::SolidLine));
    plot->yAxis->grid()->setPen(QPen(QColor(220, 220, 220), 1, Qt::SolidLine));
    plot->xAxis->grid()->setSubGridPen(QPen(QColor(240, 240, 240), 1, Qt::DotLine));
    plot->yAxis->grid()->setSubGridPen(QPen(QColor(240, 240, 240), 1, Qt::DotLine));

    plot->xAxis->setRange(1e-3, 1e3); plot->yAxis->setRange(1e-3, 1e2);

    plot->legend->setVisible(true);
    plot->legend->setFont(QFont("Microsoft YaHei", 9));
    plot->legend->setBrush(QBrush(QColor(255, 255, 255, 200)));

    ui->chartWidget->setTitle("复合页岩油储层试井曲线");
}

void ModelWidget01_06::setupConnections() {
    connect(ui->calculateButton, &QPushButton::clicked, this, &ModelWidget01_06::onCalculateClicked);
    connect(ui->resetButton, &QPushButton::clicked, this, &ModelWidget01_06::onResetParameters);
    connect(ui->chartWidget, &ChartWidget::exportDataTriggered, this, &ModelWidget01_06::onExportData);
    connect(ui->btnExportDataTab, &QPushButton::clicked, this, &ModelWidget01_06::onExportData);
    connect(ui->LEdit, &QLineEdit::editingFinished, this, &ModelWidget01_06::onDependentParamsChanged);
    connect(ui->LfEdit, &QLineEdit::editingFinished, this, &ModelWidget01_06::onDependentParamsChanged);
    connect(ui->checkShowPoints, &QCheckBox::toggled, this, &ModelWidget01_06::onShowPointsToggled);

    // [新增] 转发模型选择按钮信号
    connect(ui->btnSelectModel, &QPushButton::clicked, this, &ModelWidget01_06::requestModelSelection);
}

void ModelWidget01_06::setHighPrecision(bool high) { m_highPrecision = high; }

QVector<double> ModelWidget01_06::parseInput(const QString& text) {
    QVector<double> values;
    QString cleanText = text;
    cleanText.replace("，", ",");
    QStringList parts = cleanText.split(",", Qt::SkipEmptyParts);
    for(const QString& part : parts) {
        bool ok;
        double v = part.trimmed().toDouble(&ok);
        if(ok) values.append(v);
    }
    if(values.isEmpty()) values.append(0.0);
    return values;
}

void ModelWidget01_06::setInputText(QLineEdit* edit, double value) {
    if(!edit) return;
    edit->setText(QString::number(value, 'g', 8));
}

// [核心修复] 重置参数函数
void ModelWidget01_06::onResetParameters() {
    ModelParameter* mp = ModelParameter::instance();

    setInputText(ui->phiEdit, mp->getPhi());
    setInputText(ui->hEdit, mp->getH());
    setInputText(ui->muEdit, mp->getMu());
    setInputText(ui->BEdit, mp->getB());
    setInputText(ui->CtEdit, mp->getCt());
    setInputText(ui->qEdit, mp->getQ());

    setInputText(ui->tEdit, 1000.0);
    setInputText(ui->pointsEdit, 100);

    setInputText(ui->kfEdit, 1e-3);
    setInputText(ui->kmEdit, 1e-4);
    setInputText(ui->LEdit, 1000.0);
    setInputText(ui->LfEdit, 100.0);
    setInputText(ui->nfEdit, 4);
    setInputText(ui->rmDEdit, 4.0);
    setInputText(ui->omga1Edit, 0.4);
    setInputText(ui->omga2Edit, 0.08);
    setInputText(ui->remda1Edit, 0.001);
    setInputText(ui->gamaDEdit, 0.02);

    // [修复] 不使用 isVisible() 判断，因为在初始化时可能为 false
    // 而是直接根据模型类型判断是否应该赋值
    bool isInfinite = (m_type == Model_1 || m_type == Model_2);
    if (!isInfinite) {
        setInputText(ui->reDEdit, 10.0);
    }

    bool hasStorage = (m_type == Model_1 || m_type == Model_3 || m_type == Model_5);
    if (hasStorage) {
        setInputText(ui->cDEdit, 0.01);
        setInputText(ui->sEdit, 1.0);
    }

    onDependentParamsChanged();
}

void ModelWidget01_06::onDependentParamsChanged() {
    double L = parseInput(ui->LEdit->text()).first();
    double Lf = parseInput(ui->LfEdit->text()).first();
    if (L > 1e-9) setInputText(ui->LfDEdit, Lf / L);
    else setInputText(ui->LfDEdit, 0.0);
}

void ModelWidget01_06::onShowPointsToggled(bool checked) {
    MouseZoom* plot = ui->chartWidget->getPlot();
    for(int i = 0; i < plot->graphCount(); ++i) {
        if (checked) plot->graph(i)->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssDisc, 5));
        else plot->graph(i)->setScatterStyle(QCPScatterStyle::ssNone);
    }
    plot->replot();
}

void ModelWidget01_06::onCalculateClicked() {
    ui->calculateButton->setEnabled(false);
    ui->calculateButton->setText("计算中...");
    QCoreApplication::processEvents();
    runCalculation();
    ui->calculateButton->setEnabled(true);
    ui->calculateButton->setText("开始计算");
}

void ModelWidget01_06::runCalculation() {
    MouseZoom* plot = ui->chartWidget->getPlot();
    plot->clearGraphs();

    QMap<QString, QVector<double>> rawParams;
    rawParams["phi"] = parseInput(ui->phiEdit->text());
    rawParams["h"] = parseInput(ui->hEdit->text());
    rawParams["mu"] = parseInput(ui->muEdit->text());
    rawParams["B"] = parseInput(ui->BEdit->text());
    rawParams["Ct"] = parseInput(ui->CtEdit->text());
    rawParams["q"] = parseInput(ui->qEdit->text());
    rawParams["t"] = parseInput(ui->tEdit->text());

    rawParams["kf"] = parseInput(ui->kfEdit->text());
    rawParams["km"] = parseInput(ui->kmEdit->text());
    rawParams["L"] = parseInput(ui->LEdit->text());
    rawParams["Lf"] = parseInput(ui->LfEdit->text());
    rawParams["nf"] = parseInput(ui->nfEdit->text());
    rawParams["rmD"] = parseInput(ui->rmDEdit->text());
    rawParams["omega1"] = parseInput(ui->omga1Edit->text());
    rawParams["omega2"] = parseInput(ui->omga2Edit->text());
    rawParams["lambda1"] = parseInput(ui->remda1Edit->text());
    rawParams["gamaD"] = parseInput(ui->gamaDEdit->text());

    if (ui->reDEdit->isVisible()) rawParams["reD"] = parseInput(ui->reDEdit->text());
    else rawParams["reD"] = {0.0};

    if (ui->cDEdit->isVisible()) {
        rawParams["cD"] = parseInput(ui->cDEdit->text());
        rawParams["S"] = parseInput(ui->sEdit->text());
    } else {
        rawParams["cD"] = {0.0};
        rawParams["S"] = {0.0};
    }

    QString sensitivityKey = "";
    QVector<double> sensitivityValues;
    for(auto it = rawParams.begin(); it != rawParams.end(); ++it) {
        if(it.key() == "t") continue;
        if(it.value().size() > 1) {
            sensitivityKey = it.key();
            sensitivityValues = it.value();
            break;
        }
    }
    bool isSensitivity = !sensitivityKey.isEmpty();

    QMap<QString, double> baseParams;
    for(auto it = rawParams.begin(); it != rawParams.end(); ++it) {
        baseParams[it.key()] = it.value().isEmpty() ? 0.0 : it.value().first();
    }
    baseParams["N"] = m_highPrecision ? 8.0 : 4.0;
    if(baseParams["L"] > 1e-9) baseParams["LfD"] = baseParams["Lf"] / baseParams["L"];
    else baseParams["LfD"] = 0;

    int nPoints = ui->pointsEdit->text().toInt();
    if(nPoints < 5) nPoints = 5;

    double maxTime = baseParams.value("t", 1000.0);
    if(maxTime < 1e-3) maxTime = 1000.0;
    QVector<double> t = ModelManager::generateLogTimeSteps(nPoints, -3.0, log10(maxTime));

    int iterations = isSensitivity ? sensitivityValues.size() : 1;
    iterations = qMin(iterations, (int)m_colorList.size());

    QString resultTextHeader = QString("计算完成 (%1)\n").arg(getModelName());
    if(isSensitivity) resultTextHeader += QString("敏感性参数: %1\n").arg(sensitivityKey);

    for(int i = 0; i < iterations; ++i) {
        QMap<QString, double> currentParams = baseParams;
        double val = 0;
        if (isSensitivity) {
            val = sensitivityValues[i];
            currentParams[sensitivityKey] = val;
            if (sensitivityKey == "L" || sensitivityKey == "Lf") {
                if(currentParams["L"] > 1e-9) currentParams["LfD"] = currentParams["Lf"] / currentParams["L"];
            }
        }

        ModelCurveData res = calculateTheoreticalCurve(currentParams, t);
        res_tD = std::get<0>(res);
        res_pD = std::get<1>(res);
        res_dpD = std::get<2>(res);

        QColor curveColor = isSensitivity ? m_colorList[i] : Qt::red;
        QString legendName;
        if (isSensitivity) legendName = QString("%1 = %2").arg(sensitivityKey).arg(val);
        else legendName = "理论曲线";

        plotCurve(res, legendName, curveColor, isSensitivity);
    }

    QString resultText = resultTextHeader;
    resultText += "t(h)\t\tDp(MPa)\t\tdDp(MPa)\n";
    for(int i=0; i<res_pD.size(); ++i) {
        resultText += QString("%1\t%2\t%3\n").arg(res_tD[i],0,'e',4).arg(res_pD[i],0,'e',4).arg(res_dpD[i],0,'e',4);
    }
    ui->resultTextEdit->setText(resultText);

    ui->chartWidget->getPlot()->rescaleAxes();
    if(plot->xAxis->range().lower <= 0) plot->xAxis->setRangeLower(1e-3);
    if(plot->yAxis->range().lower <= 0) plot->yAxis->setRangeLower(1e-3);
    plot->replot();

    onShowPointsToggled(ui->checkShowPoints->isChecked());
    emit calculationCompleted(getModelName(), baseParams);
}

void ModelWidget01_06::plotCurve(const ModelCurveData& data, const QString& name, QColor color, bool isSensitivity) {
    MouseZoom* plot = ui->chartWidget->getPlot();

    const QVector<double>& t = std::get<0>(data);
    const QVector<double>& p = std::get<1>(data);
    const QVector<double>& d = std::get<2>(data);

    QCPGraph* graphP = plot->addGraph();
    graphP->setData(t, p);
    graphP->setPen(QPen(color, 2, Qt::SolidLine));

    QCPGraph* graphD = plot->addGraph();
    graphD->setData(t, d);

    if (isSensitivity) {
        graphD->setPen(QPen(color, 2, Qt::DashLine));
        graphP->setName(name);
        graphD->removeFromLegend();
    } else {
        graphP->setPen(QPen(Qt::red, 2));
        graphP->setName("压力");
        graphD->setPen(QPen(Qt::blue, 2));
        graphD->setName("压力导数");
    }
}

void ModelWidget01_06::onExportData() {
    if (res_tD.isEmpty()) return;
    QString defaultDir = ModelParameter::instance()->getProjectPath();
    if(defaultDir.isEmpty()) defaultDir = ".";
    QString path = QFileDialog::getSaveFileName(this, "导出CSV数据", defaultDir + "/CalculatedData.csv", "CSV Files (*.csv)");
    if (path.isEmpty()) return;
    QFile f(path);
    if (f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&f);
        out << "t,Dp,dDp\n";
        for (int i = 0; i < res_tD.size(); ++i) {
            double dp = (i < res_dpD.size()) ? res_dpD[i] : 0.0;
            out << res_tD[i] << "," << res_pD[i] << "," << dp << "\n";
        }
        f.close();
        QMessageBox::information(this, "导出成功", "数据文件已保存");
    }
}

ModelCurveData ModelWidget01_06::calculateTheoreticalCurve(const QMap<QString, double>& params, const QVector<double>& providedTime)
{
    QVector<double> tPoints = providedTime;
    if (tPoints.isEmpty()) {
        tPoints = ModelManager::generateLogTimeSteps(100, -3.0, 3.0);
    }

    double phi = params.value("phi", 0.05);
    double mu = params.value("mu", 0.5);
    double B = params.value("B", 1.05);
    double Ct = params.value("Ct", 5e-4);
    double q = params.value("q", 5.0);
    double h = params.value("h", 20.0);
    double kf = params.value("kf", 1e-3);
    double L = params.value("L", 1000.0);

    QVector<double> tD_vec;
    tD_vec.reserve(tPoints.size());
    for(double t : tPoints) {
        double val = 14.4 * kf * t / (phi * mu * Ct * pow(L, 2));
        tD_vec.append(val);
    }

    QVector<double> PD_vec, Deriv_vec;
    auto func = std::bind(&ModelWidget01_06::flaplace_composite, this, std::placeholders::_1, std::placeholders::_2);
    calculatePDandDeriv(tD_vec, params, func, PD_vec, Deriv_vec);

    double factor = 1.842e-3 * q * mu * B / (kf * h);
    QVector<double> finalP(tPoints.size()), finalDP(tPoints.size());

    for(int i=0; i<tPoints.size(); ++i) {
        finalP[i] = factor * PD_vec[i];
        finalDP[i] = factor * Deriv_vec[i];
    }

    return std::make_tuple(tPoints, finalP, finalDP);
}

void ModelWidget01_06::calculatePDandDeriv(const QVector<double>& tD, const QMap<QString, double>& params,
                                           std::function<double(double, const QMap<QString, double>&)> laplaceFunc,
                                           QVector<double>& outPD, QVector<double>& outDeriv)
{
    int numPoints = tD.size();
    outPD.resize(numPoints);
    outDeriv.resize(numPoints);

    int N_param = (int)params.value("N", 4);
    int N = m_highPrecision ? N_param : 4;
    if (N % 2 != 0) N = 4;
    double ln2 = log(2.0);

    double gamaD = params.value("gamaD", 0.0);

    for (int k = 0; k < numPoints; ++k) {
        double t = tD[k];
        if (t <= 1e-12) { outPD[k] = 0; continue; }
        double pd_val = 0.0;
        for (int m = 1; m <= N; ++m) {
            double z = m * ln2 / t;
            double pf = laplaceFunc(z, params);
            if (std::isnan(pf) || std::isinf(pf)) pf = 0.0;
            pd_val += stefestCoefficient(m, N) * pf;
        }
        outPD[k] = pd_val * ln2 / t;

        if (std::abs(gamaD) > 1e-9) {
            double arg = 1.0 - gamaD * outPD[k];
            if (arg > 1e-12) {
                outPD[k] = -1.0 / gamaD * std::log(arg);
            }
        }
    }
    if (numPoints > 2) outDeriv = PressureDerivativeCalculator::calculateBourdetDerivative(tD, outPD, 0.1);
    else outDeriv.fill(0.0);
}

double ModelWidget01_06::flaplace_composite(double z, const QMap<QString, double>& p) {
    double kf = p.value("kf");
    double km = p.value("km");
    double LfD = p.value("LfD");
    double rmD = p.value("rmD");
    double reD = p.value("reD", 0.0);
    double omga1 = p.value("omega1");
    double omga2 = p.value("omega2");
    double remda1 = p.value("lambda1");
    int nf = (int)p.value("nf", 4); if(nf < 1) nf = 1;
    double M12 = kf / km;
    QVector<double> xwD;
    if (nf == 1) { xwD.append(0.0); } else {
        double start = -0.9; double end = 0.9; double step = (end - start) / (nf - 1);
        for(int i=0; i<nf; ++i) xwD.append(start + i * step);
    }
    double temp = omga2;
    double fs1 = omga1 + remda1 * temp / (remda1 + z * temp);
    double fs2 = M12 * temp;

    double pf = PWD_composite(z, fs1, fs2, M12, LfD, rmD, reD, nf, xwD, m_type);

    bool hasStorage = (m_type == Model_1 || m_type == Model_3 || m_type == Model_5);
    if (hasStorage) {
        double CD = p.value("cD", 0.0);
        double S = p.value("S", 0.0);
        if (CD > 1e-12 || std::abs(S) > 1e-12) {
            pf = (z * pf + S) / (z + CD * z * z * (z * pf + S));
        }
    }

    return pf;
}

double ModelWidget01_06::PWD_composite(double z, double fs1, double fs2, double M12, double LfD, double rmD, double reD, int nf, const QVector<double>& xwD, ModelType type) {
    using namespace boost::math;
    QVector<double> ywD(nf, 0.0);
    double gama1 = sqrt(z * fs1);
    double gama2 = sqrt(z * fs2);
    double arg_g2_rm = gama2 * rmD;
    double arg_g1_rm = gama1 * rmD;

    double k0_g2 = cyl_bessel_k(0, arg_g2_rm);
    double k1_g2 = cyl_bessel_k(1, arg_g2_rm);
    double k0_g1 = cyl_bessel_k(0, arg_g1_rm);
    double k1_g1 = cyl_bessel_k(1, arg_g1_rm);

    double term_mAB_i0 = 0.0;
    double term_mAB_i1 = 0.0;

    bool isInfinite = (type == Model_1 || type == Model_2);
    bool isClosed = (type == Model_3 || type == Model_4);
    bool isConstP = (type == Model_5 || type == Model_6);

    if (!isInfinite) {
        double arg_re = gama2 * reD;
        double i1_re_s = scaled_besseli(1, arg_re);
        double i0_re_s = scaled_besseli(0, arg_re);
        double k1_re = cyl_bessel_k(1, arg_re);
        double k0_re = cyl_bessel_k(0, arg_re);
        double i0_g2_s = scaled_besseli(0, arg_g2_rm);
        double i1_g2_s = scaled_besseli(1, arg_g2_rm);

        if (isClosed) {
            if (i1_re_s > 1e-100) {
                term_mAB_i0 = (k1_re / i1_re_s) * i0_g2_s * std::exp(arg_g2_rm - arg_re);
                term_mAB_i1 = (k1_re / i1_re_s) * i1_g2_s * std::exp(arg_g2_rm - arg_re);
            }
        } else if (isConstP) {
            if (i0_re_s > 1e-100) {
                term_mAB_i0 = -(k0_re / i0_re_s) * i0_g2_s * std::exp(arg_g2_rm - arg_re);
                term_mAB_i1 = -(k0_re / i0_re_s) * i1_g2_s * std::exp(arg_g2_rm - arg_re);
            }
        }
    }

    double term1 = term_mAB_i0 + k0_g2;
    double term2 = term_mAB_i1 - k1_g2;

    double Acup = M12 * gama1 * k1_g1 * term1 + gama2 * k0_g1 * term2;

    double i1_g1_s = scaled_besseli(1, arg_g1_rm);
    double i0_g1_s = scaled_besseli(0, arg_g1_rm);

    double Acdown_scaled = M12 * gama1 * i1_g1_s * term1 - gama2 * i0_g1_s * term2;

    if (std::abs(Acdown_scaled) < 1e-100) Acdown_scaled = 1e-100;

    double Ac_prefactor = Acup / Acdown_scaled;

    int size = nf + 1;
    Eigen::MatrixXd A_mat(size, size);
    Eigen::VectorXd b_vec(size);
    b_vec.setZero(); b_vec(nf) = 1.0;

    for (int i = 0; i < nf; ++i) {
        for (int j = 0; j < nf; ++j) {
            auto integrand = [&](double a) -> double {
                double dist = std::sqrt(std::pow(xwD[i] - xwD[j] - a, 2) + std::pow(ywD[i] - ywD[j], 2));
                double arg_dist = gama1 * dist; if (arg_dist < 1e-10) arg_dist = 1e-10;

                double term2 = 0.0;
                double exponent = arg_dist - arg_g1_rm;
                if (exponent > -700.0) {
                    term2 = Ac_prefactor * scaled_besseli(0, arg_dist) * std::exp(exponent);
                }
                return cyl_bessel_k(0, arg_dist) + term2;
            };
            double val = adaptiveGauss(integrand, -LfD, LfD, 1e-5, 0, 10);
            A_mat(i, j) = z * val / (M12 * z * 2 * LfD);
        }
    }
    for (int i = 0; i < nf; ++i) { A_mat(i, nf) = -1.0; A_mat(nf, i) = z; }
    A_mat(nf, nf) = 0.0;

    return A_mat.fullPivLu().solve(b_vec)(nf);
}

double ModelWidget01_06::scaled_besseli(int v, double x) {
    if (x < 0) x = -x;
    if (x > 600.0) return 1.0 / std::sqrt(2.0 * M_PI * x);
    return boost::math::cyl_bessel_i(v, x) * std::exp(-x);
}
double ModelWidget01_06::gauss15(std::function<double(double)> f, double a, double b) {
    static const double X[] = { 0.0, 0.201194, 0.394151, 0.570972, 0.724418, 0.848207, 0.937299, 0.987993 };
    static const double W[] = { 0.202578, 0.198431, 0.186161, 0.166269, 0.139571, 0.107159, 0.070366, 0.030753 };
    double h = 0.5 * (b - a); double c = 0.5 * (a + b); double s = W[0] * f(c);
    for (int i = 1; i < 8; ++i) { double dx = h * X[i]; s += W[i] * (f(c - dx) + f(c + dx)); }
    return s * h;
}
double ModelWidget01_06::adaptiveGauss(std::function<double(double)> f, double a, double b, double eps, int depth, int maxDepth) {
    double c = (a + b) / 2.0; double v1 = gauss15(f, a, b); double v2 = gauss15(f, a, c) + gauss15(f, c, b);
    if (depth >= maxDepth || std::abs(v1 - v2) < 1e-10 * std::abs(v2) + eps) return v2;
    return adaptiveGauss(f, a, c, eps/2, depth+1, maxDepth) + adaptiveGauss(f, c, b, eps/2, depth+1, maxDepth);
}
double ModelWidget01_06::stefestCoefficient(int i, int N) {
    double s = 0.0; int k1 = (i + 1) / 2; int k2 = std::min(i, N / 2);
    for (int k = k1; k <= k2; ++k) {
        double num = pow(k, N / 2.0) * factorial(2 * k);
        double den = factorial(N / 2 - k) * factorial(k) * factorial(k - 1) * factorial(i - k) * factorial(2 * k - i);
        if(den!=0) s += num/den;
    }
    return ((i + N / 2) % 2 == 0 ? 1.0 : -1.0) * s;
}
double ModelWidget01_06::factorial(int n) { if(n<=1)return 1; double r=1; for(int i=2;i<=n;++i)r*=i; return r; }
