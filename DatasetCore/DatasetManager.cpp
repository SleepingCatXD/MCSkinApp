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

    // 【修复】：使用新的嵌套结构
    QJsonObject aiTraining = currentEntry["ai_training"].toObject();
    aiTraining["prompt"] = promptText;
    currentEntry["ai_training"] = aiTraining;

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

    // 1. 元数据 (来源、版权、授权)
    QJsonObject meta;
    meta["is_ai_generated"] = false;
    meta["is_commercial"] = false;
    meta["permission_ai"] = false;
    meta["author"] = "";
    meta["source_url"] = "";
    meta["permission_evidence_path"] = ""; // 预留接口：以后用于存放聊天截图的相对路径
    entry["meta"] = meta;

    // 2. 提示词数据
    QJsonObject aiTraining;
    aiTraining["prompt"] = "";
    aiTraining["negative_prompt"] = "";
    entry["ai_training"] = aiTraining;

    // 3. 人物角色特征 (仅留空接口，以后扩展)
    entry["attributes"] = QJsonObject();

    // 4. 文件列表
    entry["files"] = QJsonObject();

    return entry;
}

// 无感打标逻辑更新 (适应新的 meta 结构)
void DatasetManager::updateCategoryTags(QJsonObject& entry, const QString& category) {
    QJsonObject meta = entry["meta"].toObject();
    bool isChanged = false;

    // 如果分类为人类绘制的皮肤或视图，可以顺手把 is_ai_generated 关掉
    if (category == "gt_skin" || category == "gt_view" || category == "gt_project") {
        meta["is_ai_generated"] = false;
        isChanged = true;
    }
    // 如果明确分类为 AI 预测生成的图，打上 AI 标签
    if (category == "pred_skin" || category == "pred_view") {
        meta["is_ai_generated"] = true;
        isChanged = true;
    }

    if (isChanged) {
        entry["meta"] = meta;
    }
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
