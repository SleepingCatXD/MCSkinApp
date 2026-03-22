#include "MCSkinApp.h"
#include "ui_MCSkinApp.h"
#include "DatasetManager.h"
#include "AnnotationWindow.h"
#include "ImageCropperLabel.h"
#include <QInputDialog>
#include <QMessageBox>
#include <QStatusBar>
#include <QFileDialog>
#include <QSettings>
#include <QPixmap>
#include <QDebug>

MCSkinApp::MCSkinApp(QWidget* parent)
    : QMainWindow(parent)
    , m_currentIndex(-1)
{
    ui.setupUi(this);

    this->setWindowTitle("MC Skin Dataset Manager");

    ui.imagePreviewLabel->setAlignment(Qt::AlignCenter);
    ui.imagePreviewLabel->setMinimumSize(200, 200);
    ui.lblAvatar->installEventFilter(this); // 让主窗口监听头像的鼠标事件

    m_isProjectLocked = false;
    m_isPreviewingProjectFile = false;
    m_isCroppingState = false; // 初始肯定不是裁剪状态
    m_currentWorkingId = "";
    m_currentPreviewFileName = "";

    // Manual connections using the renamed slots (prevents double triggers)
    connect(ui.btnSelectWorkspace, &QPushButton::clicked, this, &MCSkinApp::handleSelectWorkspace);
    connect(ui.btnChar, &QPushButton::clicked, this, &MCSkinApp::handleBtnChar);
    connect(ui.btnStyle, &QPushButton::clicked, this, &MCSkinApp::handleBtnStyle);
    connect(ui.btnPredView, &QPushButton::clicked, this, &MCSkinApp::handleBtnPredView);
    connect(ui.btnPredSkin, &QPushButton::clicked, this, &MCSkinApp::handleBtnPredSkin);
    connect(ui.btnGtSkin, &QPushButton::clicked, this, &MCSkinApp::handleBtnGtSkin);
    connect(ui.btnGtView, &QPushButton::clicked, this, &MCSkinApp::handleBtnGtView);
    connect(ui.btnFirst, &QPushButton::clicked, this, &MCSkinApp::handleBtnFirst);
    connect(ui.btnPrev, &QPushButton::clicked, this, &MCSkinApp::handleBtnPrev);
    connect(ui.btnNext, &QPushButton::clicked, this, &MCSkinApp::handleBtnNext);
    connect(ui.btnOpenProject, &QPushButton::clicked, this, &MCSkinApp::handleOpenProject);
    connect(ui.btnNewProject, &QPushButton::clicked, this, &MCSkinApp::handleNewProject);
    connect(ui.btnUnlockProject, &QPushButton::clicked, this, &MCSkinApp::handleUnlockProject);
    connect(ui.listProjectFiles, &QListWidget::itemClicked, this, &MCSkinApp::handleProjectFileClicked);
    connect(ui.btnSetPrompt, &QPushButton::clicked, this, &MCSkinApp::handleBtnSetPrompt); // 【新增】
    connect(ui.btnCropAvatar, &QPushButton::clicked, this, &MCSkinApp::handleBtnCropAvatar);
    connect(ui.btnBackToStaging, &QPushButton::clicked, this, &MCSkinApp::handleBtnBackToStaging);
    connect(ui.btnRemoveFile, &QPushButton::clicked, this, &MCSkinApp::handleBtnRemoveFile);

    // 绑定菜单栏的 View -> Gallery Database 动作
    connect(ui.actionGallery_Database, &QAction::triggered, this, &MCSkinApp::handleOpenProject);
    connect(ui.actionDeep_Annotation, &QAction::triggered, this, [=]() {
        if (!m_isProjectLocked) {
            // 提示用户先锁定或选中一个项目
            return;
        }
        // 呼出 AnnotationWindow，并把当前的 m_currentWorkingId 传给它
        AnnotationWindow* annoWin = new AnnotationWindow(m_currentWorkingId, datasetDir.path());
        annoWin->setAttribute(Qt::WA_DeleteOnClose);
        annoWin->setWindowModality(Qt::ApplicationModal); // 模态窗口，阻塞主界面
        annoWin->show();
        });

    // 任何状态改变，立刻触发保存
    connect(ui.chkMain_IsAI, &QCheckBox::clicked, this, &MCSkinApp::saveQuickTags);
    connect(ui.chkMain_IsCommercial, &QCheckBox::clicked, this, &MCSkinApp::saveQuickTags);
    // 文本框失去焦点或按下回车时触发保存
    connect(ui.editMain_Author, &QLineEdit::editingFinished, this, &MCSkinApp::saveQuickTags);
    connect(ui.editMain_SourceUrl, &QLineEdit::editingFinished, this, &MCSkinApp::saveQuickTags);

    // 初始状态刷新
    updateUiState();

    // Override UI labels to English to clear any existing garbled text
    ui.lblWorkspacePath->setText("Workspace: Not Selected");
    ui.fileNameLabel->setText("Filename");
    ui.fileInfoLabel->setText("Size Info");
    ui.imagePreviewLabel->setText("Please select a workspace.");

    loadSettings();
}

MCSkinApp::~MCSkinApp()
{
}

void MCSkinApp::loadSettings()
{
    QSettings settings("MyStudio", "MCSkinManager");
    QString lastPath = settings.value("workspacePath", "").toString();

    if (!lastPath.isEmpty()) {
        setWorkspace(lastPath);
    }
}

void MCSkinApp::handleSelectWorkspace()
{
    QString dir = QFileDialog::getExistingDirectory(this, "Select Workspace Folder", workspaceDir.path());
    if (!dir.isEmpty()) {
        QSettings settings("MyStudio", "MCSkinManager");
        settings.setValue("workspacePath", dir);
        setWorkspace(dir);
    }
}

void MCSkinApp::setWorkspace(const QString& path)
{
    workspaceDir.setPath(path);
    ui.lblWorkspacePath->setText("Workspace: " + path);

    // Setup staging directory (where you put raw images)
    stagingDir.setPath(workspaceDir.filePath("staging"));
    if (!stagingDir.exists()) {
        stagingDir.mkpath(".");
        qDebug() << "Created staging directory:" << stagingDir.path();
    }

    // Setup dataset directory (where processed files go)
    datasetDir.setPath(workspaceDir.filePath("Minecraft_Skin_Dataset/data"));
    if (!datasetDir.exists()) {
        datasetDir.mkpath(".");
        qDebug() << "Created dataset directory:" << datasetDir.path();
    }

    // 每次重新选择工作区或扫描时，清空当前工作 ID
    m_currentWorkingId = "";
    m_pendingPromptText = "";

    // 自动扫描 staging 文件夹中的 txt 文件提取 prompt
    QStringList txtFilters;
    txtFilters << "*.txt";
    QFileInfoList txtFiles = stagingDir.entryInfoList(txtFilters, QDir::Files);
    if (!txtFiles.isEmpty()) {
        QFile txtFile(txtFiles.first().absoluteFilePath());
        if (txtFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
            m_pendingPromptText = QString::fromUtf8(txtFile.readAll()).trimmed();
            txtFile.close();
            // 可选：读取完后把原 txt 文件删掉或重命名以防重复读取
            QFile::remove(txtFiles.first().absoluteFilePath());
        }
    }

    // 扫描图片和文本
    QStringList filters;
    filters << "*.png" << "*.jpg" << "*.jpeg" << "*.webp" << "*.txt"; // 新增 .txt
    m_fileList = stagingDir.entryInfoList(filters, QDir::Files | QDir::NoDotAndDotDot);

    m_currentIndex = 0;
    showCurrentFile();
}

void MCSkinApp::showCurrentFile()
{
    // 每次显示 Staging 图片，解除预览状态
    m_isPreviewingProjectFile = false;
    m_currentPreviewFileName = "";
    updateUiState();

    if (m_currentIndex < 0 || m_currentIndex >= m_fileList.size()) {
        ui.imagePreviewLabel->clear();
        ui.imagePreviewLabel->setText("No pending images in 'staging' folder!");
        ui.fileNameLabel->setText("No file");
        ui.fileInfoLabel->setText("");
        m_currentImage = QImage();
        return;
    }

    QFileInfo fileInfo = m_fileList.at(m_currentIndex);
    ui.fileNameLabel->setText(fileInfo.fileName());

    // 如果是文本文件
    if (fileInfo.suffix().toLower() == "txt") {
        QFile txtFile(fileInfo.absoluteFilePath());
        if (txtFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QString content = QString::fromUtf8(txtFile.readAll());
            txtFile.close();

            ui.imagePreviewLabel->setWordWrap(true);
            ui.imagePreviewLabel->setAlignment(Qt::AlignLeft | Qt::AlignTop);
            ui.imagePreviewLabel->setText(content); // 直接在原本显示图片的地方显示文字
            ui.fileInfoLabel->setText("Type: Text Prompt");
            m_currentImage = QImage(); // 清空当前图片缓存

            // 自动存入待处理 Prompt 变量，方便点击 "Set Prompt" 时直接使用
            m_pendingPromptText = content.trimmed();
        }
        return;
    }

    // 如果是图片文件 (恢复图片显示的设置)
    ui.imagePreviewLabel->setWordWrap(false);
    ui.imagePreviewLabel->setAlignment(Qt::AlignCenter);

    if (m_currentImage.load(fileInfo.absoluteFilePath())) {
        ui.fileInfoLabel->setText(QString("Size: %1 x %2").arg(m_currentImage.width()).arg(m_currentImage.height()));
        updateImageDisplay();
    }
    else {
        ui.imagePreviewLabel->setText("Failed to load image.");
    }
}

void MCSkinApp::updateImageDisplay()
{
    if (!m_currentImage.isNull() && ui.imagePreviewLabel->size().isValid()) {
        QPixmap pixmap = QPixmap::fromImage(m_currentImage);
        QPixmap scaledPixmap = pixmap.scaled(ui.imagePreviewLabel->size(),
            Qt::KeepAspectRatio,
            Qt::SmoothTransformation);
        ui.imagePreviewLabel->setPixmap(scaledPixmap);
    }
}

void MCSkinApp::resizeEvent(QResizeEvent* event)
{
    QMainWindow::resizeEvent(event);
    updateImageDisplay();
}
void MCSkinApp::processClassification(const QString& category)
{
    if (!m_isProjectLocked) {
        // 防呆保障，其实按钮应该已经被禁用了
        return;
    }

    if (m_currentIndex < 0 || m_currentIndex >= m_fileList.size()) return;

    QString originalFilePath = m_fileList.at(m_currentIndex).absoluteFilePath();

    // 调用底层库（注意这里传入的是我们锁定的 m_currentWorkingId）
    bool success = DatasetManager::processAndArchiveFile(
        workspaceDir.path(),
        originalFilePath,
        category,
        m_currentWorkingId
    );

    if (success) {
        this->statusBar()->showMessage("Moved to " + m_currentWorkingId, 2000);
        m_fileList.removeAt(m_currentIndex);
        if (m_currentIndex >= m_fileList.size()) {
            m_currentIndex = m_fileList.size() - 1;
        }

        // 关键：文件移过去后，刷新左侧栏！
        refreshProjectList();

        showCurrentFile(); // 中间显示下一张 staging 的图
    }
}

void MCSkinApp::handleBtnChar() { processClassification("input_char"); }
void MCSkinApp::handleBtnStyle() { processClassification("input_style"); }
void MCSkinApp::handleBtnPredView() { processClassification("pred_view"); }
void MCSkinApp::handleBtnPredSkin() { processClassification("pred_skin"); }
void MCSkinApp::handleBtnGtSkin() { processClassification("gt_skin"); }
void MCSkinApp::handleBtnGtView() { processClassification("gt_view"); }


void MCSkinApp::handleBtnFirst()
{
    if (!m_fileList.isEmpty()) {
        m_currentIndex = 0;
        showCurrentFile();
    }
}

void MCSkinApp::handleBtnPrev()
{
    if (m_currentIndex > 0) {
        m_currentIndex--;
        showCurrentFile();
    }
    else {
        // 已经在第一张了，在底部状态栏浮现提示，2000 毫秒 (2秒) 后自动消失
        this->statusBar()->showMessage("Already at the first image!", 2000);
    }
}

void MCSkinApp::handleBtnNext()
{
    if (m_currentIndex < m_fileList.size() - 1) {
        m_currentIndex++;
        showCurrentFile();
    }
    else {
        // 已经在最后一张了，提示 2 秒
        this->statusBar()->showMessage("This is the last image!", 2000);
    }
}

// 核心：控制 UI 元素的可用性，防呆设计
void MCSkinApp::updateUiState()
{
    // 规则 1：只有锁定项目、且不在预览项目文件、且没在截图时，才能点击分类按钮！
    bool canClassify = m_isProjectLocked && !m_isPreviewingProjectFile && !m_isCroppingState;

    ui.btnChar->setEnabled(canClassify);
    ui.btnStyle->setEnabled(canClassify);
    ui.btnPredView->setEnabled(canClassify);
    ui.btnPredSkin->setEnabled(canClassify);
    ui.btnGtSkin->setEnabled(canClassify);
    ui.btnGtView->setEnabled(canClassify);

    // 规则 2：截图状态下，禁止上一张/下一张/跳过切图！
    bool canNavigate = !m_isCroppingState && !m_isPreviewingProjectFile;
    ui.btnNext->setEnabled(canNavigate);
    ui.btnPrev->setEnabled(canNavigate);
    ui.btnFirst->setEnabled(canNavigate);

    // 规则 3：项目锁定、截图相关的控制
    ui.btnNewProject->setEnabled(!m_isCroppingState); // 只要不截图随时能新建
    ui.btnUnlockProject->setEnabled(m_isProjectLocked && !m_isCroppingState);
    ui.btnCropAvatar->setEnabled(m_isProjectLocked); // 预览项目文件时也可以截图！

    // 规则 4：新按钮的显隐控制
    ui.btnBackToStaging->setEnabled(m_isPreviewingProjectFile && !m_isCroppingState);
    ui.btnRemoveFile->setEnabled(m_isPreviewingProjectFile && !m_isCroppingState);

    // 更新文本提示
    if (m_isProjectLocked) {
        ui.lblCurrentProject->setText("Project: ID " + m_currentWorkingId);
        QString avatarPath = datasetDir.filePath(m_currentWorkingId + "/thumbnail.png");
        if (QFile::exists(avatarPath)) {
            QPixmap avatarPix(avatarPath);
            ui.lblAvatar->setPixmap(avatarPix.scaled(128, 128, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        }
        else {
            ui.lblAvatar->clear();
            ui.lblAvatar->setText("No Avatar");
        }
        // 加载快捷打标的数据
        QString metadataPath = DatasetManager::getMetadataFilePath(workspaceDir.path());
        QMap<QString, QJsonObject> metaMap = DatasetManager::loadAllMetadata(metadataPath);
        if (metaMap.contains(m_currentWorkingId)) {
            QJsonObject meta = metaMap[m_currentWorkingId]["meta"].toObject();
            ui.chkMain_IsAI->setChecked(meta["is_ai_generated"].toBool());
            ui.chkMain_IsCommercial->setChecked(meta["is_commercial"].toBool());
            ui.editMain_Author->setText(meta["author"].toString());
            ui.editMain_SourceUrl->setText(meta["source_url"].toString());
        }
    }
    else {
        ui.lblCurrentProject->setText("Project: Not Locked");
        ui.lblAvatar->clear();
        ui.lblAvatar->setText("No Avatar");
        ui.listProjectFiles->clear();

        ui.chkMain_IsAI->setChecked(false);
        ui.chkMain_IsCommercial->setChecked(false);
        ui.editMain_Author->clear();
        ui.editMain_SourceUrl->clear();
    }

}

// 点击“新建空项目”
void MCSkinApp::handleNewProject()
{
    // 【新增逻辑】：如果当前已经锁定了项目，先给用户一个视觉反馈
    if (m_isProjectLocked) {
        this->statusBar()->showMessage("Auto-unlocked previous project: " + m_currentWorkingId, 2000);
    }

    m_currentWorkingId = DatasetManager::generateNextId(datasetDir.path());
    m_isProjectLocked = true;

    updateUiState();
    refreshProjectList();

    this->statusBar()->showMessage("Locked to new project: " + m_currentWorkingId, 3000);
}

// 点击“完成并解锁”
void MCSkinApp::handleUnlockProject()
{
    m_isProjectLocked = false;
    m_currentWorkingId = "";
    updateUiState();

    // 解锁后，恢复显示 staging 中的当前图
    showCurrentFile();
    this->statusBar()->showMessage("Project unlocked.", 3000);
}

// 刷新左侧文件列表
void MCSkinApp::refreshProjectList()
{
    ui.listProjectFiles->clear();
    if (!m_isProjectLocked || m_currentWorkingId.isEmpty()) return;

    QDir projDir(datasetDir.filePath(m_currentWorkingId));
    if (!projDir.exists()) return;

    QStringList filters;
    filters << "*.png" << "*.jpg" << "*.jpeg" << "*.webp" << "*.txt";
    QFileInfoList projFiles = projDir.entryInfoList(filters, QDir::Files);

    for (const QFileInfo& info : projFiles) {
        //过滤掉缩略图本身，保持列表干净
        if (info.fileName() == "thumbnail.png") continue;
        ui.listProjectFiles->addItem(info.fileName());
    }
}

void MCSkinApp::handleProjectFileClicked(QListWidgetItem* item)
{
    if (!m_isProjectLocked || m_currentWorkingId.isEmpty()) return;

    // 进入预览模式
    m_isPreviewingProjectFile = true;
    m_currentPreviewFileName = item->text();
    updateUiState(); // 立刻锁死分类按钮！

    QString fileName = item->text();
    QString filePath = datasetDir.filePath(m_currentWorkingId + "/" + fileName);
    QFileInfo fileInfo(filePath);

    // 【新增】：判断如果是文本文件
    if (fileInfo.suffix().toLower() == "txt") {
        QFile txtFile(filePath);
        if (txtFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QString content = QString::fromUtf8(txtFile.readAll());
            txtFile.close();

            ui.imagePreviewLabel->setWordWrap(true);
            ui.imagePreviewLabel->setAlignment(Qt::AlignLeft | Qt::AlignTop);
            ui.imagePreviewLabel->setText(content);
            ui.fileNameLabel->setText("[Project] " + fileName);
            ui.fileInfoLabel->setText("Type: Text File");

            m_currentImage = QImage(); // 文本不能截图，清空图片缓存
        }
    }
    else {
        // 核心修复：必须把图片加载到 m_currentImage 里，这样截图工具才能拿到高清原图
        if (m_currentImage.load(filePath)) {
            ui.imagePreviewLabel->setWordWrap(false);
            ui.imagePreviewLabel->setAlignment(Qt::AlignCenter);
            updateImageDisplay(); // 调用我们写好的等比例缩放显示函数

            ui.fileNameLabel->setText("[Project] " + fileName);
            ui.fileInfoLabel->setText(QString("Size: %1 x %2").arg(m_currentImage.width()).arg(m_currentImage.height()));
        }
    }
}
// --- 【修改】确保点击“下一张/上一张/跳过”时，恢复 Staging 视图 ---
// (因为上面的函数改变了显示的图片，但没改变 m_currentIndex，所以只要重新调用 showCurrentFile 即可恢复)
// 你的 handleBtnNext 等函数里已经有 showCurrentFile() 了，所以不需要修改，它们天然支持这种“回弹”设计。

// --- 【新增】Prompt 文本的预览与修改录入 ---
void MCSkinApp::handleBtnSetPrompt()
{
    bool ok;
    // 弹出一个多行文本输入框，标题"Set Prompt"，默认填入我们在 staging 扫到的 m_pendingPromptText
    QString text = QInputDialog::getMultiLineText(this,
        "Set Project Prompt",
        "Edit or confirm prompt text:",
        m_pendingPromptText,
        &ok);
    if (ok) {
        m_pendingPromptText = text; // 保存用户的修改

        if (m_isProjectLocked) {
            // 1. 存入底层 metadata.jsonl
            DatasetManager::updatePromptForId(workspaceDir.path(), m_currentWorkingId, m_pendingPromptText);

            // 2. 【新增】：双向存储，在项目文件夹里生成 prompt.txt
            QDir targetDir(datasetDir.filePath(m_currentWorkingId));
            if (!targetDir.exists()) targetDir.mkpath("."); // 确保文件夹存在

            QFile promptFile(targetDir.filePath("prompt.txt"));
            if (promptFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
                QTextStream out(&promptFile);
                out.setEncoding(QStringConverter::Utf8);
                out << m_pendingPromptText;
                promptFile.close();
            }

            this->statusBar()->showMessage("Prompt dual-saved to project " + m_currentWorkingId + "!", 3000);
            m_pendingPromptText = ""; // 录入完清空暂存区
            // 【新增】：立刻刷新左侧列表！
            refreshProjectList();
        }
        else {
            this->statusBar()->showMessage("Prompt cached. Please create/lock a project first.", 3000);
        }
    }
}

void MCSkinApp::handleOpenProject()
{
    // 确保有工作区
    if (datasetDir.path().isEmpty() || !datasetDir.exists()) return;

    // 实例化独立画廊窗口
    GalleryWindow* gallery = new GalleryWindow(datasetDir.path());
    // 设置为模态窗口 (主窗口变暗，必须先操作画廊)
    gallery->setAttribute(Qt::WA_DeleteOnClose); // 关掉后自动清理内存
    gallery->setWindowModality(Qt::ApplicationModal);

    // 连接画廊的信号到主窗口的槽！(关键)
    connect(gallery, &GalleryWindow::projectSelected, this, &MCSkinApp::openProjectFromGallery);

    gallery->show();
}

void MCSkinApp::handleBtnCropAvatar()
{
    // 防呆：如果没有锁定项目，或者当前没加载图片，直接忽略
    if (!m_isProjectLocked || m_currentImage.isNull()) return;

    if (!m_isCroppingState) {
        // === 状态 1：准备进入裁剪 ===
        m_isCroppingState = true;
        updateUiState(); // 【新增】：立刻锁死“切图/分类”按钮
        ui.btnCropAvatar->setText("Confirm Crop"); // 把按钮文字变成“确认裁剪”

        // 呼叫你的自定义控件，让它把黑色半透明遮罩和虚线框画出来
        ui.imagePreviewLabel->setCroppingMode(true);

        // 在底部提示一下用户怎么操作
        this->statusBar()->showMessage("Drag the box to frame the face. Scroll to resize.", 5000);
    }
    else {
        // === 状态 2：确认裁剪并保存 ===
        m_isCroppingState = false;
        updateUiState(); // 【新增】：截图完成，解锁按钮
        ui.btnCropAvatar->setText("Crop Avatar"); // 按钮文字变回去
        ui.imagePreviewLabel->setCroppingMode(false); // 关掉遮罩和虚线框

        // 1. 向自定义控件索要刚才框选区域的高清原图
        QImage croppedImg = ui.imagePreviewLabel->getCroppedImage(m_currentImage);

        if (croppedImg.isNull()) {
            this->statusBar()->showMessage("Failed to crop image.", 3000);
            return;
        }

        // 2. 将图片缩放成完美的 256x256 正方形，准备保存
        QImage finalAvatar = croppedImg.scaled(256, 256, Qt::KeepAspectRatio, Qt::SmoothTransformation);

        // 【新增】：确保目标文件夹一定存在！
        QDir targetDir(datasetDir.filePath(m_currentWorkingId));
        if (!targetDir.exists()) {
            targetDir.mkpath(".");
        }
        // 3. 拼凑保存路径
        QString avatarPath = targetDir.filePath("thumbnail.png");

        // 4. 正式写入硬盘并检查是否成功
        bool saved = finalAvatar.save(avatarPath, "PNG");
        if (!saved) {
            this->statusBar()->showMessage("Error: Failed to save avatar file to disk!", 3000);
            return;
        }

        // 5. 更新左侧头像显示
        QPixmap avatarPix = QPixmap::fromImage(finalAvatar).scaled(128, 128, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        ui.lblAvatar->setPixmap(avatarPix);

        // 完工提示
        this->statusBar()->showMessage("Avatar saved to project " + m_currentWorkingId + "!", 3000);
    }
}

void MCSkinApp::handleBtnBackToStaging()
{
    // 主动要求退出预览，显示回 staging 的进度
    showCurrentFile();
}

void MCSkinApp::handleBtnRemoveFile()
{
    if (!m_isPreviewingProjectFile || m_currentPreviewFileName.isEmpty()) return;

    // 1. 将文件物理移动回 staging 目录
    QString projectFilePath = datasetDir.filePath(m_currentWorkingId + "/" + m_currentPreviewFileName);
    QString stagingFilePath = stagingDir.filePath(m_currentPreviewFileName);

    // 如果 staging 已经有同名文件了，加个前缀防覆盖
    if (QFile::exists(stagingFilePath)) {
        stagingFilePath = stagingDir.filePath("returned_" + m_currentPreviewFileName);
    }

    if (QFile::rename(projectFilePath, stagingFilePath)) {
        // 2. 从 JSONL 中抹除记录
        DatasetManager::removeFileFromProject(workspaceDir.path(), m_currentWorkingId, m_currentPreviewFileName);

        // 3. 重新扫描 Staging 文件夹，把这只“迷途羔羊”加回队列末尾
        QStringList filters;
        filters << "*.png" << "*.jpg" << "*.jpeg" << "*.webp" << "*.txt";
        m_fileList = stagingDir.entryInfoList(filters, QDir::Files | QDir::NoDotAndDotDot);

        // 4. 刷新左侧项目列表
        refreshProjectList();

        // 5. 回到 Staging 视图
        showCurrentFile();

        this->statusBar()->showMessage("File returned to Staging!", 3000);
    }
    else {
        QMessageBox::warning(this, "Error", "Failed to return file. Is it opened in another app?");
    }
}

// 【新增】：实现点击头像触发画廊
bool MCSkinApp::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == ui.lblAvatar && event->type() == QEvent::MouseButtonPress) {
        // 如果点击了头像，且当前有工作区，就打开画廊
        handleOpenProject();
        return true;
    }
    return QMainWindow::eventFilter(watched, event);
}

// 【新增】：接收从画廊传过来的项目 ID 并加载
void MCSkinApp::openProjectFromGallery(const QString& projectId)
{
    QDir projDir(datasetDir.filePath(projectId));
    if (projDir.exists()) {
        if (m_isProjectLocked) {
            this->statusBar()->showMessage("Auto-closed previous project.", 1000);
        }

        m_currentWorkingId = projectId;
        m_isProjectLocked = true;

        // 退出截图和预览状态
        m_isPreviewingProjectFile = false;
        m_isCroppingState = false;

        updateUiState();       // 刷新 UI 并加载头像
        refreshProjectList();  // 刷新左侧文件列表
        showCurrentFile();     // 中间恢复显示 Staging 的图

        this->statusBar()->showMessage("Project " + projectId + " opened from Gallery.", 3000);
    }
}

void MCSkinApp::saveQuickTags()
{
    if (!m_isProjectLocked || m_currentWorkingId.isEmpty()) return;

    QString metadataPath = DatasetManager::getMetadataFilePath(workspaceDir.path());
    QMap<QString, QJsonObject> metaMap = DatasetManager::loadAllMetadata(metadataPath);

    // 【修复】：如果 JSON 里还没有这个项目，就立刻用默认模板创建一个！
    QJsonObject entry = metaMap.contains(m_currentWorkingId) ?
        metaMap[m_currentWorkingId] :
        DatasetManager::createDefaultJsonEntry(m_currentWorkingId);

    QJsonObject meta = entry["meta"].toObject();

    meta["is_ai_generated"] = ui.chkMain_IsAI->isChecked();
    meta["is_commercial"] = ui.chkMain_IsCommercial->isChecked();
    meta["author"] = ui.editMain_Author->text().trimmed();
    meta["source_url"] = ui.editMain_SourceUrl->text().trimmed();

    entry["meta"] = meta;
    metaMap[m_currentWorkingId] = entry;
    DatasetManager::saveAllMetadata(metadataPath, metaMap);

    this->statusBar()->showMessage("Quick tags saved.", 2000);
}
