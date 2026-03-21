#pragma once

#include <QWidget>
#include <QString>
#include "ui_AnnotationWindow.h"

class AnnotationWindow : public QWidget
{
    Q_OBJECT

public:
    // 修改构造函数：强制要求传入项目 ID 和工作区路径
    explicit AnnotationWindow(const QString& projectId, const QString& workspacePath, QWidget* parent = nullptr);
    ~AnnotationWindow();

private slots:
    // 声明我们在 cpp 里写的保存按钮槽函数
    void onSaveCloseClicked();

private:
    Ui::AnnotationWindow ui;

    // 保存从主界面传过来的核心状态变量
    QString m_projectId;
    QString m_workspacePath;

    // 内部数据加载逻辑
    void loadData();
};
