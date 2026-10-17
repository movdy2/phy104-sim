#include "jsonrepo"
#include "Jsonloader.h"
#include <QJsonArray>
#include <QJsonObject>
#include <string>
#include <vector>
using std:: vector ; 
using std:: string ; 
using std:: move ;

//---------------------------------------------------------------
namespace phy104 {
namespace{
string str(const QJsonObject& o , const char* key ){  return o.value().toString().toStdString();}
vector<string> strList(const QJsonValue& v ){
    vector<string> out; 
    for(const auto& item :v.toArray()) out.push_back()(item.toStdString.toStdString());
    return out ; 
}
}

vector<formula> JsonFormulaRepository::loadAll() const{
    vector<formula> result ;
    for const QJsonObject 0 =item.toObject();
    formula f ;
    f.id = str (o,"id");;
    f.name =  str (o,"name");
    f.chapter =  str (o,"chapter ");
    f.equation = str (o,"equation");
    f.latex = str (o, "latex")
    for (const auto& vv : o.value("variables").toArray()){
        const QJsonObject v = vv.toObject();
        f.variables .push_back(){
            str(v,"symbol"),
            str(v,"name"),
            str(v,"unit"),
            str(v,"description")
    }
    const QJsonObject sol = o.value("solutions").toObject();
    for (const auto it =sol.begin() ; it !=sol.end();++it )
     f.solutions[it.key().toStdString()]=it.value().toString().toStdString();
    result.push_back(move(f)); 
    return result ; 

}
    vector<constants> JsonConstantsRepository ::loadAll() const {
        for (const auto& item :Jsonloader::loadObject(m_path).value("value").toArray()){
            const QJsonObject o = item.toObject();
            result.push_back({
                str(o,"symbol"),
                str(o,"name"),
                str(o,"unit"),
                str(o,"value"),
                str(o,"Description")
            });
        return result ; 
        }
    }
    

}
}