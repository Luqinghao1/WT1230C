#ifndef FITTINGPARAMETERCHART_H
#define FITTINGPARAMETERCHART_H

#include <QObject>
#include <QTableWidget>
#include <QList>
#include <QMap>
#include "modelmanager.h"

// 定义拟合参数结构体
struct FitParameter {
    QString name;           // 参数内部英文名 (例如 "k", "S")
    QString displayName;    // 参数显示中文名 (例如 "渗透率")
    double value;           // 当前参数值
    bool isFit;             // 是否参与拟合 (true: 变量, false: 定值)
    double min;             // 参数下限
    double max;             // 参数上限
    bool isVisible;         // 是否在主界面表格中显示
};

// 拟合参数图表管理类
class FittingParameterChart : public QObject
{
    Q_OBJECT
public:
    // 构造函数：初始化表格样式和列
    explicit FittingParameterChart(QTableWidget* parentTable, QObject *parent = nullptr);

    // 设置模型管理器
    void setModelManager(ModelManager* m);

    // 重置参数为默认值
    void resetParams(ModelManager::ModelType type);

    // 获取/设置参数列表
    QList<FitParameter> getParameters() const;
    void setParameters(const QList<FitParameter>& params);

    // 切换模型（保留公有参数值）
    void switchModel(ModelManager::ModelType newType);

    // 从表格同步数据到内存
    void updateParamsFromTable();

    // 刷新表格显示（核心修改：排序、颜色、格式）
    void refreshParamTable();

    // 静态辅助函数：获取规范的参数显示信息
    static void getParamDisplayInfo(const QString& name, QString& chName, QString& symbol, QString& uniSymbol, QString& unit);

private:
    QTableWidget* m_table;
    ModelManager* m_modelManager;
    QList<FitParameter> m_params;

    // 辅助函数：添加单行数据
    void addRowToTable(const FitParameter& p, int& serialNo, bool highlight);
};

#endif // FITTINGPARAMETERCHART_H
