#include "StudyHubPage.h"

#include "AppContext.h"
#include "Components.h"
#include "DiagramsPage.h"
#include "LatexRenderer.h"
#include "MarkdownWorkspace.h"

#include <QAbstractItemView>
#include <QFrame>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QListWidget>
#include <QScrollArea>
#include <QStackedWidget>
#include <QTableWidget>
#include <QVBoxLayout>
#include <utility>

namespace phy104 {

StudyHubPage::StudyHubPage(std::vector<StudyTopic> topics, AppContext* context,
                           DiagramsPage* opticalSetups, QWidget* parent)
    : BasePage("Study Hub", "STUDY / Chapters, notes and optical setups", context, parent),
      m_topics(std::move(topics)) {
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

    // ---------------------------------------------------------------- shared lab notes, also available from the Notes sidebar shortcut
    auto* notesPanel = new Panel("Lab notes", "AUTOSAVED");
    m_notes = new MarkdownWorkspace;
    const QString savedNotes = QString::fromStdString(context->log.notes());
    m_notes->setMarkdown(savedNotes.isEmpty() ? MarkdownWorkspace::starterNotes() : savedNotes);
    notesPanel->body()->setContentsMargins(12, 0, 12, 10);
    notesPanel->body()->addWidget(m_notes, 1);
    m_sectionStack->addWidget(notesPanel);
    connect(m_notes, &MarkdownWorkspace::contentChanged, context, &AppContext::setNotes);
    connect(context, &AppContext::notesAppended, this, [this] {
        const QString latest = QString::fromStdString(m_ctx->log.notes());
        if (m_notes->markdown() != latest) m_notes->setMarkdown(latest);
    });
    connect(context, &AppContext::notesSaved, this, [this] {
        const QString latest = QString::fromStdString(m_ctx->log.notes());
        if (m_notes->markdown() != latest) m_notes->setMarkdown(latest);
        m_notes->setSavedText("Saved " + m_ctx->lastSavedTime());
    });

    // ---------------------------------------------------------------- full optical setup gallery
    m_sectionStack->addWidget(opticalSetups);

    connect(m_sections, &ModeTabs::currentChanged, m_sectionStack, &QStackedWidget::setCurrentIndex);
    connect(m_list, &QListWidget::currentRowChanged, this, &StudyHubPage::selectTopic);
    if (!m_topics.empty()) m_list->setCurrentRow(0);
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
