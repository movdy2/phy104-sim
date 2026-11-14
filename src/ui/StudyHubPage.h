#pragma once
#include "BasePage.h"
#include "StudyHub.h"

#include <QMap>
#include <QString>
#include <vector>

class QComboBox;
class QLabel;
class QListWidget;
class QPlainTextEdit;
class QStackedWidget;
class QTableWidget;
class QTimer;
class QVBoxLayout;

namespace phy104 {

class DiagramsPage;
class ModeTabs;
class Panel;

/// Study Hub: chapters with summaries, rules and constants, per-chapter notes and optical setups.
class StudyHubPage : public BasePage {
    Q_OBJECT
public:
    explicit StudyHubPage(std::vector<StudyTopic> topics, AppContext* context,
                          DiagramsPage* opticalSetups, QString notesFilePath, QWidget* parent = nullptr);
    ~StudyHubPage() override;

    QListWidget* topicList() const { return m_list; }
    QTableWidget* constantTable() const { return m_constants; }              // constants for the selected chapter
    QLabel* summaryLabel() const { return m_summary; }
    QPlainTextEdit* notesEditor() const { return m_notesEditor; }
    QString notesFilePath() const { return m_notesFilePath; }
    void flushStudyNotes();
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
    QComboBox* m_noteChapter = nullptr;
    QPlainTextEdit* m_notesEditor = nullptr;
    QLabel* m_noteStatus = nullptr;
    QTimer* m_noteSaveTimer = nullptr;
    QString m_notesFilePath;
    QString m_loadedNoteKey;
    QMap<QString, QString> m_studyNotes;
    bool m_loadingNote = false;
    bool m_notesDirty = false;
    Panel* m_rulesPanel = nullptr;

    QString noteKey(const StudyTopic& topic) const;
    void loadCurrentNote();
    void saveCurrentNote();
};

} // namespace phy104
