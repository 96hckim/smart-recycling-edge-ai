#pragma once

#include <QObject>

class QLabel;
class QMovie;

// 에코 트리 성장 단계 정의
enum class TreeStage {
    READY = 0, // 대기 (씨앗/화분)
    SPROUT = 1, // 새싹 (1~2개)
    SAPLING = 2, // 묘목 (3~4개)
    MATURE = 3 // 완성된 큰 나무 (5개 이상)
};

class EcoTreeController : public QObject {
    Q_OBJECT

public:
    explicit EcoTreeController(QLabel* movieLabel, QLabel* statusLabel, QObject* parent = nullptr);

    void updateCount(int totalCount);
    void reset();

private:
    void initMovie();
    void showWaitingPlaceholder();
    void updateStatusText(TreeStage stage, int count);
    TreeStage calculateStage(int count) const;

private:
    QLabel* m_lblMovie { nullptr };
    QLabel* m_lblStatus { nullptr };
    QMovie* m_movie { nullptr };
    TreeStage m_currentStage { TreeStage::READY };
    int m_targetFrame { 0 };
};