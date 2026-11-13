#include "SessionLog.h"

#include <algorithm>
#include <cstdio>

using std::max;
using std::vector;
using std::string;

namespace phy104 {

RunRecord& SessionLog::addRun(const string& experiment, const string& description,
                              const string& result, bool valid) {
    int next = 1;
    for (const auto& r : m_runs) next = max(next, r.id + 1);
    m_runs.push_back({next, experiment, description, result, valid});
    return m_runs.back();
}

vector<RunRecord> SessionLog::recent(size_t count) const {
    vector<RunRecord> out(m_runs.rbegin(), m_runs.rend());
    if (out.size() > count) out.resize(count);
    return out;
}

void SessionLog::restoreRuns(vector<RunRecord> runs) {
    m_runs = std::move(runs);
}

void SessionLog::setNotes(string text) {
    m_notes = std::move(text);
    for (auto& [name, snap] : m_snapshots) snap.notesContent = m_notes;
}

void SessionLog::setSnapshot(const Snapshot& snapshot) {
    Snapshot copy = snapshot;
    copy.notesContent = m_notes;
    m_snapshots[snapshot.experiment] = std::move(copy);
}

void SessionLog::restoreSnapshots(std::map<string, Snapshot> snapshots) {
    m_snapshots = std::move(snapshots);
    for (auto& [name, snap] : m_snapshots) snap.notesContent = m_notes;
}

const Snapshot* SessionLog::snapshot(const string& experiment) const {
    const auto it = m_snapshots.find(experiment);
    return it == m_snapshots.end() ? nullptr : &it->second;
}

string SessionLog::runLabel(int id) {
    char buffer[16];
    std::snprintf(buffer, sizeof buffer, "#%03d", id);
    return buffer;
}

} // namespace phy104