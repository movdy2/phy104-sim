#pragma once 
#include <iostream>
#include <string>
#include <optional>

using std::string ;
using std::optional;

namespace phy104 :: phycalc {

//   قانون الانعكاس (Law of Reflection)
// الانعكاس الكلي الداخلي (Total Internal Reflection - TIR):** يحدث فقط عند الانتقال من وسط معامل انكساره أعلى إلى أقل 
    struct refractionresult{
     double reflectedDeg = 0.0 ;
     double totalInternalReflection = 0.0 ;
     double refractedDeg = 0.0 ;   
    };

     refractionresult refraction(double n1 , double n2 ,double theta1Deg ):
     optional<double> criticalAngleDeg(double n1 , double n2 );
    
     double refractiveIndex(double speedOfLight , double speedInMedium)
    struct ImageResult {
        bool atInfinity  = false ;
        double imagedistance = 0.0 ; 
        double magnification = 0.0 ; 
        bool real = false ; 
        bool upright = false ; 
        string description = ""; 
    };

    ImageResult imageFormation(double objectdistance , double focalLength );

    double fringeSpacing (double wavelength , double screenDistance , double slitSeparation);
    double fringePosition (int order , double wavelength , double screenDistance ,double slitSeparation);
    double relativeIntensity (double y , double wavelength ,double slitSeparation);

}