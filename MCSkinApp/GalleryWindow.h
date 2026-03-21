#pragma once

#include <QWidget>
#include "ui_GalleryWindow.h"
#include <QListWidgetItem>

class GalleryWindow : public QWidget
{
	Q_OBJECT

public:
	// 构造函数需要传入 dataset 的路径，以便画廊知道去哪找图片
	explicit GalleryWindow(const QString& datasetPath, QWidget* parent = nullptr);
	~GalleryWindow();

signals:
	// 自定义信号：当用户选好项目时，发射这个信号通知主界面
	void projectSelected(const QString& projectId);

private slots:
	void on_btnOpen_clicked();
	void on_btnCancel_clicked();
	void on_listGallery_itemDoubleClicked(QListWidgetItem* item);
	void on_btnDelete_clicked();
	void on_btnSearch_clicked();

private:
	Ui::GalleryWindow ui;
	QString m_datasetPath;

	void loadProjects(); // 加载并渲染所有头像
};
