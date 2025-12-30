/*
 * 文件名: datacalculate.h
 * 文件作用: 数据计算处理类头文件
 * 功能描述:
 * 1. 包含时间转换的配置对话框类 TimeConversionDialog。
 * 2. 提供 DataCalculate 类，用于执行时间格式转换和压降计算逻辑。
 * 3. 所有的计算操作都直接修改传入的 QStandardItemModel。
 */

#ifndef DATACALCULATE_H
#define DATACALCULATE_H

#include <QObject>
#include <QDialog>
#include <QStandardItemModel>
#include <QRadioButton>
#include <QComboBox>
#include <QLineEdit>
#include <QLabel>
#include "dataeditorwidget.h" // 获取相关结构体定义

// 时间转换配置结构体
struct TimeConversionConfig {
    int dateColumnIndex;
    int timeColumnIndex;
    int sourceTimeColumnIndex;
    QString outputUnit;
    QString newColumnName;
    bool useDateAndTime;
};

// 压降计算结果结构体
struct PressureDropResult {
    bool success;
    QString errorMessage;
    int addedColumnIndex;
    QString columnName;
    int processedRows;
};

// 时间转换结果结构体
struct TimeConversionResult {
    bool success;
    QString errorMessage;
    int addedColumnIndex;
    QString columnName;
    int processedRows;
};

// ============================================================================
// 时间转换设置对话框类
// ============================================================================
class TimeConversionDialog : public QDialog
{
    Q_OBJECT
public:
    explicit TimeConversionDialog(const QStringList& columnNames, QWidget* parent = nullptr);
    TimeConversionConfig getConversionConfig() const;

private slots:
    void onPreviewClicked();
    void onConversionModeChanged();

private:
    void setupUI();
    void updateUIForMode();
    // 辅助函数：生成预览字符串
    QString previewConversion(const QString& d, const QString& t, const QString& unit);

    QStringList m_columnNames;
    QRadioButton* m_dateTimeRadio;
    QRadioButton* m_timeOnlyRadio;
    QComboBox* m_dateColumnCombo;
    QComboBox* m_timeColumnCombo;
    QComboBox* m_sourceColumnCombo;
    QComboBox* m_outputUnitCombo;
    QLineEdit* m_newColumnNameEdit;
    QLabel* m_previewLabel;
};

// ============================================================================
// 数据计算逻辑处理类
// ============================================================================
class DataCalculate : public QObject
{
    Q_OBJECT
public:
    explicit DataCalculate(QObject* parent = nullptr);

    // 执行时间转换逻辑
    TimeConversionResult convertTimeColumn(QStandardItemModel* model,
                                           QList<ColumnDefinition>& definitions,
                                           const TimeConversionConfig& config);

    // 执行压降计算逻辑
    PressureDropResult calculatePressureDrop(QStandardItemModel* model,
                                             QList<ColumnDefinition>& definitions);

private:
    // 辅助函数：时间解析
    QTime parseTimeString(const QString& timeStr) const;
    QDate parseDateString(const QString& dateStr) const;
    QDateTime combineDateAndTime(const QDate& date, const QTime& time) const;
    double convertTimeToUnit(double seconds, const QString& unit) const;

    // 辅助函数：查找压力列
    int findPressureColumn(QStandardItemModel* model, const QList<ColumnDefinition>& definitions) const;
};

#endif // DATACALCULATE_H
