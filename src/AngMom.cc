
#include "AngMom.hh"
#include <cmath>
#include <iostream>


namespace AngMom
{
 
 double fct(double x)
 {
    if (x<0) return 0;
    double f=1;
    for (double g=2;g<=x;g=g+1) {f *= g;}
    return f;
 }
 
 double Tri(double j1, double j2, double j3)
 {
    return fct(j1+j2-j3) * fct(j1-j2+j3) * fct(-j1+j2+j3)/fct(j1+j2+j3+1);
 }
 
 //#####################################################################
 //  Wigner 6-J symbol { j1 j2 j3 }  See definition and Racah formula at
 //                    { J1 J2 J3 }  http://mathworld.wolfram.com/Wigner6j-Symbol.html
 double SixJ(double j1, double j2, double j3, double J1, double J2,double J3)
 {
   
   double triads[4][3] = {{j1,j2,j3},{j1,J2,J3},{J1,j2,J3},{J1,J2,j3}};
   
   for (int i=0;i<4;i++)
   {
      if ( (triads[i][0]+triads[i][1]<triads[i][2])
        || (triads[i][0]-triads[i][1]>triads[i][2])
        || ((int)(2*(triads[i][0]+triads[i][1]+triads[i][2]))%2>0) )
          return 0;
   } 
   double sixj = 0;
   for (double t=j1+j2+j3; t<=(j1+j2+j3+J1+J2+J3); t+=1)
   {
      double ff = fct(t-j1-j2-j3)*fct(t-j1-J2-J3)*
                 fct(t-J1-j2-J3)*fct(t-J1-J2-j3)*
                 fct(j1+j2+J1+J2-t)*fct(j2+j3+J2+J3-t)*
                 fct(j3+j1+J3+J1-t) ;
      if (ff>0)
      {
         sixj += pow(-1,t) * fct(t+1)/ff;
      }
   
   }
   for (int i=0;i<4;i++)
   {
      sixj *= sqrt(Tri( triads[i][0],triads[i][1],triads[i][2]));
   }
   return sixj;
 }


double NineJ(double j1,double j2, double J12, double j3, double j4, double J34, double J13, double J24, double J)
{
   float ninej = 0;
   int ph = 1-2*(abs(int(J-j1))%2);
   for (float g = fabs(J-j1); g<=J+j1; g+=1)
   {
      ninej +=  ph * (2*g+1)
                * SixJ(j1,j2,J12,J34,J,g)
                * SixJ(j3,j4,J34,j2,g,J24)
                * SixJ(J13,J24,J,g,j1,j3);
   }
   return ninej;
}

}