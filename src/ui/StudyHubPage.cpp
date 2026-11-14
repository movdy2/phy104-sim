#include "StudyHubPage.h"

#include "AppContext.h"
#include "Components.h"
#include "DiagramsPage.h"
#include "JsonRepositories.h"
#include "LatexRenderer.h"

#include <QAbstractItemView>
#include <QComboBox>
#include <QFileInfo>
#include <QFrame>
#include <QHash>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QScrollArea>
#include <QSet>
#include <QSplitter>
#include <QStringList>
#include <QStackedWidget>
#include <QTableWidget>
#include <QTimer>
#include <QVector>
#include <QVBoxLayout>
#include <algorithm>
#include <cmath>
#include <utility>

namespace phy104 {
namespace {

/// Build a compact local, extractive draft from the most informative sentences in a note.
QString extractNoteSummary(const QString& source) {
    const QStringList sentences = source.split(QRegularExpression(QStringLiteral("(?<=[.!?])\\s+|\\n+")), Qt::SkipEmptyParts);
    if (sentences.isEmpty()) return {};

    static const QSet<QString> stopWords = {
        "a", "about", "also", "an", "and", "are", "as", "at", "be", "because", "but", "by", "can",
        "does", "for", "from", "had", "has", "have", "in", "into", "is", "it", "its", "more", "most",
        "not", "of", "on", "or", "our", "only", "so", "that", "the", "their", "then", "there", "these",
        "this", "those", "to", "use", "used", "using", "was", "were", "when", "which", "while", "with",
        "you", "your"
    };
    static const QRegularExpression wordPattern(QStringLiteral("[\\p{L}\\p{N}]{3,}"));
    QHash<QString, int> frequency;
    for (const QString& sentence : sentences) {
        auto words = wordPattern.globalMatch(sentence.toLower());
        while (words.hasNext()) {
            const QString word = words.next().captured();
            if (!stopWords.contains(word)) ++frequency[word];
        }
    }

    struct ScoredSentence { qsizetype index; double score; };
    QVector<ScoredSentence> ranked;
    ranked.reserve(sentences.size());
    for (qsizetype i = 0; i < sentences.size(); ++i) {
        int contentWords = 0;
        double score = 0.0;
        auto words = wordPattern.globalMatch(sentences.at(i).toLower());
        while (words.hasNext()) {
            const QString word = words.next().captured();
            const int count = frequency.value(word);
            if (count > 0) {
                ++contentWords;
                score += 1.0 + std::log(static_cast<double>(count));
            }
        }
        if (contentWords > 0) score /= std::sqrt(static_cast<double>(contentWords));
        if (i == 0) score *= 1.05;                 // slight preference for an opening definition
        ranked.push_back({i, score});
    }
    std::stable_sort(ranked.begin(), ranked.end(), [](const ScoredSentence& a, const ScoredSentence& b) {
        return a.score > b.score;
    });

    const qsizetype sentenceCount = sentences.size();
    const qsizetype wanted = std::min<qsizetype>(3, std::max<qsizetype>(1,
        static_cast<qsizetype>(std::ceil(static_cast<double>(sentenceCount) * 0.28))));
    QSet<qsizetype> selected;
    for (qsizetype i = 0; i < wanted && i < ranked.size(); ++i) selected.insert(ranked[i].index);
    QStringList summary;
    for (qsizetype i = 0; i < sentenceCount; ++i)
        if (selected.contains(i)) summary.push_back(sentences.at(i).trimmed());
    return summary.join(" ");
}

} // namespace

StudyHubPage::StudyHubPage(std::vector<StudyTopic> topics, AppContext* context,
                           DiagramsPage* opticalSetups, QString notesFilePath, QWidget* parent)
    : BasePage("Study Hub", "STUDY / Chapters, JSON notes and optical setups", context, parent),
      m_topics(std::move(topics)), m_notesFilePath(std::move(notesFilePath)) {
    m_sections = new ModeTabs({"Chapters", "Notes", "Optical Setups"});
    m_sections->setObjectName("studyHubTabs");
    content()->addWidget(m_sections);

    m_sectionStack = new QStackedWidget;
    m_sectionStack->setObjectName("studyHubSections");
    content()->addWidget(m_sectionStack, 1);

    // ---------------------------------------------------------------- chapters: topic list, summary/rules, and chapter-specific constants
    auto* chaptersPage = new QWidget;
    auto* columns = new QHBoxLayout(chaptersPage);
    columns->setContentsMargins(0, 0, 0, 0);
    columns->setSpacing(12);

    auto* chapterColumn = new QWidget;
    chapterColumn->setFixedWidth(252);
    auto* chapterLayout = new QVBoxLayout(chapterColumn);
    chapterLayout->setContentsMargins(0, 0, 0, 0);
    chapterLayout->setSpacing(12);
    auto* chapters = new Panel("Chapters", QString::number(m_topics.size()));
    m_list = new QListWidget;
    m_list->setObjectName("studyTopics");
    m_list->setMinimumHeight(160);
    for (const StudyTopic& topic : m_topics) {
        auto* item = new QListWidgetItem(QString::fromStdString(topic.title));
        item->setToolTip(QString::fromStdString(topic.chapter));
        m_list->addItem(item);
    }
    chapters->body()->addWidget(m_list, 1);
    chapterLayout->addWidget(chapters, 1);
    columns->addWidget(chapterColumn);

    auto* middleColumn = new QVBoxLayout;
    middleColumn->setContentsMargins(0, 0, 0, 0);
    middleColumn->setSpacing(12);
    auto* summaryPanel = new Panel("Summary");
    m_chapter = new QLabel;
    m_chapter->setObjectName("caption");
    m_title = new QLabel;
    m_title->setObjectName("studyTitle");
    m_title->setWordWrap(true);
    m_summary = new QLabel;
    m_summary->setObjectName("studyBody");
    m_summary->setWordWrap(true);
    m_summary->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    summaryPanel->body()->addWidget(m_chapter);
    summaryPanel->body()->addWidget(m_title);
    summaryPanel->body()->addWidget(m_summary);
    middleColumn->addWidget(summaryPanel);

    m_rulesPanel = new Panel("Rules", "formula · why it holds");
    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->viewport()->setAutoFillBackground(false);
    auto* rulesHolder = new QWidget;
    m_rules = new QVBoxLayout(rulesHolder);
    m_rules->setContentsMargins(0, 0, 0, 0);
    m_rules->setSpacing(10);
    scroll->setWidget(rulesHolder);
    m_rulesPanel->body()->addWidget(scroll, 1);
    middleColumn->addWidget(m_rulesPanel, 1);
    columns->addLayout(middleColumn, 1);

    auto* constantsColumn = new QWidget;
    constantsColumn->setFixedWidth(250);
    auto* constantsLayout = new QVBoxLayout(constantsColumn);
    constantsLayout->setContentsMargins(0, 0, 0, 0);
    constantsLayout->setSpacing(12);
    auto* chapterConstants = new Panel("Constants", "this chapter");
    m_constants = new QTableWidget(0, 2);
    m_constants->setHorizontalHeaderLabels({"Name", "Value"});
    m_constants->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_constants->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_constants->horizontalHeader()->setStretchLastSection(true);
    m_constants->verticalHeader()->hide();
    chapterConstants->body()->addWidget(m_constants, 1);
    constantsLayout->addWidget(chapterConstants, 1);
    columns->addWidget(constantsColumn);
    m_sectionStack->addWidget(chaptersPage);

    // ---------------------------------------------------------------- per-chapter notes and editable summaries in a separate JSON file
    auto* notesPanel = new Panel("Chapter notes", "JSON · NOTES + SUMMARIES · AUTOSAVED");
    m_noteChapter = new QComboBox;
    m_noteChapter->setObjectName("studyHubNoteChapter");
    for (const StudyTopic& topic : m_topics)
        m_noteChapter->addItem(QString::fromStdString(topic.title), noteKey(topic));
    m_notesEditor = new QPlainTextEdit;
    m_notesEditor->setObjectName("studyHubNoteEditor");
    m_notesEditor->setPlaceholderText("Write plain-text notes for the selected chapter…");
    m_notesEditor->setLineWrapMode(QPlainTextEdit::WidgetWidth);
    m_noteSummaryEditor = new QPlainTextEdit;
    m_noteSummaryEditor->setObjectName("studyHubNoteSummaryEditor");
    m_noteSummaryEditor->setPlaceholderText("Write a short recap, or generate an extractive draft from your notes…");
    m_noteSummaryEditor->setLineWrapMode(QPlainTextEdit::WidgetWidth);
    m_summarizeNoteButton = new QPushButton("Generate summary");
    m_summarizeNoteButton->setObjectName("studyHubSummarizeNote");
    m_summarizeNoteButton->setToolTip("Create an editable extractive draft from your notes on this device; no network service is used.");
    m_summarizeNoteButton->setEnabled(false);

    auto* noteEditors = new QSplitter(Qt::Horizontal);
    noteEditors->setObjectName("studyHubNoteSplit");
    noteEditors->addWidget(labeled("Notes", m_notesEditor));
    auto* summaryColumn = new QWidget;
    auto* summaryLayout = new QVBoxLayout(summaryColumn);
    summaryLayout->setContentsMargins(0, 0, 0, 0);
    summaryLayout->setSpacing(8);
    summaryLayout->addWidget(labeled("Summary", m_noteSummaryEditor), 1);
    summaryLayout->addWidget(m_summarizeNoteButton, 0, Qt::AlignRight);
    noteEditors->addWidget(summaryColumn);
    noteEditors->setStretchFactor(0, 2);
    noteEditors->setStretchFactor(1, 1);

    m_noteStatus = new QLabel;
    m_noteStatus->setObjectName("caption");
    m_noteStatus->setWordWrap(true);
    notesPanel->body()->setContentsMargins(12, 0, 12, 10);
    notesPanel->body()->addWidget(labeled("Chapter", m_noteChapter));
    notesPanel->body()->addWidget(noteEditors, 1);
    notesPanel->body()->addWidget(m_noteStatus);
    m_sectionStack->addWidget(notesPanel);

    if (!loadStudyHubNotes(m_notesFilePath, m_studyNotes, &m_notesLoadError)) {
        m_notesLoadFailed = true;
        m_noteChapter->setEnabled(false);
        m_notesEditor->setReadOnly(true);
        m_noteSummaryEditor->setReadOnly(true);
        m_summarizeNoteButton->setEnabled(false);
    }
    m_noteSaveTimer = new QTimer(this);
    m_noteSaveTimer->setSingleShot(true);
    m_noteSaveTimer->setInterval(650);
    connect(m_noteSaveTimer, &QTimer::timeout, this, &StudyHubPage::saveCurrentNote);
    connect(m_noteChapter, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int) {
        flushStudyNotes();
        loadCurrentNote();
    });
    const auto queueStudyNoteSave = [this] {
        if (m_loadingNote || m_notesLoadFailed) return;
        m_noteStatus->setText("Unsaved · notes and summary will be saved to " + QFileInfo(m_notesFilePath).fileName());
        m_noteSaveTimer->start();
    };
    connect(m_notesEditor, &QPlainTextEdit::textChanged, this, queueStudyNoteSave);
    connect(m_noteSummaryEditor, &QPlainTextEdit::textChanged, this, queueStudyNoteSave);
    connect(m_notesEditor, &QPlainTextEdit::textChanged, this, [this] {
        m_summarizeNoteButton->setEnabled(!m_notesLoadFailed && !m_notesEditor->toPlainText().trimmed().isEmpty());
    });
    connect(m_summarizeNoteButton, &QPushButton::clicked, this, [this] {
        m_noteSummaryEditor->setPlainText(extractNoteSummary(m_notesEditor->toPlainText()));
    });
    loadCurrentNote();
    if (m_notesLoadFailed)
        m_noteStatus->setText("Notes are read-only; the JSON file could not be loaded and was left unchanged. " + m_notesLoadError);

    // ---------------------------------------------------------------- full optical setup gallery
    m_sectionStack->addWidget(opticalSetups);

    connect(m_sections, &ModeTabs::currentChanged, m_sectionStack, &QStackedWidget::setCurrentIndex);
    connect(m_list, &QListWidget::currentRowChanged, this, &StudyHubPage::selectTopic);
    if (!m_topics.empty()) m_list->setCurrentRow(0);
}

StudyHubPage::~StudyHubPage() { flushStudyNotes(); }

QString StudyHubPage::noteKey(const StudyTopic& topic) const {
    if (!topic.id.empty()) return QString::fromStdString(topic.id);
    if (!topic.chapter.empty()) return QString::fromStdString(topic.chapter);
    return QString::fromStdString(topic.title);
}

void StudyHubPage::loadCurrentNote() {
    if (!m_notesEditor || !m_noteSummaryEditor) return;
    const QString key = m_noteChapter ? m_noteChapter->currentData().toString() : QString();
    const StudyHubNote note = m_studyNotes.value(key);
    m_loadingNote = true;
    m_notesEditor->setPlainText(QString::fromStdString(note.text));
    m_noteSummaryEditor->setPlainText(QString::fromStdString(note.summary));
    m_loadedNoteKey = key;
    m_loadingNote = false;
    if (key.isEmpty()) m_noteStatus->setText("Select a chapter to write its notes.");
    else if (m_notesDirty) m_noteStatus->setText("Unsaved chapter changes · JSON save pending for " + QFileInfo(m_notesFilePath).fileName());
    else m_noteStatus->setText("Notes and summaries · autosave to " + QFileInfo(m_notesFilePath).fileName());
}

void StudyHubPage::saveCurrentNote() {
    if (m_notesLoadFailed || !m_notesEditor || !m_noteSummaryEditor || m_loadedNoteKey.isEmpty()) return;
    const StudyHubNote note{m_notesEditor->toPlainText().toStdString(), m_noteSummaryEditor->toPlainText().toStdString()};
    if (note.text.empty() && note.summary.empty()) {
        if (m_studyNotes.remove(m_loadedNoteKey) > 0) m_notesDirty = true;
    } else if (!m_studyNotes.contains(m_loadedNoteKey) || m_studyNotes.value(m_loadedNoteKey) != note) {
        m_studyNotes.insert(m_loadedNoteKey, note);
        m_notesDirty = true;
    }
    if (!m_notesDirty) return;
    QString error;
    if (saveStudyHubNotes(m_notesFilePath, m_studyNotes, &error)) {
        m_notesDirty = false;
        m_noteStatus->setText("Saved notes + summary to JSON · " + QFileInfo(m_notesFilePath).fileName());
    } else {
        m_noteStatus->setText("Could not save JSON notes: " + error);
        emit m_ctx->message("Could not save Study Hub notes: " + error);
    }
}

void StudyHubPage::flushStudyNotes() {
    if (m_noteSaveTimer && m_noteSaveTimer->isActive()) m_noteSaveTimer->stop();
    saveCurrentNote();
}

int StudyHubPage::currentSection() const { return m_sections->current(); }

void StudyHubPage::selectSection(int index) {
    if (index < 0 || index >= m_sectionStack->count()) return;
    m_sections->setCurrent(index);             // ModeTabs does not emit for programmatic changes
    m_sectionStack->setCurrentIndex(index);
}

void StudyHubPage::selectTopic(int row) {
    if (row < 0 || row >= int(m_topics.size())) return;
    const StudyTopic& topic = m_topics[std::size_t(row)];

    m_chapter->setText(QString::fromStdString(topic.chapter).toUpper());
    m_title->setText(QString::fromStdString(topic.title));
    m_summary->setText(QString::fromStdString(topic.summary));
    m_rulesPanel->setTitle("Rules · " + QString::fromStdString(topic.title));

    clearLayout(m_rules);
    int index = 0;
    for (const StudyRule& rule : topic.rules) {
        if (index > 0) {
            auto* line = new QFrame;
            line->setObjectName("ruleLine");
            line->setFrameShape(QFrame::HLine);
            m_rules->addWidget(line);
        }
        auto* formula = new LatexWidget;
        formula->setAlignLeft(true);
        formula->setLatex(LatexRenderer::fromPlainMath(QString::fromStdString(rule.formula)));
        m_rules->addWidget(formula);
        if (!rule.explain.empty()) {
            auto* explain = new QLabel(QString::fromStdString(rule.explain));
            explain->setObjectName("studyBody");
            explain->setWordWrap(true);
            m_rules->addWidget(explain);
        }
        ++index;
    }
    m_rules->addStretch(1);

    m_constants->setRowCount(int(topic.constants.size()));
    for (int r = 0; r < int(topic.constants.size()); ++r) {
        m_constants->setItem(r, 0, new QTableWidgetItem(QString::fromStdString(topic.constants[std::size_t(r)].name)));
        m_constants->setItem(r, 1, new QTableWidgetItem(QString::fromStdString(topic.constants[std::size_t(r)].value)));
    }
    m_constants->resizeColumnToContents(0);
}

} // namespace phy104
