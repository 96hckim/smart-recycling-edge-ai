#include "app_config.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

FrameMetadata parseJetsonFrameJson(const QByteArray& jsonData)
{
    FrameMetadata meta;
    QJsonDocument doc = QJsonDocument::fromJson(jsonData);
    if (!doc.isObject())
        return meta;

    QJsonObject root = doc.object();
    meta.timestamp = root.value("timestamp").toDouble();
    meta.fps = root.value("fps").toDouble();
    meta.inferMs = root.value("infer_ms").toDouble();

    QJsonArray detArray = root.value("detections").toArray();
    for (const QJsonValue& val : detArray) {
        QJsonObject dObj = val.toObject();
        Detection d;
        d.classId = dObj.value("class_id").toInt();
        d.className = dObj.value("class_name").toString();
        d.confidence = dObj.value("confidence").toDouble();

        // 파서 단계에서 RecycleCategory 자동 판정
        d.category = Config::parseCategory(d.className);

        QJsonObject boxObj = dObj.value("box").toObject();
        d.box = QRect(boxObj.value("x").toInt(),
            boxObj.value("y").toInt(),
            boxObj.value("w").toInt(),
            boxObj.value("h").toInt());

        meta.detections.append(d);
    }

    return meta;
}