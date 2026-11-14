#pragma once
// JSON implementations of the repository interfaces declared in core/Repositories.h
#include "Repositories.h"
#include <QMap>
#include <QString>

namespace phy104 {

class JsonFormulaRepository : public IFormulaRepository {
public:
    explicit JsonFormulaRepository(QString path) : m_path(std::move(path)) {}
    std::vector<Formula> loadAll() const override;
private:
    QString m_path;
};

class JsonVariableRepository : public IVariableRepository {
public:
    explicit JsonVariableRepository(QString path) : m_path(std::move(path)) {}
    std::vector<Variable> loadAll() const override;
private:
    QString m_path;
};

class JsonChapterRepository : public IChapterRepository {
public:
    explicit JsonChapterRepository(QString path) : m_path(std::move(path)) {}
    std::vector<Chapter> loadAll() const override;
private:
    QString m_path;
};

class JsonConstantsRepository : public IConstantsRepository {
public:
    explicit JsonConstantsRepository(QString path) : m_path(std::move(path)) {}
    std::vector<PhysicsConstant> loadAll() const override;
private:
    QString m_path;
};

class JsonPresetsRepository : public IPresetsRepository {
public:
    explicit JsonPresetsRepository(QString path) : m_path(std::move(path)) {}
    OpticsPresets load() const override;
private:
    QString m_path;
};

class JsonReferenceRepository : public IReferenceRepository {
public:
    explicit JsonReferenceRepository(QString path) : m_path(std::move(path)) {}
    std::vector<ReferenceTopic> loadAll() const override;
    std::string unitDiscipline() const override;
private:
    QString m_path;
};

/// data/study_hub.json - a JSON array of chapters (order = course order, no map: the names are the titles).
class JsonStudyHubRepository : public IStudyHubRepository {
public:
    explicit JsonStudyHubRepository(QString path) : m_path(std::move(path)) {}
    std::vector<StudyTopic> loadAll() const override;
private:
    QString m_path;
};

/// Separate user-writable JSON file: chapter key -> full note text and its optional summary.
/// Returns false for malformed/unsupported files without modifying `notes`, so user data is not silently discarded.
bool loadStudyHubNotes(const QString& path, QMap<QString, StudyHubNote>& notes, QString* error = nullptr);
/// Refuses to overwrite an existing malformed/unsupported notes file.
bool saveStudyHubNotes(const QString& path, const QMap<QString, StudyHubNote>& notes, QString* error = nullptr);

/// The ".phy" session file: JSON with the saved runs and the session notes.
class JsonSessionStore : public ISessionStore {
public:
    explicit JsonSessionStore(QString path) : m_path(std::move(path)) {}
    bool save(const SessionLog& log) const override;
    bool load(SessionLog& log) const override;
    const QString& path() const { return m_path; }
private:
    QString m_path;
};

class JsonKeyboardRepository : public IKeyboardRepository {
public:
    explicit JsonKeyboardRepository(QString path) : m_path(std::move(path)) {}
    std::vector<KeyboardGroup> loadAll() const override;
    std::vector<ScientificPage> scientific() const override;
private:
    QString m_path;
};

} // namespace phy104
