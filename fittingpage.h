/*
 * 文件名: fittingpage.h
 * 文件作用: 拟合页面容器类头文件
 * 功能描述:
 * 1. 管理多个拟合分析页签 (FittingWidget)。
 * 2. 负责将项目级数据（如模型管理器、观测数据模型）传递给各个子页签。
 * 3. 实现多页签的创建、重命名、删除及保存恢复功能。
 */

#ifndef FITTINGPAGE_H
#define FITTINGPAGE_H

#include <QWidget>
#include <QJsonObject>
#include <QTabWidget>
#include <QStandardItemModel> // 新增
#include "modelmanager.h"

// 前置声明
class FittingWidget;

namespace Ui {
class FittingPage;
}

class FittingPage : public QWidget
{
    Q_OBJECT

public:
    explicit FittingPage(QWidget *parent = nullptr);
    ~FittingPage();



    // 设置模型管理器（传递给子页面）
    void setModelManager(ModelManager* m);

    // 设置项目数据模型（用于传递给子页面的数据加载弹窗）
    void setProjectDataModel(QStandardItemModel* model);

    // 接收来自外部的数据并设置到当前激活页签
    void setObservedDataToCurrent(const QVector<double>& t, const QVector<double>& p, const QVector<double>& d);

    // 初始化/重置基本参数
    void updateBasicParameters();

    // 重置拟合分析
    void resetAnalysis();

    // 从项目文件加载所有拟合分析的状态
    void loadAllFittingStates();

    // 保存所有拟合分析的状态到项目文件
    void saveAllFittingStates();

private slots:
    // 页签管理槽函数
    void on_btnNewAnalysis_clicked();
    void on_btnRenameAnalysis_clicked();
    void on_btnDeleteAnalysis_clicked();

    // 响应子页面的保存请求
    void onChildRequestSave();

private:
    Ui::FittingPage *ui;
    ModelManager* m_modelManager;
    QStandardItemModel* m_projectModel; // [新增] 保存模型指针

    // 内部函数：创建新页签
    FittingWidget* createNewTab(const QString& name, const QJsonObject& initData = QJsonObject());
    // 生成唯一的页签名称
    QString generateUniqueName(const QString& baseName);
};

#endif // FITTINGPAGE_H
