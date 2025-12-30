/*
 * 文件名: plottingdialog4.h
 * 文件作用: 曲线样式管理/编辑对话框头文件
 * 功能描述:
 * 1. 提供对已生成曲线的样式（颜色、点形、线型、图例）进行修改的功能。
 * 2. 支持单条曲线和双曲线（如压力+导数）的分别设置。
 */

#ifndef PLOTTINGDIALOG4_H
#define PLOTTINGDIALOG4_H

#include <QDialog>
#include <QStandardItemModel>
#include <QColor>
#include "qcustomplot.h"

namespace Ui {
class PlottingDialog4;
}

class PlottingDialog4 : public QDialog
{
    Q_OBJECT

public:
    // 构造函数
    explicit PlottingDialog4(QStandardItemModel* model, QWidget *parent = nullptr);
    ~PlottingDialog4();

    /**
     * @brief 初始化数据 (支持单/双曲线)
     * @param hasSecondCurve 是否包含第二条曲线
     * @param name1 主曲线图例
     * @param xCol X轴列索引
     * @param yCol Y轴列索引
     * @param shape1 主曲线点形
     * @param c1 主曲线点颜色
     * @param ls1 主曲线线型
     * @param lc1 主曲线线颜色
     * @param name2 副曲线图例 (可选)
     * @param shape2 副曲线点形 (可选)
     * @param c2 副曲线点颜色 (可选)
     * @param ls2 副曲线线型 (可选)
     * @param lc2 副曲线线颜色 (可选)
     */
    void setInitialData(bool hasSecondCurve,
                        QString name1, int xCol, int yCol,
                        QCPScatterStyle::ScatterShape shape1, QColor c1, Qt::PenStyle ls1, QColor lc1,
                        QString name2 = "",
                        QCPScatterStyle::ScatterShape shape2 = QCPScatterStyle::ssNone, QColor c2 = Qt::black,
                        Qt::PenStyle ls2 = Qt::SolidLine, QColor lc2 = Qt::black);

    // --- Getters Curve 1 ---
    QString getLegendName1() const;
    int getXColumn() const;
    int getYColumn() const;
    QCPScatterStyle::ScatterShape getPointShape1() const;
    QColor getPointColor1() const;
    Qt::PenStyle getLineStyle1() const;
    QColor getLineColor1() const;

    // --- Getters Curve 2 ---
    QString getLegendName2() const;
    QCPScatterStyle::ScatterShape getPointShape2() const;
    QColor getPointColor2() const;
    Qt::PenStyle getLineStyle2() const;
    QColor getLineColor2() const;

private slots:
    void onBtnColor1Clicked();
    void onBtnLineColor1Clicked();
    void onBtnColor2Clicked();
    void onBtnLineColor2Clicked();

private:
    Ui::PlottingDialog4 *ui;
    QStandardItemModel* m_dataModel;

    QColor m_color1, m_lineColor1;
    QColor m_color2, m_lineColor2;

    void populateComboBoxes();
    void updateColorButton(QPushButton* btn, const QColor& c);
};

#endif // PLOTTINGDIALOG4_H
