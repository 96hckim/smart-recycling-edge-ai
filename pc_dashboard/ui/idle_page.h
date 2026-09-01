#ifndef IDLE_PAGE_H
#define IDLE_PAGE_H

#include <QWidget>

namespace Ui {
class IdlePage;
}

class IdlePage : public QWidget {
    Q_OBJECT

public:
    explicit IdlePage(QWidget* parent = nullptr);
    ~IdlePage() override;

signals:
    void sigMemberStartRequested(const QString& userId = "MEMBER_DEMO_USER");
    void sigGuestStartRequested();

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private slots:
    void on_btnGuestStart_clicked();

private:
    Ui::IdlePage* ui;
};

#endif // IDLE_PAGE_H