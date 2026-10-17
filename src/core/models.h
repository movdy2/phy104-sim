#pragma once
#include <iostream>
#include <string>
#include <map>
#include <vector>
using std::string;
using std::vector;
using std:: map ; 


//------------------------------------------------
namespace phy104{


    struct variables {
    char  symbol ; 
    string name ; 
    string unit ; 
    string description ; 
    string display ;
    
  };




 struct formulas {
    string id ;
    string name ; 
    string equation ;
    string chapter ; 
    string vector<variables> ; 
    map<string , string> solutions ;

    const variables* findVariable(const string& sym) const {
      for (const auto& v : variables)
            if (v.symbol == sym ) return &v;
      return nullptr;
    }
   
   };

 struct constant {
    char symbol ;
    string name ; 
    string unit ; 
    double value = 0.0 ;
    string description; 
 };


}
