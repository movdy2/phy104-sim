#pragma once
#include "ExperimentReports.h"
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace phy104 {

/// One saved calculation ("run") shown in the dashboard table.
struct RunRecord {
    int id = 0;
    std::string experiment;     // "Refraction & TIR"
    std::string description;    // "Glass → air / 30°"
    std::string result;         // "θ₂ = 48.5904°"
    bool valid = true;
};

/// What an experiment page is currently showing (used for export and the dashboard cards).
struct Snapshot {
    std::string experiment;
    std::vector<std::pair<std::string, std::string>> parameters;
    std::vector<LcdRegister> registers;
    std::string headline;
    DataSeries series;          // measurement table for export
    std::string notesContent;   // the study notes (Markdown) carried with this snapshot
};

/// In-memory state of the lab session: saved runs, session notes and the latest snapshot
/// of every experiment. Persistence is done by the data layer.
class SessionLog {
public:
    /// Appends a run; ids continue after the highest existing id.
    RunRecord& addRun(const std::string& experiment, const std::string& description,
                      const std::string& result, bool valid);

    const std::vector<RunRecord>& runs() const { return m_runs; }
    int savedCalculations() const { return static_cast<int>(m_runs.size()); }
    /// Newest first, at most `count` entries.
    std::vector<RunRecord> recent(std::size_t count) const;
    void restoreRuns(std::vector<RunRecord> runs);

    /// Stores the snapshot; its `notesContent` is stamped with the current notes.
    void setSnapshot(const Snapshot& snapshot);
    const Snapshot* snapshot(const std::string& experiment) const;
    const std::map<std::string, Snapshot>& snapshots() const { return m_snapshots; }
    /// Used when a session file is loaded: snapshots are restored and re-stamped with the notes.
    void restoreSnapshots(std::map<std::string, Snapshot> snapshots);

    /// Sets the study notes (Markdown). The text is also copied into every snapshot's `notesContent`,
    /// so a snapshot always carries the notes that were current when it was taken.
    void setNotes(std::string text);
    const std::string& notes() const { return m_notes; }

    /// Equation Lab: the written equations (LaTeX), in order.
    const std::vector<std::string>& equations() const { return m_equations; }
    void setEquations(std::vector<std::string> list) { m_equations = std::move(list); }

    int sessionNumber() const { return m_sessionNumber; }
    void setSessionNumber(int n) { m_sessionNumber = n; }

    /// "#014"
    static std::string runLabel(int id);

private:
    std::vector<RunRecord> m_runs;
    std::map<std::string, Snapshot> m_snapshots;
    std::string m_notes;
    std::vector<std::string> m_equations;
    int m_sessionNumber = 7;
};

} // namespace phy104