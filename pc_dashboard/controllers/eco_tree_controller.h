/**
 * 누적 배출량에 연동된 환경 기여 시각화(QMovie 나무 성장 애니메이션) 제어기 헤더.
 */
#pragma once

#include <QObject>

class QLabel;
class QMovie;

// 투입 누적 수량에 따른 나무 시각화 단계
enum class TreeStage {
    BASE_TREE, // 0개: 기본 밑둥/가지 상태
    SPROUT, // 1~2개: 새싹 발현
    SAPLING, // 3~4개: 잎 무성 단계
    MATURE // 5개 이상: 성장 완료
};

class EcoTreeController : public QObject {
    Q_OBJECT

public:
    explicit EcoTreeController(QLabel* movieLabel, QLabel* statusLabel, QObject* parent = nullptr);

    // 배출 수량 변경에 따른 단계 판정 및 구간 재생
    void updateCount(int totalCount);
    // 기본 나무(0개) 프레임으로 리셋
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