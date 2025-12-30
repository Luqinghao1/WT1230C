/*
 * chartsetting1.h
 * 文件作用：图表设置对话框类的头文件
 * 功能描述：
 * 1. 声明 ChartSetting1 类，继承自 QDialog
 * 2. 包含对图表对象 (MouseZoom/QCustomPlot) 和标题对象 (QCPTextElement) 的引用
 * 3. 声明用于初始化界面数据 (initData) 和应用设置 (applySettings) 的私有函数
 * 4. 声明界面按钮的槽函数
 */

#ifndef CHARTSETTING1_H
#define CHARTSETTING1_H

#include <QDialog>
#include "mousezoom.h" // 引用自定义绘图控件头文件
#include "qcustomplot.h"

namespace Ui {
class ChartSetting1;
}

class ChartSetting1 : public QDialog
{
    Q_OBJECT

public:
    // 构造函数：接收绘图控件指针、标题指针和父窗口
    explicit ChartSetting1(MouseZoom* plot, QCPTextElement* title, QWidget *parent = nullptr);
    ~ChartSetting1();

private slots:
    // 确定按钮槽函数：应用设置并关闭对话框
    void on_btnOk_clicked();

    // 取消按钮槽函数：直接关闭对话框
    void on_btnCancel_clicked();

    // 应用按钮槽函数：应用设置但不关闭对话框
    void on_btnApply_clicked();

private:
    Ui::ChartSetting1 *ui;
    MouseZoom* m_plot;       // 指向主界面的绘图控件
    QCPTextElement* m_title; // 指向图表的标题元素

    // 初始化界面数据：读取当前图表的设置并显示在界面上
    void initData();

    // 应用设置：将界面上的设置应用到图表对象中
    void applySettings();
};

#endif // CHARTSETTING1_H
