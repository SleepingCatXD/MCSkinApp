#pragma once

#include <QString>
#include <QDir>
#include <QJsonObject>
#include <QJsonArray>
#include <QMap>

class DatasetManager
{
public:
    // Core function: Move file, rename it, and update JSONL
    // Note: targetId is passed by reference. If empty, it generates a new one.
    static bool processAndArchiveFile(const QString& workspacePath,
        const QString& originalFilePath,
        const QString& category,
        QString& inOutTargetId);

    // Update the prompt text for a specific ID
    static bool updatePromptForId(const QString& workspacePath,
        const QString& targetId,
        const QString& promptText);

    static QString generateNextId(const QString& datasetPath);

    // 从项目中移除文件并清理 JSON 记录
    static bool removeFileFromProject(const QString& workspacePath,
        const QString& targetId,
        const QString& filename);

    static QString getMetadataFilePath(const QString& workspacePath);
    static QMap<QString, QJsonObject> loadAllMetadata(const QString& metadataPath);
    static void saveAllMetadata(const QString& metadataPath, const QMap<QString, QJsonObject>& metaMap);
    static QJsonObject createDefaultJsonEntry(const QString& id);

private:
    static QString getDatasetPath(const QString& workspacePath);
    static QString getNextFilename(const QString& targetDirPath, const QString& category, const QString& ext);
    static void updateCategoryTags(QJsonObject& entry, const QString& category);
};