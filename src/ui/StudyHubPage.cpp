#include "StudyHubPage.h"

#include "AppContext.h"
#include "Components.h"
#include "LatexRenderer.h"

#include <QHeaderView>
#include <QLabel>
#include <QListWidget>
#include <QScrollArea>
#include <QTableWidget>
#include <QVBoxLayout>

namespace phy104 {

StudyHubPage::StudyHubPage(std::vector<StudyTopic> topics, AppContext* context, QWidget* parent)
    : BasePage("Study Hub", "STUDY / One chapter at a time: the summary, the rules and the constants behind them",
               context, parent),
      m_topics(std::move(topics)) {
    auto cols = addThreeColumns(252, 250);

    // ---------------------------------------------------------------- left: the chapters, in JSON order
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
    cols.left->addWidget(chapters, 1);

    // ---------------------------------------------------------------- centre: summary + the rules
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
    cols.center->addWidget(summaryPanel);

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
    cols.center->addWidget(m_rulesPanel, 1);

    // ---------------------------------------------------------------- right: the constants of this chapter
    auto* constants = new Panel("Constants", "this chapter");
    m_constants = new QTableWidget(0, 2);
    m_constants->setHorizontalHeaderLabels({"Name", "Value"});
    m_constants->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_constants->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_constants->horizontalHeader()->setStretchLastSection(true);
    m_constants->verticalHeader()->hide();
    constants->body()->addWidget(m_constants, 1);
    cols.right->addWidget(constants, 1);

    connect(m_list, &QListWidget::currentRowChanged, this, &StudyHubPage::selectTopic);
    if (!m_topics.empty()) m_list->setCurrentRow(0);
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
