#include "GalleryWindow.h"
#include "ui_GalleryWindow.h"
#include "DatasetManager.h"
#include <QMessageBox>
#include <QDir>
#include <QFile>
#include <QPixmap>
#include <QIcon>

GalleryWindow::GalleryWindow(const QString& datasetPath, QWidget* parent)
	: QWidget(parent), m_datasetPath(datasetPath)
{
	ui.setupUi(this);
	this->setWindowTitle("Dataset Gallery");
	this->resize(800, 600); // 给画廊一个宽敞的初始大小

	// 绑定内部按钮
	connect(ui.btnOpen, &QPushButton::clicked, this, &GalleryWindow::on_btnOpen_clicked);
	connect(ui.btnCancel, &QPushButton::clicked, this, &GalleryWindow::on_btnCancel_clicked);
	connect(ui.listGallery, &QListWidget::itemDoubleClicked, this, &GalleryWindow::on_listGallery_itemDoubleClicked);

	// 启动时加载项目
	loadProjects();

}

GalleryWindow::~GalleryWindow()
{
}

void GalleryWindow::loadProjects()
{
	ui.listGallery->clear();
	QDir dir(m_datasetPath);
	if (!dir.exists()) return;

    // 获取所有的 ID 文件夹
    QStringList projDirs = dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);

    for (const QString& id : projDirs) {
        QListWidgetItem* item = new QListWidgetItem(ui.listGallery);
        item->setText("ID: " + id);

        // 查找头像 thumbnail.png
        QString avatarPath = dir.filePath(id + "/thumbnail.png");
        if (QFile::exists(avatarPath)) {
            QPixmap pix(avatarPath);
            item->setIcon(QIcon(pix));
        }
        else {
            // 没有头像的用纯色方块占位
            QPixmap emptyPix(128, 128);
            emptyPix.fill(Qt::darkGray);
            item->setIcon(QIcon(emptyPix));
        }

        // 把真实的 ID 存在 data 里，方便提取
        item->setData(Qt::UserRole, id);
    }
}

void GalleryWindow::on_btnOpen_clicked()
{
    QListWidgetItem* item = ui.listGallery->currentItem();
    if (item) {
        QString id = item->data(Qt::UserRole).toString();
        emit projectSelected(id); // 发送信号给主窗口
        this->close();            // 关闭画廊
    }
}

void GalleryWindow::on_listGallery_itemDoubleClicked(QListWidgetItem* item)
{
    if (item) {
        QString id = item->data(Qt::UserRole).toString();
        emit projectSelected(id);
        this->close();
    }
}

void GalleryWindow::on_btnCancel_clicked()
{
    this->close();
}

void GalleryWindow::on_btnDelete_clicked()
{
    QListWidgetItem* item = ui.listGallery->currentItem();
    if (!item) {
        QMessageBox::information(this, "Hint", "Please select a project to delete first.");
        return;
    }

    QString id = item->data(Qt::UserRole).toString();

    // 1. 弹出极其严重的二次确认警告！
    QMessageBox::StandardButton reply;
    reply = QMessageBox::warning(this, "DANGER: Delete Project",
        "Are you sure you want to completely delete Project ID: " + id + "?\n\nThis will erase all its images, metadata, and the thumbnail. This cannot be undone!",
        QMessageBox::Yes | QMessageBox::No);

    if (reply == QMessageBox::Yes) {
        // 2. 物理删除文件夹及内部所有内容
        // 这里假设你在 DatasetCore 里提供了一个外部可以访问 datasetDir 的路径
        QDir projDir(m_datasetPath + "/" + id);
        if (projDir.exists()) {
            projDir.removeRecursively(); // Qt 提供的一键抹除文件夹方法
        }

        // 3. 从 metadata.jsonl 中删除该 ID
        // (你需要去 DatasetManager.cpp 里写一个 removeProject(workspacePath, id) 的静态方法)
        // DatasetManager::removeProject(m_workspacePath, id);

        // 4. 从 UI 画廊中移走这个图标
        delete ui.listGallery->takeItem(ui.listGallery->row(item));

        QMessageBox::information(this, "Deleted", "Project " + id + " has been deleted.");
    }
}

// 搜索功能的框架 (暂不实现底层，只做 UI 响应)
void GalleryWindow::on_btnSearch_clicked()
{
    QString keyword = ui.editSearch->text();
    QString filter = ui.comboFilter->currentText();

    QMessageBox::information(this, "Search WIP",
        "Search functionality is under construction.\nKeyword: " + keyword + "\nFilter: " + filter);

    // 未来的逻辑：重新调用 DatasetManager 读取 JSONL，
    // 匹配关键字，然后清空 ui.listGallery 并重新填充符合条件的项目。
}
