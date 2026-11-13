#pragma once
#include "Models.h"
#include <QPointer>
#include <QWidget>
#include <functional>
#include <vector>

class QLineEdit;

namespace phy104 {

/// Keyboard built from keyboard.json (groups of keys). Buttons never take focus, so the
/// QLineEdit the user was typing in keeps the cursor. The widget remembers the last
/// focused QLineEdit (QApplication::focusChanged) and inserts text there.
class VirtualKeyboard : public QWidget {
    Q_OBJECT
public:
    explicit VirtualKeyboard(const std::vector<KeyboardGroup>& groups, QWidget* parent = nullptr);

    /// Used when no field was focused yet (or the focused one is on another page):
    /// returns the field of the current page that should receive the text.
    void setFallbackProvider(std::function<QLineEdit*()> provider) { m_fallback = std::move(provider); }

signals:
    void symbolInserted(const QString& text);

private slots:
    void onFocusChanged(QWidget* old, QWidget* now);

private:
    void insertText(const QString& text);
    void backspace();
    QLineEdit* effectiveTarget() const;

    QPointer<QLineEdit> m_target;
    std::function<QLineEdit*()> m_fallback;
};

} // namespace phy104
