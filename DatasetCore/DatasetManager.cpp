#include "DatasetManager.h"
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QTextStream>
#include <QDebug>
#include <algorithm> // For std::sort

QString DatasetManager::getDatasetPath(const QString& workspacePath) {
    return QDir(workspacePath).filePath("Minecraft_Skin_Dataset/data");
}

QString DatasetManager::getMetadataFilePath(const QString& workspacePath) {
    return QDir(workspacePath).filePath("Minecraft_Skin_Dataset/metadata.jsonl");
}

QString DatasetManager::generateNextId(const QString& datasetPath) {
    QDir dir(datasetPath);
    if (!dir.exists()) dir.mkpath(".");

    QStringList subDirs = dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    int maxId = 0;
    for (const QString& d : subDirs) {
        bool ok;
        int id = d.toInt(&ok);
        if (ok && id > maxId) maxId = id;
    }
    return QString("%1").arg(maxId + 1, 6, 10, QChar('0'));
}

QString DatasetManager::getNextFilename(const QString& targetDirPath, const QString& category, const QString& ext) {
    QDir dir(targetDirPath);
    QStringList existingFiles = dir.entryList(QStringList() << category + "*", QDir::Files);

    if (existingFiles.isEmpty()) {
        return category + ext;
    }
    int count = existingFiles.size() + 1;
    return QString("%1_%2%3").arg(category).arg(count, 2, 10, QChar('0')).arg(ext);
}

bool DatasetManager::processAndArchiveFile(const QString& workspacePath,
    const QString& originalFilePath,
    const QString& category,
    QString& inOutTargetId)
{
    QFileInfo oldFileInfo(originalFilePath);
    if (!oldFileInfo.exists()) return false;

    QString datasetPath = getDatasetPath(workspacePath);
    QString metadataPath = getMetadataFilePath(workspacePath);

    // 1. Generate new ID if empty
    if (inOutTargetId.isEmpty()) {
        inOutTargetId = generateNextId(datasetPath);
    }

    // 2. Create target directory
    QDir targetDir(QDir(datasetPath).filePath(inOutTargetId));
    if (!targetDir.exists()) {
        targetDir.mkpath(".");
    }

    // 3. Rename and move file
    QString ext = oldFileInfo.suffix().toLower();
    if (!ext.startsWith(".")) ext = "." + ext;

    QString newFilename = getNextFilename(targetDir.path(), category, ext);
    QString newFilePath = targetDir.filePath(newFilename);

    if (!QFile::rename(originalFilePath, newFilePath)) {
        return false;
    }

    // 4. Update JSONL
    QMap<QString, QJsonObject> metaMap = loadAllMetadata(metadataPath);
    QJsonObject currentEntry = metaMap.contains(inOutTargetId) ? metaMap[inOutTargetId] : createDefaultJsonEntry(inOutTargetId);

    QJsonObject filesObj = currentEntry["files"].toObject();
    QJsonArray categoryArray = filesObj.contains(category) ? filesObj[category].toArray() : QJsonArray();

    categoryArray.append(newFilename);
    filesObj[category] = categoryArray;
    currentEntry["files"] = filesObj;

    updateCategoryTags(currentEntry, category);

    metaMap[inOutTargetId] = currentEntry;
    saveAllMetadata(metadataPath, metaMap);

    return true;
}

bool DatasetManager::updatePromptForId(const QString& workspacePath, const QString& targetId, const QString& promptText) {
    if (targetId.isEmpty()) return false;

    QString metadataPath = getMetadataFilePath(workspacePath);
    QMap<QString, QJsonObject> metaMap = loadAllMetadata(metadataPath);

    QJsonObject currentEntry = metaMap.contains(targetId) ? metaMap[targetId] : createDefaultJsonEntry(targetId);
    currentEntry["prompt"] = promptText;

    metaMap[targetId] = currentEntry;
    saveAllMetadata(metadataPath, metaMap);
    return true;
}

QMap<QString, QJsonObject> DatasetManager::loadAllMetadata(const QString& metadataPath) {
    QMap<QString, QJsonObject> map;
    QFile file(metadataPath);
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream in(&file);
        in.setEncoding(QStringConverter::Utf8);
        while (!in.atEnd()) {
            QString line = in.readLine().trimmed();
            if (line.isEmpty()) continue;
            QJsonDocument doc = QJsonDocument::fromJson(line.toUtf8());
            if (doc.isObject()) {
                QJsonObject obj = doc.object();
                map[obj["id"].toString()] = obj;
            }
        }
        file.close();
    }
    return map;
}

void DatasetManager::saveAllMetadata(const QString& metadataPath, const QMap<QString, QJsonObject>& metaMap) {
    QFile file(metadataPath);
    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&file);
        out.setEncoding(QStringConverter::Utf8);

        QList<QString> keys = metaMap.keys();
        std::sort(keys.begin(), keys.end());

        for (const QString& key : keys) {
            QJsonDocument doc(metaMap[key]);
            out << doc.toJson(QJsonDocument::Compact) << "\n";
        }
        file.close();
    }
}

QJsonObject DatasetManager::createDefaultJsonEntry(const QString& id) {
    QJsonObject entry;
    entry["id"] = id;
    entry["prompt"] = "";
    QJsonObject copyright;
    copyright["is_licensed"] = false;
    copyright["can_commercial"] = false;
    copyright["is_human_drawn"] = false;
    copyright["is_ai_generated"] = false;
    entry["copyright"] = copyright;
    entry["files"] = QJsonObject();
    return entry;
}

void DatasetManager::updateCategoryTags(QJsonObject& entry, const QString& category) {
    QJsonObject copyright = entry["copyright"].toObject();
    if (category == "gt_skin" || category == "gt_view" || category == "gt_project") {
        copyright["is_human_drawn"] = true;
    }
    if (category == "pred_skin" || category == "pred_view") {
        copyright["is_ai_generated"] = true;
    }
    entry["copyright"] = copyright;
}

bool DatasetManager::removeFileFromProject(const QString& workspacePath, const QString& targetId, const QString& filename) {
    QString metadataPath = getMetadataFilePath(workspacePath);
    QMap<QString, QJsonObject> metaMap = loadAllMetadata(metadataPath);
    if (!metaMap.contains(targetId)) return false;

    QJsonObject entry = metaMap[targetId];
    QJsonObject filesObj = entry["files"].toObject();
    bool found = false;

    // 如果退回的是 prompt，清空 prompt 字段
    if (filename == "prompt.txt") {
        entry["prompt"] = "";
        found = true;
    }
    // 遍历所有分类，寻找并剔除该文件名
    else {
        for (auto it = filesObj.begin(); it != filesObj.end(); ++it) {
            QJsonArray arr = it.value().toArray();
            for (int i = 0; i < arr.size(); ++i) {
                if (arr[i].toString() == filename) {
                    arr.removeAt(i);
                    filesObj[it.key()] = arr;
                    found = true;
                    break;
                }
            }
            if (found) break;
        }
    }

    if (found) {
        entry["files"] = filesObj;
        metaMap[targetId] = entry;
        saveAllMetadata(metadataPath, metaMap);
        return true;
    }
    return false;
}