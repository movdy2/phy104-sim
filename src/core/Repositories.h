#pragma once
// Repository interfaces (Dependency Inversion). The business layer only knows
// these; the data layer provides JSON implementations; tests provide fakes.
#include "Models.h"
#include "StudyHub.h"
#include "SessionLog.h"
#include <vector>

namespace phy104 {

class IFormulaRepository {
public:
    virtual ~IFormulaRepository() = default;
    virtual std::vector<Formula> loadAll() const = 0;
};

class IVariableRepository {
public:
    virtual ~IVariableRepository() = default;
    virtual std::vector<Variable> loadAll() const = 0;
};

class IChapterRepository {
public:
    virtual ~IChapterRepository() = default;
    virtual std::vector<Chapter> loadAll() const = 0;
};

class IConstantsRepository {
public:
    virtual ~IConstantsRepository() = default;
    virtual std::vector<PhysicsConstant> loadAll() const = 0;
};

class IPresetsRepository {
public:
    virtual ~IPresetsRepository() = default;
    virtual OpticsPresets load() const = 0;
};

class IReferenceRepository {
public:
    virtual ~IReferenceRepository() = default;
    virtual std::vector<ReferenceTopic> loadAll() const = 0;
    virtual std::string unitDiscipline() const = 0;
};

/// The "Study Hub": chapters with a summary, the rules (formula + why it holds) and their constants.
class IStudyHubRepository {
public:
    virtual ~IStudyHubRepository() = default;
    virtual std::vector<StudyTopic> loadAll() const = 0;
};

/// Saves/loads the lab session (runs + notes) - the "optics_lab.phy" file.
class ISessionStore {
public:
    virtual ~ISessionStore() = default;
    virtual bool save(const SessionLog& log) const = 0;
    virtual bool load(SessionLog& log) const = 0;
};

class IKeyboardRepository {
public:
    virtual ~IKeyboardRepository() = default;
    virtual std::vector<KeyboardGroup> loadAll() const = 0;
    virtual std::vector<ScientificPage> scientific() const = 0;
};

} // namespace phy104
