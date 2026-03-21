#include "AnnotationWindow.h"
#include "DatasetManager.h"
#include <QMessageBox>

AnnotationWindow::AnnotationWindow(const QString& projectId, const QString& workspacePath, QWidget* parent)
    : QWidget(parent), m_projectId(projectId), m_workspacePath(workspacePath)
{
    ui.setupUi(this);
    this->setWindowTitle("Deep Annotation Desk - Project " + m_projectId);

    // 绑定按钮
    connect(ui.btnSaveClose, &QPushButton::clicked, this, &AnnotationWindow::onSaveCloseClicked);
    connect(ui.btnCancel, &QPushButton::clicked, this, &QWidget::close); // 取消直接关闭窗口

    // 窗口一打开，立刻加载数据
    loadData();
}

AnnotationWindow::~AnnotationWindow() {}

void AnnotationWindow::loadData()
{
    QString metadataPath = DatasetManager::getMetadataFilePath(m_workspacePath);
    QMap<QString, QJsonObject> metaMap = DatasetManager::loadAllMetadata(metadataPath);

    if (!metaMap.contains(m_projectId)) return;
    QJsonObject entry = metaMap[m_projectId];

    // 1. 加载版权与来源 (Meta)
    QJsonObject metaInfo = entry["meta"].toObject();
    ui.chkAIGenerated->setChecked(metaInfo["is_ai_generated"].toBool(false));
    ui.chkIsCommercial->setChecked(metaInfo["is_commercial"].toBool(false));
    ui.chkPermissionAI->setChecked(metaInfo["permission_ai"].toBool(false));

    ui.editAuthor->setText(metaInfo["author"].toString(""));
    ui.editSourceUrl->setText(metaInfo["source_url"].toString(""));

    // 2. 加载 AI 训练文本
    QJsonObject aiTraining = entry["ai_training"].toObject();
    ui.editPrompt->setPlainText(aiTraining["prompt"].toString());
    ui.editNegativePrompt->setPlainText(aiTraining["negative_prompt"].toString());

    // 3. 加载参考图 (极其强壮的绝对路径拼接法)
    QString projectDirPath = QDir(m_workspacePath).filePath("Minecraft_Skin_Dataset/data/" + m_projectId);
    QDir projectDir(projectDirPath);

    QString avatarPath = projectDir.filePath("thumbnail.png");
    QPixmap pix;

    if (QFile::exists(avatarPath)) {
        pix.load(avatarPath);
    }
    else {
        // 后备方案：如果没有头像，抓取第一张存在的图片
        QStringList filters;
        filters << "*.png" << "*.jpg" << "*.jpeg" << "*.webp";
        QStringList images = projectDir.entryList(filters, QDir::Files);
        if (!images.isEmpty()) {
            pix.load(projectDir.filePath(images.first()));
        }
    }

    if (!pix.isNull()) {
        ui.lblReferenceImage->setPixmap(pix.scaled(256, 256, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        ui.lblReferenceImage->setAlignment(Qt::AlignCenter);
    }
    else {
        ui.lblReferenceImage->setText("No Image Available");
    }
}

void AnnotationWindow::onSaveCloseClicked()
{
    QString metadataPath = DatasetManager::getMetadataFilePath(m_workspacePath);
    QMap<QString, QJsonObject> metaMap = DatasetManager::loadAllMetadata(metadataPath);

    if (!metaMap.contains(m_projectId)) return;
    QJsonObject entry = metaMap[m_projectId];

    // 1. 保存版权与来源 (Meta)
    QJsonObject metaInfo = entry["meta"].toObject();
    metaInfo["is_ai_generated"] = ui.chkAIGenerated->isChecked();
    metaInfo["is_commercial"] = ui.chkIsCommercial->isChecked();
    metaInfo["permission_ai"] = ui.chkPermissionAI->isChecked();
    metaInfo["author"] = ui.editAuthor->text().trimmed();
    metaInfo["source_url"] = ui.editSourceUrl->text().trimmed();
    entry["meta"] = metaInfo;

    // 2. 保存 AI 训练文本
    QJsonObject aiTraining = entry["ai_training"].toObject();
    aiTraining["prompt"] = ui.editPrompt->toPlainText().trimmed();
    aiTraining["negative_prompt"] = ui.editNegativePrompt->toPlainText().trimmed();
    entry["ai_training"] = aiTraining;

    // 3. 预留物理特征接口 (不改变原有数据，如果是新项目则给个空壳)
    if (!entry.contains("attributes")) {
        entry["attributes"] = QJsonObject();
    }

    // 写回文件
    metaMap[m_projectId] = entry;
    DatasetManager::saveAllMetadata(metadataPath, metaMap);

    QMessageBox::information(this, "Success", "Annotations saved successfully!");
    this->close();
}
