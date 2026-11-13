#include "ConstantsPage.h"

#include <QApplication>
#include <QClipboard>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>

namespace phy104 {

ConstantsPage::ConstantsPage(const IConstantsRepository& repository, QWidget* parent)
    : QWidget(parent), m_constants(repository.loadAll()) {
    m_search = new QLineEdit;
    m_search->setPlaceholderText("Search constants (name or symbol)...");

    m_table = new QTableWidget(int(m_constants.size()), 4);
    m_table->setHorizontalHeaderLabels({"Symbol", "Name", "Value", "Unit"});
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->verticalHeader()->hide();

    for (int row = 0; row < int(m_constants.size()); ++row) {
        const auto& c = m_constants[std::size_t(row)];
        m_table->setItem(row, 0, new QTableWidgetItem(QString::fromStdString(c.symbol)));
        m_table->setItem(row, 1, new QTableWidgetItem(QString::fromStdString(c.name)));
        m_table->setItem(row, 2, new QTableWidgetItem(QString::number(c.value, 'g', 12)));
        m_table->setItem(row, 3, new QTableWidgetItem(QString::fromStdString(c.unit)));
    }
    m_table->resizeColumnsToContents();

    m_description = new QLabel("Select a constant to see its description.");
    m_description->setWordWrap(true);

    auto* copyBtn = new QPushButton("Copy value");

    auto* root = new QVBoxLayout(this);
    auto* title = new QLabel("Physics Constants");
    title->setObjectName("pageTitle");
    root->addWidget(title);
    root->addWidget(m_search);
    root->addWidget(m_table, 1);
    root->addWidget(m_description);
    root->addWidget(copyBtn);

    connect(m_search, &QLineEdit::textChanged, this, &ConstantsPage::applyFilter);
    connect(m_table, &QTableWidget::itemSelectionChanged, this, &ConstantsPage::onSelectionChanged);
    connect(copyBtn, &QPushButton::clicked, this, &ConstantsPage::copySelected);
}

int ConstantsPage::selectedIndex() const {
    const auto rows = m_table->selectionModel()->selectedRows();
    return rows.isEmpty() ? -1 : rows.first().row();
}

void ConstantsPage::applyFilter(const QString& text) {
    for (int row = 0; row < int(m_constants.size()); ++row) {
        const auto& c = m_constants[std::size_t(row)];
        const QString haystack = QString::fromStdString(c.symbol + " " + c.name + " " + c.description);
        m_table->setRowHidden(row, !haystack.contains(text, Qt::CaseInsensitive));
    }
}

void ConstantsPage::onSelectionChanged() {
    const int i = selectedIndex();
    if (i >= 0) m_description->setText(QString::fromStdString(m_constants[std::size_t(i)].description));
}

void ConstantsPage::copySelected() {
    const int i = selectedIndex();
    if (i < 0) return;
    // Copies the plain number so it can be pasted straight into a solver field.
    QApplication::clipboard()->setText(QString::number(m_constants[std::size_t(i)].value, 'g', 12));
}

} // namespace phy104
