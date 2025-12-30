/*
 * 文件名: plottingdialog1.h
 * 文件作用: 新建单一曲线配置对话框头文件
 * 功能描述:
 * 1. 设置曲线基础信息（名称、图例）。
 * 2. 选择X/Y数据源及设置坐标轴标签。
 * 3. 设置详细的点/线样式（包含颜色调色盘）。
 */

#ifndef PLOTTINGDIALOG1_H
#define PLOTTINGDIALOG1_H

#include <QDialog>
#include <QStandardItemModel>
#include <QColor>
#include "qcustomplot.h"

namespace Ui {
class PlottingDialog1;
}

class PlottingDialog1 : public QDialog
{
    Q_OBJECT

public:
    explicit PlottingDialog1(QStandardItemModel* model, QWidget *parent = nullptr);
    ~PlottingDialog1();

    // --- 获取用户配置 ---
    QString getCurveName() const;
    QString getLegendName() const;
    int getXColumn() const;
    int getYColumn() const;
    QString getXLabel() const;
    QString getYLabel() const;

    // 样式获取
    QCPScatterStyle::ScatterShape getPointShape() const;
    QColor getPointColor() const;
    Qt::PenStyle getLineStyle() const;
    QColor getLineColor() const;

    bool isNewWindow() const;

private slots:
    // 列改变时自动更新图例和标签
    void onXColumnChanged(int index);
    void onYColumnChanged(int index);

    // 弹出调色盘
    void selectPointColor();
    void selectLineColor();

private:
    Ui::PlottingDialog1 *ui;
    QStandardItemModel* m_dataModel;
    static int s_curveCounter; // 静态计数器，用于生成默认名称

    QColor m_pointColor; // 当前选择的点颜色
    QColor m_lineColor;  // 当前选择的线颜色

    void populateComboBoxes(); // 初始化下拉框
    void setupStyleOptions();  // 初始化样式选项
    void updateColorButton(QPushButton* btn, const QColor& color); // 更新按钮背景色
};

#endif // PLOTTINGDIALOG1_H
