#pragma once
#include "models.h"
#include "repo.h"
#include <memory>
#include <optional>
#include <string>
#include <vector>
#include <utility> 

using std::unique_ptr;
using std::string;
using std::vector;
using std::optional;

namespace phy104 {

class formulasolver {
public:
    explicit formulasolver(unique_ptr<Iformularepository> repo);

    const vector<formulas>& getAllFormulas() const { return m_formulas; }
    
    vector<string> chapters() const; 
    vector<const formulas*> bychapter(const string& chapter) const; 
    
    static optional<string> validate(const formulas& formula);

private:
    vector<formulas> m_formulas; 
};

} 