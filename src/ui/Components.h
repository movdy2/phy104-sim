#pragma once
// Reusable widgets of the Figma component sheet. Styling is in ThemeManager's style sheet.
#include "ExperimentReports.h"

#include <QFrame>
#include <QList>

class QButtonGroup;
class QHBoxLayout;
class QLabel;
class QLineEdit;
class QPushButton;
class QVBoxLayout;

namespace phy104 {

/// Card with a header (title left, small mono meta text right) and a body layout.
class Panel : public QFrame {
    Q_OBJECT
public:
    explicit Panel(const QString& title, const QString& meta = QString(), QWidget* parent = nullptr);
    QVBoxLayout* body() const { return m_body; }
    void setMeta(const QString& meta);
    void setTitle(const QString& title);
private:
    QLabel* m_title;
    QLabel* m_meta;
    QVBoxLayout* m_body;
};

/// Numeric input with an optional unit on the right, amber focus ring and red error state.
///  * accepts numbers AND expressions: 0.5*mm, 1/3, sqrt(2), π, 30 (degrees when the unit is °)
///  * `edited()` is emitted while typing (after a short pause), on Enter and when focus leaves
///  * optional clickable ▲▼ arrows (also Up/Down keys and the mouse wheel)
///  * `setError(true, "Enter d > 0 mm.")` draws a red ring and shows the message under the field
class UnitField : public QFrame {
    Q_OBJECT
public:
    explicit UnitField(const QString& unit = QString(), QWidget* parent = nullptr);

    double value(bool* ok = nullptr) const;
    void setValue(double value, int decimals = 4);     // trailing zeros are kept: "10.00"
    QString text() const;
    void setText(const QString& text);
    void setUnit(const QString& unit);
    QString unit() const;
    void setSpinner(bool enabled, double step = 1.0);   // ▲▼ arrows
    void setShowPlus(bool on) { m_plus = on; }
    bool showsPlus() const { return m_plus; }          // "+30.00" for signed quantities
    void setError(bool on, const QString& message = QString());
    bool hasError() const { return m_error; }
    int decimals() const { return m_decimals; }
    QLineEdit* edit() const { return m_edit; }

signals:
    void edited();                                      // live edit, Enter, focus out, or a spinner step
    void errorTextChanged(const QString& message);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void repolish();
    void step(int direction);

    QLineEdit* m_edit;
    QLabel* m_unit;
    QWidget* m_arrows;
    class QTimer* m_liveTimer;
    bool m_spinnerOn = false;
    bool m_plus = false;
    bool m_error = false;
    double m_step = 1.0;
    int m_decimals = 4;
};

/// Casio-style on-screen keypad for the Calculator page (same theme as the rest of the app).
class CalculatorKeypad : public QWidget {
    Q_OBJECT
public:
    explicit CalculatorKeypad(QWidget* parent = nullptr);
signals:
    void insertText(const QString& text);     // digits, operators, "sin(" ...
    void backspace();                         // DEL
    void clearAll();                          // AC
    void equals();                            // =
};

/// Two or more mutually exclusive buttons ("Thin lens | Mirror").
class ModeTabs : public QWidget {
    Q_OBJECT
public:
    explicit ModeTabs(const QStringList& labels, QWidget* parent = nullptr);
    int current() const;
    void setCurrent(int index);
signals:
    void currentChanged(int index);
private:
    QButtonGroup* m_group;
};

/// One Casio-style register: title + mode, numbered lines, big result, footer.
class LcdRegisterView : public QFrame {
    Q_OBJECT
public:
    explicit LcdRegisterView(QWidget* parent = nullptr);
    void setRegister(const LcdRegister& reg);
private:
    QVBoxLayout* m_layout;
};

/// Short explanatory card (amber title) or error card (red).
class NoticeCard : public QFrame {
    Q_OBJECT
public:
    explicit NoticeCard(QWidget* parent = nullptr);
    void setNotice(const Notice& notice);
    void setNotice(const QString& title, const QString& body, bool error);
private:
    QLabel* m_title;
    QLabel* m_body;
};

/// Caption above a field: vertical widget (small muted label + field).
QWidget* labeled(const QString& label, QWidget* field);

/// Removes and deletes every child item of a layout.
void clearLayout(QLayout* layout);

} // namespace phy104
