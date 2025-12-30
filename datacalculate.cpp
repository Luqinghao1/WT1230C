/*
 * 文件名: datacalculate.cpp
 * 文件作用: 数据计算处理类实现文件
 * 功能描述:
 * 1. 实现时间转换弹窗的UI构建和交互。
 * 2. 实现核心的时间数据解析和转换算法。
 * 3. 实现基于压力列的压降计算算法。
 */

#include "datacalculate.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QPushButton>
#include <QDebug>
#include <QDateTime>

// ============================================================================
// TimeConversionDialog 实现
// ============================================================================

TimeConversionDialog::TimeConversionDialog(const QStringList& columnNames, QWidget* parent)
    : QDialog(parent), m_columnNames(columnNames)
{
    setupUI();
    updateUIForMode();
}

void TimeConversionDialog::setupUI()
{
    setWindowTitle("时间转换设置");
    resize(500, 400);
    // 设置白色背景黑色字体，统一UI风格
    setStyleSheet("QDialog { background-color: white; color: black; font-family: \"Microsoft YaHei\", Arial; } "
                  "QLabel { color: black; background: transparent; } "
                  "QGroupBox { color: black; border: 1px solid #ccc; margin-top: 10px; font-weight: bold; } "
                  "QGroupBox::title { subcontrol-origin: margin; subcontrol-position: top left; padding: 0 3px; } "
                  "QRadioButton { color: black; background: transparent; } "
                  "QComboBox { color: black; background-color: white; border: 1px solid #ccc; padding: 2px; } "
                  "QComboBox QAbstractItemView { background-color: white; color: black; selection-background-color: #e0e0e0; } "
                  "QLineEdit { color: black; background-color: white; border: 1px solid #ccc; padding: 2px; } "
                  "QPushButton { color: white; background-color: #4a90e2; border: none; border-radius: 4px; padding: 6px 12px; } "
                  "QPushButton:hover { background-color: #357abd; }");

    QVBoxLayout* mainLayout = new QVBoxLayout(this);

    // 模式选择组
    QGroupBox* modeGroup = new QGroupBox("转换模式");
    QVBoxLayout* modeLayout = new QVBoxLayout(modeGroup);
    m_dateTimeRadio = new QRadioButton("日期+时刻模式 (yyyy-MM-dd hh:mm:ss)");
    m_timeOnlyRadio = new QRadioButton("仅时间模式 (累计时间)");
    m_timeOnlyRadio->setChecked(true); // 默认选中间模式
    modeLayout->addWidget(m_dateTimeRadio);
    modeLayout->addWidget(m_timeOnlyRadio);
    mainLayout->addWidget(modeGroup);

    connect(m_dateTimeRadio, &QRadioButton::toggled, this, &TimeConversionDialog::onConversionModeChanged);

    // 参数配置组
    QGroupBox* configGroup = new QGroupBox("配置参数");
    QFormLayout* formLayout = new QFormLayout(configGroup);

    // 初始化下列框
    m_dateColumnCombo = new QComboBox;
    m_dateColumnCombo->addItems(m_columnNames);
    m_timeColumnCombo = new QComboBox;
    m_timeColumnCombo->addItems(m_columnNames);
    m_sourceColumnCombo = new QComboBox;
    m_sourceColumnCombo->addItems(m_columnNames);

    m_newColumnNameEdit = new QLineEdit("时间");
    m_outputUnitCombo = new QComboBox;
    m_outputUnitCombo->addItems({"h", "min", "s"});

    formLayout->addRow("日期列:", m_dateColumnCombo);
    formLayout->addRow("时刻列:", m_timeColumnCombo);
    formLayout->addRow("源时间列:", m_sourceColumnCombo);
    formLayout->addRow("新列名:", m_newColumnNameEdit);
    formLayout->addRow("输出单位:", m_outputUnitCombo);
    mainLayout->addWidget(configGroup);

    // 预览区域
    QGroupBox* previewGroup = new QGroupBox("预览");
    QVBoxLayout* prevLayout = new QVBoxLayout(previewGroup);
    QPushButton* btnPreview = new QPushButton("生成预览");
    connect(btnPreview, &QPushButton::clicked, this, &TimeConversionDialog::onPreviewClicked);
    m_previewLabel = new QLabel("点击按钮查看效果");
    m_previewLabel->setStyleSheet("color: #666; font-style: italic;");
    prevLayout->addWidget(btnPreview);
    prevLayout->addWidget(m_previewLabel);
    mainLayout->addWidget(previewGroup);

    // 底部按钮
    QHBoxLayout* btnLayout = new QHBoxLayout;
    btnLayout->addStretch();
    QPushButton* btnOk = new QPushButton("确定");
    QPushButton* btnCancel = new QPushButton("取消");
    btnOk->setStyleSheet("background-color: #28a745; color: white;");
    btnCancel->setStyleSheet("background-color: #6c757d; color: white;");

    connect(btnOk, &QPushButton::clicked, this, &QDialog::accept);
    connect(btnCancel, &QPushButton::clicked, this, &QDialog::reject);

    btnLayout->addWidget(btnOk);
    btnLayout->addWidget(btnCancel);
    mainLayout->addLayout(btnLayout);
}

void TimeConversionDialog::updateUIForMode()
{
    bool useDate = m_dateTimeRadio->isChecked();
    // 根据模式启用或禁用对应的下拉框
    m_dateColumnCombo->setEnabled(useDate);
    m_timeColumnCombo->setEnabled(useDate);
    m_sourceColumnCombo->setEnabled(!useDate);
}

void TimeConversionDialog::onConversionModeChanged()
{
    updateUIForMode();
}

void TimeConversionDialog::onPreviewClicked()
{
    QString unit = m_outputUnitCombo->currentText();
    QString preview;

    // 修复字符串拼接错误：使用 QString() 包裹中文字符串字面量
    if (m_dateTimeRadio->isChecked()) {
        QString val = (unit=="h" ? "1.000" : (unit=="min" ? "60.000" : "3600.000"));
        preview = QString("示例: 2025-01-01 10:00:00 -> 0 ") + unit + QString("\n");
        preview += QString("示例: 2025-01-01 11:00:00 -> ") + val + QString(" ") + unit;
    } else {
        QString val = (unit=="h" ? "0.500" : (unit=="min" ? "30.000" : "1800.000"));
        preview = QString("示例: 10:00:00 (基准) -> 0 ") + unit + QString("\n");
        preview += QString("示例: 10:30:00 -> ") + val + QString(" ") + unit;
    }
    m_previewLabel->setText(preview);
}

TimeConversionConfig TimeConversionDialog::getConversionConfig() const
{
    TimeConversionConfig c;
    c.useDateAndTime = m_dateTimeRadio->isChecked();
    c.dateColumnIndex = m_dateColumnCombo->currentIndex();
    c.timeColumnIndex = m_timeColumnCombo->currentIndex();
    c.sourceTimeColumnIndex = m_sourceColumnCombo->currentIndex();
    c.newColumnName = m_newColumnNameEdit->text();
    c.outputUnit = m_outputUnitCombo->currentText();
    return c;
}

// ============================================================================
// DataCalculate 实现
// ============================================================================

DataCalculate::DataCalculate(QObject* parent) : QObject(parent) {}

TimeConversionResult DataCalculate::convertTimeColumn(QStandardItemModel* model,
                                                      QList<ColumnDefinition>& definitions,
                                                      const TimeConversionConfig& config)
{
    TimeConversionResult result;
    result.success = false;
    result.processedRows = 0;

    if (!model) {
        result.errorMessage = "数据模型为空";
        return result;
    }

    int rowCount = model->rowCount();
    if (rowCount == 0) {
        result.errorMessage = "没有数据";
        return result;
    }

    // 在末尾插入新列
    int newColIdx = model->columnCount();
    model->insertColumn(newColIdx);

    // 更新列定义
    ColumnDefinition newDef;
    newDef.name = config.newColumnName + "\\" + config.outputUnit;
    newDef.type = WellTestColumnType::Time;
    newDef.unit = config.outputUnit;
    newDef.decimalPlaces = 3;
    definitions.append(newDef);

    // 设置表头
    model->setHorizontalHeaderItem(newColIdx, new QStandardItem(newDef.name));

    // 计算逻辑
    QDateTime baseTime;
    bool baseSet = false;

    for (int i = 0; i < rowCount; ++i) {
        double val = 0.0;
        bool valid = false;

        if (config.useDateAndTime) {
            // 日期+时刻模式
            QString dStr = model->item(i, config.dateColumnIndex)->text();
            QString tStr = model->item(i, config.timeColumnIndex)->text();
            QDate d = parseDateString(dStr);
            QTime t = parseTimeString(tStr);
            if (d.isValid() && t.isValid()) {
                QDateTime dt = combineDateAndTime(d, t);
                if (!baseSet) { baseTime = dt; baseSet = true; }
                double seconds = baseTime.secsTo(dt);
                val = convertTimeToUnit(seconds, config.outputUnit);
                valid = true;
            }
        } else {
            // 仅时间模式
            QString tStr = model->item(i, config.sourceTimeColumnIndex)->text();
            QTime t = parseTimeString(tStr);
            if (t.isValid()) {
                // 如果没有日期，取当前日期与该时间组合
                if (!baseSet) {
                    baseTime = QDateTime(QDate::currentDate(), t);
                    baseSet = true;
                }
                QDateTime dt(QDate::currentDate(), t);
                // 处理跨天情况(简单处理: 如果时间比基准小很多，假设是第二天)
                if (dt < baseTime) dt = dt.addDays(1);

                double seconds = baseTime.secsTo(dt);
                val = convertTimeToUnit(seconds, config.outputUnit);
                valid = true;
            }
        }

        if (valid) {
            model->setItem(i, newColIdx, new QStandardItem(QString::number(val, 'f', 3)));
            result.processedRows++;
        } else {
            model->setItem(i, newColIdx, new QStandardItem(""));
        }
    }

    result.success = true;
    result.addedColumnIndex = newColIdx;
    result.columnName = newDef.name;
    return result;
}

PressureDropResult DataCalculate::calculatePressureDrop(QStandardItemModel* model,
                                                        QList<ColumnDefinition>& definitions)
{
    PressureDropResult result;
    result.success = false;

    int pIdx = findPressureColumn(model, definitions);
    if (pIdx == -1) {
        result.errorMessage = "未找到压力列，请先定义列属性。";
        return result;
    }

    QString unit = definitions[pIdx].unit;
    int newColIdx = model->columnCount();
    model->insertColumn(newColIdx);

    ColumnDefinition newDef;
    newDef.name = "压降\\" + unit;
    newDef.type = WellTestColumnType::PressureDrop;
    newDef.unit = unit;
    newDef.decimalPlaces = 3;
    definitions.append(newDef);

    model->setHorizontalHeaderItem(newColIdx, new QStandardItem(newDef.name));

    double initialPressure = 0.0;
    bool initSet = false;

    for (int i = 0; i < model->rowCount(); ++i) {
        QString pText = model->item(i, pIdx)->text();
        bool ok;
        double p = pText.toDouble(&ok);

        if (ok) {
            if (!initSet) { initialPressure = p; initSet = true; }
            double drop = initialPressure - p;
            model->setItem(i, newColIdx, new QStandardItem(QString::number(drop, 'f', 3)));
            result.processedRows++;
        } else {
            model->setItem(i, newColIdx, new QStandardItem(""));
        }
    }

    result.success = true;
    result.addedColumnIndex = newColIdx;
    result.columnName = newDef.name;
    return result;
}

// 辅助函数实现
QTime DataCalculate::parseTimeString(const QString& timeStr) const {
    QStringList fmts = {"hh:mm:ss", "h:mm:ss", "hh:mm"};
    for(const auto& f : fmts) {
        QTime t = QTime::fromString(timeStr, f);
        if(t.isValid()) return t;
    }
    return QTime();
}

QDate DataCalculate::parseDateString(const QString& dateStr) const {
    QStringList fmts = {"yyyy-MM-dd", "yyyy/MM/dd"};
    for(const auto& f : fmts) {
        QDate d = QDate::fromString(dateStr, f);
        if(d.isValid()) return d;
    }
    return QDate();
}

QDateTime DataCalculate::combineDateAndTime(const QDate& date, const QTime& time) const {
    return QDateTime(date, time);
}

double DataCalculate::convertTimeToUnit(double seconds, const QString& unit) const {
    if (unit == "h") return seconds / 3600.0;
    if (unit == "min") return seconds / 60.0;
    return seconds;
}

int DataCalculate::findPressureColumn(QStandardItemModel* model, const QList<ColumnDefinition>& definitions) const {
    for(int i=0; i<definitions.size(); ++i) {
        if(definitions[i].type == WellTestColumnType::Pressure) return i;
    }
    // 简单的名称回退查找
    for(int i=0; i<model->columnCount(); ++i) {
        QString h = model->headerData(i, Qt::Horizontal).toString();
        if(h.contains("压力") || h.contains("pressure", Qt::CaseInsensitive)) return i;
    }
    return -1;
}
