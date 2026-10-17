#pragma once 
#include "models.h"
#include <iostream>
#include <string>
#include <vector>
#include <unordered_map>
using std::string;
using std::vector;
using std::unordered_map;
//---------------------------------------------------------------------------------------------------------------------

namespace phy104 {


    class Iformularepository {
        public:
        virtual ~Iformularepository()= default ;
        virtual vector< formulas> loadAll() const =0 ; 
    };
    class Ivariablesrepository{
        public:
        virtual ~Ivariablesrepository() = default ; 
        virtual vector<variables> loadAll() const = 0 ;
    };

    class Iconstantrepository{
        public:
        virtual ~Iconstantrepository() = default ; 
        virtual vector<constant> loadAll() const = 0 ; 

    };
}