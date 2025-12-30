/*
 * 文件名: fittingdatadialog.h
 * 文件作用: 拟合数据加载配置窗口头文件
 * 功能描述:
 * 1. 声明 FittingDataSettings 结构体，用于封装用户的选择（列索引、试井类型、初始压力、平滑参数等）。
 * 2. 声明 FittingDataDialog 类，提供从项目或文件加载数据、预览数据、配置列映射的界面。
 * 3. 包含了文件解析逻辑（CSV, TXT, Excel）。
 */

#ifndef FITTINGDATADIALOG_H
#define FITTINGDATADIALOG_H

#include <QDialog>
#include <QStandardItemModel>

namespace Ui {
class FittingDataDialog;
}

// 试井类型枚举
enum WellTestType {
    Test_Drawdown = 0, // 压力降落试井
    Test_Buildup = 1   // 压力恢复试井
};

// 拟合数据加载配置结构体
struct FittingDataSettings {
    bool isFromProject;         // true:从项目加载, false:从文件加载
    QString filePath;           // 文件路径

    int timeColIndex;           // 时间列索引
    int pressureColIndex;       // 压力列索引
    int derivColIndex;          // 导数列索引 (-1 表示自动计算)
    int skipRows;               // 跳过首行数

    WellTestType testType;      // 试井类型 (降落/恢复)
    double initialPressure;     // 地层初始压力 Pi (仅降落试井需要)

    bool enableSmoothing;       // 是否启用平滑
    int smoothingSpan;          // 平滑窗口大小 (奇数)
};

class FittingDataDialog : public QDialog
{
    Q_OBJECT

public:
    // 构造函数：需要传入项目数据模型用于预览
    explicit FittingDataDialog(QStandardItemModel* projectModel, QWidget *parent = nullptr);
    ~FittingDataDialog();

    // 获取用户确认后的配置
    FittingDataSettings getSettings() const;

    // 获取当前显示在预览表格中的数据模型
    QStandardItemModel* getPreviewModel() const;

private slots:
    // 数据来源改变时触发
    void onSourceChanged();

    // 点击浏览按钮时触发
    void onBrowseFile();

    // 导数列选择改变时触发（用于控制平滑选项的启用状态）
    void onDerivColumnChanged(int index);

    // 试井类型改变时触发 (控制初始压力输入框的启用状态)
    void onTestTypeChanged();

    // 启用平滑复选框切换时触发
    void onSmoothingToggled(bool checked);

    // 点击确定按钮时的校验
    void onAccepted();

private:
    Ui::FittingDataDialog *ui;

    QStandardItemModel* m_projectModel; // 项目数据引用
    QStandardItemModel* m_fileModel;    // 文件数据临时模型

    // 更新列选择下拉框的内容
    void updateColumnComboBoxes(const QStringList& headers);

    // 解析文本文件 (CSV/TXT)
    bool parseTextFile(const QString& filePath);

    // 解析 Excel 文件
    bool parseExcelFile(const QString& filePath);
};

#endif // FITTINGDATADIALOG_H
