/*
 * main.cpp
 * 文件作用：应用程序入口
 * 功能描述：
 * 1. 初始化 QApplication 应用程序对象
 * 2. 设置应用程序的全局窗口图标 (PWT.png)
 * 3. 应用全局样式表 (StyleSheet) 以美化界面控件
 * 4. 设置全局调色板以适配不同系统主题的文本颜色
 * 5. 启动主窗口
 */

#include "mainwindow.h"
#include <QApplication>
#include <QStyleFactory>
#include <QMessageBox>
#include <QFileDialog>
#include <QIcon>

int main(int argc, char *argv[])
{
// [修复] 解决 HighDpiScaling 在 Qt6 中已废弃的警告
// 只有在 Qt 6.0 之前的版本才需要手动启用，Qt 6 默认启用
#if (QT_VERSION < QT_VERSION_CHECK(6, 0, 0))
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
#endif

    QApplication app(argc, argv);

    // 设置软件全局图标
    app.setWindowIcon(QIcon(":/new/prefix1/Resource/PWT.png"));

    // 设置全局样式，确保所有对话框和消息框的文本都显示为黑色
    QString styleSheet = R"(
        /* 全局黑色文字样式 */
        QLabel, QLineEdit, QComboBox, QPushButton, QToolButton,
        QTreeView, QHeaderView, QTableView, QTabBar, QRadioButton,
        QCheckBox, QGroupBox, QMenu, QMenuBar, QStatusBar,
        QListView, QListWidget, QTextEdit, QPlainTextEdit {
            color: black;
        }

        /* 消息框样式 */
        QMessageBox QLabel {
            color: black;
        }

        /* 文件对话框样式 */
        QFileDialog QLabel, QFileDialog QTreeView, QFileDialog QComboBox {
            color: black;
        }

        /* 数据编辑器样式 */
        DataEditorWidget, DataEditorWidget * {
            color: black;
        }

        QTableView {
            alternate-background-color: #f0f0f0;
            background-color: white;
            gridline-color: #d0d0d0;
        }

        QTableView::item {
            color: black;
        }

        QHeaderView::section {
            background-color: #e0e0e0;
            color: black;
            padding: 4px;
            border: 1px solid #c0c0c0;
        }

        QPushButton {
            background-color: #e0e0e0;
            border: 1px solid #c0c0c0;
            padding: 5px 15px;
            min-width: 80px;
        }

        QPushButton:hover {
            background-color: #d0d0d0;
        }

        QPushButton:pressed {
            background-color: #c0c0c0;
        }
    )";

    app.setStyleSheet(styleSheet);

    // 设置所有已存在的对话框类的调色板
    QPalette darkTextPalette;
    darkTextPalette.setColor(QPalette::WindowText, Qt::black);
    darkTextPalette.setColor(QPalette::Text, Qt::black);
    darkTextPalette.setColor(QPalette::ButtonText, Qt::black);

    QApplication::setPalette(darkTextPalette);

    MainWindow w;
    w.show();

    return app.exec();
}
