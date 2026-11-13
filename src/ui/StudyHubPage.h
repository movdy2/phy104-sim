#pragma once
#include "BasePage.h"
#include "StudyHub.h"

#include <vector>

class QLabel;
class QListWidget;
class QTableWidget;
class QVBoxLayout;

namespace phy104 {

class Panel;

/// "Study Hub" - one chapter at a time, the way you revise for the exam: a short summary, the rules
/// (formula + why it holds) and the constants of that chapter. Everything comes from data/study_hub.json,
/// which is a JSON array, so the sidebar order is the order of the course.
class StudyHubPage : public BasePage {
    Q_OBJECT
public:
    explicit StudyHubPage(std::vector<StudyTopic> topics, AppContext* context, QWidget* parent = nullptr);

    QListWidget* topicList() const { return m_list; }
    QTableWidget* constantTable() const { return m_constants; }
    QLabel* summaryLabel() const { return m_summary; }
    int topicCount() const { return int(m_topics.size()); }

public slots:
    void selectTopic(int row);

private:
    std::vector<StudyTopic> m_topics;
    QListWidget* m_list = nullptr;
    QLabel* m_chapter = nullptr;
    QLabel* m_title = nullptr;
    QLabel* m_summary = nullptr;
    QVBoxLayout* m_rules = nullptr;          // filled again for every chapter
    QTableWidget* m_constants = nullptr;
    Panel* m_rulesPanel = nullptr;
};

} // namespace phy104
