#pragma once
#include "BasePage.h"
#include "StudyHub.h"

#include <vector>

class QLabel;
class QListWidget;
class QStackedWidget;
class QTableWidget;
class QVBoxLayout;

namespace phy104 {

class ConstantsPage;
class DiagramsPage;
class MarkdownWorkspace;
class ModeTabs;
class Panel;

/// Study Hub: chapter notes, the shared lab notes, optical setups and physical constants.
class StudyHubPage : public BasePage {
    Q_OBJECT
public:
    explicit StudyHubPage(std::vector<StudyTopic> topics, AppContext* context,
                          ConstantsPage* constants, DiagramsPage* opticalSetups, QWidget* parent = nullptr);

    QListWidget* topicList() const { return m_list; }
    QTableWidget* constantTable() const { return m_constants; }              // constants for the selected chapter
    QLabel* summaryLabel() const { return m_summary; }
    MarkdownWorkspace* notesWorkspace() const { return m_notes; }
    int topicCount() const { return int(m_topics.size()); }
    int currentSection() const;
    void selectSection(int index);

public slots:
    void selectTopic(int row);

private:
    std::vector<StudyTopic> m_topics;
    ModeTabs* m_sections = nullptr;
    QStackedWidget* m_sectionStack = nullptr;
    QListWidget* m_list = nullptr;
    QLabel* m_chapter = nullptr;
    QLabel* m_title = nullptr;
    QLabel* m_summary = nullptr;
    QVBoxLayout* m_rules = nullptr;          // filled again for every chapter
    QTableWidget* m_constants = nullptr;
    MarkdownWorkspace* m_notes = nullptr;
    Panel* m_rulesPanel = nullptr;
};

} // namespace phy104
