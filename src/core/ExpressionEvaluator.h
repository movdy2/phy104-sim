#pragma once
#include <map>
#include <string>
#include <vector>
 using std::string; 
 using std::vector ; 
 using std:: map ; 


namespace phy104 {
    class ExpressionEvaluator{
   using VariableMap = map<string,double>;
    static vector<string> identifiers(const string& expression);
    static bool isFunctionName(const string& name );
    static string normalize(string text);

    };
   

}




