#include "VirtualKeyboard.h"

#include <QApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

namespace phy104 {

VirtualKeyboard::VirtualKeyboard(const std::vector<KeyboardGroup>& groups, QWidget* parent)
    : QWidget(parent) {
    auto* root = new QVBoxLayout(this);
    root->setSpacing(4);

    auto makeButton = [this](const QString& label) {
        auto* b = new QPushButton(label);
        b->setFocusPolicy(Qt::NoFocus);               // do not steal focus from the input field
        b->setMinimumWidth(44);
        return b;
    };

    QHBoxLayout* lastRow = nullptr;
    for (const auto& group : groups) {
        auto* row = new QHBoxLayout;
        row->setSpacing(4);
        auto* label = new QLabel(QString::fromStdString(group.name));
        label->setMinimumWidth(70);
        row->addWidget(label);
        for (const auto& key : group.keys) {
            const QString text = QString::fromStdString(key);
            auto* button = makeButton(text);
            connect(button, &QPushButton::clicked, this, [this, text] { insertText(text); });
            row->addWidget(button);
        }
        row->addStretch();
        root->addLayout(row);
        lastRow = row;
    }

    if (lastRow) {                                    // editing keys at the end of the last row
        auto* back = makeButton("⌫");
        connect(back, &QPushButton::clicked, this, &VirtualKeyboard::backspace);
        auto* clear = makeButton("Clear");
        connect(clear, &QPushButton::clicked, this, [this] { if (QLineEdit* t = effectiveTarget()) t->clear(); });
        lastRow->insertWidget(lastRow->count() - 1, back);
        lastRow->insertWidget(lastRow->count() - 1, clear);
    }

    connect(qApp, &QApplication::focusChanged, this, &VirtualKeyboard::onFocusChanged);
}

void VirtualKeyboard::onFocusChanged(QWidget*, QWidget* now) {
    if (auto* edit = qobject_cast<QLineEdit*>(now)) m_target = edit;
}

QLineEdit* VirtualKeyboard::effectiveTarget() const {
    if (m_target && m_target->isVisibleTo(window()) && m_target->isEnabled() && !m_target->isReadOnly()) return m_target;
    return m_fallback ? m_fallback() : nullptr;
}

void VirtualKeyboard::insertText(const QString& text) {
    QLineEdit* target = effectiveTarget();
    if (!target) return;
    target->setFocus();                                   // keep typing in the same field afterwards
    target->insert(text);
    emit symbolInserted(text);
}

void VirtualKeyboard::backspace() {
    if (QLineEdit* target = effectiveTarget()) { target->setFocus(); target->backspace(); }
}

} // namespace phy104
