#pragma once

#include "Repositories.h"
#include <QString>
#include <vector>
#include <utility>

using std::vector;
using std::move;

namespace phy104 {

class JsonFormulaRepository : public IFormulaRepository {
public:
    explicit JsonFormulaRepository(QString path) : m_path(move(path)) {}
    vector<Formula> loadAll() const override;
private:
    QString m_path;
};

class JsonVariableRepository : public IVariableRepository {
public:
    explicit JsonVariableRepository(QString path) : m_path(move(path)) {}
    vector<Variable> loadAll() const override;
private:
    QString m_path;
};

class JsonConstantsRepository : public IConstantsRepository {
public:
    explicit JsonConstantsRepository(QString path) : m_path(move(path)) {}
    vector<PhysicsConstant> loadAll() const override;
private:
    QString m_path;
};

class JsonSummaryRepository : public ISummaryRepository {
public:
    explicit JsonSummaryRepository(QString path) : m_path(move(path)) {}
    vector<NoteEntry> loadAll() const override;
private:
    QString m_path;
};

class JsonKeyboardRepository : public IKeyboardRepository {
public:
    explicit JsonKeyboardRepository(QString path) : m_path(move(path)) {}
    vector<KeyboardGroup> loadAll() const override;
private:
    QString m_path;
};

} // namespace phy104