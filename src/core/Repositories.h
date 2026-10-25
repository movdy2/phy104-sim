#pragma once
#include "Models.h"
#include <iostream>
#include <string>
#include <vector>
#include <unordered_map>
using std::string;
using std::vector;
using std::unordered_map;

//---------------------------------------------------------------------------------------------------------------------

namespace phy104 {

class IFormulaRepository {
public:
    virtual ~IFormulaRepository() = default;
    virtual vector<Formula> loadAll() const = 0;
};

class IVariableRepository {
public:
    virtual ~IVariableRepository() = default;
    virtual vector<Variable> loadAll() const = 0;
};

class IConstantsRepository {
public:
    virtual ~IConstantsRepository() = default;
    virtual vector<PhysicsConstant> loadAll() const = 0;
};

class ISummaryRepository {
public:
    virtual ~ISummaryRepository() = default;
    virtual vector<NoteEntry> loadAll() const = 0;
};

class IKeyboardRepository {
public:
    virtual ~IKeyboardRepository() = default;
    virtual vector<KeyboardGroup> loadAll() const = 0;
};

} // namespace phy104