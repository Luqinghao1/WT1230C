/*
 * mainwindow.cpp
 * 文件作用：主窗口类实现文件
 * 功能描述：
 * 1. 初始化程序主框架，加载导航栏和各个子功能模块。
 * 2. 管理项目的生命周期（新建、打开、关闭、保存）。
 * 3. 协调不同模块间的数据传递（例如：从数据界面到绘图界面）。
 * 4. [修改] 断开了切换到拟合界面时的自动数据传输逻辑。
 * 5. [新增] 实现了将项目数据模型传递给拟合界面，以支持手动加载数据。
 */

#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "navbtn.h"
#include "wt_projectwidget.h"
#include "dataeditorwidget.h"
#include "modelmanager.h"
#include "modelparameter.h"
#include "wt_plottingwidget.h"
#include "fittingpage.h"
#include "settingswidget.h"

#include <QDateTime>
#include <QMessageBox>
#include <QDebug>
#include <QStandardItemModel>
#include <QTimer>
#include <QSpacerItem>
#include <QStackedWidget>
#include <cmath>
#include <QStatusBar>

// 辅助函数：统一的消息框样式定义
static QString getGlobalMessageBoxStyle()
{
    return "QMessageBox { background-color: #ffffff; color: #000000; }"
           "QLabel { color: #000000; background-color: transparent; }"
           "QPushButton { "
           "   color: #000000; "
           "   background-color: #f0f0f0; "
           "   border: 1px solid #c0c0c0; "
           "   border-radius: 3px; "
           "   padding: 5px 15px; "
           "   min-width: 60px;"
           "}"
           "QPushButton:hover { background-color: #e0e0e0; }"
           "QPushButton:pressed { background-color: #d0d0d0; }";
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_isProjectLoaded(false)
{
    ui->setupUi(this);
    // 设置软件标题
    this->setWindowTitle("PWT 压力试井分析系统 - 陆相泥纹型及混积型页岩油压裂水平井非均匀产液机制与试井解释方法研究");
    this->setMinimumWidth(1024);
    init();
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::init()
{
    // --- 1. 初始化导航栏按钮 ---
    // 循环创建6个主要功能按钮
    for(int i = 0 ; i<6;i++)
    {
        NavBtn* btn = new NavBtn(ui->widgetNav);
        btn->setMinimumWidth(110);
        btn->setIndex(i);
        btn->setStyleSheet("color: black;");

        // 根据索引设置图标和文本
        switch (i) {
        case 0:
            btn->setPicName("border-image: url(:/new/prefix1/Resource/X0.png);",tr("项目"));
            btn->setClickedStyle();
            ui->stackedWidget->setCurrentIndex(0); // 默认显示首页
            break;
        case 1:
            btn->setPicName("border-image: url(:/new/prefix1/Resource/X1.png);",tr("数据"));
            break;
        case 2:
            btn->setPicName("border-image: url(:/new/prefix1/Resource/X2.png);",tr("模型"));
            break;
        case 3:
            btn->setPicName("border-image: url(:/new/prefix1/Resource/X3.png);",tr("图表"));
            break;
        case 4:
            btn->setPicName("border-image: url(:/new/prefix1/Resource/X4.png);",tr("拟合"));
            break;
        case 5:
            btn->setPicName("border-image: url(:/new/prefix1/Resource/X5.png);",tr("设置"));
            break;
        default:
            break;
        }
        m_NavBtnMap.insert(btn->getName(),btn);
        ui->verticalLayoutNav->addWidget(btn);

        // 连接按钮点击信号
        connect(btn,&NavBtn::sigClicked,[=](QString name)
                {
                    int targetIndex = m_NavBtnMap.value(name)->getIndex();

                    // 权限检查：必须先加载项目才能进入核心功能区
                    if ((targetIndex >= 1 && targetIndex <= 4) && !m_isProjectLoaded) {
                        QMessageBox msgBox;
                        msgBox.setWindowTitle("提示");
                        msgBox.setText("当前无活动项目，请先在“项目”界面新建或打开一个项目！");
                        msgBox.setIcon(QMessageBox::Warning);
                        msgBox.setStyleSheet(getMessageBoxStyle());
                        msgBox.exec();
                        return;
                    }

                    // 互斥更新按钮样式
                    QMap<QString,NavBtn*>::Iterator item = m_NavBtnMap.begin();
                    while (item != m_NavBtnMap.end()) {
                        if(item.key() != name)
                        {
                            ((NavBtn*)(item.value()))->setNormalStyle();
                        }
                        item++;
                    }

                    // 切换主界面堆叠页
                    ui->stackedWidget->setCurrentIndex(targetIndex);

                    // 特定页面的自动触发逻辑
                    if (name == tr("图表")) {
                        onTransferDataToPlotting();
                    }
                    else if (name == tr("拟合")) {
                        // [修改] 移除了此处调用 transferDataToFitting() 的逻辑
                        // 以避免每次进入拟合界面时自动覆盖用户已加载的数据
                        // 用户现在通过拟合界面内的“加载观测数据”按钮手动加载数据
                    }
                });
    }

    // 添加垂直弹簧，使按钮靠上排列
    QSpacerItem* verticalSpacer = new QSpacerItem(20, 40, QSizePolicy::Minimum, QSizePolicy::Expanding);
    ui->verticalLayoutNav->addSpacerItem(verticalSpacer);

    // --- 2. 初始化底部时间显示 ---
    ui->labelTime->setText(QDateTime::currentDateTime().toString("yyyy-MM-dd hh-mm-ss").replace(" ","\n"));
    connect(&m_timer,&QTimer::timeout,[=]
            {
                ui->labelTime->setText(QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss").replace(" ","\n"));
                ui->labelTime->setStyleSheet("color: black;");
            });
    m_timer.start(1000);

    // --- 3. 初始化子功能模块 ---

    // 3.1 项目管理界面
    m_ProjectWidget = new WT_ProjectWidget(ui->pageMonitor);
    ui->verticalLayoutMonitor->addWidget(m_ProjectWidget);
    connect(m_ProjectWidget, &WT_ProjectWidget::projectOpened, this, &MainWindow::onProjectOpened);
    connect(m_ProjectWidget, &WT_ProjectWidget::projectClosed, this, &MainWindow::onProjectClosed);
    connect(m_ProjectWidget, &WT_ProjectWidget::fileLoaded, this, &MainWindow::onFileLoaded);

    // 3.2 数据编辑器界面
    m_DataEditorWidget = new DataEditorWidget(ui->pageHand);
    ui->verticalLayoutHandle->addWidget(m_DataEditorWidget);
    connect(m_DataEditorWidget, &DataEditorWidget::fileChanged, this, &MainWindow::onFileLoaded);
    connect(m_DataEditorWidget, &DataEditorWidget::dataChanged, this, &MainWindow::onDataEditorDataChanged);

    // 3.3 模型管理界面
    m_ModelManager = new ModelManager(this);
    m_ModelManager->initializeModels(ui->pageParamter);
    connect(m_ModelManager, &ModelManager::calculationCompleted,
            this, &MainWindow::onModelCalculationCompleted);

    // 3.4 绘图界面
    m_PlottingWidget = new WT_PlottingWidget(ui->pageData);
    ui->verticalLayout_2->addWidget(m_PlottingWidget);

    // 3.5 拟合界面
    if (ui->pageFitting && ui->verticalLayoutFitting) {
        m_FittingPage = new FittingPage(ui->pageFitting);
        ui->verticalLayoutFitting->addWidget(m_FittingPage);
        m_FittingPage->setModelManager(m_ModelManager);
    } else {
        qWarning() << "MainWindow: pageFitting或verticalLayoutFitting为空！无法创建拟合界面";
        m_FittingPage = nullptr;
    }

    // 3.6 设置界面
    m_SettingsWidget = new SettingsWidget(ui->pageAlarm);
    ui->verticalLayout_3->addWidget(m_SettingsWidget);
    connect(m_SettingsWidget, &SettingsWidget::settingsChanged,
            this, &MainWindow::onSystemSettingsChanged);

    // 调用各模块的初始化钩子（打印日志）
    initProjectForm();
    initDataEditorForm();
    initModelForm();
    initPlottingForm();
    initFittingForm();
}

void MainWindow::initProjectForm() { qDebug() << "初始化项目界面"; }
void MainWindow::initDataEditorForm() { qDebug() << "初始化数据编辑器界面"; }
void MainWindow::initModelForm() { if (m_ModelManager) qDebug() << "模型界面初始化完成"; }
void MainWindow::initPlottingForm() { qDebug() << "初始化绘图界面"; }
void MainWindow::initFittingForm() { if (m_FittingPage) qDebug() << "拟合界面初始化完成"; }

// 响应项目打开或新建事件
void MainWindow::onProjectOpened(bool isNew)
{
    qDebug() << "项目已加载，模式:" << (isNew ? "新建" : "打开");
    m_isProjectLoaded = true;

    // 1. 刷新模型参数
    if (m_ModelManager) {
        m_ModelManager->updateAllModelsBasicParameters();
    }

    // 2. 加载数据编辑器内容
    if (m_DataEditorWidget) {
        if (!isNew) {
            m_DataEditorWidget->loadFromProjectData();
        }
        // [新增] 将数据模型指针传递给拟合界面，以便其从项目中加载数据
        if (m_FittingPage) {
            m_FittingPage->setProjectDataModel(m_DataEditorWidget->getDataModel());
        }
    }

    // 3. 恢复拟合界面状态
    if (m_FittingPage) {
        m_FittingPage->updateBasicParameters();
        m_FittingPage->loadAllFittingStates();
    }

    // 4. 加载绘图数据
    if (m_PlottingWidget) {
        m_PlottingWidget->loadProjectData();
    }

    updateNavigationState();

    QString title = isNew ? "新建项目成功" : "加载项目成功";
    QString text = isNew ? "新项目已创建。\n基础参数已初始化，您可以开始进行数据录入或模型计算。"
                         : "项目文件加载完成。\n历史参数、数据及图表分析状态已完整恢复。";

    QMessageBox msgBox;
    msgBox.setWindowTitle(title);
    msgBox.setText(text);
    msgBox.setIcon(QMessageBox::Information);
    msgBox.setStyleSheet(getMessageBoxStyle());
    msgBox.exec();
}

// 响应项目关闭事件
void MainWindow::onProjectClosed()
{
    qDebug() << "项目已关闭，重置界面状态...";
    m_isProjectLoaded = false;
    m_hasValidData = false;

    // 1. 清空数据编辑器 (之前修改过的)
    if (m_DataEditorWidget) {
        m_DataEditorWidget->clearAllData();
    }

    // 2. [关键] 清空图表界面
    if (m_PlottingWidget) {
        m_PlottingWidget->clearAllPlots(); // 调用刚才新增的函数
    }

    // 3. [关键] 重置拟合界面
    if (m_FittingPage) {
        m_FittingPage->resetAnalysis();    // 调用刚才新增的函数
    }

    // 4. 清除模型管理器缓存 (如果之前添加了)
    if (m_ModelManager) {
        m_ModelManager->clearCache();
    }
    // 这一步如果不做，新建项目时会从 ModelParameter 重新读取旧数据
    ModelParameter::instance()->resetAllData();
    // 界面跳转回首页
    ui->stackedWidget->setCurrentIndex(0);
    updateNavigationState();

    // 提示信息
    QMessageBox msgBox;
    msgBox.setWindowTitle("提示");
    msgBox.setText("项目已保存并关闭。");
    msgBox.setIcon(QMessageBox::Information);
    msgBox.setStyleSheet(getMessageBoxStyle());
    msgBox.exec();
}


// 响应外部文件加载（如 CSV 导入）
void MainWindow::onFileLoaded(const QString& filePath, const QString& fileType)
{
    qDebug() << "文件加载：" << filePath;
    if (!m_isProjectLoaded) {
        QMessageBox msgBox;
        msgBox.setWindowTitle("警告");
        msgBox.setText("请先创建或打开项目！");
        msgBox.setIcon(QMessageBox::Warning);
        msgBox.setStyleSheet(getMessageBoxStyle());
        msgBox.exec();
        return;
    }

    ui->stackedWidget->setCurrentIndex(1); // 跳转到数据页

    // 更新导航栏状态
    QMap<QString,NavBtn*>::Iterator item = m_NavBtnMap.begin();
    while (item != m_NavBtnMap.end()) {
        ((NavBtn*)(item.value()))->setNormalStyle();
        if(item.key() == tr("数据")) {
            ((NavBtn*)(item.value()))->setClickedStyle();
        }
        item++;
    }

    if (m_DataEditorWidget && sender() != m_DataEditorWidget) {
        m_DataEditorWidget->loadData(filePath, fileType);
    }

    // [新增] 当数据变动时，同步更新拟合页面的模型引用
    if (m_FittingPage && m_DataEditorWidget) {
        m_FittingPage->setProjectDataModel(m_DataEditorWidget->getDataModel());
    }

    m_hasValidData = true;
    QTimer::singleShot(1000, this, &MainWindow::onDataReadyForPlotting);
}

void MainWindow::onPlotAnalysisCompleted(const QString &analysisType, const QMap<QString, double> &results)
{
    qDebug() << "绘图分析完成：" << analysisType;
}

void MainWindow::onDataReadyForPlotting()
{
    transferDataFromEditorToPlotting();
}

void MainWindow::onTransferDataToPlotting()
{
    if (!hasDataLoaded()) return;
    transferDataFromEditorToPlotting();
}

void MainWindow::onDataEditorDataChanged()
{
    if (ui->stackedWidget->currentIndex() == 3) {
        transferDataFromEditorToPlotting();
    }
    m_hasValidData = hasDataLoaded();
}

void MainWindow::onModelCalculationCompleted(const QString &analysisType, const QMap<QString, double> &results)
{
    qDebug() << "模型计算完成：" << analysisType;
}

// 旧的数据传输函数，现保留但不再自动调用
void MainWindow::transferDataToFitting()
{
    if (!m_FittingPage || !m_DataEditorWidget) return;

    QStandardItemModel* model = m_DataEditorWidget->getDataModel();
    if (!model || model->rowCount() == 0) {
        return;
    }

    QVector<double> tVec, pVec, dVec;
    double p_initial = 0.0;

    // 寻找初始压力（第一列时间，第二列压力）
    for(int r=0; r<model->rowCount(); ++r) {
        QModelIndex idx = model->index(r, 1);
        if(idx.isValid()) {
            double p = idx.data().toDouble();
            if (std::abs(p) > 1e-6) {
                p_initial = p;
                break;
            }
        }
    }

    // 提取并计算压差
    for(int r=0; r<model->rowCount(); ++r) {
        double t = model->index(r, 0).data().toDouble();
        double p_raw = model->index(r, 1).data().toDouble();
        if (t > 0) {
            tVec.append(t);
            pVec.append(std::abs(p_raw - p_initial));
        }
    }

    // Bourdet 导数计算逻辑
    dVec.resize(tVec.size());
    if (tVec.size() > 2) {
        dVec[0] = 0;
        dVec[tVec.size()-1] = 0;
        for(int i=1; i<tVec.size()-1; ++i) {
            double lnt1 = std::log(tVec[i-1]);
            double lnt2 = std::log(tVec[i]);
            double lnt3 = std::log(tVec[i+1]);
            if (std::abs(lnt2 - lnt1) < 1e-9 || std::abs(lnt3 - lnt2) < 1e-9) {
                dVec[i] = 0; continue;
            }
            double d1 = (pVec[i] - pVec[i-1]) / (lnt2 - lnt1);
            double d2 = (pVec[i+1] - pVec[i]) / (lnt3 - lnt2);
            double w1 = (lnt3 - lnt2) / (lnt3 - lnt1);
            double w2 = (lnt2 - lnt1) / (lnt3 - lnt1);
            dVec[i] = d1 * w1 + d2 * w2;
        }
    }

    m_FittingPage->setObservedDataToCurrent(tVec, pVec, dVec);
}

void MainWindow::onFittingProgressChanged(int progress)
{
    if (this->statusBar()) {
        this->statusBar()->showMessage(QString("正在拟合... %1%").arg(progress));
        if(progress >= 100) this->statusBar()->showMessage("拟合完成", 5000);
    }
}

void MainWindow::onSystemSettingsChanged()
{
    qDebug() << "系统设置已变更";
}

void MainWindow::onPerformanceSettingsChanged() {}

QStandardItemModel* MainWindow::getDataEditorModel() const
{
    if (!m_DataEditorWidget) return nullptr;
    return m_DataEditorWidget->getDataModel();
}

QString MainWindow::getCurrentFileName() const
{
    if (!m_DataEditorWidget) return QString();
    return m_DataEditorWidget->getCurrentFileName();
}

bool MainWindow::hasDataLoaded()
{
    if (!m_DataEditorWidget) return false;
    return m_DataEditorWidget->hasData();
}

void MainWindow::transferDataFromEditorToPlotting()
{
    if (!m_DataEditorWidget || !m_PlottingWidget) return;
    QStandardItemModel* model = m_DataEditorWidget->getDataModel();
    m_PlottingWidget->setDataModel(model);
    if (model && model->rowCount() > 0) {
        m_hasValidData = true;
    }
}

void MainWindow::updateNavigationState()
{
    QMap<QString,NavBtn*>::Iterator item = m_NavBtnMap.begin();
    while (item != m_NavBtnMap.end()) {
        ((NavBtn*)(item.value()))->setNormalStyle();
        if(item.key() == tr("项目")) {
            ((NavBtn*)(item.value()))->setClickedStyle();
        }
        item++;
    }
}

QString MainWindow::getMessageBoxStyle() const
{
    return getGlobalMessageBoxStyle();
}

