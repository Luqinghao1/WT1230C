#include "fittingparameterchart.h"
#include <QHeaderView>
#include <QTableWidgetItem>
#include <QDebug>
#include <QBrush>
#include <QColor>

FittingParameterChart::FittingParameterChart(QTableWidget *parentTable, QObject *parent)
    : QObject(parent), m_table(parentTable), m_modelManager(nullptr)
{
    if(m_table) {
        // [修改] 1. 增加序号列
        QStringList headers;
        headers << "序号" << "参数名称" << "数值" << "单位";
        m_table->setColumnCount(headers.size());
        m_table->setHorizontalHeaderLabels(headers);

        // [修改] 2. 设置表头样式：灰色背景，黑字
        m_table->horizontalHeader()->setStyleSheet(
            "QHeaderView::section { background-color: #E0E0E0; color: black; font-weight: bold; border: 1px solid #A0A0A0; }"
            );

        // [修改] 3. 列宽设置为 Interactive (用户可拖拽调整)，并设置初始宽度以填满左侧区域
        m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
        m_table->horizontalHeader()->setStretchLastSection(true); // 最后一列填充剩余空间，保证铺满

        // 初始列宽分配 (假设左侧宽度约350px)
        m_table->setColumnWidth(0, 40);  // 序号
        m_table->setColumnWidth(1, 160); // 参数名称 (较宽)
        m_table->setColumnWidth(2, 80);  // 数值
        // 单位列自动填充剩余

        m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
        m_table->setAlternatingRowColors(false); // 关闭自动交替，手动控制颜色
        m_table->verticalHeader()->setVisible(false); // 隐藏行号
    }
}

void FittingParameterChart::setModelManager(ModelManager *m)
{
    m_modelManager = m;
}

void FittingParameterChart::resetParams(ModelManager::ModelType type)
{
    if(!m_modelManager) return;
    m_params.clear();

    QMap<QString, double> defaultMap = m_modelManager->getDefaultParameters(type);
    QMapIterator<QString, double> it(defaultMap);
    while(it.hasNext()) {
        it.next();
        FitParameter p;
        p.name = it.key();
        p.value = it.value();
        p.isFit = false; // 默认不拟合

        if (p.value > 0) {
            p.min = p.value * 0.01; p.max = p.value * 100.0;
        } else {
            p.min = 0.0; p.max = 100.0;
        }

        QString symbol, uniSym, unit;
        getParamDisplayInfo(p.name, p.displayName, symbol, uniSym, unit);
        p.isVisible = true; // 默认显示
        m_params.append(p);
    }
    refreshParamTable();
}

QList<FitParameter> FittingParameterChart::getParameters() const
{
    return m_params;
}

void FittingParameterChart::setParameters(const QList<FitParameter> &params)
{
    m_params = params;
    refreshParamTable();
}

void FittingParameterChart::switchModel(ModelManager::ModelType newType)
{
    QMap<QString, double> oldValues;
    for(const auto& p : m_params) oldValues.insert(p.name, p.value);

    resetParams(newType);

    for(auto& p : m_params) {
        if(oldValues.contains(p.name)) p.value = oldValues[p.name];
    }
    refreshParamTable();
}

void FittingParameterChart::updateParamsFromTable()
{
    if(!m_table) return;

    for(int i = 0; i < m_table->rowCount(); ++i) {
        // 参数名存储在第1列(参数名称)的 UserRole
        QTableWidgetItem* itemKey = m_table->item(i, 1);
        if(!itemKey) continue;

        QString key = itemKey->data(Qt::UserRole).toString();

        // 数值在第2列
        QTableWidgetItem* itemVal = m_table->item(i, 2);
        double val = itemVal->text().toDouble();

        for(auto& p : m_params) {
            if(p.name == key) {
                p.value = val;
                break;
            }
        }
    }
}

void FittingParameterChart::refreshParamTable()
{
    if(!m_table) return;

    m_table->blockSignals(true);
    m_table->setRowCount(0);

    int serialNo = 1;

    // [修改] 排序逻辑：
    // 第一遍：添加 可见 且 拟合 (高亮) 的参数
    for(const auto& p : m_params) {
        if(p.isVisible && p.isFit) {
            addRowToTable(p, serialNo, true);
        }
    }

    // 第二遍：添加 可见 且 不拟合 (普通) 的参数
    for(const auto& p : m_params) {
        if(p.isVisible && !p.isFit) {
            addRowToTable(p, serialNo, false);
        }
    }

    m_table->blockSignals(false);
}

void FittingParameterChart::addRowToTable(const FitParameter& p, int& serialNo, bool highlight)
{
    int row = m_table->rowCount();
    m_table->insertRow(row);

    // 设置背景色：高亮为淡黄色，普通为白色
    QColor bgColor = highlight ? QColor(255, 255, 224) : Qt::white;

    // 0. 序号列
    QTableWidgetItem* numItem = new QTableWidgetItem(QString::number(serialNo++));
    numItem->setFlags(numItem->flags() & ~Qt::ItemIsEditable);
    numItem->setTextAlignment(Qt::AlignCenter);
    numItem->setBackground(bgColor);
    m_table->setItem(row, 0, numItem);

    // 1. 参数名称列：中文名 (英文名)
    // [修改] 严格执行格式要求
    QString displayNameFull = QString("%1 (%2)").arg(p.displayName).arg(p.name);

    QTableWidgetItem* nameItem = new QTableWidgetItem(displayNameFull);
    nameItem->setFlags(nameItem->flags() & ~Qt::ItemIsEditable);
    nameItem->setData(Qt::UserRole, p.name); // 存储英文 Key
    nameItem->setBackground(bgColor);
    if(highlight) {
        QFont f = nameItem->font(); f.setBold(true); nameItem->setFont(f);
    }
    m_table->setItem(row, 1, nameItem);

    // 2. 数值列
    QTableWidgetItem* valItem = new QTableWidgetItem(QString::number(p.value, 'g', 6));
    valItem->setBackground(bgColor);
    if(highlight) {
        QFont f = valItem->font(); f.setBold(true); valItem->setFont(f);
    }
    m_table->setItem(row, 2, valItem);

    // 3. 单位列
    QString dummy, symbol, uniSym, unit;
    getParamDisplayInfo(p.name, dummy, symbol, uniSym, unit);
    if(unit == "无因次" || unit == "小数") unit = "-";

    QTableWidgetItem* unitItem = new QTableWidgetItem(unit);
    unitItem->setFlags(unitItem->flags() & ~Qt::ItemIsEditable);
    unitItem->setBackground(bgColor);
    m_table->setItem(row, 3, unitItem);
}

// [修改] 参数名称映射表：严格对应中文名 (英文名) 格式
void FittingParameterChart::getParamDisplayInfo(const QString &name, QString &chName, QString &symbol, QString &uniSym, QString &unit)
{
    // 参考 modelwidget01-06.cpp 的参数含义
    if(name == "k")      { chName = "渗透率";         unit = "mD"; }
    else if(name == "h")      { chName = "有效厚度";       unit = "m"; }
    else if(name == "phi")    { chName = "孔隙度";         unit = "小数"; }
    else if(name == "mu")     { chName = "流体粘度";       unit = "mPa·s"; }
    else if(name == "B")      { chName = "体积系数";       unit = "无因次"; }
    else if(name == "Ct")     { chName = "综合压缩系数";   unit = "MPa⁻¹"; }
    else if(name == "rw")     { chName = "井筒半径";       unit = "m"; }
    else if(name == "q")      { chName = "测试产量";       unit = "m³/d"; }

    else if(name == "C")      { chName = "井筒储存系数";   unit = "m³/MPa"; }
    else if(name == "cD")     { chName = "无因次井储";     unit = "无因次"; }
    else if(name == "S")      { chName = "表皮系数";       unit = "无因次"; }

    else if(name == "L")      { chName = "水平井长";       unit = "m"; }
    else if(name == "Lf")     { chName = "裂缝半长";       unit = "m"; }
    else if(name == "nf")     { chName = "裂缝条数";       unit = "条"; }
    else if(name == "kf")     { chName = "裂缝渗透率";     unit = "mD"; }
    else if(name == "km")     { chName = "基质渗透率";     unit = "mD"; }

    else if(name == "reD")    { chName = "无因次泄油半径"; unit = "无因次"; }
    else if(name == "lambda1"){ chName = "窜流系数";       unit = "无因次"; }
    else if(name == "omega1") { chName = "储容比1";        unit = "无因次"; }
    else if(name == "omega2") { chName = "储容比2";        unit = "无因次"; }
    else if(name == "gamaD")  { chName = "压敏系数";       unit = "无因次"; }
    else if(name == "rmD")    { chName = "无因次内半径";   unit = "无因次"; }
    else if(name == "LfD")    { chName = "无因次缝长";     unit = "无因次"; }
    else { chName = name; unit = ""; }

    symbol = name; uniSym = name; // 这里的符号留作备用
}
