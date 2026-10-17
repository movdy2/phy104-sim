#include <vector>
#include <algorithm>
#include "formulasolver.h"
#include <map>


using std::string;
using std::vector;
using std::unordered_map;
using std:: unique_ptr ;
using std:: find;
using std:: map ;


//------------------------------------


namespace phy104 {

    formulasolver :: formulasolver (unique_ptr<Iformularepository> repo ){
        if (repo ) {
            m_formulas = repo-> loadAll();
        }
    }

    vector<string> formulasolver ::chapters() const{
        vector<string> result;
        for(const auto& f : m_formulas){

            if (find(result.begin(),result.end(),f.chapter)==result.end()){
                result.push_back(f.chapter);
            }
        }
        return result ;
    }
    vector<const formulas*> formulasolver::bychapter(const string& chapter) const{
        vector<const formula*>result;
        for (const auto& f : m_formulas){
            if(f.chapter == chapter){
                result.push_back(&f);
            }

        }
        return result  ;
    }



   optional<string> formulasolver::validate(const formulas& formula) {
   
    return std::nullopt;
}
}