/*
 * chartwidget.h
 * 文件作用: 通用图表组件头文件 (重构版)
 * 功能描述:
 * 1. 封装 MouseZoom (QCustomPlot) 绘图控件。
 * 2. 负责管理图表的布局模式：单坐标系 (Mode_Single) 和 堆叠坐标系 (Mode_Stacked)。
 * 3. 负责处理坐标轴联动、标题显示、字体颜色统一设置。
 * 4. 提供底部工具栏：导出图片、图表设置 (自动分发)、导出数据 (信号转发)、重置视图。
 * 5. 对外提供获取绘图对象及各区域坐标轴矩形的接口。
 */

#ifndef CHARTWIDGET_H
#define CHARTWIDGET_H

#include <QWidget>
#include "mousezoom.h"
#include "qcustomplot.h"

namespace Ui {
class ChartWidget;
}

class ChartWidget : public QWidget
{
    Q_OBJECT

public:
    // 图表模式枚举
    enum ChartMode {
        Mode_Single,    // 单坐标系（如：双对数诊断图）
        Mode_Stacked    // 堆叠坐标系（如：压力+产量历史图）
    };

    // 构造函数
    explicit ChartWidget(QWidget *parent = nullptr);
    // 析构函数
    ~ChartWidget();

    // 获取内部绘图控件指针
    MouseZoom* getPlot();

    // 设置图表标题
    void setTitle(const QString& title);

    // 设置图表模式（核心函数：构建布局）
    void setChartMode(ChartMode mode);

    // 获取当前图表模式
    ChartMode getChartMode() const;

    // 获取堆叠模式下的上方坐标轴矩形（用于外部添加 Graph）
    QCPAxisRect* getTopRect() const;

    // 获取堆叠模式下的下方坐标轴矩形
    QCPAxisRect* getBottomRect() const;

signals:
    // 信号：请求导出数据（只负责通知，不包含业务逻辑）
    void exportDataTriggered();

private slots:
    // 槽函数：导出图片
    void on_btnExportImg_clicked();
    // 槽函数：图表设置
    void on_btnChartSettings_clicked();
    // 槽函数：导出数据（转发信号）
    void on_btnExportData_clicked();
    // 槽函数：重置视图
    void on_btnResetView_clicked();

private:
    Ui::ChartWidget *ui;

    QCPTextElement* m_titleElement; // 图表标题指针

    ChartMode m_currentMode;        // 当前图表模式
    QCPAxisRect* m_topRect;         // 堆叠模式上方矩形
    QCPAxisRect* m_bottomRect;      // 堆叠模式下方矩形

    // 初始化图表基础样式（默认为单图）
    void initChartStyle();
};

#endif // CHARTWIDGET_H
