/*
 * 文件名: plottingdialog2.h
 * 文件作用: 压力产量分析配置对话框头文件
 * 功能描述:
 * 1. 声明了获取图表名称、压力数据列及样式、产量数据列及样式的方法。
 * 2. 声明了获取坐标轴标签的方法。
 * 3. 声明了判断是否在新建窗口显示的方法。
 */

#ifndef PLOTTINGDIALOG2_H
#define PLOTTINGDIALOG2_H

#include <QDialog>
#include <QStandardItemModel>
#include <QColor>
#include "qcustomplot.h"

namespace Ui {
class PlottingDialog2;
}

class PlottingDialog2 : public QDialog
{
    Q_OBJECT

public:
    explicit PlottingDialog2(QStandardItemModel* model, QWidget *parent = nullptr);
    ~PlottingDialog2();

    // --- 全局设置 ---
    QString getChartName() const;

    // --- 压力数据 (上方图表) ---
    QString getPressLegend() const;
    int getPressXCol() const;
    int getPressYCol() const;
    QCPScatterStyle::ScatterShape getPressShape() const;
    QColor getPressPointColor() const;
    Qt::PenStyle getPressLineStyle() const;
    QColor getPressLineColor() const;

    // --- 产量数据 (下方图表) ---
    QString getProdLegend() const;
    int getProdXCol() const;
    int getProdYCol() const;
    int getProdGraphType() const; // 0=阶梯, 1=散点, 2=折线
    QColor getProdColor() const;

    // --- 坐标轴标签 ---
    QString getXLabel() const; // 时间轴
    QString getPLabel() const; // 压力轴
    QString getQLabel() const; // 产量轴

    // --- 显示设置 ---
    bool isNewWindow() const; // 是否在新建窗口显示

private slots:
    // 列名变化时自动更新图例名称
    void onPressYColChanged(int index);
    void onProdYColChanged(int index);

    // 颜色选择按钮槽函数
    void selectPressPointColor();
    void selectPressLineColor();
    void selectProdColor();

private:
    Ui::PlottingDialog2 *ui;
    QStandardItemModel* m_dataModel;
    static int s_counter;

    // 内部存储选中的颜色
    QColor m_pressPointColor;
    QColor m_pressLineColor;
    QColor m_prodColor;

    void populateComboBoxes(); // 初始化下拉列表
    void setupStyleOptions();  // 初始化样式选项
    void updateColorButton(QPushButton* btn, const QColor& color); // 更新按钮背景色
};

#endif // PLOTTINGDIALOG2_H
