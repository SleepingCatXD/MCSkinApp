#pragma once

#include <QtWidgets/QMainWindow>
#include "ui_MCSkinApp.h"
#include "GalleryWindow.h"
#include "ImageCropperLabel.h"
#include <QDir>
#include <QFileInfoList>
#include <QImage>
#include <QResizeEvent>
#include <QListWidgetItem>

class MCSkinApp : public QMainWindow
{
    Q_OBJECT

public:
    MCSkinApp(QWidget* parent = nullptr);
    ~MCSkinApp();

protected:
    void resizeEvent(QResizeEvent* event) override;
    // 新增：用于捕获鼠标点击 QLabel 的事件
    bool eventFilter(QObject* watched, QEvent* event) override;

private slots:
    // --- 新增：项目控制 ---
    void handleNewProject();
    void handleUnlockProject();
    void handleProjectFileClicked(QListWidgetItem* item); // 点击左侧列表
    void handleBtnSetPrompt();
    void handleOpenProject();

    // Renamed to prevent Qt's auto-connection from firing twice
    void handleSelectWorkspace();

    void handleBtnChar();
    void handleBtnStyle();
    void handleBtnPredView();
    void handleBtnPredSkin();
    void handleBtnGtSkin();
    void handleBtnGtView();

    void handleBtnFirst();
    void handleBtnPrev();
    void handleBtnNext();

    void handleBtnCropAvatar(); // 截取头像按钮的点击事件

    void handleBtnBackToStaging(); // 【新增】
    void handleBtnRemoveFile();    // 【新增】

    // 新增：专门处理从画廊传回来的 ID
    void openProjectFromGallery(const QString& projectId);

    void saveQuickTags(); // 统一处理快捷打标的保存

private:
    Ui::MCSkinAppClass ui;

    QDir workspaceDir;
    QDir stagingDir;   // New: Isolated folder for unprocessed images
    QDir datasetDir;   // New: Dedicated folder for the final dataset
    QFileInfoList m_fileList;

    int m_currentIndex;
    QImage m_currentImage;
    bool m_isProjectLocked;            // 1. 是否已锁定一个项目
    bool m_isPreviewingProjectFile;    // 2. 是否正在查看左侧列表里的项目文件
    bool m_isCroppingState;            // 3. 是否正处于拖拽截图的蒙版模式

    QString m_currentWorkingId; // 当前锁定的项目 ID
    QString m_currentPreviewFileName;  // 记录正在查看的项目文件名
    QString m_pendingPromptText;


    void loadSettings();
    void setWorkspace(const QString& path);
    void showCurrentFile();
    void updateImageDisplay();
    void processClassification(const QString& category);

    // --- 新增：UI 状态刷新 ---
    void updateUiState(); // 根据锁定状态开启/禁用按钮
    void refreshProjectList(); // 刷新左侧栏的文件列表
};
