/*
 * paramselectdialog.cpp
 * 文件作用：参数选择配置对话框的具体实现
 * 功能描述：
 * 1. 初始化对话框，显示参数列表
 * 2. 提供勾选功能：是否显示、是否拟合
 * 3. 约束逻辑：勾选“拟合变量”时，强制勾选“显示”
 * 4. 修复了Qt 6.9版本下的废弃API警告 (checkStateChanged)
 */

#include "paramselectdialog.h"
#include "ui_paramselectdialog.h"
#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QHeaderView>
#include <QHBoxLayout>

ParamSelectDialog::ParamSelectDialog(const QList<FitParameter> &params, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::ParamSelectDialog),
    m_params(params)
{
    ui->setupUi(this);

    this->setWindowTitle("拟合参数配置");

    // 显式连接信号槽，解决“需要点击两次”的问题
    // 将按钮的 clicked 信号连接到我们自定义的槽函数
    connect(ui->btnOk, &QPushButton::clicked, this, &ParamSelectDialog::onConfirm);
    connect(ui->btnCancel, &QPushButton::clicked, this, &ParamSelectDialog::onCancel);

    // 确保按钮不会默认截获回车键导致意外行为，除非是默认按钮
    ui->btnCancel->setAutoDefault(false);

    initTable();
}

ParamSelectDialog::~ParamSelectDialog()
{
    delete ui;
}

void ParamSelectDialog::initTable()
{
    QStringList headers;
    headers << "显示" << "参数名称" << "当前数值" << "单位" << "拟合变量" << "下限" << "上限";
    ui->tableWidget->setColumnCount(headers.size());
    ui->tableWidget->setHorizontalHeaderLabels(headers);
    ui->tableWidget->setRowCount(m_params.size());

    for(int i = 0; i < m_params.size(); ++i) {
        const FitParameter& p = m_params[i];

        // 0. 显示勾选框
        QWidget* pWidgetVis = new QWidget();
        QHBoxLayout* pLayoutVis = new QHBoxLayout(pWidgetVis);
        QCheckBox* chkVis = new QCheckBox();
        chkVis->setChecked(p.isVisible);
        pLayoutVis->addWidget(chkVis);
        pLayoutVis->setAlignment(Qt::AlignCenter);
        pLayoutVis->setContentsMargins(0,0,0,0);
        ui->tableWidget->setCellWidget(i, 0, pWidgetVis);

        // 1. 参数名称 (只读)
        QString displayNameFull = QString("%1 (%2)").arg(p.displayName).arg(p.name);
        QTableWidgetItem* nameItem = new QTableWidgetItem(displayNameFull);
        nameItem->setFlags(nameItem->flags() & ~Qt::ItemIsEditable);
        nameItem->setData(Qt::UserRole, p.name);
        ui->tableWidget->setItem(i, 1, nameItem);

        // 2. 数值
        QDoubleSpinBox* spinVal = new QDoubleSpinBox();
        spinVal->setRange(-9e9, 9e9); spinVal->setDecimals(6); spinVal->setValue(p.value);
        spinVal->setFrame(false);
        ui->tableWidget->setCellWidget(i, 2, spinVal);

        // 3. 单位
        QString dummy, dummy2, dummy3, unitStr;
        FittingParameterChart::getParamDisplayInfo(p.name, dummy, dummy2, dummy3, unitStr);
        if(unitStr == "无因次" || unitStr == "小数") unitStr = "-";
        QTableWidgetItem* unitItem = new QTableWidgetItem(unitStr);
        unitItem->setFlags(unitItem->flags() & ~Qt::ItemIsEditable);
        ui->tableWidget->setItem(i, 3, unitItem);

        // 4. 拟合勾选框
        QWidget* pWidgetFit = new QWidget();
        QHBoxLayout* pLayoutFit = new QHBoxLayout(pWidgetFit);
        QCheckBox* chkFit = new QCheckBox();
        chkFit->setChecked(p.isFit);
        pLayoutFit->addWidget(chkFit);
        pLayoutFit->setAlignment(Qt::AlignCenter);
        pLayoutFit->setContentsMargins(0,0,0,0);
        ui->tableWidget->setCellWidget(i, 4, pWidgetFit);

        // 约束逻辑：勾选拟合 -> 强制显示
        // [修复] 使用 checkStateChanged 代替已过时的 stateChanged
        connect(chkFit, &QCheckBox::checkStateChanged, [chkVis](Qt::CheckState state){
            if (state == Qt::Checked) {
                chkVis->setChecked(true);
                chkVis->setEnabled(false); // 强制显示，不可取消
            } else {
                chkVis->setEnabled(true); // 恢复可控
            }
        });

        // 初始化状态
        if (p.isFit) {
            chkVis->setChecked(true);
            chkVis->setEnabled(false);
        }

        // 5. 下限
        QDoubleSpinBox* spinMin = new QDoubleSpinBox();
        spinMin->setRange(-9e9, 9e9); spinMin->setDecimals(6); spinMin->setValue(p.min);
        spinMin->setFrame(false);
        ui->tableWidget->setCellWidget(i, 5, spinMin);

        // 6. 上限
        QDoubleSpinBox* spinMax = new QDoubleSpinBox();
        spinMax->setRange(-9e9, 9e9); spinMax->setDecimals(6); spinMax->setValue(p.max);
        spinMax->setFrame(false);
        ui->tableWidget->setCellWidget(i, 6, spinMax);
    }

    ui->tableWidget->resizeColumnsToContents();
    ui->tableWidget->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
}

void ParamSelectDialog::collectData()
{
    for(int i = 0; i < ui->tableWidget->rowCount(); ++i) {
        if(i >= m_params.size()) break;

        QWidget* wVis = ui->tableWidget->cellWidget(i, 0);
        QCheckBox* chkVis = wVis ? wVis->findChild<QCheckBox*>() : nullptr;
        if(chkVis) m_params[i].isVisible = chkVis->isChecked();

        QDoubleSpinBox* spinVal = qobject_cast<QDoubleSpinBox*>(ui->tableWidget->cellWidget(i, 2));
        if(spinVal) m_params[i].value = spinVal->value();

        QWidget* wFit = ui->tableWidget->cellWidget(i, 4);
        QCheckBox* chkFit = wFit ? wFit->findChild<QCheckBox*>() : nullptr;
        if(chkFit) m_params[i].isFit = chkFit->isChecked();

        QDoubleSpinBox* spinMin = qobject_cast<QDoubleSpinBox*>(ui->tableWidget->cellWidget(i, 5));
        if(spinMin) m_params[i].min = spinMin->value();

        QDoubleSpinBox* spinMax = qobject_cast<QDoubleSpinBox*>(ui->tableWidget->cellWidget(i, 6));
        if(spinMax) m_params[i].max = spinMax->value();
    }
}

QList<FitParameter> ParamSelectDialog::getUpdatedParams() const
{
    return m_params;
}

// 确定槽
void ParamSelectDialog::onConfirm()
{
    collectData();
    accept(); // 这里只调用一次 accept，Qt 会处理关闭
}

// 取消槽
void ParamSelectDialog::onCancel()
{
    reject();
}
