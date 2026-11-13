#include "RecentExportPanel.h"

#include <QDesktopServices>
#include <QFileInfo>
#include <QLabel>
#include <QPushButton>
#include <QUrl>

namespace phy104 {
namespace {

QString fileSize(qint64 bytes) {
    if (bytes < 1024) return QString("%1 B").arg(bytes);
    const double size = static_cast<double>(bytes);
    if (bytes < 1024 * 1024) return QString("%1 KB").arg(size / 1024.0, 0, 'f', 1);
    return QString("%1 MB").arg(size / (1024.0 * 1024.0), 0, 'f', 1);
}

} // namespace

RecentExportPanel::RecentExportPanel(QWidget* parent)
    : Panel("Recent export", "REMEMBERED", parent) {
    m_file = new QLabel("No export yet");
    m_file->setObjectName("sectionTitle");
    m_file->setWordWrap(true);

    m_meta = new QLabel("—");
    m_meta->setObjectName("caption");
    m_meta->setWordWrap(true);

    m_location = new QLabel("The file you export last is remembered here and in the .phy session.");
    m_location->setObjectName("hint");
    m_location->setWordWrap(true);

    m_openFolder = new QPushButton("Open folder");
    m_openFolder->setObjectName("secondary");
    m_openFolder->setEnabled(false);

    body()->setSpacing(6);
    body()->addWidget(m_file);
    body()->addWidget(m_meta);
    body()->addWidget(m_location);
    body()->addWidget(m_openFolder);
    connect(m_openFolder, &QPushButton::clicked, this, &RecentExportPanel::openFolder);
}

void RecentExportPanel::setRecord(const ExportRecord& record) {
    m_path = QString::fromStdString(record.path);
    if (m_path.isEmpty()) {
        m_file->setText("No export yet");
        m_meta->setText("—");
        m_location->setText("The file you export last is remembered here and in the .phy session.");
        m_openFolder->setEnabled(false);
        return;
    }

    const QFileInfo info(m_path);
    m_file->setText(info.fileName());
    QString metadata = QString::fromStdString(record.format);
    if (record.series == "report")
        metadata += QString(" · %1 runs · %2 experiments").arg(record.rows).arg(record.columns);
    else if (record.rows > 0 || record.columns > 0)
        metadata += QString(" · %1 rows × %2 columns").arg(record.rows).arg(record.columns);
    if (info.exists()) metadata += " · " + fileSize(info.size());
    m_meta->setText(metadata);
    m_location->setText(QString::fromStdString(record.when) + "\n" + info.absolutePath() +
                        (info.exists() ? QString() : QString("  (file no longer there)")));
    m_openFolder->setEnabled(true);
}

void RecentExportPanel::openFolder() {
    if (m_path.isEmpty()) return;
    QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(m_path).absolutePath()));
}

} // namespace phy104
