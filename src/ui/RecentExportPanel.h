#pragma once

#include "Components.h"
#include "SessionLog.h"

#include <QString>

class QLabel;
class QPushButton;

namespace phy104 {

/// Shared recent-export card used by both data export destinations.
class RecentExportPanel : public Panel {
    Q_OBJECT
public:
    explicit RecentExportPanel(QWidget* parent = nullptr);

    void setRecord(const ExportRecord& record);

private:
    void openFolder();

    QLabel* m_file = nullptr;
    QLabel* m_meta = nullptr;
    QLabel* m_location = nullptr;
    QPushButton* m_openFolder = nullptr;
    QString m_path;
};

} // namespace phy104
