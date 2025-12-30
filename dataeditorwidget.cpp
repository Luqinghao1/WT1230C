/*
 * 文件名: dataeditorwidget.cpp
 * 文件作用: 数据编辑器主窗口实现文件
 * 功能描述:
 * 1. 实现了表格数据的增删改查、排序和过滤功能。
 * 2. 集成了 DataImportDialog，支持配置化导入 CSV/TXT 文件。
 * 3. 集成了 QAxObject，支持直接读取 Excel (.xls/.xlsx) 文件内容到表格。
 * 4. 实现了数据与项目文件的同步保存与恢复。
 */

#include "dataeditorwidget.h"
#include "ui_dataeditorwidget.h"
#include "datacolumndialog.h"
#include "datacalculate.h"
#include "modelparameter.h"
#include "dataimportdialog.h"

#include <QFileDialog>
#include <QMessageBox>
#include <QTextStream>
#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTimer>
#include <QTextCodec>
#include <QLineEdit>
#include <QEvent>
#include <QAxObject> // 用于 Excel 操作
#include <QDir>      // 用于路径转换

// ============================================================================
// 内部类：NoContextMenuDelegate 实现
// ============================================================================
class EditorEventFilter : public QObject {
public:
    EditorEventFilter(QObject *parent) : QObject(parent) {}
protected:
    bool eventFilter(QObject *obj, QEvent *event) override {
        if (event->type() == QEvent::ContextMenu) {
            return true; // 拦截右键菜单事件
        }
        return QObject::eventFilter(obj, event);
    }
};

QWidget *NoContextMenuDelegate::createEditor(QWidget *parent, const QStyleOptionViewItem &option,
                                             const QModelIndex &index) const
{
    QWidget *editor = QStyledItemDelegate::createEditor(parent, option, index);
    if (editor) {
        editor->installEventFilter(new EditorEventFilter(editor));
    }
    return editor;
}

// ============================================================================
// DataEditorWidget 实现
// ============================================================================

DataEditorWidget::DataEditorWidget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::DataEditorWidget),
    m_dataModel(new QStandardItemModel(this)),
    m_proxyModel(new QSortFilterProxyModel(this)),
    m_undoStack(new QUndoStack(this))
{
    ui->setupUi(this);
    initUI();
    setupModel();
    setupConnections();

    // 初始化搜索防抖定时器
    m_searchTimer = new QTimer(this);
    m_searchTimer->setSingleShot(true);
    m_searchTimer->setInterval(300);
    connect(m_searchTimer, &QTimer::timeout, this, [this](){
        m_proxyModel->setFilterWildcard(ui->searchLineEdit->text());
    });
}

DataEditorWidget::~DataEditorWidget()
{
    delete ui;
}

void DataEditorWidget::initUI()
{
    ui->dataTableView->setContextMenuPolicy(Qt::CustomContextMenu);
    ui->dataTableView->setItemDelegate(new NoContextMenuDelegate(this));
    updateButtonsState();
}

void DataEditorWidget::setupModel()
{
    m_proxyModel->setSourceModel(m_dataModel);
    m_proxyModel->setFilterCaseSensitivity(Qt::CaseInsensitive);
    ui->dataTableView->setModel(m_proxyModel);
    ui->dataTableView->setSelectionBehavior(QAbstractItemView::SelectItems);
    ui->dataTableView->setSelectionMode(QAbstractItemView::ExtendedSelection);
}

void DataEditorWidget::setupConnections()
{
    connect(ui->btnOpenFile, &QPushButton::clicked, this, &DataEditorWidget::onOpenFile);
    connect(ui->btnSave, &QPushButton::clicked, this, &DataEditorWidget::onSave);
    connect(ui->btnDefineColumns, &QPushButton::clicked, this, &DataEditorWidget::onDefineColumns);
    connect(ui->btnTimeConvert, &QPushButton::clicked, this, &DataEditorWidget::onTimeConvert);
    connect(ui->btnPressureDropCalc, &QPushButton::clicked, this, &DataEditorWidget::onPressureDropCalc);
    connect(ui->searchLineEdit, &QLineEdit::textChanged, this, &DataEditorWidget::onSearchTextChanged);
    connect(ui->dataTableView, &QTableView::customContextMenuRequested, this, &DataEditorWidget::onCustomContextMenu);
    connect(m_dataModel, &QStandardItemModel::itemChanged, this, &DataEditorWidget::onModelDataChanged);
}

void DataEditorWidget::updateButtonsState()
{
    bool hasData = m_dataModel->rowCount() > 0 && m_dataModel->columnCount() > 0;
    ui->btnSave->setEnabled(hasData);
    ui->btnDefineColumns->setEnabled(hasData);
    ui->btnTimeConvert->setEnabled(hasData);
    ui->btnPressureDropCalc->setEnabled(hasData);
}

// ============================================================================
// 公共接口
// ============================================================================

QStandardItemModel* DataEditorWidget::getDataModel() const { return m_dataModel; }
QString DataEditorWidget::getCurrentFileName() const { return m_currentFilePath; }
bool DataEditorWidget::hasData() const { return m_dataModel->rowCount() > 0; }

void DataEditorWidget::loadData(const QString& filePath, const QString& fileType)
{
    if (loadFileInternal(filePath)) {
        emit fileChanged(filePath, fileType);
    }
}

// ============================================================================
// 文件加载
// ============================================================================

void DataEditorWidget::onOpenFile()
{
    QString filter = "所有支持文件 (*.csv *.txt *.xls *.xlsx);;CSV 文件 (*.csv);;文本文件 (*.txt);;Excel (*.xls *.xlsx);;所有文件 (*.*)";
    QString path = QFileDialog::getOpenFileName(this, "打开数据文件", "", filter);
    if (path.isEmpty()) return;

    // JSON 直接加载
    if (path.endsWith(".json", Qt::CaseInsensitive)) {
        loadData(path, "json");
        return;
    }

    // 弹出数据导入配置对话框
    DataImportDialog dlg(path, this);
    if (dlg.exec() == QDialog::Accepted) {
        DataImportSettings settings = dlg.getSettings();

        m_currentFilePath = path;
        ui->filePathLabel->setText("当前文件: " + path);

        if (loadFileWithConfig(settings)) {
            ui->statusLabel->setText("加载成功");
            updateButtonsState();
            emit fileChanged(path, "text");
            emit dataChanged();
        } else {
            ui->statusLabel->setText("加载失败");
        }
    }
}

bool DataEditorWidget::loadFileInternal(const QString& path)
{
    DataImportSettings defaultSettings;
    defaultSettings.filePath = path;
    defaultSettings.encoding = "Auto";
    defaultSettings.separator = "Auto";
    defaultSettings.startRow = 1;
    defaultSettings.useHeader = true;
    defaultSettings.headerRow = 1;
    defaultSettings.isExcel = false;

    // 简单后缀判断
    if (path.endsWith(".xls", Qt::CaseInsensitive) || path.endsWith(".xlsx", Qt::CaseInsensitive)) {
        defaultSettings.isExcel = true;
    }

    if (path.endsWith(".json", Qt::CaseInsensitive)) {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly)) return false;
        QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
        file.close();
        if (doc.isArray()) {
            deserializeJsonToModel(doc.array());
            return true;
        }
        return false;
    } else {
        return loadFileWithConfig(defaultSettings);
    }
}

bool DataEditorWidget::loadFileWithConfig(const DataImportSettings& settings)
{
    m_dataModel->clear();
    m_columnDefinitions.clear();

    // ================= Excel 加载逻辑 =================
    if (settings.isExcel) {
        // [修复] 将 headerProcessed 声明在 if 块的最顶层，确保后续逻辑可见
        bool headerProcessed = false;

        QAxObject excel("Excel.Application");
        if (excel.isNull()) {
            QMessageBox::critical(this, "错误", "未检测到 Excel 程序，无法读取 .xls/.xlsx 文件。\n请安装 Microsoft Excel 或 WPS，或者将文件另存为 CSV 格式。");
            return false;
        }
        excel.setProperty("Visible", false);
        excel.setProperty("DisplayAlerts", false);

        QAxObject *workbooks = excel.querySubObject("Workbooks");
        if (!workbooks) return false;

        // 打开工作簿
        QAxObject *workbook = workbooks->querySubObject("Open(const QString&)", QDir::toNativeSeparators(settings.filePath));
        if (!workbook) {
            excel.dynamicCall("Quit()");
            QMessageBox::critical(this, "错误", "无法打开 Excel 文件，可能是文件被占用或格式错误。");
            return false;
        }

        QAxObject *sheets = workbook->querySubObject("Worksheets");
        QAxObject *sheet = sheets->querySubObject("Item(int)", 1); // 读取第一个 Sheet

        if (sheet) {
            QAxObject *usedRange = sheet->querySubObject("UsedRange");
            if (usedRange) {
                // 将数据读入 QVariantList (效率较高)
                QVariant varData = usedRange->dynamicCall("Value()");

                QList<QList<QVariant>> rowsData;

                // 处理返回的数据类型
                if (varData.type() == QVariant::List) {
                    QList<QVariant> rows = varData.toList();
                    for (const QVariant &row : rows) {
                        if (row.type() == QVariant::List) {
                            rowsData.append(row.toList());
                        }
                    }
                }

                int startIdx = settings.startRow - 1;
                int headerIdx = settings.headerRow - 1;

                for (int i = 0; i < rowsData.size(); ++i) {
                    // 跳过非数据行且非表头行
                    if (i < startIdx && !(settings.useHeader && i == headerIdx)) continue;

                    QList<QVariant> row = rowsData[i];
                    QStringList fields;
                    for (const QVariant &cell : row) fields.append(cell.toString());

                    // 表头处理
                    if (settings.useHeader && i == headerIdx) {
                        m_dataModel->setHorizontalHeaderLabels(fields);
                        for(const QString& h : fields) {
                            ColumnDefinition def; def.name = h;
                            m_columnDefinitions.append(def);
                        }
                        headerProcessed = true;
                    }
                    // 数据行处理
                    else if (i >= startIdx) {
                        QList<QStandardItem*> items;
                        for(const QString& field : fields) items.append(new QStandardItem(field));
                        m_dataModel->appendRow(items);
                    }
                }
                delete usedRange;
            }
            delete sheet;
        }

        workbook->dynamicCall("Close()");
        delete workbook;
        delete workbooks;
        excel.dynamicCall("Quit()");

        // 默认表头处理（如果未找到表头或列定义为空）
        if (!headerProcessed || m_columnDefinitions.isEmpty()) {
            int cols = m_dataModel->columnCount();
            QStringList defHeaders;
            for(int i=0; i<cols; i++) {
                QString name = QString("Col %1").arg(i+1);
                defHeaders << name;
                ColumnDefinition def; def.name = name;
                m_columnDefinitions.append(def);
            }
            m_dataModel->setHorizontalHeaderLabels(defHeaders);
        }
        return true;
    }

    // ================= 文本文件加载逻辑 =================
    QFile file(settings.filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::critical(this, "错误", "无法打开文件: " + settings.filePath);
        return false;
    }

    // 选择解码器
    QTextCodec* codec = nullptr;
    if (settings.encoding.startsWith("GBK")) codec = QTextCodec::codecForName("GBK");
    else if (settings.encoding.startsWith("UTF-8")) codec = QTextCodec::codecForName("UTF-8");
    else if (settings.encoding.startsWith("ISO")) codec = QTextCodec::codecForName("ISO-8859-1");
    else codec = QTextCodec::codecForLocale();
    if (!codec) codec = QTextCodec::codecForName("UTF-8");

    // 读取全部内容并解码
    QByteArray fileData = file.readAll();
    file.close();

    QString fileContent = codec->toUnicode(fileData);
    QTextStream in(&fileContent);

    QList<QString> allLines;
    while (!in.atEnd()) {
        allLines.append(in.readLine());
    }

    // 确定分隔符
    QChar separator = ',';
    if (!allLines.isEmpty()) {
        QString firstLine = allLines.first();
        if (settings.separator.contains("Tab")) separator = '\t';
        else if (settings.separator.contains("Space")) separator = ' ';
        else if (settings.separator.contains("Semicolon")) separator = ';';
        else if (settings.separator.contains("Auto")) {
            if (firstLine.count('\t') > firstLine.count(',')) separator = '\t';
        }
    }

    bool headerProcessed = false;
    int startIdx = settings.startRow - 1;
    int headerIdx = settings.headerRow - 1;

    // 解析文本行
    for (int i = 0; i < allLines.size(); ++i) {
        if (i < startIdx && !(settings.useHeader && i == headerIdx)) continue;
        QString line = allLines[i].trimmed();
        if (line.isEmpty()) continue;

        QStringList fields = line.split(separator);
        // 去除引号
        for(int k=0; k<fields.size(); ++k) {
            QString f = fields[k].trimmed();
            if (f.startsWith('"') && f.endsWith('"')) f = f.mid(1, f.length()-2);
            fields[k] = f;
        }

        if (settings.useHeader && i == headerIdx) {
            m_dataModel->setHorizontalHeaderLabels(fields);
            for(const QString& h : fields) {
                ColumnDefinition def; def.name = h;
                m_columnDefinitions.append(def);
            }
            headerProcessed = true;
        } else if (i >= startIdx) {
            QList<QStandardItem*> items;
            for(const QString& field : fields) items.append(new QStandardItem(field));
            m_dataModel->appendRow(items);
        }
    }

    if (!headerProcessed || m_columnDefinitions.isEmpty()) {
        int cols = m_dataModel->columnCount();
        QStringList defHeaders;
        for(int i=0; i<cols; i++) {
            QString name = QString("Col %1").arg(i+1);
            defHeaders << name;
            ColumnDefinition def; def.name = name;
            m_columnDefinitions.append(def);
        }
        m_dataModel->setHorizontalHeaderLabels(defHeaders);
    }

    return true;
}

// ============================================================================
// 数据保存与恢复
// ============================================================================

void DataEditorWidget::onSave()
{
    QJsonArray data = serializeModelToJson();
    ModelParameter::instance()->saveTableData(data);
    ModelParameter::instance()->saveProject();
    QMessageBox::information(this, "保存", "数据已成功保存至项目文件(.pwt)。");
}

void DataEditorWidget::loadFromProjectData()
{
    QJsonArray data = ModelParameter::instance()->getTableData();
    if (!data.isEmpty()) {
        deserializeJsonToModel(data);
        ui->statusLabel->setText("已恢复项目数据");
        updateButtonsState();
        emit dataChanged();
    } else {
        m_dataModel->clear();
        m_columnDefinitions.clear();
        ui->statusLabel->setText("无数据");
        updateButtonsState();
    }
}

QJsonArray DataEditorWidget::serializeModelToJson() const
{
    QJsonArray array;
    QJsonObject headerObj;
    QJsonArray headers;
    for(int i=0; i<m_dataModel->columnCount(); ++i) {
        headers.append(m_dataModel->headerData(i, Qt::Horizontal).toString());
    }
    headerObj["headers"] = headers;
    array.append(headerObj);

    for(int i=0; i<m_dataModel->rowCount(); ++i) {
        QJsonArray rowArr;
        for(int j=0; j<m_dataModel->columnCount(); ++j) {
            rowArr.append(m_dataModel->item(i, j)->text());
        }
        QJsonObject rowObj;
        rowObj["row_data"] = rowArr;
        array.append(rowObj);
    }
    return array;
}

void DataEditorWidget::deserializeJsonToModel(const QJsonArray& array)
{
    m_dataModel->clear();
    m_columnDefinitions.clear();
    if (array.isEmpty()) return;

    QJsonObject headerObj = array.first().toObject();
    if (headerObj.contains("headers")) {
        QJsonArray headers = headerObj["headers"].toArray();
        QStringList headerLabels;
        for(const auto& h : headers) headerLabels << h.toString();
        m_dataModel->setHorizontalHeaderLabels(headerLabels);

        for(const QString& h : headerLabels) {
            ColumnDefinition def;
            def.name = h;
            m_columnDefinitions.append(def);
        }
    }

    for(int i=1; i<array.size(); ++i) {
        QJsonObject rowObj = array[i].toObject();
        if (rowObj.contains("row_data")) {
            QJsonArray rowArr = rowObj["row_data"].toArray();
            QList<QStandardItem*> items;
            for(const auto& val : rowArr) {
                items.append(new QStandardItem(val.toString()));
            }
            m_dataModel->appendRow(items);
        }
    }
}

// ============================================================================
// 功能模块
// ============================================================================

void DataEditorWidget::onDefineColumns()
{
    QStringList currentHeaders;
    for(int i=0; i<m_dataModel->columnCount(); ++i)
        currentHeaders << m_dataModel->headerData(i, Qt::Horizontal).toString();

    DataColumnDialog dlg(currentHeaders, m_columnDefinitions, this);
    if (dlg.exec() == QDialog::Accepted) {
        m_columnDefinitions = dlg.getColumnDefinitions();
        for(int i=0; i<m_columnDefinitions.size(); ++i) {
            if (i < m_dataModel->columnCount()) {
                m_dataModel->setHeaderData(i, Qt::Horizontal, m_columnDefinitions[i].name);
            }
        }
        emit dataChanged();
    }
}

void DataEditorWidget::onTimeConvert()
{
    DataCalculate calculator;
    QStringList headers;
    for(int i=0; i<m_dataModel->columnCount(); ++i)
        headers << m_dataModel->headerData(i, Qt::Horizontal).toString();

    TimeConversionDialog dlg(headers, this);
    dlg.setStyleSheet("background-color: white; color: black;");

    if (dlg.exec() == QDialog::Accepted) {
        TimeConversionConfig config = dlg.getConversionConfig();
        TimeConversionResult res = calculator.convertTimeColumn(m_dataModel, m_columnDefinitions, config);

        if (res.success) QMessageBox::information(this, "成功", "时间转换完成");
        else QMessageBox::warning(this, "失败", res.errorMessage);
    }
}

void DataEditorWidget::onPressureDropCalc()
{
    DataCalculate calculator;
    PressureDropResult res = calculator.calculatePressureDrop(m_dataModel, m_columnDefinitions);

    if (res.success) QMessageBox::information(this, "成功", "压降计算完成");
    else QMessageBox::warning(this, "失败", res.errorMessage);
}

// ============================================================================
// 右键菜单与编辑
// ============================================================================

void DataEditorWidget::onSearchTextChanged()
{
    m_searchTimer->start();
}

void DataEditorWidget::onCustomContextMenu(const QPoint& pos)
{
    QMenu menu(this);
    menu.setStyleSheet("QMenu { background-color: white; color: black; border: 1px solid #ccc; }"
                       "QMenu::item { padding: 5px 20px; }"
                       "QMenu::item:selected { background-color: #e0e0e0; color: black; }");

    menu.addAction("在下方插入行", [=](){ onAddRow(2); });
    menu.addAction("在上方插入行", [=](){ onAddRow(1); });
    menu.addAction("删除选中行", this, &DataEditorWidget::onDeleteRow);

    menu.addSeparator();

    QAction* actAddColRight = menu.addAction("在右侧插入列", [=](){ onAddCol(2); });
    QAction* actAddColLeft = menu.addAction("在左侧插入列", [=](){ onAddCol(1); });
    menu.addAction("删除选中列", this, &DataEditorWidget::onDeleteCol);

    menu.exec(ui->dataTableView->mapToGlobal(pos));
}

void DataEditorWidget::onAddRow(int insertMode)
{
    int row = m_dataModel->rowCount();
    QModelIndex currIdx = ui->dataTableView->currentIndex();

    if (insertMode == 1 || insertMode == 2) {
        if (currIdx.isValid()) {
            int sourceRow = m_proxyModel->mapToSource(currIdx).row();
            row = (insertMode == 1) ? sourceRow : sourceRow + 1;
        }
    } else {
        if (currIdx.isValid()) {
            row = m_proxyModel->mapToSource(currIdx).row() + 1;
        }
    }

    int colCount = m_dataModel->columnCount() > 0 ? m_dataModel->columnCount() : 1;
    QList<QStandardItem*> items;
    for(int i=0; i<colCount; ++i) items << new QStandardItem("");

    m_dataModel->insertRow(row, items);
    updateButtonsState();
}

void DataEditorWidget::onDeleteRow()
{
    QModelIndexList idxs = ui->dataTableView->selectionModel()->selectedRows();
    if (idxs.isEmpty()) {
        QModelIndexList cellIdxs = ui->dataTableView->selectionModel()->selectedIndexes();
        QSet<int> rowSet;
        for(auto idx : cellIdxs) rowSet.insert(idx.row());
        for(int r : rowSet) idxs.append(m_proxyModel->index(r, 0));
    }

    if (idxs.isEmpty()) return;

    QList<int> sourceRows;
    for(auto proxyIdx : idxs) {
        sourceRows << m_proxyModel->mapToSource(proxyIdx).row();
    }

    std::sort(sourceRows.begin(), sourceRows.end());
    auto last = std::unique(sourceRows.begin(), sourceRows.end());
    sourceRows.erase(last, sourceRows.end());

    std::sort(sourceRows.begin(), sourceRows.end(), std::greater<int>());

    for(int r : sourceRows) {
        m_dataModel->removeRow(r);
    }
    updateButtonsState();
}

void DataEditorWidget::onAddCol(int insertMode)
{
    int col = m_dataModel->columnCount();
    QModelIndex currIdx = ui->dataTableView->currentIndex();

    if (insertMode == 1 || insertMode == 2) {
        if (currIdx.isValid()) {
            int sourceCol = m_proxyModel->mapToSource(currIdx).column();
            col = (insertMode == 1) ? sourceCol : sourceCol + 1;
        }
    }

    m_dataModel->insertColumn(col);

    ColumnDefinition def;
    def.name = "新列";
    if (col < m_columnDefinitions.size()) {
        m_columnDefinitions.insert(col, def);
    } else {
        m_columnDefinitions.append(def);
    }

    m_dataModel->setHeaderData(col, Qt::Horizontal, "新列");
}

void DataEditorWidget::onDeleteCol()
{
    QModelIndexList idxs = ui->dataTableView->selectionModel()->selectedColumns();
    if (idxs.isEmpty()) {
        QModelIndexList cellIdxs = ui->dataTableView->selectionModel()->selectedIndexes();
        QSet<int> colSet;
        for(auto idx : cellIdxs) colSet.insert(idx.column());
        for(int c : colSet) idxs.append(m_proxyModel->index(0, c));
    }

    if (idxs.isEmpty()) return;

    QList<int> sourceCols;
    for(auto proxyIdx : idxs) {
        sourceCols << m_proxyModel->mapToSource(proxyIdx).column();
    }

    std::sort(sourceCols.begin(), sourceCols.end());
    auto last = std::unique(sourceCols.begin(), sourceCols.end());
    sourceCols.erase(last, sourceCols.end());

    std::sort(sourceCols.begin(), sourceCols.end(), std::greater<int>());

    for(int c : sourceCols) {
        m_dataModel->removeColumn(c);
        if(c < m_columnDefinitions.size()) {
            m_columnDefinitions.removeAt(c);
        }
    }
    updateButtonsState();
}

void DataEditorWidget::onModelDataChanged()
{
}


// 清空所有数据
void DataEditorWidget::clearAllData()
{
    // 清空数据模型
    if (m_dataModel) {
        m_dataModel->clear();
    }
    m_columnDefinitions.clear();

    // 清空路径记录
    m_currentFilePath.clear();

    // 重置UI显示
    ui->filePathLabel->setText("当前文件: ");
    ui->statusLabel->setText("无数据");

    // 更新按钮状态（禁用保存等按钮）
    updateButtonsState();

    // 发送数据变更信号，通知其他模块
    emit dataChanged();
}
