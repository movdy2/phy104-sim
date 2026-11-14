#pragma once
#include "BasePage.h"

class QComboBox;
class QLabel;
class QPushButton;
class QStackedWidget;
class QToolButton;
class QTableWidget;

namespace phy104 {

class LineChart;
class ModeTabs;
struct RunRecord;

class DashboardPage : public BasePage {
    Q_OBJECT
public:
    /// Page indexes the cards open (see MainWindow order).
    DashboardPage(AppContext* context, QWidget* parent = nullptr);

    void setNotesPage(int index) { m_notesPage = index; }
    void setExportPage(int index) { m_exportPage = index; }

private:
    void refresh();
    void refreshCompare();                       // overlay two saved runs in the "Compare" tab
    void deleteSelectedRun();
    const RunRecord* runById(int id) const;
    int m_notesPage = 5;
    int m_exportPage = 5;

    QToolButton* m_browseExperiments = nullptr;
    QLabel* m_savedValue;
    QLabel* m_seriesValue;
    QList<QLabel*> m_results;          // "last result" line of each experiment card
    QTableWidget* m_table;
    QLabel* m_lastSaved;
    ModeTabs* m_recentMode;
    QStackedWidget* m_recentStack;
    QComboBox *m_cmpA, *m_cmpB;
    LineChart* m_cmpChart;
    QLabel* m_cmpSummary;
    QPushButton* m_deleteRun;
};

} // namespace phy104
