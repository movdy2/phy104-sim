#pragma once
#include "Repositories.h"
#include <QString>
#include <vector>
#include <utility>

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

class JsonSummaryRepository : public ISummaryRepository {
public:
    explicit JsonSummaryRepository(QString path) : m_path(std::move(path)) {}
    std::vector<NoteEntry> loadAll() const override;
private:
    QString m_path;
};

class JsonKeyboardRepository : public IKeyboardRepository {
public:
    explicit JsonKeyboardRepository(QString path) : m_path(std::move(path)) {}
    std::vector<KeyboardGroup> loadAll() const override;
private:
    QString m_path;
};

class JsonSessionStore : public ISessionStore {
public:
    explicit JsonSessionStore(QString path) : m_path(std::move(path)) {}
    bool save(const SessionLog& log) const override;
    bool load(SessionLog& log) const override;
private:
    QString m_path;
};

} // namespace phy104