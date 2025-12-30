#ifndef MODELMANAGER_H
#define MODELMANAGER_H

#include <QObject>
#include <QMap>
#include <QVector>
#include <QStackedWidget>
#include <QPushButton>

#include "modelwidget01-06.h"

class ModelManager : public QObject
{
    Q_OBJECT

public:
    using ModelType = ModelWidget01_06::ModelType;
    static const ModelType Model_1 = ModelWidget01_06::Model_1;
    static const ModelType Model_2 = ModelWidget01_06::Model_2;
    static const ModelType Model_3 = ModelWidget01_06::Model_3;
    static const ModelType Model_4 = ModelWidget01_06::Model_4;
    static const ModelType Model_5 = ModelWidget01_06::Model_5;
    static const ModelType Model_6 = ModelWidget01_06::Model_6;

    explicit ModelManager(QWidget* parent = nullptr);
    ~ModelManager();

    void initializeModels(QWidget* parentWidget);
    void switchToModel(ModelType modelType);
    static QString getModelTypeName(ModelType type);
    ModelCurveData calculateTheoreticalCurve(ModelType type, const QMap<QString, double>& params, const QVector<double>& providedTime = QVector<double>());
    QMap<QString, double> getDefaultParameters(ModelType type);
    void setHighPrecision(bool high);
    void updateAllModelsBasicParameters();
    static QVector<double> generateLogTimeSteps(int count, double startExp, double endExp);

    void setObservedData(const QVector<double>& t, const QVector<double>& p, const QVector<double>& d);
    void getObservedData(QVector<double>& t, QVector<double>& p, QVector<double>& d) const;
    bool hasObservedData() const;
    void clearCache();

signals:
    void modelSwitched(ModelType newType, ModelType oldType);
    void calculationCompleted(const QString& analysisType, const QMap<QString, double>& results);

private slots:
    void onSelectModelClicked();
    void onWidgetCalculationCompleted(const QString& t, const QMap<QString, double>& r);

private:
    void createMainWidget();
    void connectModelSignals();

private:
    QWidget* m_mainWidget;
    QStackedWidget* m_modelStack;
    QVector<ModelWidget01_06*> m_modelWidgets;
    ModelType m_currentModelType;

    QVector<double> m_cachedObsTime;
    QVector<double> m_cachedObsPressure;
    QVector<double> m_cachedObsDerivative;
};

#endif // MODELMANAGER_H
