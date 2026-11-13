#include "BasePage.h"
#include "AppContext.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

namespace phy104 {

BasePage::BasePage(const QString& title, const QString& subtitle, AppContext* context, QWidget* parent)
    : QWidget(parent), m_ctx(context) {
    setObjectName("pageArea");
    // the content is centred and limited in width, so very wide windows do not stretch the layout
    auto* outer = new QHBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    auto* inner = new QWidget;
    inner->setMaximumWidth(1500);
    inner->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    outer->addWidget(inner);
    outer->setAlignment(inner, Qt::AlignHCenter);
    auto* root = new QVBoxLayout(inner);
    root->setContentsMargins(20, 22, 20, 14);
    root->setSpacing(14);

    auto* header = new QHBoxLayout;
    auto* titles = new QVBoxLayout;
    titles->setSpacing(4);
    auto* t = new QLabel(title);
    t->setObjectName("pageTitle");
    auto* s = new QLabel(subtitle);
    s->setObjectName("pageSubtitle");
    titles->addWidget(t);
    titles->addWidget(s);
    m_units = new QLabel("SI UNITS · DEG");
    m_units->setObjectName("unitsTag");
    auto* units = m_units;
    auto* save = new QPushButton("Save session");
    m_saveButton = save;
    save->setObjectName("outline");
    save->setCursor(Qt::PointingHandCursor);
    m_exportButton = new QPushButton("Export");
    m_exportButton->setObjectName("primary");
    m_exportButton->setMinimumWidth(130);
    m_exportButton->setCursor(Qt::PointingHandCursor);
    m_exportButton->hide();
    header->addLayout(titles, 1);
    header->addWidget(units, 0, Qt::AlignTop);
    header->addSpacing(10);
    header->addWidget(save, 0, Qt::AlignTop);
    header->addSpacing(8);
    header->addWidget(m_exportButton, 0, Qt::AlignTop);
    root->addLayout(header);

    m_content = new QVBoxLayout;
    m_content->setSpacing(12);
    root->addLayout(m_content, 1);

    connect(save, &QPushButton::clicked, this, [this] {
        if (m_ctx->saveSession()) emit m_ctx->message("Session saved · " + m_ctx->sessionFile);
    });
}

void BasePage::setUnitsTag(const QString& text) { m_units->setText(text); }

void BasePage::useSaveExportHeader() {
    m_units->hide();
    m_saveButton->setText("Save");
    m_saveButton->setMinimumWidth(130);
    m_exportButton->show();
    connect(m_exportButton, &QPushButton::clicked, this, &BasePage::exportRequested);
}

void BasePage::setStatusInfo(const StatusInfo& info, bool paused) {
    m_status = info;
    m_paused = paused;
    emit stateChanged();
}

BasePage::Columns BasePage::addThreeColumns(int leftWidth, int rightWidth) {
    auto* row = new QHBoxLayout;
    row->setSpacing(12);
    Columns c;
    auto make = [&](int width) {
        auto* col = new QVBoxLayout;
        col->setSpacing(12);
        auto* holder = new QWidget;
        holder->setLayout(col);
        col->setContentsMargins(0, 0, 0, 0);
        if (width > 0) holder->setFixedWidth(width);
        row->addWidget(holder, width > 0 ? 0 : 1);
        return col;
    };
    c.left = make(leftWidth);
    c.center = make(0);
    c.right = make(rightWidth);
    m_content->addLayout(row, 1);
    return c;
}

} // namespace phy104
