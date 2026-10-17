#pragma once
#include "repo.h"
#include <QString>
#include <vector>
using std:: vector ; 
using std::move ; 

namespace phy 104 {

    class JsonFormulaRepository :: Iformularepository {
        public:
        explicit JsonFormulaRepository(QString path) ; m_path(move(path)){}
        vector<Formula>loadAll const override; 
        private:
        QString m_path ; 
    }; 
      class JsonvariablesrRepository :: Ivariablesrepository {
        public:
        explicit JsonvariablesrRepository(QString path) ; m_path(move(path)){}
        vector<variablesr>loadAll const override; 
        private:
        QString m_path ; 
    }; 
  class JsonConstantsRepository :: Iconstantrepository {
        public:
        explicit JsonConstantsRepository(QString path) ; m_path(move(path)){}
        vector<constant>loadAll const override; 
        private:
        QString m_path ; 
    }; 



}