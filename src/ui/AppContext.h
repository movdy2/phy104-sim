#pragma once
#include "Repositories.h"
#include "SessionLog.h"

#include <QObject>
#include <QString>
#include <memory>

namespace phy104 {

/// State shared by all pages: the lab session, the optics presets, the formula reference,
/// and the autosave logic. Pages talk to the business layer through this object.
class AppContext : public QObject {
    Q_OBJECT
public:
    AppContext(OpticsPresets presets, std::vector<ReferenceTopic> topics, std::string unitDiscipline,
               std::unique_ptr<ISessionStore> store, QString sessionFile);

    SessionLog log;
    OpticsPresets presets;
    std::vector<ReferenceTopic> topics;
    std::string unitDiscipline;
    QString sessionFile;                         // display name, e.g. "optics_lab.phy"

    /// An experiment page publishes what it currently shows (dashboard cards, export).
    void publish(const Snapshot& snapshot);
    /// "Calculate & Simulate": appends a run to the session and autosaves.
    void recordRun(const QString& experiment, const QString& description, const QString& result, bool valid);
    void setNotes(const QString& markdown);      // autosaves
    /// "Send to Notes": appends a Markdown block to the session notes, saves, and tells the notes page to reload.
    void appendNotes(const QString& markdown);
    /// Writes the .phy file. Returns false (and emits message) on failure.
    bool saveSession();
    QString lastSavedTime() const { return m_lastSaved; }
    /// "RUN 014": the number the next saved calculation will get.
    QString nextRunLabel() const;

signals:
    void snapshotChanged();
    void runsChanged();
    void notesSaved();
    void notesAppended();                        // the notes text was changed from outside the editor
    void message(const QString& text);           // shown in the status bar

private:
    std::unique_ptr<ISessionStore> m_store;
    QString m_lastSaved = "--:--";
};

} // namespace phy104
