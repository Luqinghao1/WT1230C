/*
 * 文件名: wt_fittingwidget.h
 * 文件作用: 试井拟合分析主界面类的头文件
 * 功能描述:
 * 1. 定义拟合分析界面的主要控件成员变量和布局逻辑。
 * 2. 声明用于Levenberg-Marquardt非线性回归拟合的核心算法函数。
 * 3. 声明观测数据（时间、压差、导数）的管理函数。
 * 4. 提供与外部模块（如主窗口、模型管理器）的交互接口。
 */

#ifndef WT_FITTINGWIDGET_H
#define WT_FITTINGWIDGET_H

#include <QWidget>
#include <QMap>
#include <QVector>
#include <QFutureWatcher>
#include <QJsonObject>
#include <QStandardItemModel>
#include "modelmanager.h"
#include "mousezoom.h"
#include "chartsetting1.h"
#include "fittingparameterchart.h"
#include "paramselectdialog.h"

namespace Ui { class FittingWidget; }

class FittingWidget : public QWidget
{
    Q_OBJECT

public:
    // 构造函数
    explicit FittingWidget(QWidget *parent = nullptr);
    // 析构函数
    ~FittingWidget();

    // 设置模型管理器，用于获取理论模型计算服务
    void setModelManager(ModelManager* m);

    // 设置项目数据模型，用于从项目表格中直接加载数据
    void setProjectDataModel(QStandardItemModel* model);

    // 设置观测数据（时间、压差、导数）并更新绘图
    // [注意]: 此处存储的 p 必须是计算好的压差 (Delta P)
    void setObservedData(const QVector<double>& t, const QVector<double>& deltaP, const QVector<double>& deriv);

    // 更新基础参数（预留接口，用于同步孔渗饱等物性参数）
    void updateBasicParameters();

    // 从JSON对象加载拟合界面的所有状态（包括参数、数据、视图范围）
    void loadFittingState(const QJsonObject& data = QJsonObject());

    // 获取当前拟合界面的所有状态为JSON对象，用于保存项目
    QJsonObject getJsonState() const;

signals:
    // 拟合计算完成信号，携带最终模型类型和参数
    void fittingCompleted(ModelManager::ModelType modelType, const QMap<QString, double>& parameters);

    // 迭代更新信号，用于在拟合过程中实时刷新界面（误差显示、曲线绘制）
    void sigIterationUpdated(double error, QMap<QString, double> currentParams, QVector<double> t, QVector<double> p, QVector<double> d);

    // 进度条更新信号
    void sigProgress(int progress);

    // 请求父级页面保存项目的信号
    void sigRequestSave();

private slots:
    // 按钮槽函数：点击加载观测数据
    void on_btnLoadData_clicked();

    // 按钮槽函数：点击开始拟合
    void on_btnRunFit_clicked();

    // 按钮槽函数：点击停止拟合
    void on_btnStop_clicked();

    // 按钮槽函数：点击刷新/生成理论曲线
    void on_btnImportModel_clicked();

    // 按钮槽函数：导出拟合参数到文件
    void on_btnExportData_clicked();

    // 按钮槽函数：导出当前图表为图片或PDF
    void on_btnExportChart_clicked();

    // 按钮槽函数：重置参数为默认值
    void on_btnResetParams_clicked();

    // 按钮槽函数：复位图表视图范围
    void on_btnResetView_clicked();

    // 按钮槽函数：打开图表设置对话框
    void on_btnChartSettings_clicked();

    // 按钮槽函数：选择解释模型
    void on_btn_modelSelect_clicked();

    // 按钮槽函数：手动选择需要拟合的参数
    void on_btnSelectParams_clicked();

    // 按钮槽函数：保存当前分析结果
    void on_btnSaveFit_clicked();

    // 按钮槽函数：导出分析报告
    void on_btnExportReport_clicked();

    // 内部逻辑槽：处理迭代更新信号，刷新UI
    void onIterationUpdate(double err, const QMap<QString,double>& p, const QVector<double>& t, const QVector<double>& p_curve, const QVector<double>& d_curve);

    // 内部逻辑槽：处理拟合完成后的收尾工作
    void onFitFinished();

    // 内部逻辑槽：处理权重滑块数值变更
    void onSliderWeightChanged(int value);

private:
    Ui::FittingWidget *ui;
    ModelManager* m_modelManager;          // 模型计算核心模块指针
    QStandardItemModel* m_projectModel;    // 项目数据表格模型指针

    MouseZoom* m_plot;                     // 自定义绘图控件
    QCPTextElement* m_plotTitle;           // 图表标题元素
    ModelManager::ModelType m_currentModelType; // 当前选中的解释模型类型

    FittingParameterChart* m_paramChart;   // 参数表格逻辑管理类

    // 本地存储的观测数据
    QVector<double> m_obsTime;             // 观测时间
    QVector<double> m_obsDeltaP;           // 观测压差 (Delta P)
    QVector<double> m_obsDerivative;       // 观测导数

    // 拟合任务控制状态
    bool m_isFitting;                      // 是否正在拟合中
    bool m_stopRequested;                  // 是否收到了停止请求
    QFutureWatcher<void> m_watcher;        // 异步任务监视器

    // 初始化绘图控件的样式和布局
    void setupPlot();

    // 初始化默认模型和参数
    void initializeDefaultModel();

    // 根据当前参数表的值，计算并更新理论曲线
    void updateModelCurve();

    // 启动非线性回归优化任务（在子线程运行）
    void runOptimizationTask(ModelManager::ModelType modelType, QList<FitParameter> fitParams, double weight);

    // Levenberg-Marquardt 算法的具体实现
    void runLevenbergMarquardtOptimization(ModelManager::ModelType modelType, QList<FitParameter> params, double weight);

    // 计算当前参数下的残差向量（理论值与观测值的差异）
    QVector<double> calculateResiduals(const QMap<QString, double>& params, ModelManager::ModelType modelType, double weight);

    // 计算雅可比矩阵（残差对各个待拟合参数的偏导数）
    QVector<QVector<double>> computeJacobian(const QMap<QString, double>& params, const QVector<double>& residuals, const QVector<int>& fitIndices, ModelManager::ModelType modelType, const QList<FitParameter>& currentFitParams, double weight);

    // 求解线性方程组 (Ax = b)，用于LM算法中的迭代步长计算
    QVector<double> solveLinearSystem(const QVector<QVector<double>>& A, const QVector<double>& b);

    // 计算残差平方和（SSE），作为目标函数值
    double calculateSumSquaredError(const QVector<double>& residuals);

    // 获取图表的Base64编码字符串，用于生成HTML报告
    QString getPlotImageBase64();

    // 在图表上绘制曲线数据
    void plotCurves(const QVector<double>& t, const QVector<double>& p, const QVector<double>& d, bool isModel);
};

#endif // WT_FITTINGWIDGET_H
