#include "SessionLog.h"

#include <algorithm>
#include <cstdio>

namespace phy104 {

RunRecord& SessionLog::addRun(const std::string& experiment, const std::string& description,
                              const std::string& result, bool valid) {
    int next = 1;
    for (const auto& r : m_runs) next = std::max(next, r.id + 1);
    m_runs.push_back({next, experiment, description, result, valid});
    return m_runs.back();
}

std::vector<RunRecord> SessionLog::recent(std::size_t count) const {
    std::vector<RunRecord> out(m_runs.rbegin(), m_runs.rend());
    if (out.size() > count) out.resize(count);
    return out;
}

void SessionLog::restoreRuns(std::vector<RunRecord> runs) { m_runs = std::move(runs); }

void SessionLog::setNotes(std::string text) {
    m_notes = std::move(text);
    for (auto& [name, snap] : m_snapshots) snap.notesContent = m_notes;
}

void SessionLog::setSnapshot(const Snapshot& snapshot) {
    Snapshot copy = snapshot;
    copy.notesContent = m_notes;
    m_snapshots[snapshot.experiment] = std::move(copy);
}

void SessionLog::restoreSnapshots(std::map<std::string, Snapshot> snapshots) {
    m_snapshots = std::move(snapshots);
    for (auto& [name, snap] : m_snapshots) snap.notesContent = m_notes;
}

const Snapshot* SessionLog::snapshot(const std::string& experiment) const {
    const auto it = m_snapshots.find(experiment);
    return it == m_snapshots.end() ? nullptr : &it->second;
}

std::string SessionLog::runLabel(int id) {
    char buffer[16];
    std::snprintf(buffer, sizeof buffer, "#%03d", id);
    return buffer;
}

} // namespace phy104
