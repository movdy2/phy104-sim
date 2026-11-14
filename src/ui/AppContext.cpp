#include "AppContext.h"

#include <QTime>

namespace phy104 {

AppContext::AppContext(OpticsPresets p, std::vector<ReferenceTopic> t, std::string unit,
                       std::unique_ptr<ISessionStore> store, QString file)
    : presets(std::move(p)), topics(std::move(t)), unitDiscipline(std::move(unit)),
      sessionFile(std::move(file)), m_store(std::move(store)) {
    if (m_store && m_store->load(log)) m_lastSaved = "restored";      // previous session
}

void AppContext::publish(const Snapshot& snapshot) {
    log.setSnapshot(snapshot);
    m_lastSeries[snapshot.experiment] = snapshot.series;       // what a saved run of this page will carry
    emit snapshotChanged();
}

void AppContext::recordRun(const QString& experiment, const QString& description, const QString& result, bool valid) {
    RunRecord& run = log.addRun(experiment.toStdString(), description.toStdString(), result.toStdString(), valid);
    auto series = m_lastSeries.find(experiment.toStdString());
    if (series != m_lastSeries.end() && !series->second.rows.empty())
        run.series = series->second;                           // the sweep stays attached to its run
    emit runsChanged();
    saveSession();
    emit message("Saved calculation " + QString::fromStdString(SessionLog::runLabel(log.runs().back().id)) + " · " + result);
}

bool AppContext::deleteRun(int id) {
    if (!log.removeRun(id)) return false;
    emit runsChanged();
    saveSession();
    emit message("Deleted " + QString::fromStdString(SessionLog::runLabel(id)));
    return true;
}

void AppContext::setNotes(const QString& markdown) {
    log.setNotes(markdown.toStdString());
    saveSession();
    emit notesSaved();
}

void AppContext::appendNotes(const QString& markdown) {
    QString text = QString::fromStdString(log.notes());
    if (!text.isEmpty() && !text.endsWith('\n')) text += '\n';
    setNotes(text + (text.isEmpty() ? "" : "\n") + markdown);
    emit notesAppended();
    emit message("Sent to Notes");
}

void AppContext::recordExport(const ExportRecord& record) {
    log.setLastExport(record);
    saveSession();                                             // the recent export survives a restart
    emit exportRecorded();
    emit message("Exported " + QString::fromStdString(record.path));
}

bool AppContext::saveSession() {
    if (!m_store) return false;
    const bool ok = m_store->save(log);
    if (ok) m_lastSaved = QTime::currentTime().toString("HH:mm");
    else emit message("Could not save " + sessionFile);
    return ok;
}

QString AppContext::nextRunLabel() const {
    int next = 1;
    for (const auto& r : log.runs()) next = std::max(next, r.id + 1);
    return "RUN " + QString::number(next).rightJustified(3, '0');
}

} // namespace phy104
