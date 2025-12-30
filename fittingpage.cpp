/*
 * 文件名: fittingpage.cpp
 * 文件作用: 拟合页面容器类实现文件
 * 功能描述:
 * 1. 实现了多页签管理逻辑（增删改）。
 * 2. 负责将全局的模型管理器和数据模型分发给具体的拟合子控件。
 * 3. 实现了拟合状态的序列化与反序列化，支持项目保存恢复。
 */

#include "fittingpage.h"
#include "ui_fittingpage.h"
#include "wt_fittingwidget.h"
#include "modelparameter.h"
#include <QInputDialog>
#include <QMessageBox>
#include <QJsonArray>
#include <QDebug>

FittingPage::FittingPage(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::FittingPage),
    m_modelManager(nullptr),
    m_projectModel(nullptr)
{
    ui->setupUi(this);
}

FittingPage::~FittingPage()
{
    delete ui;
}

// 设置模型管理器，并分发给所有现有子页签
void FittingPage::setModelManager(ModelManager *m)
{
    m_modelManager = m;
    for(int i = 0; i < ui->tabWidget->count(); ++i) {
        FittingWidget* w = qobject_cast<FittingWidget*>(ui->tabWidget->widget(i));
        if(w) w->setModelManager(m);
    }
}

// [新增] 设置项目数据模型，并分发给所有现有子页签
void FittingPage::setProjectDataModel(QStandardItemModel *model)
{
    m_projectModel = model;
    for(int i = 0; i < ui->tabWidget->count(); ++i) {
        FittingWidget* w = qobject_cast<FittingWidget*>(ui->tabWidget->widget(i));
        if(w) {
            w->setProjectDataModel(model);
        }
    }
}

// 将观测数据设置到当前激活页签，若无则自动创建
void FittingPage::setObservedDataToCurrent(const QVector<double> &t, const QVector<double> &p, const QVector<double> &d)
{
    FittingWidget* current = qobject_cast<FittingWidget*>(ui->tabWidget->currentWidget());
    if (current) {
        current->setObservedData(t, p, d);
    } else {
        on_btnNewAnalysis_clicked();
        current = qobject_cast<FittingWidget*>(ui->tabWidget->currentWidget());
        if(current) current->setObservedData(t, p, d);
    }
}

void FittingPage::updateBasicParameters()
{
    for(int i = 0; i < ui->tabWidget->count(); ++i) {
        FittingWidget* w = qobject_cast<FittingWidget*>(ui->tabWidget->widget(i));
        if(w) w->updateBasicParameters();
    }
}

// 创建新页签并初始化
FittingWidget* FittingPage::createNewTab(const QString &name, const QJsonObject &initData)
{
    FittingWidget* w = new FittingWidget(this);

    // 注入依赖
    if(m_modelManager) w->setModelManager(m_modelManager);
    if(m_projectModel) w->setProjectDataModel(m_projectModel); // [新增] 注入数据模型

    connect(w, &FittingWidget::sigRequestSave, this, &FittingPage::onChildRequestSave);

    int index = ui->tabWidget->addTab(w, name);
    ui->tabWidget->setCurrentIndex(index);

    if(!initData.isEmpty()) {
        w->loadFittingState(initData);
    }

    return w;
}

QString FittingPage::generateUniqueName(const QString &baseName)
{
    QString name = baseName;
    int counter = 1;
    bool exists = true;
    while(exists) {
        exists = false;
        for(int i=0; i<ui->tabWidget->count(); ++i) {
            if(ui->tabWidget->tabText(i) == name) {
                exists = true;
                break;
            }
        }
        if(exists) {
            counter++;
            name = QString("%1 %2").arg(baseName).arg(counter);
        }
    }
    return name;
}

// 新建分析按钮
void FittingPage::on_btnNewAnalysis_clicked()
{
    QStringList items;
    items << "空白分析 (Blank)";
    for(int i=0; i<ui->tabWidget->count(); ++i) {
        items << "复制: " + ui->tabWidget->tabText(i);
    }

    bool ok;
    QString item = QInputDialog::getItem(this, "新建分析", "请选择创建方式:", items, 0, false, &ok);
    if (!ok || item.isEmpty()) return;

    QString newName = generateUniqueName("Analysis");

    if (item == "空白分析 (Blank)") {
        createNewTab(newName);
    } else {
        int indexToCopy = items.indexOf(item) - 1;
        FittingWidget* source = qobject_cast<FittingWidget*>(ui->tabWidget->widget(indexToCopy));
        if(source) {
            QJsonObject state = source->getJsonState();
            createNewTab(newName, state);
        }
    }
}

// 重命名按钮
void FittingPage::on_btnRenameAnalysis_clicked()
{
    int idx = ui->tabWidget->currentIndex();
    if(idx < 0) return;

    QString oldName = ui->tabWidget->tabText(idx);
    bool ok;
    QString newName = QInputDialog::getText(this, "重命名", "请输入新的分析名称:", QLineEdit::Normal, oldName, &ok);
    if(ok && !newName.isEmpty()) {
        ui->tabWidget->setTabText(idx, newName);
    }
}

// 删除按钮
void FittingPage::on_btnDeleteAnalysis_clicked()
{
    int idx = ui->tabWidget->currentIndex();
    if(idx < 0) return;

    if(ui->tabWidget->count() == 1) {
        QMessageBox::warning(this, "警告", "至少需要保留一个分析页面！");
        return;
    }

    if(QMessageBox::question(this, "确认", "确定要删除当前分析页吗？\n此操作不可恢复。") == QMessageBox::Yes) {
        QWidget* w = ui->tabWidget->widget(idx);
        ui->tabWidget->removeTab(idx);
        delete w;
    }
}

// 保存所有状态
void FittingPage::saveAllFittingStates()
{
    QJsonArray analysesArray;
    for(int i=0; i<ui->tabWidget->count(); ++i) {
        FittingWidget* w = qobject_cast<FittingWidget*>(ui->tabWidget->widget(i));
        if(w) {
            QJsonObject pageObj = w->getJsonState();
            pageObj["_tabName"] = ui->tabWidget->tabText(i);
            analysesArray.append(pageObj);
        }
    }

    QJsonObject root;
    root["version"] = "2.0";
    root["analyses"] = analysesArray;

    ModelParameter::instance()->saveFittingResult(root);
}

// 加载所有状态
void FittingPage::loadAllFittingStates()
{
    QJsonObject root = ModelParameter::instance()->getFittingResult();
    if(root.isEmpty()) {
        if(ui->tabWidget->count() == 0) createNewTab("Analysis 1");
        return;
    }

    ui->tabWidget->clear();

    if(root.contains("analyses") && root["analyses"].isArray()) {
        QJsonArray arr = root["analyses"].toArray();
        for(int i=0; i<arr.size(); ++i) {
            QJsonObject pageObj = arr[i].toObject();
            QString name = pageObj.contains("_tabName") ? pageObj["_tabName"].toString() : QString("Analysis %1").arg(i+1);
            createNewTab(name, pageObj);
        }
    } else {
        // 兼容旧版单一状态
        createNewTab("Analysis 1", root);
    }

    if(ui->tabWidget->count() == 0) createNewTab("Analysis 1");
}

void FittingPage::onChildRequestSave()
{
    saveAllFittingStates();
    QMessageBox::information(this, "保存成功", "所有分析页的状态已保存到项目文件 (pwt) 中。");
}



// [新增] 实现重置功能
void FittingPage::resetAnalysis()
{
    // 1. 循环删除所有页签及其内部的 Widget
    // QTabWidget::clear() 只移除不删除，所以必须手动 delete
    while (ui->tabWidget->count() > 0) {
        QWidget* w = ui->tabWidget->widget(0);
        ui->tabWidget->removeTab(0); // 先从界面移除
        delete w;                    // 再销毁对象
    }

    // 2. 重新创建一个默认的空白分析页，恢复初始状态
    createNewTab("Analysis 1");
}
