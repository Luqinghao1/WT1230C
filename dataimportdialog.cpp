/*
 * dataimportdialog.cpp
 * 文件作用：数据导入配置对话框实现文件
 * 功能描述:
 * 1. 实现了基于 QTextCodec 的文本文件预览。
 * 2. 实现了基于 QAxObject 的 Excel 文件预览。
 * 3. 实现了 SpinBox 交互优化（防抖 + 样式修复）。
 */

#include "dataimportdialog.h"
#include "ui_dataimportdialog.h"
#include <QFile>
#include <QDebug>
#include <QMessageBox>
#include <QStandardItemModel>
#include <QAxObject>
#include <QDir>

DataImportDialog::DataImportDialog(const QString& filePath, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::DataImportDialog),
    m_filePath(filePath),
    m_isInitializing(true),
    m_isExcelFile(false)
{
    ui->setupUi(this);
    this->setWindowTitle("数据导入配置");
    this->setStyleSheet(getStyleSheet());

    // 初始化防抖定时器，避免 SpinBox 连续点击导致卡顿
    m_previewTimer = new QTimer(this);
    m_previewTimer->setSingleShot(true);
    m_previewTimer->setInterval(200); // 200ms 延迟
    connect(m_previewTimer, &QTimer::timeout, this, &DataImportDialog::doUpdatePreview);

    initUI();
    loadDataForPreview();

    m_isInitializing = false;
    doUpdatePreview(); // 首次直接刷新

    // 连接信号
    connect(ui->comboEncoding, SIGNAL(currentIndexChanged(int)), this, SLOT(onSettingChanged()));
    connect(ui->comboSeparator, SIGNAL(currentIndexChanged(int)), this, SLOT(onSettingChanged()));

    connect(ui->spinStartRow, SIGNAL(valueChanged(int)), this, SLOT(onSettingChanged()));
    connect(ui->spinHeaderRow, SIGNAL(valueChanged(int)), this, SLOT(onSettingChanged()));

    connect(ui->checkUseHeader, &QCheckBox::toggled, [=](bool checked){
        ui->spinHeaderRow->setEnabled(checked);
        onSettingChanged();
    });
}

DataImportDialog::~DataImportDialog()
{
    delete ui;
}

void DataImportDialog::initUI()
{
    ui->comboEncoding->addItem("UTF-8");
    ui->comboEncoding->addItem("GBK/GB2312");
    ui->comboEncoding->addItem("System (Local)");
    ui->comboEncoding->addItem("ISO-8859-1");
    ui->comboEncoding->setCurrentIndex(0);

    ui->comboSeparator->addItem("自动识别 (Auto)");
    ui->comboSeparator->addItem("逗号 (Comma ,)");
    ui->comboSeparator->addItem("制表符 (Tab \\t)");
    ui->comboSeparator->addItem("空格 (Space )");
    ui->comboSeparator->addItem("分号 (Semicolon ;)");

    ui->spinStartRow->setRange(1, 999999);
    ui->spinStartRow->setValue(1);

    ui->checkUseHeader->setChecked(true);
    ui->spinHeaderRow->setRange(1, 999999);
    ui->spinHeaderRow->setValue(1);
}

void DataImportDialog::loadDataForPreview()
{
    // 检测是否为 Excel 文件
    if (m_filePath.endsWith(".xls", Qt::CaseInsensitive) ||
        m_filePath.endsWith(".xlsx", Qt::CaseInsensitive)) {
        m_isExcelFile = true;
        readExcelForPreview();

        // Excel 不需要编码和分隔符选项，禁用之
        ui->comboEncoding->setEnabled(false);
        ui->comboSeparator->setEnabled(false);
        return;
    }

    // 普通文本文件读取逻辑
    QFile file(m_filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        QMessageBox::warning(this, "错误", "无法打开文件进行预览。");
        return;
    }

    m_previewLines.clear();
    int count = 0;
    while (!file.atEnd() && count < 50) {
        m_previewLines.append(file.readLine());
        count++;
    }
    file.close();
}

void DataImportDialog::readExcelForPreview()
{
    m_excelPreviewData.clear();

    QAxObject excel("Excel.Application");
    if (excel.isNull()) {
        QMessageBox::warning(this, "警告", "未检测到 Excel 程序，无法预览 Excel 文件。\n请安装 Microsoft Excel 或 WPS。");
        return;
    }
    excel.setProperty("Visible", false);
    excel.setProperty("DisplayAlerts", false);

    QAxObject *workbooks = excel.querySubObject("Workbooks");
    if (!workbooks) return;

    // 打开文件 (路径需转为本地分隔符)
    QAxObject *workbook = workbooks->querySubObject("Open(const QString&)", QDir::toNativeSeparators(m_filePath));
    if (!workbook) {
        excel.dynamicCall("Quit()");
        return;
    }

    QAxObject *sheets = workbook->querySubObject("Worksheets");
    QAxObject *sheet = sheets->querySubObject("Item(int)", 1); // 读取第一个 Sheet

    if (sheet) {
        // 读取前 50 行
        QAxObject *usedRange = sheet->querySubObject("UsedRange");
        if (usedRange) {
            QAxObject *rows = usedRange->querySubObject("Rows");
            int rowCount = rows->property("Count").toInt();
            int readCount = (rowCount > 50) ? 50 : rowCount; // 预览只读50行

            QAxObject *columns = usedRange->querySubObject("Columns");
            int colCount = columns->property("Count").toInt();
            if (colCount > 20) colCount = 20; // 预览限制列数防止卡顿

            // 逐格读取 (对于50x20规模，速度可接受且稳定)
            for (int r = 1; r <= readCount; ++r) {
                QStringList rowData;
                for (int c = 1; c <= colCount; ++c) {
                    QAxObject *cell = sheet->querySubObject("Cells(int,int)", r, c);
                    if (cell) {
                        rowData.append(cell->property("Value").toString());
                        delete cell;
                    } else {
                        rowData.append("");
                    }
                }
                m_excelPreviewData.append(rowData);
            }
            delete columns;
            delete rows;
            delete usedRange;
        }
        delete sheet;
    }

    workbook->dynamicCall("Close()");
    delete workbook;
    delete workbooks;
    excel.dynamicCall("Quit()");
}

void DataImportDialog::onSettingChanged()
{
    if (m_isInitializing) return;
    m_previewTimer->start(); // 重置定时器，防抖
}

void DataImportDialog::doUpdatePreview()
{
    ui->tablePreview->clear();

    // ================= Excel 预览逻辑 =================
    if (m_isExcelFile) {
        if (m_excelPreviewData.isEmpty()) {
            ui->tablePreview->setRowCount(0);
            ui->tablePreview->setColumnCount(0);
            return;
        }

        int startRow = ui->spinStartRow->value() - 1;
        int headerRow = ui->spinHeaderRow->value() - 1;
        bool useHeader = ui->checkUseHeader->isChecked();

        QStringList headers;
        QList<QStringList> dataRows;

        for (int i = 0; i < m_excelPreviewData.size(); ++i) {
            // 跳过逻辑
            if (i < startRow && !(useHeader && i == headerRow)) continue;

            if (useHeader && i == headerRow) headers = m_excelPreviewData[i];
            else if (i >= startRow) dataRows.append(m_excelPreviewData[i]);
        }

        int colCount = 0;
        if (!headers.isEmpty()) colCount = headers.size();
        else if (!dataRows.isEmpty()) colCount = dataRows.first().size();

        ui->tablePreview->setColumnCount(colCount);
        if (!headers.isEmpty()) ui->tablePreview->setHorizontalHeaderLabels(headers);
        else {
            QStringList defHeaders;
            for(int i=0; i<colCount; i++) defHeaders << QString("Col %1").arg(i+1);
            ui->tablePreview->setHorizontalHeaderLabels(defHeaders);
        }

        ui->tablePreview->setRowCount(dataRows.size());
        for(int r=0; r<dataRows.size(); ++r) {
            for(int c=0; c<dataRows[r].size() && c < colCount; ++c) {
                ui->tablePreview->setItem(r, c, new QTableWidgetItem(dataRows[r][c]));
            }
        }
        return;
    }

    // ================= 文本文件预览逻辑 =================
    QTextCodec* codec = nullptr;
    QString encName = ui->comboEncoding->currentText();
    if (encName.startsWith("GBK")) codec = QTextCodec::codecForName("GBK");
    else if (encName.startsWith("UTF-8")) codec = QTextCodec::codecForName("UTF-8");
    else if (encName.startsWith("ISO")) codec = QTextCodec::codecForName("ISO-8859-1");
    else codec = QTextCodec::codecForLocale();
    if (!codec) codec = QTextCodec::codecForName("UTF-8");

    int startRow = ui->spinStartRow->value() - 1;
    int headerRow = ui->spinHeaderRow->value() - 1;
    bool useHeader = ui->checkUseHeader->isChecked();

    QStringList headers;
    QList<QStringList> dataRows;

    QChar separator = ',';
    if (!m_previewLines.isEmpty()) {
        QString firstLine = codec->toUnicode(m_previewLines.first());
        separator = getSeparatorChar(ui->comboSeparator->currentText(), firstLine);
    }

    for (int i = 0; i < m_previewLines.size(); ++i) {
        if (i < startRow && !(useHeader && i == headerRow)) continue;

        QString line = codec->toUnicode(m_previewLines[i]).trimmed();
        if (line.isEmpty()) continue;

        QStringList fields = line.split(separator);
        for (int k=0; k<fields.size(); ++k) {
            QString f = fields[k].trimmed();
            if (f.startsWith('"') && f.endsWith('"')) f = f.mid(1, f.length()-2);
            fields[k] = f;
        }

        if (useHeader && i == headerRow) headers = fields;
        else if (i >= startRow) dataRows.append(fields);
    }

    int colCount = 0;
    if (!headers.isEmpty()) colCount = headers.size();
    else if (!dataRows.isEmpty()) colCount = dataRows.first().size();

    ui->tablePreview->setColumnCount(colCount);
    if (!headers.isEmpty()) ui->tablePreview->setHorizontalHeaderLabels(headers);
    else {
        QStringList defHeaders;
        for(int i=0; i<colCount; i++) defHeaders << QString("Col %1").arg(i+1);
        ui->tablePreview->setHorizontalHeaderLabels(defHeaders);
    }

    ui->tablePreview->setRowCount(dataRows.size());
    for(int r=0; r<dataRows.size(); ++r) {
        for(int c=0; c<dataRows[r].size() && c < colCount; ++c) {
            ui->tablePreview->setItem(r, c, new QTableWidgetItem(dataRows[r][c]));
        }
    }
}

DataImportSettings DataImportDialog::getSettings() const
{
    DataImportSettings s;
    s.filePath = m_filePath;
    s.encoding = ui->comboEncoding->currentText();
    s.separator = ui->comboSeparator->currentText();
    s.startRow = ui->spinStartRow->value();
    s.useHeader = ui->checkUseHeader->isChecked();
    s.headerRow = ui->spinHeaderRow->value();
    s.isExcel = m_isExcelFile;
    return s;
}

QChar DataImportDialog::getSeparatorChar(const QString& sepStr, const QString& lineData)
{
    if (sepStr.contains("Comma")) return ',';
    if (sepStr.contains("Tab")) return '\t';
    if (sepStr.contains("Space")) return ' ';
    if (sepStr.contains("Semicolon")) return ';';
    if (sepStr.contains("Auto")) {
        if (lineData.count('\t') > lineData.count(',')) return '\t';
        return ',';
    }
    return ',';
}

QString DataImportDialog::getStyleSheet() const
{
    // [修复] 移除了 QSpinBox 的 border 属性，解决按钮无法点击的问题
    return "QDialog, QWidget { background-color: #ffffff; color: #000000; }"
           "QLabel { color: #000000; }"
           "QGroupBox { color: #000000; font-weight: bold; border: 1px solid #cccccc; margin-top: 10px; }"
           "QGroupBox::title { subcontrol-origin: margin; subcontrol-position: top left; padding: 0 5px; }"
           "QComboBox { background-color: #ffffff; color: #000000; border: 1px solid #999999; padding: 3px; }"
           "QSpinBox { background-color: #ffffff; color: #000000; padding: 2px; }"
           "QCheckBox { color: #000000; }"
           "QTableWidget { gridline-color: #cccccc; color: #000000; background-color: #ffffff; alternate-background-color: #f9f9f9; }"
           "QHeaderView::section { background-color: #f0f0f0; color: #000000; border: 1px solid #cccccc; }"
           "QPushButton { background-color: #f0f0f0; color: #000000; border: 1px solid #999999; padding: 5px 15px; border-radius: 3px; }"
           "QPushButton:hover { background-color: #e0e0e0; }";
}
