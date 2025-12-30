#ifndef PARAMSELECTDIALOG_H
#define PARAMSELECTDIALOG_H

#include <QDialog>
#include <QTableWidget>
#include "fittingparameterchart.h"

namespace Ui {
class ParamSelectDialog;
}

// ===========================================================================
// 类名：ParamSelectDialog
// 作用：拟合参数配置弹窗
// 功能：
// 1. 提供一个大表格，列出所有模型参数
// 2. 允许用户勾选参数是否在主界面显示 (isVisible)
// 3. 允许用户勾选参数是否参与自动拟合 (isFit)
// 4. 设置每个参数的上下限范围 (Min/Max)
// 5. 内部维护逻辑：如果勾选了拟合，则强制勾选显示
// ===========================================================================

class ParamSelectDialog : public QDialog
{
    Q_OBJECT

public:
    // 构造函数
    explicit ParamSelectDialog(const QList<FitParameter>& params, QWidget *parent = nullptr);
    ~ParamSelectDialog();

    // 获取用户修改后的参数列表
    QList<FitParameter> getUpdatedParams() const;

private:
    Ui::ParamSelectDialog *ui;

    // 暂存的参数列表副本
    QList<FitParameter> m_params;

    // 初始化表格视图
    void initTable();
    // 收集表格数据到 m_params
    void collectData();

private slots:
    // 自定义槽函数，避免自动连接
    void onConfirm();
    void onCancel();
};

#endif // PARAMSELECTDIALOG_H
