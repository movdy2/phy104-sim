#pragma once
#include "BasePage.h"

class QLabel;
class QTableWidget;

namespace phy104 {

class DashboardPage : public BasePage {
    Q_OBJECT
public:
    /// Page indexes the cards open (see MainWindow order).
    DashboardPage(AppContext* context, QWidget* parent = nullptr);

    void setNotesPage(int index) { m_notesPage = index; }

private:
    void refresh();
    int m_notesPage = 5;

    QLabel* m_savedValue;
    QLabel* m_seriesValue;
    QList<QLabel*> m_results;          // "last result" line of each experiment card
    QTableWidget* m_table;
    QLabel* m_lastSaved;
};

} // namespace phy104
