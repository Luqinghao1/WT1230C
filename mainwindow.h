/*
 * mainwindow.h
 * 文件作用：主窗口类头文件
 * 功能描述：
 * 1. 管理整个应用程序的主界面框架 (PWT 压力试井分析系统)
 * 2. 初始化各个功能子模块 (项目、数据、模型、绘图、拟合、设置)
 * 3. 协调模块间的数据流转与状态管理
 * 4. 响应项目的新建、打开、关闭操作，控制功能权限
 */

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QMap>
#include <QTimer>
#include <QStandardItemModel>
#include "modelmanager.h"

// 前向声明子窗口类，减少头文件依赖
class NavBtn;
class WT_ProjectWidget;
class DataEditorWidget;
class WT_PlottingWidget; // 使用新的图表类
class FittingPage;
class SettingsWidget;

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

    // 系统初始化函数，构建界面和逻辑连接
    void init();

    // 各个子界面的初始化辅助函数
    void initProjectForm();
    void initDataEditorForm();
    void initModelForm();
    void initPlottingForm();
    void initFittingForm();

private slots:
    // --- 项目管理相关槽函数 ---
    // 响应项目新建或打开成功的信号
    void onProjectOpened(bool isNew);
    // 响应项目关闭的信号，用于重置界面状态
    void onProjectClosed();
    // 响应外部文件加载信号
    void onFileLoaded(const QString& filePath, const QString& fileType);

    // --- 数据与绘图相关槽函数 ---
    // 绘图分析完成后的回调
    void onPlotAnalysisCompleted(const QString &analysisType, const QMap<QString, double> &results);
    // 数据准备好可以绘图时的回调
    void onDataReadyForPlotting();
    // 触发数据从编辑器传输到绘图模块
    void onTransferDataToPlotting();
    // 数据编辑器内容发生变化时的回调
    void onDataEditorDataChanged();

    // --- 设置与模型相关槽函数 ---
    // 系统通用设置变更回调
    void onSystemSettingsChanged();
    // 性能设置变更回调（预留）
    void onPerformanceSettingsChanged();
    // 模型计算完成后的回调
    void onModelCalculationCompleted(const QString &analysisType, const QMap<QString, double> &results);
    // 拟合进度更新回调
    void onFittingProgressChanged(int progress);

private:
    Ui::MainWindow *ui;

    // --- 子功能模块指针 ---
    WT_ProjectWidget* m_ProjectWidget;      // 项目管理界面
    DataEditorWidget* m_DataEditorWidget;   // 数据编辑界面
    ModelManager* m_ModelManager;           // 模型管理核心类
    WT_PlottingWidget* m_PlottingWidget;    // 绘图显示界面
    FittingPage* m_FittingPage;             // 自动拟合界面
    SettingsWidget* m_SettingsWidget;       // 系统设置界面

    // 导航栏按钮映射表
    QMap<QString, NavBtn*> m_NavBtnMap;
    // 系统时间定时器
    QTimer m_timer;
    // 标记当前是否持有有效的试井数据
    bool m_hasValidData = false;

    // 标记是否已加载项目（新建或打开），用于控制功能访问权限
    bool m_isProjectLoaded = false;

    // --- 内部私有辅助函数 ---
    // 将数据从编辑器传输至绘图模块
    void transferDataFromEditorToPlotting();
    // 更新导航栏按钮的激活状态
    void updateNavigationState();
    // 将数据传输至拟合模块
    void transferDataToFitting();

    // 获取数据编辑器的数据模型
    QStandardItemModel* getDataEditorModel() const;
    // 获取当前打开的数据文件名
    QString getCurrentFileName() const;
    // 检查是否有数据被加载
    bool hasDataLoaded();

    // 获取统一的弹窗样式（白底黑字）
    QString getMessageBoxStyle() const;
};

#endif // MAINWINDOW_H
