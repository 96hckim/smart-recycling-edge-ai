#pragma once

#include <QObject>

class QLabel;
class QMovie;

enum class TreeStage {
    BASE_TREE, // 0개: 잎이 없는 기본 나무 (초기 세팅)
    SPROUT, // 1~2개: 잎 돋아남
    SAPLING, // 3~4개: 잎 무성해짐
    MATURE // 5개 이상: 완성된 나무
};

class EcoTreeController : public QObject {
    Q_OBJECT

public:
    explicit EcoTreeController(QLabel* movieLabel, QLabel* statusLabel, QObject* parent = nullptr);

    void updateCount(int totalCount);
    void reset();

private:
    void initMovie();
    void showBaseTree();
    void updateStatusText(TreeStage stage, int count);
    TreeStage calculateStage(int count) const;

private:
    QLabel* m_lblMovie { nullptr };
    QLabel* m_lblStatus { nullptr };
    QMovie* m_movie { nullptr };
    TreeStage m_currentStage { TreeStage::BASE_TREE };
    int m_targetFrame { 0 };
};