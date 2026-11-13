#pragma once
#include "Models.h"

#include <QList>
#include <QMainWindow>
#include <vector>

class QButtonGroup;
class QLabel;
class QLineEdit;
class QPushButton;
class QStackedWidget;
class QTimer;
class QVBoxLayout;

namespace phy104 {

class AppContext;
class BasePage;
class VirtualKeyboard;

/// Figma shell: sidebar (brand, modules, tools, session info) | pages | status bar.
class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    MainWindow(AppContext* context, const std::vector<KeyboardGroup>& keyboardLayout, QWidget* parent = nullptr);

    /// Adds a page to the stack. `section` 0 = main module list, 1 = tools list. Returns the stack index.
    int addPage(const QString& title, const QString& icon, QWidget* page, int section = 0);
    /// A second sidebar entry that opens an existing page.
    void addNavAlias(const QString& title, const QString& icon, int pageIndex, int section = 0);

    int pageCount() const;
    int currentPage() const;
    QWidget* page(int index) const;
    int indexOf(QWidget* page) const;
    QList<QPushButton*> navButtons() const;
    QPushButton* themeButton() const { return m_themeButton; }
    QPushButton* keyboardButton() const { return m_keyboardButton; }
    VirtualKeyboard* keyboard() const { return m_keyboard; }
    QLabel* engineLabel() const { return m_engine; }
    QString statusLeftText() const;

public slots:
    void setCurrentPage(int index);

signals:
    void themeToggled(bool dark);

private:
    void addNavButton(const QString& title, const QString& icon, int pageIndex, int section);
    void refreshIcons();
    void refreshStatus();
    void showMessage(const QString& text);
    QLineEdit* fallbackField() const;

    AppContext* m_ctx;
    QStackedWidget* m_stack;
    QVBoxLayout* m_modules;
    QVBoxLayout* m_tools;
    QLabel* m_toolsHeader;
    QButtonGroup* m_navGroup;
    struct NavEntry { QPushButton* button; QString icon; int page; };
    std::vector<NavEntry> m_nav;
    VirtualKeyboard* m_keyboard;
    QLabel *m_statusLeft, *m_statusMid, *m_statusRight;
    QLabel* m_engine;
    QString m_transient;
    QTimer* m_messageTimer;
    QPushButton *m_themeButton, *m_keyboardButton;
};

} // namespace phy104
