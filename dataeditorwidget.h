/*
 * 文件名: dataeditorwidget.h
 * 文件作用: 数据编辑器主窗口头文件
 * 功能描述:
 * 1. 定义数据编辑器的主界面类 DataEditorWidget。
 * 2. 声明表格数据模型、代理模型和撤销栈，用于管理数据的显示和编辑。
 * 3. 声明文件加载、保存、列定义、数据计算等核心功能的槽函数。
 * 4. 声明与 Excel 读取及数据导入配置相关的辅助函数。
 */

#ifndef DATAEDITORWIDGET_H
#define DATAEDITORWIDGET_H

#include <QWidget>
#include <QStandardItemModel>
#include <QSortFilterProxyModel>
#include <QUndoStack>
#include <QMenu>
#include <QJsonArray>
#include <QStyledItemDelegate>
#include <QTimer>
#include "dataimportdialog.h" // 引用导入配置对话框头文件

// 定义列的枚举类型，表示每一列数据的物理含义
enum class WellTestColumnType {
    SerialNumber, Date, Time, TimeOfDay, Pressure, Temperature, FlowRate,
    Depth, Viscosity, Density, Permeability, Porosity, WellRadius,
    SkinFactor, Distance, Volume, PressureDrop, Custom
};

// 定义列属性结构体，包含名称、类型、单位等信息
struct ColumnDefinition {
    QString name;
    WellTestColumnType type;
    QString unit;
    bool isRequired;
    int decimalPlaces;

    ColumnDefinition() : type(WellTestColumnType::Custom), isRequired(false), decimalPlaces(3) {}
};

namespace Ui {
class DataEditorWidget;
}

// ----------------------------------------------------------------------------
// 自定义委托类：NoContextMenuDelegate
// 作用：用于接管表格单元格的编辑控件创建，屏蔽默认的右键菜单，防止干扰自定义交互。
// ----------------------------------------------------------------------------
class NoContextMenuDelegate : public QStyledItemDelegate
{
    Q_OBJECT
public:
    explicit NoContextMenuDelegate(QObject *parent = nullptr) : QStyledItemDelegate(parent) {}

    // 重写创建编辑器的方法
    QWidget *createEditor(QWidget *parent, const QStyleOptionViewItem &option,
                          const QModelIndex &index) const override;
};

// ----------------------------------------------------------------------------
// 主类：DataEditorWidget
// 作用：实现数据的表格化展示、编辑、文件导入导出及相关计算功能的入口。
// ----------------------------------------------------------------------------
class DataEditorWidget : public QWidget
{
    Q_OBJECT

public:
    explicit DataEditorWidget(QWidget *parent = nullptr);
    ~DataEditorWidget();

    // 清空所有数据和状态
    void clearAllData();

    // 从项目参数中加载保存的数据（用于打开项目时恢复状态）
    void loadFromProjectData();

    // 获取当前的数据模型指针
    QStandardItemModel* getDataModel() const;

    // 加载指定路径的数据文件，支持自动识别类型
    void loadData(const QString& filePath, const QString& fileType = "auto");

    // 获取当前已加载文件的路径
    QString getCurrentFileName() const;

    // 判断当前表格中是否有数据
    bool hasData() const;

    // 获取当前的列定义列表
    QList<ColumnDefinition> getColumnDefinitions() const { return m_columnDefinitions; }

signals:
    // 数据发生变更时发送的信号
    void dataChanged();
    // 文件成功加载后发送的信号
    void fileChanged(const QString& filePath, const QString& fileType);

private slots:
    // 打开文件按钮点击槽函数
    void onOpenFile();
    // 保存按钮点击槽函数
    void onSave();
    // 定义列属性按钮点击槽函数
    void onDefineColumns();
    // 时间格式转换按钮点击槽函数
    void onTimeConvert();
    // 压降计算按钮点击槽函数
    void onPressureDropCalc();

    // 搜索框文本变化时的槽函数（带防抖）
    void onSearchTextChanged();

    // 表格右键菜单请求槽函数
    void onCustomContextMenu(const QPoint& pos);

    // 添加行（insertMode: 0=末尾, 1=上方, 2=下方）
    void onAddRow(int insertMode = 0);
    // 删除选中行
    void onDeleteRow();
    // 添加列（insertMode: 0=末尾, 1=左侧, 2=右侧）
    void onAddCol(int insertMode = 0);
    // 删除选中列
    void onDeleteCol();

    // 模型数据变化时的通用处理槽
    void onModelDataChanged();

private:
    Ui::DataEditorWidget *ui;

    QStandardItemModel* m_dataModel;       // 标准数据模型，存储实际数据
    QSortFilterProxyModel* m_proxyModel;   // 代理模型，用于排序和过滤
    QUndoStack* m_undoStack;               // 撤销栈（预留）

    QList<ColumnDefinition> m_columnDefinitions; // 列属性定义列表
    QString m_currentFilePath;             // 当前文件路径
    QMenu* m_contextMenu;                  // 右键菜单
    QTimer* m_searchTimer;                 // 搜索防抖定时器

    // 初始化界面控件
    void initUI();
    // 建立信号槽连接
    void setupConnections();
    // 初始化数据模型配置
    void setupModel();
    // 根据是否有数据更新按钮的启用状态
    void updateButtonsState();

    // 内部文件加载流程
    bool loadFileInternal(const QString& path);
    // 根据配置项读取文件（支持文本和Excel）
    bool loadFileWithConfig(const DataImportSettings& settings);

    // 将当前表格数据序列化为 JSON 数组
    QJsonArray serializeModelToJson() const;
    // 将 JSON 数组反序列化回表格模型
    void deserializeJsonToModel(const QJsonArray& array);
};

#endif // DATAEDITORWIDGET_H
