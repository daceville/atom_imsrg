#include "imsrg_util.hh"
#include "AngMom.hh"
#include <boost/multiprecision/cpp_bin_float.hpp>
#include <boost/math/special_functions/gamma.hpp>
#include <boost/multiprecision/float128.hpp>
#include <boost/implicit_cast.hpp>
#include <gsl/gsl_integration.h>
#include <list>
#include <cmath>
#include <algorithm>
#include <stdlib.h>
#include <math.h>
#include <gsl/gsl_math.h>
#include <gsl/gsl_monte.h>
#include <gsl/gsl_monte_vegas.h>
#include <gsl/gsl_sf_laguerre.h>
#include <gsl/gsl_sf_legendre.h>
#include <gsl/gsl_sf_gamma.h>
#include <gsl/gsl_sf_coupling.h>
#include <gsl/gsl_sf_hyperg.h>

#include "cubature.h"
#include "hcubature.c"


using namespace AngMom;

/// imsrg_util namespace. Used to define some helpful functions.
namespace imsrg_util
{


 Operator OperatorFromString(ModelSpace& modelspace, string opname)
 {
           if (opname == "R2_p1")         return R2_1body_Op(modelspace,"proton") ;
      else if (opname == "R2_p2")         return R2_2body_Op(modelspace,"proton") ;
      else if (opname == "R2_n1")         return R2_1body_Op(modelspace,"neutron") ;
      else if (opname == "R2_n2")         return R2_2body_Op(modelspace,"neutron") ;
      else if (opname == "Rp2")           return Rp2_corrected_Op(modelspace,modelspace.GetTargetMass(),modelspace.GetTargetZ()) ;
      else if (opname == "Rn2")           return Rn2_corrected_Op(modelspace,modelspace.GetTargetMass(),modelspace.GetTargetZ()) ;
      else if (opname == "Rm2")           return Rm2_corrected_Op(modelspace,modelspace.GetTargetMass(),modelspace.GetTargetZ()) ;
      else if (opname == "E2")            return ElectricMultipoleOp(modelspace,2) ;
      else if (opname == "E4")            return ElectricMultipoleOp(modelspace,4) ;
      else if (opname == "E6")            return ElectricMultipoleOp(modelspace,6) ;
      else if (opname == "M1")            return MagneticMultipoleOp(modelspace,1) ;
      else if (opname == "M3")            return MagneticMultipoleOp(modelspace,3) ;
      else if (opname == "M5")            return MagneticMultipoleOp(modelspace,5) ;
      else if (opname == "M1p")           return MagneticMultipoleOp_pn(modelspace,1,"proton") ;
      else if (opname == "M1n")           return MagneticMultipoleOp_pn(modelspace,1,"neutron") ;
      else if (opname == "Fermi")         return AllowedFermi_Op(modelspace) ;
      else if (opname == "GamowTeller")   return AllowedGamowTeller_Op(modelspace) ;
      else if (opname == "Iso2")          return Isospin2_Op(modelspace) ;
      else if (opname == "R2CM")          return R2CM_Op(modelspace) ;
      else if (opname == "HCM")           return HCM_Op(modelspace) ;
      else if (opname == "Rso")           return RpSpinOrbitCorrection(modelspace) ;
      else if (opname == "RadialOverlap") return RadialOverlap(modelspace);
      else if (opname == "Sigma")         return Sigma_Op(modelspace);
      else if (opname == "Sigma_p")         return Sigma_Op_pn(modelspace,"proton");
      else if (opname == "Sigma_n")         return Sigma_Op_pn(modelspace,"neutron");
      else if (opname.substr(0,4) == "HCM_") // GetHCM with a different frequency, ie HCM_24 for hw=24
      {
         double hw_HCM; // frequency of trapping potential
         istringstream(opname.substr(4,opname.size())) >> hw_HCM;
         int A = modelspace.GetTargetMass();
         return TCM_Op(modelspace) + 0.5*A*M_NUCLEON*hw_HCM*hw_HCM/HBARC/HBARC*R2CM_Op(modelspace);
      }
      else if (opname.substr(0,4) == "Rp2Z") // Get point proton radius for specified Z, e.g. Rp2Z10 for neon
      {
        int Z_rp;
        istringstream(opname.substr(4,opname.size())) >> Z_rp;
        return Rp2_corrected_Op(modelspace,modelspace.GetTargetMass(),Z_rp) ;
      }
      else if (opname.substr(0,4) == "Rn2Z") // Get point neutron radius for specified Z
      {
        int Z_rp;
        istringstream(opname.substr(4,opname.size())) >> Z_rp;
        return Rn2_corrected_Op(modelspace,modelspace.GetTargetMass(),Z_rp) ;
      }
      else if (opname.substr(0,4) == "rhop") // point radius density at position r, e.g. rhop1.25
      {
        double rr;
        istringstream(opname.substr(4,opname.size())) >> rr;
        return ProtonDensityAtR(modelspace,rr);
      }
      else if (opname.substr(0,4) == "rhon") // point radius density at position r
      {
        double rr;
        istringstream(opname.substr(4,opname.size())) >> rr;
        NeutronDensityAtR(modelspace,rr);
      }
      else if (opname.substr(0,6) == "OneOcc") // Get occupation of specified orbit, e.g. OneOccp1p3
      {
         map<char,int> lvals = {{'s',0},{'p',1},{'d',2},{'f',3},{'g',4},{'h',5}};
         char pn,lspec;
         int n,l,j,t;
         istringstream(opname.substr(6,1)) >> pn;
         istringstream(opname.substr(7,1)) >> n;
         istringstream(opname.substr(8,1)) >> lspec;
         istringstream(opname.substr(9,opname.size())) >> j;
         l = lvals[lspec];
         t = pn == 'p' ? -1 : 1;
         return NumberOp(modelspace,n,l,j,t) ;
      }
      else if (opname.substr(0,6) == "AllOcc") // Get occupation of orbit, summed over all values of radial quantum number n, e.g. AllOccpp3
      {
         map<char,int> lvals = {{'s',0},{'p',1},{'d',2},{'f',3},{'g',4},{'h',5}};
         char pn,lspec;
         int l,j,t;
         istringstream(opname.substr(6,1)) >> pn;
         istringstream(opname.substr(7,1)) >> lspec;
         istringstream(opname.substr(8,opname.size())) >> j;
         l = lvals[lspec];
         t = pn == 'p' ? -1 : 1;
         return NumberOpAlln(modelspace,l,j,t) ;
      }
      else if (opname.substr(0,9) == "protonFBC") // Fourier bessel coefficient of order nu
      {
         int nu;
         istringstream(opname.substr(9,opname.size())) >> nu;
         return FourierBesselCoeff( modelspace, nu, 8.0, modelspace.proton_orbits);
      }
      else if (opname.substr(0,10) == "neutronFBC") // Fourier bessel coefficient of order nu
      {
         int nu;
         istringstream(opname.substr(10,opname.size())) >> nu;
         return FourierBesselCoeff( modelspace, nu, 8.0, modelspace.neutron_orbits) ;
      }
      else //need to remove from the list
      {
         cout << "Unknown operator: " << opname << endl;
      }
      return Operator();

 }

 Operator NumberOp(ModelSpace& modelspace, int n, int l, int j2, int tz2)
 {
   Operator NumOp = Operator(modelspace);
   int indx = modelspace.Index1(n,l,j2,tz2);
   NumOp.ZeroBody = 0;
   NumOp.EraseOneBody();
   NumOp.EraseTwoBody();
   NumOp.OneBody(indx,indx) = 1;
   return NumOp;
 }

 Operator NumberOpAlln(ModelSpace& modelspace, int l, int j2, int tz2)
 {
   Operator NumOp = Operator(modelspace);
   for (int n=0;n<=(modelspace.GetEmax()-l)/2;++n)
   {
     int indx = modelspace.Index1(n,l,j2,tz2);
     NumOp.OneBody(indx,indx) = 1;
   }
   return NumOp;
 }

 double HO_density(int n, int l, double hw, double r)
 {
    double v = M_NUCLEON * hw / (HBARC*HBARC);
    double Norm = pow(v/2.,1.5+l) * M_SQRT2/M_SQRTPI * pow(2,n+2*l+3) * gsl_sf_fact(n) / gsl_sf_doublefact(2*n + 2*l + 1);
    double L = gsl_sf_laguerre_n(n, l+0.5, v*r*r);
    double rho = Norm * pow(r,2*l) * exp(-v * r*r) * L * L;
    return rho;
 }

double HO_Radial_psi(int n, int l, double hw, double r)
{
   double b = sqrt( (HBARC*HBARC) / (hw * M_NUCLEON) );
   double x = r/b;
   double Norm = 2*sqrt( gsl_sf_fact(n) * pow(2,n+l) / M_SQRTPI / gsl_sf_doublefact(2*n+2*l+1) / pow(b,3.0) );
   double L = gsl_sf_laguerre_n(n,l+0.5,x*x);
   double psi = Norm * pow(x,l) * exp(-x*x*0.5) * L;
   return psi;
}

 // Just do the HF transformation
 vector<double> GetOccupationsHF(HartreeFock& hf)
 {
    ModelSpace* modelspace = hf.Hbare.modelspace;
    int norb = modelspace->GetNumberOrbits();
    vector<double> occupation(norb);

    for (int i=0; i<norb; ++i)
    {
      Orbit & oi = modelspace->GetOrbit(i);
      // Get the number operator for orbit i
      Operator N_bare = NumberOp(*modelspace,oi.n,oi.l,oi.j2,oi.tz2);
      // Transform it to the normal-ordered HF basis
      Operator N_NO = hf.TransformToHFBasis(N_bare).DoNormalOrdering();
      occupation[i] = N_NO.ZeroBody;
      cout << oi.n << " " << oi.l << " " << oi.j2 << "/2 " << occupation[i] << endl;
    }
    return occupation;
 }

 // Do the full IMSRG transformation
 vector<double> GetOccupations(HartreeFock& hf, IMSRGSolver& imsrgsolver)
 {
    ModelSpace* modelspace = imsrgsolver.modelspace;
    int norb = modelspace->GetNumberOrbits();
    vector<double> occupation(norb,0);

    for (int i=0; i<norb; ++i)
    {
      Orbit & oi = modelspace->GetOrbit(i);
      // Get the number operator for orbit i
      Operator N_bare = NumberOp(*modelspace,oi.n,oi.l,oi.j2,oi.tz2);
      // Transform it to the normal-ordered HF basis
      Operator N_NO = hf.TransformToHFBasis(N_bare).DoNormalOrdering();
      // Transform to the imsrg evolved basis
      Operator N_final = imsrgsolver.Transform(N_NO);
      occupation[i] = N_final.ZeroBody;
    }
    return occupation;
 }

 vector<double> GetDensity( vector<double>& occupation, vector<double>& R, vector<int>& orbits, ModelSpace& modelspace )
 {
     int nr_steps = R.size();
     double hw = modelspace.GetHbarOmega();
     vector<double> dens(nr_steps,0);
     for (int& i : orbits)
     {
       Orbit & oi = modelspace.GetOrbit(i);
       for (int ir=0;ir<nr_steps;++ir)
       {
          dens[ir] += HO_density(oi.n,oi.l,hw,R[ir]) * occupation[i];
       }
     }
     return dens;
 }

 Operator Single_Ref_1B_Density_Matrix(ModelSpace& modelspace)
 {
    Operator DM(modelspace,0,0,0,2);
//    for (index_t a : modelspace.holes)
    for (auto& a : modelspace.holes)
    {
//       index_t a = it_a.first;
       DM.OneBody(a,a) = 1.0;
    }
    return DM;
 }

 double Get_Charge_Density( Operator& DM, double r)
 {
   ModelSpace* modelspace = DM.GetModelSpace();
   double hw = modelspace->GetHbarOmega();
   double rho=0;
   for (index_t i : modelspace->proton_orbits )
   {
      Orbit& oi = modelspace->GetOrbit(i);
      for ( index_t j : DM.OneBodyChannels[{oi.l,oi.j2,oi.tz2}] )
      {
        if (abs(DM.OneBody(i,j))<1e-7) continue;
        Orbit& oj = modelspace->GetOrbit(j);
        rho += DM.OneBody(i,j) * HO_Radial_psi(oi.n, oi.l, hw, r) * HO_Radial_psi(oj.n, oj.l, hw, r);
      }
   }
   return rho;
 }

void GenerateRadialIntegrals(ModelSpace& modelspace, int ind)
{
    cout << "Entering GenerateRadialIntegrals for ind=" << ind << endl;
    //cout << "Resizing radList; size=" << ind << endl;
    //modelspace.radList.resize(ind);
    int l2max = ind % 100;
    ind = (ind-l2max) / 100;
    int l1max = ind % 100;
    ind = (ind-l1max) / 100;
    int n2max = ind % 100;
    ind = (ind-n2max) / 100;
    int n1max = ind % 100;
    cout << "n1max=" << n1max << " l1max=" << l1max << " n2max=" << n2max << " l2max=" << l2max << endl;
    int norb = modelspace.norbits;
    //#pragma omp parallel for
    for (int n1=0; n1<=n1max; n1++) // not thread safe?
    {
	//Orbit& o1 = modelspace.GetOrbit(i);
	for (int l1=0; l1<=l1max; l1++)
	{
	    for (int n2=0; n2<=n2max; n2++)
	    {
		//Orbit& o1 = modelspace.GetOrbit(i);
		for (int l2=0; l2<=l2max; l2++)
		{
		    int ind1 = 1e6*n1+1e4*n2+1e2*l1+l2;
		    //int ind2 = 1000*n2+100*n1+10*l2+l1;
		    //if (modelspace.radList.size() < max(ind1,ind2)) modelspace.radList.resize(max(ind1,ind2));
		    //if (modelspace.radList.size() < ind1) modelspace.radList.resize(ind1);
		    //if (modelspace.radList[1000*n1+100*n2+10*l1+l2] != 0 or modelspace.radList[1000*n2+100*n1+10*l2+l1] != 0)
		    //{


			if ( modelspace.radList[ind1] != 0 ) continue;
			//cout << "Generating radial integral for ind1=" << ind1 << endl;
			long double rad = RadialIntegral(n1, l1, n2, l2, -1, modelspace);
			//#pragma omp critical
			//{
			    modelspace.radList[ind1] = rad;
			//}
			//modelspace.radList[ind2] = rad;
			//cout << "Generated, moving on." << endl;
		    //}
		}
	    }
	    //Orbit& o2 = modelspace.GetOrbit(j);
	    /*if (modelspace.radList[1000*o1.n+100*o2.n+10*o1.l+o2.l] != 0 or modelspace.radList[1000*o2.n+100*o1.n+10*o2.l+o1.l] != 0)
	    {
		int rad = RadialIntegral(o1.n, o1.l, o2.n, o2.l, -1, modelspace);
		modelspace.radList[1000*o1.n+100*o2.n+10*o1.l+o2.l] = rad;
		modelspace.radList[1000*o2.n+100*o1.n+10*o2.l+o1.l] = rad;
	    }*/
	}
    }
    cout << "Exiting GenerateRadialIntegrals." << endl;
}

double getRadialIntegral(int n1, int l1, int n2, int l2, ModelSpace& modelspace)
{
    unsigned long long int ind = 0;
    if (n2 < n1) ind = 1e6*n2 + 1e2*l2 + 1e4*n1 + l1;
    else ind = 1e6*n1 + 1e2*l1 + 1e4*n2 + l2;
    // Should be good for lmax < 10; could use some ordering to reduce the number of possiblities further
    double rad;
    auto it = modelspace.radList.find(ind);
    if ( it != modelspace.radList.end() )  return modelspace.radList[ind];
    cout << "Didn't find " << ind << " in rad list, making a new one" << endl;
    //#pragma omp critical
    //{
	rad = RadialIntegral(n1, l1, n2, l2, -1, modelspace);
	cout << "Calculated, writing to radList." << endl;
	modelspace.radList[ind] = rad;
    //}
    cout << "Radial integral made, returning." << endl;
    return rad;
}

double fact_int(int n, double alpha)
{
	return gsl_sf_fact(n)/pow(alpha,n+1);
}

double N_slater(int n, double zeta)
{
	return pow(2*zeta,n)*sqrt(2*zeta/gsl_sf_fact(2*n));
}

Operator SlaterOneBody(ModelSpace& modelspace)
{
        Operator op(modelspace);
        double t_start = omp_get_wtime();
        int norbits = modelspace.GetNumberOrbits();
        double z = modelspace.GetHbarOmega()/10;
        //#pragma omp parallel for
        for (int i=0; i<norbits; i++)
        {
                Orbit oi = modelspace.GetOrbit(i);
                for (int j=0; j<=i; j++)
                {
                        Orbit oj = modelspace.GetOrbit(j);
                        if (oi.l != oj.l) continue; // Orthogonal in l!
                        double T = N_slater(oi.n,z)*N_slater(oj.n,z)*pow(HBARC,2)*0.5/511000*( (oi.l*(oi.l+1)-oi.n*(oi.n-1))*fact_int(oi.n+oj.n-2,2*z) + 2*z*oi.n*fact_int(oi.n+oj.n-1,2*z) - z*z*fact_int(oi.n+oj.n,2*z) );
			double V = N_slater(oi.n,z)*N_slater(oj.n,z)*HBARC*(1./137)*modelspace.GetTargetZ()*(fact_int(oi.n+oj.n-1,2*z));
			cout << "T = " << T << endl;
			cout << "V = " << V << endl;
			cout << "Writing " << T-V << " for oi.n=" << oi.n << " oj.n=" << oj.n << " oi.l=" << oi.l << " oj.l=" << oj.l << endl;
                        op.OneBody(i,j) = T - V;
                        op.OneBody(j,i) = T - V;
                }
        }
        op.profiler.timer["SlaterOneBody"] += omp_get_wtime() - t_start;
        return op;
}

Operator CSOneBody(ModelSpace& modelspace)
{
	Operator op(modelspace);
	double t_start = omp_get_wtime();
	double Ha = 1; // 27.21138602; // 1 Hartree; should this be ~hw?
	double b = modelspace.GetHbarOmega();
	int Z = modelspace.GetTargetZ();
	int norbits = modelspace.GetNumberOrbits();

	#pragma omp parallel for
	for (int i=0; i<norbits; i++)
	{
		Orbit oi = modelspace.GetOrbit(i);
		for (int j=0; j<=i; j++)
		{
			Orbit oj = modelspace.GetOrbit(j);
			// oi.l == oj.l
			if (oi.l != oj.l || oi.j2 != oj.j2) continue;
			op.OneBody(i,j) = 0;
			double d2_dr2 = 0;
			double del = 0;
			double rd_dr = 0;
			double L2 = 0;
			double V = 0;
			if (oi.l == oj.l) 
			{
				int l = oi.l;
				// float j = oi.j2*1./2;
				int np= oi.n;
				int n = oj.n;

				if (np >= n+1) // del term
				{
					double num = (4.*n + 4.*l + 6.);
					double den = (2.*l + 3.);
					del = -num/den * sqrt( gsl_sf_fact(np)*gsl_sf_fact(n+2*l+2)*1./( gsl_sf_fact(n)*gsl_sf_fact(np+2*l+2) ) );
				} else if (np == n) {
					del = -( 4.0*n + 2.0*l + 3.0 ) / ( 2.0*l + 3.0 ); // Fucking ints masquerading as doubles...
				} else {
                                        double num = (4.*np + 4.*l + 6.);
                                        double den = (2.*l + 3.);
                                        del = -num/den * sqrt( gsl_sf_fact(n)*gsl_sf_fact(np+2*l+2)*1./( gsl_sf_fact(np)*gsl_sf_fact(n+2*l+2) ) );
				} // del term

				if (np >= n+1) // d2_dr2 term
				{
					double num = (2*l+4)*(2*l+1)*n - 2*l*(2*l+3)*np + (2*l+3)*(2*l+2);
					double den = (2*l+3) * (2*l+1);
					d2_dr2 = -num/den * sqrt( gsl_sf_fact(np)*gsl_sf_fact(n+2*l+2)*1./(gsl_sf_fact(n)*gsl_sf_fact(np+2*l+2)) );
				} else if (np == n) {
					double num = 4*n*(l+1) + 2*l + 3;
					double den = (2*l+3) * (2*l+1);
					d2_dr2 = -num/den;
				} else {
					double num = (2*l+4)*(2*l+1)*np - 2*l*(2*l+3)*n	+ (2*l+3)*(2*l+2);
                                        double den = (2*l+3) * (2*l+1);
                                        d2_dr2 = -num/den * sqrt( gsl_sf_fact(n)*gsl_sf_fact(np+2*l+2)*1./(gsl_sf_fact(np)*gsl_sf_fact(n+2*l+2)) );
				} // d2_dr2

				if (np >= n) //  1/r^2 term
				{
					double num = 2*np*(2*l+3) - 2*n*(2*l+1) + 4*l + 6;
					double den = (l+1) * (2*l+1) * (2*l+3);
					L2 = num/den * sqrt( gsl_sf_fact(np)*gsl_sf_fact(n+2*l+2)*1./(gsl_sf_fact(n)*gsl_sf_fact(np+2*l+2)) ) * l*(l+1);
				} else {
                                        double num = 2*n*(2*l+1) - 2*np*(2*l+3)	+ 4*l + 6;
                                        double den = (l+1) * (2*l+1) * (2*l+3);
                                        L2 = num/den * sqrt( gsl_sf_fact(n)*gsl_sf_fact(np+2*l+2)*1./(gsl_sf_fact(np)*gsl_sf_fact(n+2*l+2)) ) * l*(l+1);
				} // 1/r^2 term

				if (np >= n) // 1/r term
				{
					V = sqrt( (gsl_sf_fact(np)*gsl_sf_fact(n+2*l+2))/ ( gsl_sf_fact(n) * gsl_sf_fact(np+2*l+2) ) ) / (l+1.);
				} else {
					V = sqrt( (gsl_sf_fact(n)*gsl_sf_fact(np+2*l+2))/ ( gsl_sf_fact(np) * gsl_sf_fact(n+2*l+2) ) ) / (l+1.);
				} // 1/r term
				
				// Factors of b from Change of variables
				d2_dr2 = -0.5*(del)*(b*b); 
				V =  -Z*V*b;
				double me = (d2_dr2 + V) * Ha;
				//cout << "Writing T=" << d2_dr2 << " to <" << oi.n << oi.l << oi.j2 << "|O|" << oj.n << oj.l << oj.j2 << ">" << endl;
				//cout << "V=" << V << endl;
				op.OneBody(i,j) = me;
				op.OneBody(j,i) = op.OneBody(i,j);
			} // oi.l == oj.l
			else if (oi.l != oj.l) continue;
			// TODO: IMplement oi.l != oj.j, actually, no ortho in l
		} // int j
	} // int i
	
	op.profiler.timer["CoulombSturmianOneBody"] += omp_get_wtime() - t_start;
	return op;
}

struct cs_RabRcd_params { int na; int la;
			  int nb; int lb;
			  int nc; int lc;
			  int nd; int ld; int lp; double b; };

double cs_Rnl(double r, int n, int l, double b)
{
	double x = 2.*b*r;
	double norm = sqrt( pow(2*b,3) *gsl_sf_fact(n)/gsl_sf_fact(n+2*l+2) );
	return norm * pow(x, l) * exp(-x*0.5) * gsl_sf_laguerre_n(n, 2*l+2, x);
}

int cs_RabRcd(unsigned ndim, const double *x, void *fdata, unsigned fdim, double *fval)
{
    (void)(ndim); // Avoid unused parameter warnings;
    (void)(fval);
    struct cs_RabRcd_params * params = (struct cs_RabRcd_params*)fdata;

    int na = (params->na);
    int nb = (params->nb);
    int nc = (params->nc);
    int nd = (params->nd);

    int la = (params->la);
    int lb = (params->lb);
    int lc = (params->lc);
    int ld = (params->ld);

    int lp = (params->lp);

    double b = (params->b);

    double r1 = x[0]/(1-x[0]);
    double r2 = x[1]/(1-x[1]);

    double rmin = min(r1,r2);
    double rmax = max(r1,r2);

    double Ra = cs_Rnl(r1, na, la, b);
    double Rb = cs_Rnl(r2, nb, lb, b);
    double Rc = cs_Rnl(r1, nc, lc, b);
    double Rd = cs_Rnl(r2, nd, ld, b);

    fval[0] = Ra*Rb*Rc*Rd * pow(rmin,lp) *1./ pow(rmax,lp+1) * 1./pow(1-x[0],2) * 1./pow(1-x[1],2) * r1*r1*r2*r2;

    return 0;
}

double csTwoBodyME(Orbit& o1, Orbit& o2, Orbit& o3, Orbit& o4, int J, double b)
{
        double xmin[2] = {0,0};
        double xmax[2] = {1,1};
        int max_iter = 1e4;
        double max_err = 1e-4;

	double me = 0.;
        double me_err = 0.;
        int Lmin = max( abs(o1.j2-o3.j2), abs(o2.j2-o4.j2) )*0.5;
        int Lmax = min( o1.j2+o3.j2, o2.j2+o4.j2 )*0.5;
        for (int L = Lmin; L<= Lmax; L++)
        {
        	if ( (o1.l+o3.l+L)%2 !=0 || (o2.l+o4.l+L)%2 != 0) continue;
                double temp = SixJ( o1.j2*0.5,o2.j2*0.5,J, o4.j2*0.5,o3.j2*0.5,L );
                if (temp == 0)
                {
                	// cout << "SixJs Returned Zero! L=" << L << " J=" << J << endl;
                        // cout << " <(" << o1.n << o1.l << o1.j2 << ")(" << o2.n << o2.l << o2.j2 << ")" << J << "|V|(" << o3.n << o3.l << o3.j2 << ")(" << o4.n << o4.l << o4.j2 << ")>" << endl;
                        continue;
                }
                temp *= ThreeJ( o1.j2*0.5,L,o3.j2*0.5, -0.5,0,0.5) * ThreeJ( o2.j2*0.5,L,o4.j2*0.5, -0.5,0,0.5);
                if (temp == 0)
                {
                	// cout << "ThreeJs returned zero!" << endl;
                        continue;
                }

                struct cs_RabRcd_params int_params = { o1.n,o1.l, o2.n,o2.l, o3.n,o3.l, o4.n,o4.l, L, b};
                double val;
                double err;
                hcubature(1, &cs_RabRcd, &int_params, 2, xmin, xmax, max_iter, 0, max_err, ERROR_INDIVIDUAL, &val, &err);
		/*		
		cout << "L=" << L << endl;
		cout << "val=" << val << endl;
		cout << "sixj=" << SixJ( o1.j2*0.5,o2.j2*0.5,J, o4.j2*0.5,o3.j2*0.5,L ) << endl;
		cout << "3j ac=" << ThreeJ( o1.j2*0.5,L,o3.j2*0.5, -0.5,0,0.5) << endl;
		cout << "3j bd=" << ThreeJ( o2.j2*0.5,L,o4.j2*0.5, -0.5,0,0.5) << endl;
		*/
		me += temp*val;
		me_err += temp*err;
	}
	//cout << "me=" << me << " error=" << me_err << endl;
	return me;
}

struct me_params { int ch;
                   int iket;
                   int jbra;
                   int tbcJ; };


Operator CSTwoBody(ModelSpace& modelspace)
{
	Operator op(modelspace);
	double t_start = omp_get_wtime();
	int nchan = modelspace.GetNumberTwoBodyChannels();
	double b = modelspace.GetHbarOmega();
	double Ha = 1; // 27.21138602;
	vector<me_params> params_vec;
	
	for (int ch=0; ch<=nchan; ch++)
	{
		TwoBodyChannel& tbc = modelspace.GetTwoBodyChannel(ch);
                int nkets = tbc.GetNumberKets();
                if (nkets == 0) continue;
		for (int iket=0; iket < nkets; iket++)
                {
                        // cout << "iket=" << iket << endl;
                        // Ket& ket = tbc.GetKet(iket);
                        for (int jbra=0; jbra <= iket; jbra++)
                        {
				//Ket& bra = tbc.GetKet
				me_params temp = {ch,iket,jbra,tbc.J};
				//cout << "Adding ch=" << ch << " iket=" << iket << " jbra=" << jbra << endl;
				params_vec.push_back(temp);
			} // jbra
		} // iket
	} // ch

	#pragma omp parallel for
	for(unsigned i = 0; i < params_vec.size(); ++i) {
  		me_params temp = params_vec[i];
		int J    = temp.tbcJ;
		int iket = temp.iket;
		int jbra = temp.jbra;
		int ch   = temp.ch;
		TwoBodyChannel& tbc = modelspace.GetTwoBodyChannel(ch);
		Ket& ket = tbc.GetKet(iket);
		Ket& bra = tbc.GetKet(jbra);
		Orbit o3 = *ket.op;
                Orbit o4 = *ket.oq;
		Orbit o1 = *bra.op;
                Orbit o2 = *bra.oq;

		double me = 0.;
                double asym_me = 0.;

                float d12 = 0.;
                float d34 = 0.;
                if ( o1.index == o2.index ) d12 = 1.;
                if ( o3.index == o4.index ) d34 = 1.;

                // cout << "Indices:" << endl;
                // cout << o1.index << o2.index << o3.index << o4.index << endl;
                me = csTwoBodyME(o1, o2, o3, o4, tbc.J, b);
                double me_norm = Ha * sqrt( (o1.j2+1.)*(o2.j2+1.)*(o3.j2+1.)*(o4.j2+1.) ) * pow(-1, (o1.j2+o3.j2)*0.5+tbc.J);
                if ( o3.index != o4.index )
                {
                	asym_me = csTwoBodyME(o1, o2, o4, o3, tbc.J, b) * Ha * sqrt( (o1.j2+1.)*(o2.j2+1.)*(o3.j2+1.)*(o4.j2+1.) ) * pow(-1, (o1.j2+o4.j2)*0.5+tbc.J);
                } else {
                	asym_me = me * Ha * sqrt( (o1.j2+1.)*(o2.j2+1.)*(o3.j2+1.)*(o4.j2+1.) ) * pow(-1, (o1.j2+o4.j2)*0.5+tbc.J);
                }
                me *= me_norm;
                /*
                cout << endl;
                cout << "s me=" << me << endl;
                cout << "a me=" << asym_me << endl;
                */
                me = me - pow(-1,(o3.j2+o4.j2)*0.5-tbc.J) * asym_me;
                double tbme = me * 1./sqrt( (1.+d12)*(1.+d34) );
                //std:stringstream me_string;
                //me_string << o1.index << " " << o2.index << " " << o3.index << " " << o4.index << " " << tbc.J << " " << tbme;
                //cout << me_string.str() << endl;
                //cout << iket << " " << jbra << " " << ch << endl;
                //cout << o1.index << " " << o2.index << " " << o3.index << " " << o4.index << " " << tbc.J << " " << tbme << endl;
                // cout << "Setting tbme to=" << tbme << " <(" << o1.n << o1.l << o1.j2 << ")(" << o2.n << o2.l << o2.j2 << ")" << tbc.J << "|V|(" << o3.n << o3.l << o3.j2 << ")(" << o4.n$
                op.TwoBody.SetTBME(ch, iket, jbra, tbme);
                op.TwoBody.SetTBME(ch, jbra, iket, tbme);
	} // unsigned i

	/*
	#pragma omp parallel for
	for (int ch = 0; ch <= nchan; ch++)
	{
		TwoBodyChannel& tbc = modelspace.GetTwoBodyChannel(ch);
 		int nkets = tbc.GetNumberKets();
		if (nkets == 0) continue;
		for (int iket=0; iket < nkets; iket++)
		{
			// cout << "iket=" << iket << endl;
			Ket& ket = tbc.GetKet(iket);
			Orbit o3 = modelspace.GetOrbit(ket.p);
			Orbit o4 = modelspace.GetOrbit(ket.q);
			for (int jbra=0; jbra <= iket; jbra++)
			{
				Ket& bra = tbc.GetKet(jbra);
				// cout << "jbra=" << jbra << endl;
				Orbit & o1 = modelspace.GetOrbit(bra.p);
				Orbit & o2 = modelspace.GetOrbit(bra.q);
				double me = 0.;
				double asym_me = 0.;

				float d12 = 0.;
                                float d34 = 0.;
                                if ( o1.index == o2.index ) d12 = 1.;
                                if ( o3.index == o4.index ) d34 = 1.;

				// cout << "Indices:" << endl;
				// cout << o1.index << o2.index << o3.index << o4.index << endl;
				me = csTwoBodyME(o1, o2, o3, o4, tbc.J, b);
				double me_norm = Ha * sqrt( (o1.j2+1.)*(o2.j2+1.)*(o3.j2+1.)*(o4.j2+1.) ) * pow(-1, (o1.j2+o3.j2)*0.5+tbc.J);
				if ( o3.index != o4.index )
				{
					asym_me = csTwoBodyME(o1, o2, o4, o3, tbc.J, b) * Ha * sqrt( (o1.j2+1.)*(o2.j2+1.)*(o3.j2+1.)*(o4.j2+1.) ) * pow(-1, (o1.j2+o4.j2)*0.5+tbc.J);
				} else {
					asym_me = me * Ha * sqrt( (o1.j2+1.)*(o2.j2+1.)*(o3.j2+1.)*(o4.j2+1.) ) * pow(-1, (o1.j2+o4.j2)*0.5+tbc.J);
				}
				me *= me_norm;
				
				cout << endl;
				cout << "s me=" << me << endl;
				cout << "a me=" << asym_me << endl;
				
				me = me - pow(-1,(o3.j2+o4.j2)*0.5-tbc.J) * asym_me;
				double tbme = me * 1./sqrt( (1.+d12)*(1.+d34) );
				std:stringstream me_string;
				//me_string << o1.index << " " << o2.index << " " << o3.index << " " << o4.index << " " << tbc.J << " " << tbme;
				//cout << me_string.str() << endl;
				//cout << iket << " " << jbra << " " << ch << endl;
				cout << o1.index << " " << o2.index << " " << o3.index << " " << o4.index << " " << tbc.J << " " << tbme << endl;
				// cout << "Setting tbme to=" << tbme << " <(" << o1.n << o1.l << o1.j2 << ")(" << o2.n << o2.l << o2.j2 << ")" << tbc.J << "|V|(" << o3.n << o3.l << o3.j2 << ")(" << o4.n << o4.l << o4.j2 << ")>" << endl;
				op.TwoBody.SetTBME(ch, iket, jbra, tbme);
				op.TwoBody.SetTBME(ch, jbra, iket, tbme);
				//cout << op.TwoBody.GetTBME_norm(ch, ch, iket, jbra) << endl;
			} // for (int jbra
		} // for (int iket
	} // for (int ch...
	*/
	op.profiler.timer["CoulombSturmianTwoBody"] += omp_get_wtime() - t_start;
	return op;
}

Operator CSRadius(ModelSpace& modelspace)
{
	Operator op(modelspace);
        double t_start = omp_get_wtime();
        double Ha = 1; // 27.21138602; // 1 Hartree; should this be ~hw?
        double b = modelspace.GetHbarOmega();
        int Z = modelspace.GetTargetZ();
        int norbits = modelspace.GetNumberOrbits();

        #pragma omp parallel for
        for (int i=0; i<norbits; i++)
        {
                Orbit oi = modelspace.GetOrbit(i);
                for (int j=0; j<=i; j++)
                {
                        Orbit oj = modelspace.GetOrbit(j);
                        // oi.l == oj.l
                        if (oi.l != oj.l || oi.j2 != oj.j2) continue;
                        op.OneBody(i,j) = 0;
			double R = 0;
                        if (oi.l == oj.l)
                        {
                                int l = oi.l;
                                int np= oi.n;
                                int n = oj.n;
				if (np >= n+2 || np <= n-2) {
					R = 0;
				} else if (np == n+1) {
					R = -0.5*sqrt( (n+1)*(n+2*l+3) );
				} else if (np == n) {
					R = n+l+1.5;
				} else if (np == n-1) {
					R = -0.5*sqrt( (n)*(n+2*l+2) );
				} else { // Shouldn't get here anyway.
					R = 0;
				}
			}
			op.OneBody(i,j) = R*1./b;
			op.OneBody(j,i) = op.OneBody(i,j);
		}
	}
	op.profiler.timer["CoulombRadius"] += omp_get_wtime() - t_start;
	return op;
}

Operator CSRadiusSquared(ModelSpace& modelspace)
{
        Operator op(modelspace);
        double t_start = omp_get_wtime();
        double Ha = 1; // 27.21138602; // 1 Hartree; should this be ~hw?
        double b = modelspace.GetHbarOmega();
        int Z = modelspace.GetTargetZ();
        int norbits = modelspace.GetNumberOrbits();

        #pragma omp parallel for
        for (int i=0; i<norbits; i++)
        {
                Orbit oi = modelspace.GetOrbit(i);
                for (int j=0; j<=i; j++)
                {
                        Orbit oj = modelspace.GetOrbit(j);
                        // oi.l == oj.l
                        if (oi.l != oj.l || oi.j2 != oj.j2) continue;
                        op.OneBody(i,j) = 0;
                        double R2 = 0;
                        if (oi.l == oj.l)
                        {
                                int l = oi.l;
                                int np= oi.n;
                                int n = oj.n;
                                if (np >= n+3 || np <= n-3) { 
                                        R2 = 0;
                                } else if (np == n+2) {
                                        R2 = +0.25*sqrt( (n+1)*(n+2)*(n+2*l+3)*(n+2*l+4) );
                                } else if (np == n+1) {
                                        R2 = -(n+l+2)*sqrt( (n+1)*(n+2*l+3) );
                                } else if (np == n) {
                                        R2 = (n+l+1.5)*(n+l+2) + 0.5*n*(n+2*l+2);
                                } else if (np == n-1) {
                                        R2 = -(n+l+1)*sqrt( (n)*(n+2*l+2) );
                                } else if (np == n-2) {
					R2 = +0.25*sqrt( (n-1)*(n)*(n+2*l+1)*(n+2*l+2) );
				} else {
					R2 = 0;
				}
                        }
                        op.OneBody(i,j)	= R2*1./b;
                        op.OneBody(j,i)	= op.OneBody(i,j);
                }
        }
        op.profiler.timer["CoulombRadiusSquared"] += omp_get_wtime() - t_start;
	return op;
}

Operator HarmonicRadius(ModelSpace& modelspace)
{
	Operator op(modelspace);
	op.SetHermitian();
	double t_start = omp_get_wtime();
	int norbits = modelspace.GetNumberOrbits();
	#pragma omp parallel for
	for (int i=0; i<norbits; i++)
	{
		Orbit oi = modelspace.GetOrbit(i);
		for (int j=0; j<=i; j++)
		{
			Orbit oj = modelspace.GetOrbit(j);
			if (oj.l != oi.l || oj.j2 != oi.j2) continue;
			op.OneBody(i,j) = RadialIntegral(oi.n,oi.l, oj.n, oj.l, 1, modelspace);
		}
	}
	op.profiler.timer["HarmonicRadius"] += omp_get_wtime() - t_start;
	return op;
}

Operator HarmonicRadiusSquared(ModelSpace& modelspace)
{
        Operator op(modelspace);
        op.SetHermitian();
        double t_start = omp_get_wtime();
        int norbits = modelspace.GetNumberOrbits();
        #pragma omp parallel for
        for (int i=0; i<norbits; i++)
        {
                Orbit oi = modelspace.GetOrbit(i);
                for (int j=0; j<=i; j++)
                {
                        Orbit oj = modelspace.GetOrbit(j);
                        if (oj.l != oi.l || oj.j2 != oi.j2) continue;
                        op.OneBody(i,j) = RadialIntegral(oi.n,oi.l, oj.n, oj.l, 2, modelspace);
                }
        }
        op.profiler.timer["HarmonicRadius"] += omp_get_wtime() - t_start;
        return op;

}

Operator HarmonicOneBody(ModelSpace& modelspace)
{
    Operator op(modelspace);
    op.SetHermitian();
    double t_start = omp_get_wtime();
    int norbits = modelspace.GetNumberOrbits();
    double hw = modelspace.GetHbarOmega();
    #pragma omp parallel for
    for (int i=0; i<norbits; i++)
    {
	Orbit oi = modelspace.GetOrbit(i);
	double e_n = 0.5 * hw * (2*oi.n + oi.l + 3./2);
	op.OneBody(i,i) = e_n;
    }
    op.profiler.timer["HarmonicOneBody"] += omp_get_wtime() - t_start;
    return op;
}

// Creates an operator that performs <k/r>
// In oscillator basis: RadialIntegral() with L=-1;
// In H basis: For orbital n, in an atom with Z protons, expectation of Z/r = (Z/a)*SUM(1/(n^2))
Operator InverseR_Op(ModelSpace& modelspace)
{
   Operator InvR(modelspace);
   InvR.SetHermitian();
   double t_start = omp_get_wtime();
   int norbits = modelspace.GetNumberOrbits();
   #pragma omp parallel for
   for (int i=0; i<norbits; i++)
   {
     Orbit& oi = modelspace.GetOrbit(i);
     for (int j=0; j<=i; j++)
     {
	Orbit& oj = modelspace.GetOrbit(j);
	if ( oi.l != oj.l ) continue; // From spherical harmonics orthogonality; \delta_j1j2 ?
	//double temp1 = getRadialIntegral(oi.n, oi.l, oj.n, oj.l, modelspace)*(-HBARC / (137.035999139) * modelspace.GetTargetZ()); //  -Z\hbarc\alpha
	double temp1 = RadialIntegral(oi.n,oi.l, oj.n, oj.l, -1, modelspace)*(-HBARC / (137.035999139) * modelspace.GetTargetZ());
	if (temp1 == 0) cout << "Writing 0 at " << oi.n << oi.l << oj.n << oj.l << endl;
	InvR.OneBody(i,j) = temp1;
	InvR.OneBody(j,i) = temp1;
     }
   }
   // 1/137 comes from fine struct const {alpha} = e^2/(4pi{epsilon}{hbar}c) ~= 1/137
   // Therefore e^2/(4pi{epsilon}) = {hbar}{c}{alpha}; b from R^L_ab = b^L * ~R^L_ab; b = oscillator length (Suhonen 3.43,6.41); L is the degree of r^L
   double b = HBARC/sqrt( 511000 * modelspace.GetHbarOmega() ); // 511 from electron mass in eV
   // double b = 1./sqrt( modelspace.GetHbarOmega() );
   InvR *= 1./b ;
   InvR.profiler.timer["InverseR_Op"] += omp_get_wtime() - t_start;
   return InvR  ;
}

struct my_f_params { int n1; int l1; int m1;
		     int n2; int l2; int m2;
		     int n3; int l3; int m3;
		     int n4; int l4; int m4; int Z; int A;};

unsigned long CalcLesserIndex(int n1, int l1, int m1, int n2, int l2, int m2)
{
   unsigned long index = 0;
   int min_n12 = 0; //min(n1, n2);
   int max_n12 = 0;
   int min_l12 = 0;
   int max_l12 = 0;
   int min_m12 = 0;
   int max_m12 = 0;

   if(n1 < n2) {
      min_n12 = n1;
      min_l12 = l1;
      min_m12 = m1;
      max_n12 = n2;
      max_l12 = l2;
      max_m12 = m2;
   } else if(n1 > n2) {
      min_n12 = n2;
      min_l12 = l2;
      min_m12 = m2;
      max_n12 = n1;
      max_l12 = l1;
      max_m12 = m1;
   } else { // n1 == n2;
      min_n12 = n1;
      max_n12 = n2;
      if (l1 < l2) {
	 min_l12 = l1;
	 min_m12 = m1;
	 max_l12 = l2;
	 max_m12 = m2;
      } else if(l1 < l2) {
	 min_l12 = l2;
	 min_m12 = m2;
         max_l12 = l1;
	 max_m12 = m1;
      } else { // n1 == n2 && l1 == l2
	 min_l12 = l1;
	 min_m12 = min(m1, m2);
	 max_l12 = l2;
	 max_m12 = max(m1, m2);
      }
   }
   index = max_n12*10000000000
	 + max_l12*100000000
	 + max_m12*1000000
	 + min_n12*10000
	 + min_l12*100
	 + min_m12*1;
   return index;
}

unsigned long CalcCacheIndex(int J, int n1, int l1, int m1,
				int n2, int l2, int m2,
				int n3, int l3, int m3,
				int n4, int l4, int m4)
{
   unsigned long val = 0;
   unsigned long val12 = CalcLesserIndex(n1, l1, m1, n2, l2, m2);
   unsigned long val34 = CalcLesserIndex(n3, l3, m3, n4, l4, m4);
   unsigned long max_12 = max(val12, val34);
   unsigned long min_12 = min(val12, val34);
   val = J*100000000000000 + max_12*1000000000000 + min_12;
   // Basic idea is to get max(a, b) vs min(a,b), then max(ab, cd) and get a single int
   return val;
}

/*
Operator FineStructure(Modelspace& modelspace)
{
	cout << "Entering FineStructure" << endl;
	double t_start = omp_get_wtime();
	Operator fs(modelspace);
	fs.SetHermitian();
	fs.Erase();
	double alpha = 1/137.035999139;
	double e_mass = 511
	int norbits = modelspace.GetNumberOrbits();
	for (int a=0; a<norbits; a++)
	{
		Orbit& o1 = modelspace.GetOrbit(a);
		double energy = 0;
	}
} */

// Number 1
Operator KineticEnergy_Op(ModelSpace& modelspace)
{
   cout << "In KineticEnergy_Op..." << endl;
   Operator T(modelspace);
   int norbits = modelspace.GetNumberOrbits();
   double hw = modelspace.GetHbarOmega();
   double t_start = omp_get_wtime();
   #pragma omp parallel for
   for (int a=0;a<norbits;++a)
   {
      Orbit & oa = modelspace.GetOrbit(a);
      //T.OneBody(a,a) = 0.5 * hw * (2*oa.n + oa.l + 3./2);
      for ( int b : T.OneBodyChannels.at({oa.l, oa.j2, oa.tz2}) )
      {
       	 if (b<a) continue;
         Orbit & ob = modelspace.GetOrbit(b);
	 //if (ob.l != oa.l) continue;
	 if (oa.n == ob.n)
	    T.OneBody(a,b) = 0.5 * hw * (2*oa.n + oa.l + 3./2);
         if (oa.n == ob.n+1)
            T.OneBody(a,b) = 0.5 * hw * sqrt( (oa.n)*(oa.n + oa.l + 1./2) );
         else if (oa.n == ob.n-1)
            T.OneBody(a,b) = 0.5 * hw * sqrt( (ob.n)*(ob.n + ob.l + 1./2) );
         T.OneBody(b,a) = T.OneBody(a,b);
      }
   }
   #pragma omp master
   T.profiler.timer["KineticEnergy_Op"] += omp_get_wtime() - t_start;
   cout << "Exiting KineticEnergy_Op..." << endl;
   return T ;
}

struct RabRcd_params {  int na; int la; int nb; int lb;
                        int nc; int lc; int nd; int ld; int lp; int Z; };

// takes in a set of QN, return `1' if a has "<=" qn than b
// return 0 if b is "lower" than a
bool get_min_qn( int na, int la, int j2a, int nb, int lb, int j2b)
{
    if (na < nb) return true;
    if (na > nb) return false;
    // na == nb
    if (la < lb) return true;
    if (la > lb) return false;
    // la == lb
    if (j2a<j2b) return true;
    if (j2a>j2b) return false;
    // j2a==j2b
    return true;
}

unsigned long GetMinParams(struct RabRcd_params& params)
{
    int na = (params.na);
    int nb = (params.nb);
    int nc = (params.nc);
    int nd = (params.nd);

    int la = (params.la);
    int lb = (params.lb);
    int lc = (params.lc);
    int ld = (params.ld);

    int lp = (params.lp);

    int min_ac_n = 0;
    int min_ac_l = 0;
    int max_ac_n = 0;
    int max_ac_l = 0;

    int min_bd_n = 0;
    int min_bd_l = 0;
    int max_bd_n = 0;
    int max_bd_l = 0;

    bool min_ac = get_min_qn(na,la,0, nc,lc,0); // check if \vec(r_1) "<=" \vec(r_2)
    bool min_bd = get_min_qn(nb,lb,0, nd,ld,0);

    if (min_ac) { // a "<=" c
        min_ac_n = na;
        min_ac_l = la;
        max_ac_n = nc;
        max_ac_l = lc;
    } else {
	min_ac_n = nc;
        min_ac_l = lc;
        max_ac_n = na;
        max_ac_l = la;
    }
    if (min_bd) { // b "<=" d
        min_bd_n = nb;
        min_bd_l = lb;
        max_bd_n = nd;
        max_bd_l = ld;
    } else {
	min_bd_n = nd;
        min_bd_l = ld;
        max_bd_n = nb;
        max_bd_l = lb;
    }

    bool min_bra = get_min_qn(min_bd_n,min_bd_l,0, min_ac_n,min_ac,0);

    int min_bra_n = 0;
    int min_bra_l = 0;
    int max_bra_n = 0;
    int max_bra_l = 0;

    int min_ket_n = 0;
    int min_ket_l = 0;
    int max_ket_n = 0;
    int max_ket_l = 0;

    if (min_bra) { // bd "<=" ac
        min_bra_n = min_bd_n;
        min_bra_l = min_bd_l;
        max_bra_n = min_ac_n;
        max_bra_l = min_ac_l;
        min_ket_n = max_bd_n;
        min_ket_l = max_bd_n;
        max_ket_n = max_ac_n;
        max_ket_l = max_ac_l;
    } else {
	min_bra_n = min_ac_n;
        min_bra_l = min_ac_l;
        max_bra_n = min_bd_n;
        max_bra_l = min_bd_l;
        min_ket_n = max_ac_n;
        min_ket_l = max_ac_l;
        max_ket_n = max_bd_n;
        max_ket_l = max_bd_l;
    }

    return pow(10,12)* lp
         * pow(10,10)* min_bra_n
         * pow(10,8) * max_bra_n
         * pow(10,7) * min_bra_l
         * pow(10,6) * max_bra_l
         * pow(10,4) * min_ket_n
         * pow(10,2) * max_ket_n
         * pow(10,1) * min_ket_l
	 * pow(10,0) * max_ket_l;
}

double Rnl(double r, int n, int l, int Z)
{
    double c = Z/(n*BOHR_RADIUS);
    double h = pow(2*c, 3) * gsl_sf_fact(n-l-1) / ( 2*n*gsl_sf_fact(n+l) );
    return sqrt(h) * pow(2*c*r, l) * exp(-c*r) * gsl_sf_laguerre_n(n-l-1, 2*l+1, 2*r*c);
}

int RabRcd(unsigned ndim, const double *x, void *fdata, unsigned fdim, double *fval)
{
    (void)(ndim); // Avoid unused parameter warnings;
    (void)(fval);
    struct RabRcd_params * params = (struct RabRcd_params*)fdata;

    int na = (params->na);
    int nb = (params->nb);
    int nc = (params->nc);
    int nd = (params->nd);

    int la = (params->la);
    int lb = (params->lb);
    int lc = (params->lc);
    int ld = (params->ld);

    int lp = (params->lp);

    int Z = (params->Z);

    double r1 = x[0]/(1-x[0]);
    double r2 = x[1]/(1-x[1]);

    double rmin = min(r1,r2);
    double rmax = max(r1,r2);

    double Ra = Rnl(r1,na,la,Z);
    double Rb = Rnl(r2,nb,lb,Z);
    double Rc = Rnl(r1,nc,lc,Z);
    double Rd = Rnl(r2,nd,ld,Z);

    //double Rcd= Rnl(r1,nd,ld,Z);
    //double Rdc= Rnl(r2,nc,lc,Z);

    fval[0] = Ra*Rb*Rc*Rd * r1*r1 * r2*r2 * pow(rmin,lp) / pow(rmax,lp+1) * 1/pow(1-x[0],2) * 1/pow(1-x[1],2);

    return 0;
}
/*
double ElectronTwoBodyME(Orbit & oa, Orbit & ob, Orbit & oc, Orbit & od, int J)
{
  double me = 0.0;
  for (int L=max(abs(oa.j2-oc.j2)/2, abs(ob.j2-od.j2)/2); L<=min((oa.j2+oc.j2)/2, (ob.j2+od.j2)/2); ++L)
  {
    if((oa.l + oc.l + L)%2 == 1) continue;
    if((ob.l + od.l + L)%2 == 1) continue;
    double val = Integral[{oa.n, oa.l, ob.n, ob.l,
      oc.n, oc.l, od.n, od.l, L}];
    me += val * SixJs[{oa.j2, ob.j2, J, od.j2, oc.j2, L}] *
      ThreeJs[{oa.j2,L,oc.j2}] * ThreeJs[{ob.j2,L,od.j2}];
  }
  return me*sqrt( (oa.j2+1) * (ob.j2+1) * (oc.j2+1) * (od.j2+1) )*pow(-1, oa.j2+ob.j2+J);
} */

double fact(int n)
{
	double val = 0;
	try{
		if (n < 0) throw n;
		val = gsl_sf_fact(n);
	} catch (int e) {
		cout << "n<0 in factorial for n=" << n << endl;
	}
	return val;
}

double k_r(int n, int l)
{
	return sqrt( fact( n-l-1 ) * fact( n+l+1) );
}

double k_t(int l, int m)
{
	return sqrt( (2*l+1)*fact(l-m)/( 2*fact(l+m) ) );
}

double a_j(int n, int l, int j)
{
	return pow(-1, j) / ( fact(j+2*l+2)*fact(n-l-1-j)*fact(j) );
}

double b_f(int m, int n, int l)
{
	double sum = 0;
	for (int i=0; i<=n+l; i++)
	{
		sum += fact(m+i-l-1)/ ( pow(2,m+i-1)*fact(i) );
	}
	return fact(n+l)*( fact(m-l-1) - sum );
}

double c_g(int m, int n, int g)
{
	return pow(-1,g) * fact( 2*(m-g) ) / ( pow(2,m) * fact(m-2*g-n) * fact(m-g) * fact(g) );
}

double d_i(int la, int ma, int lb, int mb, int i)
{
	int sigma = la + lb + i;
	if (sigma%2 != 0) return 0;
	int Mab = abs(ma + mb);
	if (i<Mab) return 0;
	double d = 2*sqrt( fact(i-Mab) / fact(i+Mab) ) * k_t(la, ma) * k_t(lb, mb);
	double sum = 0;
	int mu = (abs(ma+mb)+ma+mb)/2;
	for (int h=0; h<=mu; h++)
	{
		int choose = gsl_sf_choose(mu, h)*pow(-1,h);
		if (choose == 0) continue;
		for (int j=0; j<= floor(0.5*(i-Mab)); j++)
		{
			double c_j = c_g( i, Mab, j);
			for (int k=0; k<= floor(0.5*(lb-ma)); k++)
			{
				double c_k = c_g( la, ma, k );
				for (int l=0; l<=floor(0.5*(lb-mb)); l++)
				{
					double c_l = c_g(lb, mb, l );
					sum += c_j*c_k*c_l / ( sigma - 2*(mu+j+k+l-h) +1 );
				}
			}
		}
	} 
	return d * sum;
}

double ee_brown_miller(	int na, int la, int ma,
			int nb, int lb, int mb,
			int nc, int lc, int mc,
			int nd, int ld, int md,
			double beta )
{
	int M = ma + mb + mc + md;
	if (M != 0) return 0;
	int L = la + lb + lc + ld;
	if (L%2 != 0) return 0;
	int nu = abs(ma + mb); // can also use abs(mc+md) should both be equal
	int gamma = min(la+lb, lc+ld);

	double Me = 2*beta*k_r(na, la)*k_r(nb, lb)*k_r(nc, lc)*k_r(nd, ld);
	double sum = 0;
	
	for (int i=nu; i<=gamma; i++)
	{
		double d_nu = d_i(la,ma, lb,mb, i) * d_i(lc,mc, ld,md, i);
		for (int a=0; a<=na-la-1; a++)
		{
			double a_a = a_j(na,la,a);
			for (int b=0; b<=nb-lb-1; b++)
			{
				double a_b = a_j(nb, lb, b);
				for (int c=0; c<=nc-lc-1; c++)
				{
					double a_c = a_j(nc, lc, c);
					for (int d=0; d<=nd-ld-1; d++)
					{
						double a_d = a_j(nd, ld, d);
						sum += d_nu*a_a*a_b*a_c*a_d*(b_f(la+lb+a+b+2, lc+ld+c+d+2, i) + b_f(lc+ld+c+d+2, la+lb+a+b+2, i) );
					}
				}
			}
		}
	}
	return sum;
}

double A_i(int n, int l)
{
	return pow(2,l)*sqrt( gsl_sf_fact(n+l)*gsl_sf_fact(n-l-1) ) / pow(n,2+l);
}

double C_i(int n, int l, int j)
{
	return pow(-2,j) / ( pow(n,j)*gsl_sf_fact(j)*gsl_sf_fact(2*l+j+1)*gsl_sf_fact(n-l-j-1) );
}

double R12_func(int n1, int n2, int n3, int n4, int l1, int l2, int l3, int l4, int L, int Z)
{
	double val = 0;
	double A = Z*16*A_i(n1,l1)*A_i(n2,l2)*A_i(n3,l3)*A_i(n4,l4)/BOHR_RADIUS;
	for (int j1=0; j1<=n1-l1-1; j1++)
	{
		double C1 = C_i(n1,l1,j1);
		for (int j2=0; j2<=n2-l2-1; j2++)
		{
			double C2 = C_i(n2,l2,j2);
			for (int j3=0; j3<=n3-l3-1; j3++)
			{
				double C3 = C_i(n3,l3,j3);
				for (int j4=0; j3<=n4-l4-1; j4++)
				{
					double C4 = C_i(n4,l4,j4);
					double T13 = pow(1./n1+1./n3,-3-j1-j3-l1-l3-L) * pow(1./n2+1./n4,-2-j2-j4-l2-l4+L) * gsl_sf_fact(2+j1+j3+l1+l3+L) * gsl_sf_fact(1+j2+j4+l2+l4-L);
					double T24 = pow(1./n2+1./n4,-3-j2-j4-l2-j4-L) * pow(1./n2+1./n4,-2-j1-j3-l1-l3+L) * gsl_sf_fact(2+j2+j4+l2+l4+L) * gsl_sf_fact(1+j1+j3+l1+l3-L);
					double F13 = pow(1./n2+1./n4,-5-l1-l2-l3-l4-j1-j2-j3-j4) * gsl_sf_hyperg_2F1(2+j1+j3+l1+l3-L,5+l1+l2+l3+l4+j1+j2+j3+j4,3+j1+j3+l1+l3-L,-n2*(n1+n3)*n4/( n1*(n2+n4)*n4));
					double F24 = pow(1./n1+1./n3,-5-l1-l2-l3-l4-j1-j2-j3-j4) * gsl_sf_hyperg_2F1(2+j2+j4+l2+l4-L,5+l1+l2+l3+l4+j1+j2+j3+j4,3+j2+j4+l2+l4-L,-n1*(n2+n4)*n3/( n2*(n1+n3)*n4));
					val += C1*C2*C3*C4*(T13+T24-gsl_sf_fact(4+l1+l2+l3+l4+j1+j2+j3+j4)*(F13+F24));
				}
			}
		}
	}
	return A*val;
}

double ElectronTwoBodyME_original( Orbit & oa, Orbit & ob, Orbit & oc, Orbit & od, int J, int Z )
{
    double me = 0;
    double xmin[2] = {0,0};
    double xmax[2] = {1,1};
    int max_iter = 1e3;
    double max_err = 1e-2;
    vector<unsigned long>:: iterator it;

    for (int L=max(abs(oa.j2-oc.j2)/2, abs(ob.j2-od.j2)/2); L<=min((oa.j2+oc.j2)/2, (ob.j2+od.j2)/2); L++)
    {
	/*struct wRabRcd_params p_abcd= { oa.n,oa.l, ob.n,ob.l,
                                       oc.n,oc.l, od.n,od.l,
                                       L, Z};
	double val;
	double err;
        unsigned long cache_val = GetMinParams(p_abcd);
        it = find(cache_list.begin(), cache_list.end(), cache_val);
	#pragma omp critical
        if (it == cache_list.end()) // didn't find in list
        {
	    hcubature(1, &RabRcd, &p_abcd, 2, xmin, xmax, max_iter, 0, max_err, ERROR_INDIVIDUAL, &val, &err);
        } else {
            val = cache[it-cache_list.begin()+1];
        }*/
	double val = R12_func( oa.n,ob.n,oc.n,od.n, oa.l,ob.l,oc.l,od.l, L, Z );

	me += val * SixJ( oa.j2/2,ob.j2/2,J, oc.j2/2,od.j2/2,L ) * ThreeJ(oa.j2/2,L,oc.j2/2, -1./2,0,1./2) * ThreeJ(ob.j2/2,L,od.j2/2, -1./2,0,1./2);
    }
    return me*sqrt( (oa.j2+1) * (ob.j2+1) * (oc.j2+1) * (od.j2+1) )*pow(-1, oa.j2+ob.j2+J);
}

Operator ElectronTwoBody_original(ModelSpace& modelspace)
{
  double t_start = omp_get_wtime();
  cout << "Entering ElectronTwoBody." << endl;
  int nchan = modelspace.GetNumberTwoBodyChannels();
  Operator V12(modelspace);
  V12.SetHermitian();
  V12.Erase();
  double PI = 3.141592;
  vector<unsigned long> cache_list;
  vector<unsigned long> cache;
  //#pragma omp parallel for schedule(dynamic,1) // Throws an error on for ( int ch : .. .  claims it requires an = before :
  for ( int ch : modelspace.SortedTwoBodyChannels )
  {
    TwoBodyChannel& tbc = modelspace.GetTwoBodyChannel(ch);
    int nkets = tbc.GetNumberKets();
    if (nkets == 0) continue; // SortedTwoBodies should only contain > 0 kets, so this should be redundant.
    #pragma omp parallel for schedule(dynamic,1)
    for (int ibra = 0; ibra < nkets; ++ibra)
    {
      Ket & bra = tbc.GetKet(ibra);
      Orbit & o1 = modelspace.GetOrbit(bra.p);
      Orbit & o2 = modelspace.GetOrbit(bra.q);
      for (int jket = ibra; jket < nkets; jket++)
      {
        Ket & ket = tbc.GetKet(jket);
        Orbit & o3 = modelspace.GetOrbit(ket.p);
        Orbit & o4 = modelspace.GetOrbit(ket.q);
	double me_34 = ElectronTwoBodyME_original( o1, o2, o3, o4, tbc.J, modelspace.GetTargetZ() );
	double me_43 = ElectronTwoBodyME_original( o1, o2, o4, o3, tbc.J, modelspace.GetTargetZ() );
	
        double me = HBARC*(1./137) / (sqrt( (1+ket.delta_pq())*(1+bra.delta_pq()) )) * (me_34 - pow(-1,o3.j2+o4.j2-tbc.J) * me_43);
        //  ( ElectronTwoBodyME_original(o1,o2,o3,o4,tbc.J,modelspace.GetTargetZ(), cache,cache_list)
        //    - pow(-1,(o1.j2+o2.j2)/2 - tbc.J) * ElectronTwoBodyME_original(o1,o2,o4,o3,tbc.J,modelspace.GetTargetZ(), cache,cache_list) );
        V12.TwoBody.SetTBME(ch, jket, ibra, me);
        V12.TwoBody.SetTBME(ch, ibra, jket, me);
        //me /=
      } // jket
    } // ibra
    cout << "time for channel: " << ch << " is " <<
      omp_get_wtime() - t_start  << " sec." << endl;
    cout << V12.TwoBody.GetMatrix(ch) << endl;
  } // ch
#pragma omp master
  V12.profiler.timer["ElectronTwoBody"] += omp_get_wtime() - t_start;
  cout << "Leaving ElectronTwoBody." << endl;
  return V12;
}

Operator ElectronTwoBody(ModelSpace& modelspace)
{
  double t_start = omp_get_wtime();
  cout << "Entering ElectronTwoBody." << endl;
  int nchan = modelspace.GetNumberTwoBodyChannels();
  Operator V12(modelspace);
  V12.SetHermitian();
  V12.Erase();
  //#pragma omp parallel for schedule(dynamic,1) // Throws an error on for ( int ch : .. .  claims it requires an = before :
  for ( int ch : modelspace.SortedTwoBodyChannels )
  {
    TwoBodyChannel& tbc = modelspace.GetTwoBodyChannel(ch);
    int nkets = tbc.GetNumberKets();
    if (nkets == 0) continue; // SortedTwoBodies should only contain > 0 kets, so this should be redundant.
    for (int ibra = 0; ibra < nkets; ++ibra)
    {
      Ket & bra = tbc.GetKet(ibra);
      Orbit & o1 = modelspace.GetOrbit(bra.p);
      Orbit & o2 = modelspace.GetOrbit(bra.q);
      for (int jket = ibra; jket < nkets; jket++)
      {
        Ket & ket = tbc.GetKet(jket);
        Orbit & o3 = modelspace.GetOrbit(ket.p);
        Orbit & o4 = modelspace.GetOrbit(ket.q);
        double me = HBARC*(1./137) / (sqrt( (1+ket.delta_pq())*(1+bra.delta_pq()) )) *
          ( ElectronTwoBodyME_original(o1,o2,o3,o4,tbc.J,modelspace.GetTargetZ())
            - pow(-1,(o1.j2+o2.j2)/2 - tbc.J) * ElectronTwoBodyME_original(o1,o2,o4,o3,tbc.J,modelspace.GetTargetZ()) );
        V12.TwoBody.SetTBME(ch, jket, ibra, me);
        V12.TwoBody.SetTBME(ch, ibra, jket, me);
      } // jket
    } // ibra
    cout << "time for channel: " << ch << " is " <<
      omp_get_wtime() - t_start  << " sec." << endl;
  } // ch
  V12.profiler.timer["ElectronTwoBody"] += omp_get_wtime() - t_start;
  cout << "Leaving ElectronTwoBody." << endl;
  return V12;
}

Operator eeCoulomb_original(ModelSpace& modelspace)
{
    double t_start = omp_get_wtime();
    cout << "Entering ElectronTwoBody." << endl;
    int nchan = modelspace.GetNumberTwoBodyChannels();
    Operator V12(modelspace);
    V12.SetHermitian();
    V12.Erase();
    double PI = 3.141592;
    vector<unsigned long> cache_list;
    vector<unsigned long> cache;
    //#pragma omp parallel for
    for ( int ch : modelspace.SortedTwoBodyChannels )
    {
    double t_start = omp_get_wtime();
	TwoBodyChannel& tbc = modelspace.GetTwoBodyChannel(ch);
        //cout << "In channel: " << ch << "with tbc.J=" << tbc.J << endl;
	//if (ch == 1) cout << "+++++ Channel is " << ch << " +++++" << endl;
	int nkets = tbc.GetNumberKets();
	//cout << "nkets is " << nkets << endl;
	if (nkets == 0) continue; // SortedTwoBodies should only contain > 0 kets, so this should be redundant.
	//bool trunc[nkets][nkets] = { {false} };
	//cout << "created trunc, moving into ibra loop." << endl;
	#pragma omp parallel for
	for (int ibra = 0; ibra < nkets; ++ibra)
	{
            Ket & bra = tbc.GetKet(ibra);
            Orbit & oa = modelspace.GetOrbit(bra.p);
            Orbit & ob = modelspace.GetOrbit(bra.q);
	    double sqr_coeff_ab = sqrt( (2*oa.l+1) * (2*ob.l+1) );
	    //cout << "In ibra: "<< ibra << " with oa=<nlj|=" << oa.n << oa.l << oa.j2 << " and ob=<nlj|="<< ob.n << ob.l << ob.j2 << endl;

	    for (int Lab=abs(oa.l-ob.l); Lab<=oa.l+ob.l; Lab++)
            {
		//cout << "Lab=" << Lab << endl;
            	//for (float sa=-0.5;sa<=0.5;sa++)
                //{
		    //if (oa.j2/2 < abs(oa.l-0.5) || oa.j2/2 > oa.l+0.5) continue;
		    //for (float sb=-0.5; sb<=0.5; sb++)
                    //{
			//if (ob.j2/2 < abs(ob.l-0.5) || ob.j2/2 > ob.l+0.5) continue;
                    	for (int Sab=0; Sab<=1; Sab++)
                        {
			    //#pragma omp critical
			    //cout << "Sab=" << Sab << endl;
			    double ab_9j = sqrt( (oa.j2+1) * (ob.j2+1) * (2*Lab+1) * (2*Sab+1) ) * modelspace.GetNineJ(oa.l,0.5,oa.j2*1./2, ob.l,0.5,ob.j2*1./2, Lab,Sab,tbc.J);
			    //cout << "ab_9j=" << ab_9j << endl;
			    if (ab_9j == 0)
			    {
				//cout << "9j is zero, moving on..." << endl;
				continue;
			    }

            		    for (int jket = ibra; jket < nkets; jket++)
            		    {
			        Ket & ket = tbc.GetKet(jket);
				Orbit & oc = modelspace.GetOrbit(ket.p);
				Orbit & od = modelspace.GetOrbit(ket.q);
				if ( (oa.l + ob.l + oc.l + od.l)%2 != 0 ) continue;
				double sqr_coeff_cd = sqrt( (2*oc.l+1) * (2*od.l+1) );
				//cout << "In jket: "<< jket << " with oc=<nlj|=" << oc.n << oc.l << oc.j2 << " and od=<nlj|="<< od.n << od.l << od.j2 << endl;
			    	double me = 0;

				for (int Lcd=abs(oc.l-od.l); Lcd<=oc.l+od.l; Lcd++)
				{
				    //cout << "Lcd=" << Lcd << endl;
				    //for (float sc=-0.5; sc<=0.5; sc++)
				    //{
					//for (float sd=-0.5; sd<=0.5; sd++)
					//{
					    //for (int Scd=0; Scd<=1; Scd++)
					    //{
						int Scd = Sab;
						//cout << "Scd=" << Scd << endl;

						//#pragma omp critical
						double cd_9j = sqrt( (oc.j2+1) * (od.j2+1) * (2*Lcd+1) * (2*Scd+1) ) * modelspace.GetNineJ(oc.l,0.5,oc.j2*1./2, od.l,0.5,od.j2*1./2, Lcd,Scd,tbc.J);
						//cout << "cd_9j=" << cd_9j << endl;
						if (cd_9j == 0)
						{
						    continue;
						}

						for (int mLab=-Lab; mLab<=Lab; mLab++)
						{
						    //cout << "mLab=" << mLab << endl;
						    for (int mSab=-Sab; mSab<=Sab; mSab++)
						    {
							//cout << "mSab=" << mSab << endl;
							int mJab = mLab + mSab;
							double LSab_clebsh = pow(-1,Lab-Sab+mJab) * sqrt(2*tbc.J+1) * ThreeJ(Lab,Sab,tbc.J, mLab,mSab,-mJab);
							//cout << "LSab_clebsh=" << LSab_clebsh << endl;
							if (LSab_clebsh == 0)
							{
							    continue;
							}

							for (int mLcd=-Lcd; mLcd<=Lcd; mLcd++)
							{
							    //cout << "mLcd=" << mLcd << endl;
							    //for (int mScd=-Scd; mScd<=Scd; mScd++)
							    //{
								int mScd = mSab;
								//cout << "mScd=" << endl;
								int mJcd = mLcd + mScd;
								double LScd_clebsh = pow(-1,Lcd-Scd+mJcd) * sqrt(2*tbc.J+1) * ThreeJ(Lcd,Scd,tbc.J, mLcd,mScd,-mJcd);
								//cout << "LScd_clebsh=" << LScd_clebsh << endl;
								if (LScd_clebsh == 0) continue;

								for (int mla=-oa.l; mla<=oa.l; mla++)
								{
								    //cout << "mla=" << mla << endl;
								    for (int mlb=-ob.l; mlb<=ob.l; mlb++)
								    {
									int Mlab = mla+mlb;
									double mlab_clebsh = pow(-1,oa.l-ob.l+Mlab) * sqrt(2*Lab+1) * ThreeJ(oa.l,ob.l,Lab, mla,mlb,-Mlab);
									//cout << "mlab_clebsh=" << mlab_clebsh << endl;
									if (mlab_clebsh == 0) continue;

									for (int mlc=-oc.l; mlc<=oc.l; mlc++)
									{
									    for (int mld=-od.l; mld<=od.l; mld++)
									    {
										//if ( mla+mlb+mlc+mld != 0 ) continue;
										int Mlcd = mlc+mld;
										double mlcd_clebsh = pow(-1,oc.l-od.l+Mlcd) * sqrt(2*Lcd+1) * ThreeJ(oc.l,od.l,Lcd, mlc,mld,-Mlcd);
										//cout << "mlcd_clebsh=" << mlcd_clebsh << endl;
										if (mlcd_clebsh == 0) continue;

										for (int lp=max(abs(oa.l-ob.l), abs(oc.l-od.l)); lp <= min(oa.l+ob.l, oc.l+od.l); lp++)
										{
										    //int mlp = mla+mlc;
										    //if (mlp != -(mlb+mld)) continue;
										    double lp_3j = 0; //ThreeJ(oa.l,lp,oc.l, 0,0,0) * ThreeJ(ob.l,lp,od.l, 0,0,0);
                                                                                    double lp_3j_inv = 0; //ThreeJ(oa.l,lp,od.l, 0,0,0) * ThreeJ(ob.l,lp,oc.l, 0,0,0);
										    int d_ab = 0;
										    int d_cd = 0;
										    double sym_term = 1;
										    if (oa.n == ob.n && oa.l == ob.l && oa.j2 == ob.j2)
                                                                                    {
                                                                                         d_ab = 1;
										    }
										    if (oc.n == od.n && oc.l == od.l && oc.j2 == od.j2)
										    {
											 d_cd = 1;
										    }
										    sym_term *= sqrt(1+d_ab); //sqrt(1+ pow(-1,tbc.J)*d_ab) / (1+d_ab);
										    sym_term *=	sqrt(1+d_cd); //sqrt(1+ pow(-1,tbc.J)*d_cd) / (1+d_cd);
										    //if (sym_term == 0) continue;


										    for (int mlp = -lp; mlp <= lp; mlp++)
										    {
										    	//double lp_3j = ThreeJ(oa.l,lp,oc.l, 0,0,0) * ThreeJ(ob.l,lp,od.l, 0,0,0);
										    	//double lp_3j_inv = ThreeJ(oa.l,lp,od.l, 0,0,0) * ThreeJ(ob.l,lp,oc.l, 0,0,0);
										    	//if (lp_3j == 0 && lp_3j_inv == 0) continue; // && because sym or antisym can be non-zero, butno affect the other

											// This part looks like a mess, will clean later
										    	double lp_ac = ThreeJ(oa.l,oc.l,lp, mla,-mlc,-mlp);
										    	double lp_bd = ThreeJ(ob.l,od.l,lp, mlb,-mld,mlp);
										    	double lp_ad = ThreeJ(oa.l,od.l,lp, mla,-mld,-mlp);
										    	double lp_bc = ThreeJ(ob.l,oc.l,lp, mlb,-mlc,mlp);

											lp_3j += pow(-1,mlc+mld) * lp_ac*lp_bd;
											lp_3j_inv += pow(-1,mlc+mld) * lp_ad*lp_bc;

											} // mlp

											if (lp_3j == 0 && lp_3j_inv == 0) continue;

											lp_3j *= ThreeJ(oa.l,lp,oc.l, 0,0,0) * ThreeJ(ob.l,lp,od.l, 0,0,0);
											lp_3j_inv *= ThreeJ(oa.l,lp,od.l, 0,0,0) * ThreeJ(ob.l,lp,oc.l, 0,0,0);

											double val_sym = 0;
											double err_sym = 0;
											double xmin[2] = {0,0};
                                                                                        double xmax[2] = {1,1};
                                                                                        int max_iter = 1e3;
                                                                                        double max_err = 1e-2;
											vector<unsigned long>:: iterator it;

											if (lp_3j != 0)  // && because sym or antisym can be non-zero, butno affect the other
											{
										    	    //cout << "Calculating RabRcd..." << endl;
											    //if (oa.n == ob.n && oa.l == ob.l) // && oa.j2 == ob.j2)
											    //{
											//	sym_term *= 1./sqrt(2);
											    //}

										    	    struct RabRcd_params p_abcd= { oa.n,oa.l,ob.n,ob.l,
															   oc.n,oc.l,od.n,od.l,
															   lp, modelspace.GetTargetZ() };

											    unsigned long cache_val = GetMinParams(p_abcd);
                                                                                            it = find(cache_list.begin(), cache_list.end(), cache_val);
											    if (it == cache_list.end()) // didn't find in list
											    {
										    	    	hcubature(1, &RabRcd, &p_abcd, 2, xmin, xmax, max_iter, 0, max_err, ERROR_INDIVIDUAL, &val_sym, &err_sym);
											    } else {
												val_sym = cache[it-cache_list.begin()+1];
											    }

										    	    //cout << "Symmetric term: " << val_sym * lp_3j << endl;

										    	    //cout << "Calculating RabRdc..." << endl;
											} // if(lp_3j!=0)
											double val_asym = 0;
											double err_asym = 0;
											if (lp_3j_inv != 0)
											{
											    //if (oc.n == od.n && oc.l == od.l) // && oc.j2 == od.j2) //
                                                                                            //{
											//	sym_term *= 1./sqrt(2);
											    //} // if(oc.n ...
										    	    struct RabRcd_params p_abdc= { oa.n,oa.l,ob.n,ob.l,
															   od.n,od.l,oc.n,oc.l,
															   lp, modelspace.GetTargetZ() };

											    long cache_val = GetMinParams(p_abdc);
                                                                                            it = find(cache_list.begin(), cache_list.end(), cache_val);
                                                                                            if (it == cache_list.end()) // didn't find in list
                                                                                            {
                                                                                        	hcubature(1, &RabRcd, &p_abdc, 2, xmin, xmax, max_iter, 0, max_err, ERROR_INDIVIDUAL, &val_asym, &err_asym);
                                                                                            } else {
                                                                                                val_asym = cache[it-cache_list.begin()+1];
                                                                                            } // if(it == ...

										    	    //cout << "Anti-symmetric term: " << val_asym * lp_3j_inv << endl;
											} // if(lp_3j_inv == 0)
										    	double val = val_sym*lp_3j - pow(-1, oc.j2*1./2+od.j2*1./2-tbc.J)*val_asym*lp_3j_inv;

										    	//cout << "Val=" << val << endl;

										    	val *= mlab_clebsh*mlcd_clebsh;
										    	val *= LSab_clebsh*LScd_clebsh;
										    	val *= ab_9j*cd_9j/sym_term; // factor of 2 in sym_term?
										    	me += val;
											/* if (tbc.J == 0 && ibra == 0 && jket == 0)
											{
											    cout << "me=" << me << endl;
											    cout << "val=" << val << endl;
											    cout << "mlab_clebsh=" << mlab_clebsh << endl;
											    cout << "mlcd_clebsh=" << mlcd_clebsh << endl;
											    cout << "LSab_clebsh=" << LSab_clebsh << endl;
											    cout << "LScd_clebsh=" << LScd_clebsh << endl;
											    cout << "val_sym=" << val_sym << endl;
											    cout << "lp_3j=" << lp_3j << endl;
											    cout << "val_asym=" << val_asym << endl;
											    cout << "lp_3j_inv=" << lp_3j_inv << endl;
											} */ // if tbc.J...
										    // } // mlp
										} // lp
									    } // mld
									} // mlc
								    } // mlb
								} // mla
							    //} // mScd
							} // mLcd
						    } // mSab
						} // mLab
					    //} // Scd
					//} // sd
				    //} // sc
				} // Lcd

				me *= sqr_coeff_ab * sqr_coeff_cd * HBARC/137.035999139;
                   		V12.TwoBody.SetTBME(ch, jket, ibra, me);
                		V12.TwoBody.SetTBME(ch, ibra, jket, me);
			    } // jket
			} // Sab
		    //} // sb
		//} // sa
	    } // Lab
	} // ibra
    cout << "time for channel: " << ch << " is " <<
      omp_get_wtime() - t_start  << " sec." << endl;
    cout << V12.TwoBody.GetMatrix(ch) << endl;
    } // channels
    V12.profiler.timer["eeCoulomb"] += omp_get_wtime() - t_start;
    cout << "Leaving ElectronTwoBody." << endl;
    return V12;
}

Operator eeCoulomb(ModelSpace& modelspace)
{
  double t_start = omp_get_wtime();
  cout << "Entering eeCoulomb; precalculating." << endl;
  //PrecalculationCoulomb(modelspace);
  int nchan = modelspace.GetNumberTwoBodyChannels();
  Operator V12(modelspace);
  V12.SetHermitian();
  V12.Erase();
  double PI = 3.141592;
  vector<unsigned long> cache_list;
  vector<unsigned long> cache;
  for ( int ch : modelspace.SortedTwoBodyChannels )
  {
    double t_start = omp_get_wtime();
    TwoBodyChannel& tbc = modelspace.GetTwoBodyChannel(ch);
    int nkets = tbc.GetNumberKets();
    if (nkets == 0) continue;
    #pragma omp parallel for
    for (int ibra = 0; ibra < nkets; ++ibra)
    {
      Ket & bra = tbc.GetKet(ibra);
      Orbit & oa = modelspace.GetOrbit(bra.p);
      Orbit & ob = modelspace.GetOrbit(bra.q);
      double sqr_coeff_ab = sqrt( (2*oa.l+1) * (2*ob.l+1) );

      for (int Lab=abs(oa.l-ob.l); Lab<=oa.l+ob.l; Lab++)
      {
        for (int Sab=0; Sab<=1; Sab++)
        {
          double ab_9j = sqrt( (oa.j2+1) * (ob.j2+1) * (2*Lab+1) * (2*Sab+1) ) * modelspace.GetNineJ(oa.l,0.5,oa.j2*1./2, ob.l,0.5,ob.j2*1./2, Lab,Sab,tbc.J);
          if (ab_9j == 0)
          {
            continue;
          }

          for (int jket = ibra; jket < nkets; jket++)
          {
            Ket & ket = tbc.GetKet(jket);
            Orbit & oc = modelspace.GetOrbit(ket.p);
            Orbit & od = modelspace.GetOrbit(ket.q);
            if ( (oa.l + ob.l + oc.l + od.l)%2 != 0 ) continue;
            double sqr_coeff_cd = sqrt( (2*oc.l+1) * (2*od.l+1) );
            double me = 0;

            for (int Lcd=abs(oc.l-od.l); Lcd<=oc.l+od.l; Lcd++)
            {
              for (int Scd=0; Scd<=1; Scd++)
              {
                if(Sab != Scd) continue;
                double cd_9j = sqrt( (oc.j2+1) * (od.j2+1) * (2*Lcd+1) * (2*Scd+1) ) * modelspace.GetNineJ(oc.l,0.5,oc.j2*1./2, od.l,0.5,od.j2*1./2, Lcd,Scd,tbc.J);
                if (cd_9j == 0)
                {
                  continue;
                }

                for (int mLab=-Lab; mLab<=Lab; mLab++)
                {
                  for (int mSab=-Sab; mSab<=Sab; mSab++)
                  {
                    int mJab = mLab + mSab;
                    double LSab_clebsh = pow(-1,Lab-Sab+mJab) * sqrt(2*tbc.J+1) * ThreeJ(Lab,Sab,tbc.J, mLab,mSab,-mJab);
                    if (LSab_clebsh == 0)
                    {
                      continue;
                    }

                    for (int mLcd=-Lcd; mLcd<=Lcd; mLcd++)
                    {
                      for (int mScd=-Scd; mScd<=Scd; mScd++)
                      {
                        int mJcd = mLcd + mScd;
                        double LScd_clebsh = pow(-1,Lcd-Scd+mJcd) * sqrt(2*tbc.J+1) * ThreeJ(Lcd,Scd,tbc.J, mLcd,mScd,-mJcd);
                        if (LScd_clebsh == 0) continue;

                        for (int mla=-oa.l; mla<=oa.l; mla++)
                        {
                          for (int mlb=-ob.l; mlb<=ob.l; mlb++)
                          {
                            int Mlab = mla+mlb;
                            double mlab_clebsh = pow(-1,oa.l-ob.l+Mlab) * sqrt(2*Lab+1) * ThreeJ(oa.l,ob.l,Lab, mla,mlb,-Mlab);
                            if (mlab_clebsh == 0) continue;

                            for (int mlc=-oc.l; mlc<=oc.l; mlc++)
                            {
                              for (int mld=-od.l; mld<=od.l; mld++)
                              {
                                int Mlcd = mlc+mld;
                                double mlcd_clebsh = pow(-1,oc.l-od.l+Mlcd) * sqrt(2*Lcd+1) * ThreeJ(oc.l,od.l,Lcd, mlc,mld,-Mlcd);
                                if (mlcd_clebsh == 0) continue;
				#pragma omp critical
                                for (int lp=max(abs(oa.l-ob.l), abs(oc.l-od.l)); lp <= min(oa.l+ob.l, od.l+od.l); lp++)
                                {
                                  double lp_3j = 0; //ThreeJ(oa.l,lp,oc.l, 0,0,0) * ThreeJ(ob.l,lp,od.l, 0,0,0);
                                  double lp_3j_inv = 0; //ThreeJ(oa.l,lp,od.l, 0,0,0) * ThreeJ(ob.l,lp,oc.l, 0,0,0);
                                  int d_ab = 0;
                                  int d_cd = 0;
                                  double sym_term = 1;
                                  if (oa.n == ob.n && oa.l == ob.l && oa.j2 == ob.j2)
                                  {
                                    d_ab = 1;
                                  }
                                  if (oc.n == od.n && oc.l == od.l && oc.j2 == od.j2)
                                  {
                                    d_cd = 1;
                                  }
                                  sym_term *= sqrt(1+d_ab); //sqrt(1+ pow(-1,tbc.J)*d_ab) / (1+d_ab);
                                  sym_term *= sqrt(1+d_cd); //sqrt(1+ pow(-1,tbc.J)*d_cd) / (1+d_cd);


                                  for (int mlp = -lp; mlp <= lp; mlp++)
                                  {

                                    double lp_ac = pow(-1,mla+mlb+mlp) * ThreeJ(oa.l,oc.l,lp, mla,-mlc,-mlp);
                                    double lp_bd = ThreeJ(ob.l,od.l,lp, mlb,-mld,mlp);
                                    double lp_ad = pow(-1,mla+mlb+mlp) * ThreeJ(oa.l,od.l,lp, mla,-mld,-mlp);
                                    double lp_bc = ThreeJ(ob.l,oc.l,lp, mlb,-mlc,mlp);

                                    lp_3j += pow(-1,mlc+mld) * lp_ac*lp_bd;
                                    lp_3j_inv += pow(-1,mlc+mld) * lp_ad*lp_bc;

                                  } // mlp

                                  if (lp_3j == 0 && lp_3j_inv == 0) continue;

                                  lp_3j *= ThreeJ(oa.l,lp,oc.l, 0,0,0) * ThreeJ(ob.l,lp,od.l, 0,0,0);
                                  lp_3j_inv *= ThreeJ(oa.l,lp,od.l, 0,0,0) * ThreeJ(ob.l,lp,oc.l, 0,0,0);
                                  double val_sym = 0; //Integral[{oa.n, oa.l, ob.n, ob.l, oc.n, oc.l, od.n, od.l, lp}];
                                  double val_asym = 0; //Integral[{oa.n, oa.l, ob.n, ob.l, od.n, od.l, oc.n, oc.l, lp}];
                                  double val = val_sym*lp_3j - pow(-1, oc.j2*1./2+od.j2*1./2-tbc.J)*val_asym*lp_3j_inv;

                                  val *= mlab_clebsh*mlcd_clebsh;
                                  val *= LSab_clebsh*LScd_clebsh;
                                  val *= ab_9j*cd_9j/sym_term; // factor of 2 in sym_term?
                                  me += val;
                                } // lp
                              } // mld
                            } // mlc
                          } // mlb
                        } // mla
                      } // mScd
                    } // mLcd
                  } // mSab
                } // mLab
              } // Scd
            } // Lcd
            me *= sqr_coeff_ab * sqr_coeff_cd * HBARC/137.035999139;
            V12.TwoBody.SetTBME(ch, jket, ibra, me);
            V12.TwoBody.SetTBME(ch, ibra, jket, me);
          } // jket
        } // Sab
      } // Lab
    } // ibra
    cout << "time for channel: " << ch << " is " <<
      omp_get_wtime() - t_start  << " sec." << endl;
    cout << V12.TwoBody.GetMatrix(ch) << endl;
  } // channels
  V12.profiler.timer["ElectronTwoBody"] += omp_get_wtime() - t_start;
  cout << "Leaving ElectronTwoBody." << endl;
  return V12;
}
/*
void PrecalculationCoulomb(ModelSpace& modelspace)
{
  int emax = modelspace.GetEmax();
  int lmax = modelspace.GetLmax();
  int jmax = 2*lmax+1;
  // Calculate elements in parallel
  for(int j1 = 1; j1<= jmax; j1+=2)
  {
    for(int j2 = 1; j2<= jmax; j2+=2)
    {
      for(int l = abs(j1-j2)/2; l<=(j1+j2)/2; ++l)
      {
        ThreeJs[{j1,l,j2}] = ThreeJ(j1*0.5,l,j2*0.5, -0.5, 0, 0.5);
      }
    }
  }
  // storing sixj symbols
  for(int j1 = 1; j1<= jmax; j1+=2)
  {
    for(int j2 = 1; j2<= jmax; j2+=2)
    {
      for(int j3 = 1; j3<= jmax; j3+=2)
      {
        for(int j4 = 1; j4<= jmax; j4+=2)
        {
          int Jmin = max(abs(j1-j2)/2, abs(j3-j4)/2);
          int Jmax = min((j1+j2)/2, (j3+j4)/2);
          int Lmin = max(abs(j1-j3)/2, abs(j2-j4)/2);
          int Lmax = min((j1+j3)/2, (j2+j4)/2);
          for (int L = Lmin; L<=Lmax; ++L)
          {
            for (int J = Jmin; J<=Jmax; ++J)
            {
              SixJs[{j1,j2,J,j4,j3,L}] = SixJ(j1*0.5,j2*0.5,J,j4*0.5,j3*0.5,L);
            }
          }
        }
      }
    }
  }
  // storing Radial integrals for coulomb int.
  double t_start = omp_get_wtime();
  int Z = modelspace.GetTargetZ();
  vector<int> n,l;
  for(int na=1;na<=emax;++na) {
    for(int la=0;la<min(lmax+1,na);++la) {
      n.push_back(na);
      l.push_back(la);
    }
  }
  int nlmax = n.size();
  double val_sym = 0;
  double err_sym = 0;
  double xmin[2] = {0,0};
  double xmax[2] = {1,1};
  int max_iter = 1e3;
  double max_err = 1e-2;
 
  for(int a=0; a<nlmax; ++a)
  {
    int na = n[a];
    int la = l[a];
    for(int b = 0; b<nlmax; ++b)
    {
      int nb = n[b];
      int lb = l[b];
      for(int c = 0; c<nlmax; ++c)
      {
        int nc = n[c];
        int lc = l[c];
        for(int d = 0; d<nlmax; ++d)
        {
          int nd = n[d];
          int ld = l[d];
          int lmin_ = max(abs(la-lc),abs(lb-ld));
          int lmax_ = min(la+lc,lb+ld);
          for(int L=lmin_; L<=lmax_; ++L)
          {
            //struct RabRcd_params p = {na,la,nb,lb,nc,lc,nd,ld,L,Z};
            //hcubature(1, &RabRcd, &p, 2, xmin, xmax,
            //    max_iter, 0, max_err, ERROR_INDIVIDUAL,
            //    &val_sym, &err_sym);
            Integral[{na,la,nb,lb,nc,lc,nd,ld,L}] = 0;
	    Integral[{na,la,nb,lb,nd,ld,nc,ld,L}] = 0;
          }
        }
      }
    }
  }
  #pragma omp parallel for
  for(int a=0; a<nlmax; ++a)
  {
    int na = n[a];
    int la = l[a];
    #pragma omp parallel for
    for(int b = 0; b<nlmax; ++b)
    {
      int nb = n[b];
      int lb = l[b];
      #pragma omp parallel for
      for(int c = 0; c<nlmax; ++c)
      {
        int nc = n[c];
        int lc = l[c];
	#pragma omp parallel for
        for(int d = 0; d<nlmax; ++d)
        {
          int nd = n[d];
          int ld = l[d];
          int lmin_ = max(abs(la-lc),abs(lb-ld));
          int lmax_ = min(la+lc,lb+ld);
	  #pragma omp critical
          for(int L=lmin_; L<=lmax_; ++L)
          {
	  struct RabRcd_params p = {na,la,nb,lb,nc,lc,nd,ld,L,Z};
	  struct RabRcd_params p_a = {na,la,nb,lb,nd,ld,nc,lc,L,Z};
          hcubature(1, &RabRcd, &p, 2, xmin, xmax,
              max_iter, 0, max_err, ERROR_INDIVIDUAL,
              &val_sym, &err_sym);
	  Integral[{na,la,nb,lb,nc,lc,nd,ld,L}] = val_sym;
          hcubature(1, &RabRcd, &p_a, 2, xmin, xmax,
              max_iter, 0, max_err, ERROR_INDIVIDUAL,
              &val_sym, &err_sym);
          Integral[{na,la,nb,lb,nd,ld,nc,ld,L}] = val_sym;
	  }
	}
      }
    }
  }
  cout << "time for storing radial integrals: " <<
    omp_get_wtime() - t_start << " sec" << endl;
} */



// Calculate Centre of Mass 1/r for a pair of particles at initial states 12, and final state 34
double CalculateCMInvR( double n1, double l1, double s1, double j1,
			double n2, double l2, double s2, double j2,
			double n3, double l3, double s3, double j3,
			double n4, double l4, double s4, double j4, ModelSpace& modelspace, double J)
{
    //cout << "Entering CalculateCMInvR" << endl;
    // Declare limits here, easier to read/modify
    int Lambda_lower = abs(l1 - l2);
    int Lambdap_lower = abs(l3 - l4);
    int Lambda_upper = abs(l1 + l2);
    int Lambdap_upper = abs(l3 + l4);

    int S_lower = 0;
    int Sp_lower = 0;
    int S_upper = 1;
    int Sp_upper = 1;

    // Conservation of Energy terms
    int p12 = 2*n1 + 2*n2 + l1 + l2;
    int p34 = 2*n3 + 2*n4 + l3 + l4;

    int nMin = 0;
    int NMin = 0;

    //cout << "nMin=" << nMin << endl;
    //cout << "nMax=" << nMax << endl;
    //cout << "NMin=" << NMin << endl;
    //cout << "NMax=" << NMax << endl;
    //cout << "lMin=" << lMin << endl;
    //cout << "lMax=" << lMax << endl;
    //cout << "LMin=" << LMin << endl;
    //cout << "LMax=" << LMax << endl;

    int npMin = 0;

    //cout << "npMin=" << npMin << endl;
    //cout << "npMax=" << npMax << endl;
    //cout << "NpMin=" << NpMin << endl;
    //cout << "NpMax=" << NpMax << endl;
    //cout << "lpMin=" << lpMin << endl;
    //cout << "lpMax=" << lpMax << endl;
    //cout << "LpMin=" << LpMin << endl;
    //cout << "LpMax=" << LpMax << endl;

    double T = 0;
    double temp = 0;
    double m = 0;

    // There has got to be a better way...
    //#pragma omp parallel for // Almost certainly thread safe, I think
    for (int Lambda = Lambda_lower; Lambda <= Lambda_upper; Lambda++)
    {
	//cout << "Looping Lambda=" << Lambda << endl;
	for (int S = S_lower; S <= S_upper; S++)
	{
	    //cout << "Looping S=" << S << endl;
	    if (J < abs(Lambda-S) or J > Lambda+S) continue;
	    //for (int Lambdap = Lambdap_lower; Lambdap <= Lambdap_upper; Lambdap++)
	    int Lambdap = Lambda;
	    //{
		//cout << "Looping Lambdap=" << Lambdap << endl;
		//for (int Sp = Sp_lower; Sp <= Sp_upper; Sp++)
		int Sp = S;
		//{
		    if ( J < abs(Lambdap - Sp) or J > (Lambdap + Sp) ) continue;
		    //cout << "Looping J34=" << J34 << endl;
		    //cout << "About to calculate some NormNineJ" << endl;
		    double i = NormNineJ(l1,s1,j1, l2,s2,j2, Lambda, S, J);
		    if(i == 0) continue;
		    double j = NormNineJ(l3,s3,j3, l4,s4,j4, Lambdap,Sp,J);
		    if(j == 0) continue;
		    //cout << "i=" << i << endl;
		    //cout << "j=" << j << endl;
		    temp = i * j;
		    //if (abs(temp) < 1e-8) continue;
		    //cout << "temp = " << temp << endl;
		    m = 0;
		    for (int n = nMin; n <= p12/2; n++)
		    {
			//cout << "Looping n=" << n << endl;
			for (int N = NMin; N <= p12/2-n; N++)
			{
			    //cout << "Looping N=" << N << endl;
			    for (int l = 0; l <= p12-2*n-2*N; l++)
			    {
				//cout << "Looping l=" << l << endl;
				int L = p12-2*n-2*N-l; // L,Lp value can be found exactly from Cons of energy

				if (p12 != 2*n+2*N+l+L) continue; // Cons of Energy
				if (Lambda < abs(l-L) or Lambda > (l+L)) continue;
				double gm1 = modelspace.GetMoshinsky(n1,l1,n2,l2, n, l, N, L, Lambda);
				if (abs(gm1) < 1e-8) continue;
				for (int np = npMin; np <= p34/2; np++)
				{
				    //cout << "Looping np=" << np << endl;
				    //#pragma omp parallel
				    for (int Np = 0; Np <= p34/2-np; Np++)
				    {
					//cout << "Looping Np=" << Np << endl;
					for (int lp = 0; lp <= p34-2*np-2*Np; lp++)
					{
					    //cout << "Looping lp=" << lp << endl;
					    int Lp = p34-2*np-2*Np-lp;
					    if (p34 != 2*np+2*Np+lp+Lp or Lambdap < abs(lp-Lp) or Lambdap > (lp+Lp)) continue;
					    if (L != Lp or N != Np) continue;
					    double gm2 = modelspace.GetMoshinsky(n3,l3,n4,l4, np,lp,Np,Lp,Lambdap);
					    if (abs(gm2) < 1e-8) continue;
					    double ri = RadialIntegral(n, l, np, lp, -1, modelspace);
					    if (abs(ri) < 1e-8) continue;
					    // Factor of sqrt(2) comes from r=(r1-r2)/sqrt(2), R=(r1+r2)/sqrt(2)
					    //double mosh_ab = modelspace.GetMoshinsky(N_ab,Lam_ab,n_ab,lam_ab,na,la,nb,lb,Lab);
					    m +=1/sqrt(2) * gm1
 						//  * modelspace.GetMoshinsky(np,lp,Np,Lp, n3,l3,n4,l4,Lambdap)
					    //m +=1/sqrt(2) * modelspace.GetMoshinsky(n,l,N,L, n1, l1, n2, l2, Lambda)
 						* gm2
						//* RadialIntegral(N, L, Np, Lp, 0)
						* ri;
						//cout << "Calculated m=" << m << endl;
					} // lp
				    } // Np
				} // np
			    } // l
			} // N
		    } // n
		T += temp*m;
		//cout << "Calculated T=" << T << endl;
		//} // Sp
	    //} // Lambdap
	} // S
    } // Lambda
    //cout << "Leaving CMInvR" << endl;
    T *= HBARC / (137.) / (BOHR_RADIUS);
    return T;
}

/*
Operator KineticEnergy_Op(ModelSpace& modelspace)
{
   Operator T(modelspace);
   int norbits = modelspace.GetNumberOrbits();
   double hw = modelspace.GetHbarOmega();
   double t_start = omp_get_wtime();
   for (int a=0;a<norbits;++a)
   {
      Orbit & oa = modelspace.GetOrbit(a);
      //T.OneBody(a,a) = 0.5 * hw * (2*oa.n + oa.l + 3./2);
      for ( int b : T.OneBodyChannels.at({oa.l, oa.j2, oa.tz2}) )
      {
         if (b<a) continue;
         Orbit & ob = modelspace.GetOrbit(b);
	 if (oa.n == ob.n)
	    T.OneBody(a,b) = 0.5 * hw * (2*oa.n + oa.l + 3./2);
         else if (oa.n == ob.n+1)
            T.OneBody(a,b) = 0.5 * hw * sqrt( (oa.n)*(oa.n + oa.l +1./2));
         else if (oa.n == ob.n-1)
            T.OneBody(a,b) = 0.5 * hw * sqrt( (ob.n)*(ob.n + ob.l +1./2));
         T.OneBody(b,a) = T.OneBody(a,b);
      }
   }
   #pragma omp master
   T.profiler.timer["KineticEnergy_Op"] += omp_get_wtime() - t_start;
   return T;
} */

// Energy in atomic systems
Operator Energy_Op(ModelSpace& modelspace)
{
   double t_start = omp_get_wtime();
   // Naive energy operator
   Operator E(modelspace);
   E.SetHermitian();
   int norbits = modelspace.GetNumberOrbits();
   double hw = modelspace.GetHbarOmega();
   int Z = modelspace.GetTargetZ();
   int A = modelspace.GetTargetMass();
   double me = M_ELECTRON * pow(10,6);
   double mn = M_NUCLEON * pow(10,6);
   double mu = (A*me*mn) / (A*mn + me);
   double alpha = 1./137;
   #pragma omp parallel for
   for (int a=0;a<norbits;++a)
   {
      Orbit & oa = modelspace.GetOrbit(a);
      //E.OneBody(a,a) = -hw/2 * Z * Z / (oa.n * oa.n);
      double k = abs(oa.j2/2 + 0.5);
      double val = k*k - Z*Z*alpha*alpha;
      //cout << "val after init=" << val << endl;
      val = oa.n - k +sqrt(val);
      //cout << "val after sqrt=" << val << endl;
      val = pow(Z*alpha/val,2);
      //cout << "val after pow=" << val << endl;
      val = 1 - 1/sqrt(1-val);
      //cout << "val after 1/sqrt=" << val << endl;
      val = mu*val;
      //cout << "val after mu*val=" << val << endl;
      //cout << "Val manually=" << -hw/2 * Z * Z / (oa.n * oa.n) << endl;
      E.OneBody(a,a) = val;
   }
   E.profiler.timer["Energy_Op"] += omp_get_wtime() - t_start;
   return E;
}



/// Relative kinetic energy, includingthe hw/A factor
/// \f[
/// T_{rel} = T - T_{CM}
/// \f]
 Operator Trel_Op(ModelSpace& modelspace)
 {
   Operator Trel( KineticEnergy_Op(modelspace) );
   Trel -= TCM_Op(modelspace);
   return Trel;
 }


/// Center of mass kinetic energy, including the hw/A factor
/// \f[
/// T = \frac{\hbar\omega}{A}\sum_{ij} t_{ij} a^{\dagger}_{i} a_{j} + \frac{\hbar\omega}{A}\frac{1}{4}\sum_{ijkl} t_{ijkl} a^{\dagger}_{i}a^{\dagger}_{j}a_{l}a_{k}
/// \f]
/// with a one-body piece
/// \f[
/// t_{ij} = \frac{1}{\hbar\omega} \left\langle i | T_{12} | j \right\rangle = \frac{1}{2}(2n_i+\ell_i+3/2) \delta_{ij} + \frac{1}{2}\sqrt{n_j(n_j+\ell_j+\frac{1}{2})} \delta_{n_i,n_j-1}\delta_{k_i k_j}
/// \f]
/// where \f$k\f$ labels all quantum numbers other than \f$n\f$ and a two-body piece
/// \f[
/// t_{ijkl} = \frac{1}{\hbar\omega} \left\langle ij | (T^{CM}_{12} - T^{rel}_{12}) | kl \right\rangle
/// \f]
 Operator TCM_Op(ModelSpace& modelspace)
 {
   double t_start = omp_get_wtime();
   int E2max = modelspace.GetE2max();
   double hw = modelspace.GetHbarOmega();
   int A = modelspace.GetTargetMass();
   int Z = modelspace.GetTargetZ();
   Operator TcmOp = Operator(modelspace);
   TcmOp.SetHermitian();
   double Mu = A; // to avoid /0
   if (modelspace.GetSystemType() == "nuclear") Mu = A; // Can clean this up later
   else if (modelspace.GetSystemType() == "atomic") Mu = A;//*(1836); // scale of nucleon to electron masses, this Doesn't make sense
   // One body piece = p**2/(2mA)
   int norb = modelspace.GetNumberOrbits();
   for (int i=0; i<norb; ++i)
   {
      Orbit & oi = modelspace.GetOrbit(i);
      for (int j : TcmOp.OneBodyChannels.at({oi.l,oi.j2,oi.tz2}) )
      {
         Orbit & oj = modelspace.GetOrbit(j);
         if (j<i) continue;
         double tij = 0;
         if (oi.n == oj.n) tij = 0.5*(2*oi.n+oi.l + 1.5) * hw/Mu;
         else if (oi.n == oj.n-1) tij = 0.5*sqrt(oj.n*(oj.n+oj.l + 0.5)) * hw/Mu;
	 else if (oi.n == oj.n+1) tij = 0.5*sqrt(oi.n*(oi.n+oi.l + 0.5)) * hw/Mu;
         TcmOp.OneBody(i,j) = tij;
         TcmOp.OneBody(j,i) = tij;
      }
   }

   // Two body piece = 2*p1*p2/(2mA) = (Tcm-Trel)/A
   int nchan = modelspace.GetNumberTwoBodyChannels();
   modelspace.PreCalculateMoshinsky();
   #pragma omp parallel for schedule(dynamic,1)  // In order to make this parallel, need to precompute the Moshinsky brackets.
   for (int ch=0; ch<nchan; ++ch)
   {
      TwoBodyChannel& tbc = modelspace.GetTwoBodyChannel(ch);
      int nkets = tbc.GetNumberKets();
      for (int ibra=0;ibra<nkets;++ibra)
      {
         Ket & bra = tbc.GetKet(ibra);
         Orbit & oi = modelspace.GetOrbit(bra.p);
         Orbit & oj = modelspace.GetOrbit(bra.q);
         if ( 2*(oi.n+oj.n)+oi.l+oj.l > E2max) continue;
         for (int iket=ibra;iket<nkets;++iket)
         {

            Ket & ket = tbc.GetKet(iket);
            Orbit & ok = modelspace.GetOrbit(ket.p);
            Orbit & ol = modelspace.GetOrbit(ket.q);
            if ( 2*(ok.n+ol.n)+ok.l+ol.l > E2max) continue;
            double p1p2 = Calculate_p1p2(modelspace,bra,ket,tbc.J) * hw/(A);
            if (abs(p1p2)>1e-7)
            {
              TcmOp.TwoBody.SetTBME(ch,ibra,iket,p1p2);
            }
         }
      }
   }
   TcmOp.profiler.timer["TCM_Op"] += omp_get_wtime() - t_start;
   return TcmOp;
 }


 // evaluate <bra| p1*p2 | ket> , omitting the prefactor  m * hbar_omega
/// This returns the antisymmetrized J-coupled two body matrix element of \f$ \vec{p}_1 \cdot \vec{p}_2 / (m\hbar\omega) \f$.
/// The formula is
/// \f{eqnarray*}{
/// \frac{1}{m\hbar\omega}
/// \left \langle a b \right| \vec{p}_1 \cdot \vec{p}_2 \left| c d \right \rangle_J
/// = \frac{1}{\sqrt{(1+\delta_{ab})(1+\delta_{cd})}} &\sum\limits_{LS}
/// \left[ \begin{array}{ccc}
///  \ell_a & s_a & j_a \\
///  \ell_b & s_b & j_b \\
///  L      & S   & J
/// \end{array} \right]
/// \left[ \begin{array}{ccc}
///  \ell_c & s_c & j_c \\
///  \ell_d & s_d & j_d \\
///  L      & S   & J
/// \end{array} \right] & \\
/// & \times \sum\limits_{\substack{N_{ab}N_{cd} \Lambda \\ n_{ab} n_{cd} \lambda}} \mathcal{A}_{abcd}^{\lambda S} \times
/// \left\langle N_{ab}\Lambda n_{ab} \lambda | n_{a} \ell_{a} n_{b} \ell_{b} \right\rangle_{L} &
/// \left\langle N_{cd}\Lambda n_{cd} \lambda | n_{c} \ell_{c} n_{d} \ell_{d} \right\rangle_{L} \\ & \times
/// \left( \left\langle N_{ab}\Lambda | t_{cm} | N_{cd} \Lambda \right \rangle
/// -\left\langle n_{ab}\lambda | t_{rel} | n_{cd} \lambda \right \rangle \right)
/// \f}
/// The antisymmetrization factor \f$ \mathcal{A}_{abcd}^{\lambda S} \f$ ensures that
/// the relative wave function is antisymmetrized.
/// It is given by \f$ \mathcal{A}_{abcd}^{\lambda S} = \left|t_{za}+t_{zc}\right| + \left| t_{za} + t_{zd} \right| (-1)^{\lambda + S + \left|T_z\right|}\f$ .
///
/// The center-of-mass and relative kinetic energies can be found by the same equation as used in the one-body piece of TCM_Op()
 double Calculate_p1p2(ModelSpace& modelspace, Ket & bra, Ket & ket, int J)
 {
   Orbit & oa = modelspace.GetOrbit(bra.p);
   Orbit & ob = modelspace.GetOrbit(bra.q);
   Orbit & oc = modelspace.GetOrbit(ket.p);
   Orbit & od = modelspace.GetOrbit(ket.q);

   int na = oa.n;
   int nb = ob.n;
   int nc = oc.n;
   int nd = od.n;

   int la = oa.l;
   int lb = ob.l;
   int lc = oc.l;
   int ld = od.l;

   double ja = oa.j2/2.0;
   double jb = ob.j2/2.0;
   double jc = oc.j2/2.0;
   double jd = od.j2/2.0;

   int fab = 2*na + 2*nb + la + lb;
   int fcd = 2*nc + 2*nd + lc + ld;
   // p1*p2 only connects kets with delta N = 0,1 ==> delta E = 0,2
   if (abs(fab-fcd)>2 or abs(fab-fcd)%2 >0 ) return 0;

   double sa,sb,sc,sd;
   sa=sb=sc=sd=0.5;

   double p1p2=0;

   // First, transform to LS coupling using 9j coefficients
   for (int Lab=abs(la-lb); Lab<= la+lb; ++Lab)
   {
     for (int Sab=0; Sab<=1; ++Sab)
     {
       if ( abs(Lab-Sab)>J or Lab+Sab<J) continue;

       double njab = NormNineJ(la,sa,ja, lb,sb,jb, Lab,Sab,J);
       if (njab == 0) continue;
       int Scd = Sab;
       int Lcd = Lab;
       double njcd = NormNineJ(lc,sc,jc, ld,sd,jd, Lcd,Scd,J);
       if (njcd == 0) continue;

       // Next, transform to rel / com coordinates with Moshinsky tranformation
       for (int N_ab=0; N_ab<=fab/2; ++N_ab)  // N_ab = CoM n for a,b
       {
         for (int Lam_ab=0; Lam_ab<= fab-2*N_ab; ++Lam_ab) // Lam_ab = CoM l for a,b
         {
           int Lam_cd = Lam_ab; // tcm and trel conserve lam and Lam, ie relative and com orbital angular momentum
           for (int lam_ab=(fab-2*N_ab-Lam_ab)%2; lam_ab<= (fab-2*N_ab-Lam_ab); lam_ab+=2) // lam_ab = relative l for a,b
           {
              if (Lab<abs(Lam_ab-lam_ab) or Lab>(Lam_ab+lam_ab) ) continue;
              // factor to account for antisymmetrization

              int asymm_factor = (abs(bra.op->tz2+ket.op->tz2) + abs(bra.op->tz2+ket.oq->tz2)*modelspace.phase( lam_ab + Sab ))/ 2;
              if ( asymm_factor ==0 ) continue;

              int lam_cd = lam_ab; // tcm and trel conserve lam and Lam
              int n_ab = (fab - 2*N_ab-Lam_ab-lam_ab)/2; // n_ab is determined by energy conservation

              double mosh_ab = modelspace.GetMoshinsky(N_ab,Lam_ab,n_ab,lam_ab,na,la,nb,lb,Lab);

              if (abs(mosh_ab)<1e-8) continue;

              for (int N_cd=max(0,N_ab-1); N_cd<=N_ab+1; ++N_cd) // N_cd = CoM n for c,d
              {
                int n_cd = (fcd - 2*N_cd-Lam_cd-lam_cd)/2; // n_cd is determined by energy conservation
                if (n_cd < 0) continue;
                if  (n_ab != n_cd and N_ab != N_cd) continue;

                double mosh_cd = modelspace.GetMoshinsky(N_cd,Lam_cd,n_cd,lam_cd,nc,lc,nd,ld,Lcd);
                if (abs(mosh_cd)<1e-8) continue;

                double tcm = 0;
                double trel = 0;
                if (n_ab == n_cd)
                {
                  if      (N_ab == N_cd)   tcm = (2*N_ab+Lam_ab+1.5);
                  else if (N_ab == N_cd+1) tcm = sqrt(N_ab*( N_ab+Lam_ab+0.5));
                  else if (N_ab == N_cd-1) tcm = sqrt(N_cd*( N_cd+Lam_ab+0.5));
                }
                if (N_ab == N_cd)
                {
                  if      (n_ab == n_cd)   trel = (2*n_ab+lam_ab+1.5);
                  else if (n_ab == n_cd+1) trel = sqrt(n_ab*( n_ab+lam_ab+0.5));
                  else if (n_ab == n_cd-1) trel = sqrt(n_cd*( n_cd+lam_cd+0.5));
                }
                double prefactor = njab * njcd * mosh_ab * mosh_cd * asymm_factor;
                p1p2 += (tcm-trel) * prefactor ;

              } // N_cd
           } // lam_ab
         } // Lam_ab
       } // N_ab

     } // Sab
   } // Lab

   // normalize. The 0.5 comes from t ~ 0.5 * (N+3/2) hw
   p1p2 *= 0.5 / sqrt((1.0+bra.delta_pq())*(1.0+ket.delta_pq()));
   return p1p2 ;

 }



////////////////////////////////////////////////////////////////////////////
/////////////  IN PROGRESS...  doesn't work yet, and for now it's slower.  /
////////////////////////////////////////////////////////////////////////////

 void Calculate_p1p2_all(Operator& OpIn)
 {
   ModelSpace* modelspace = OpIn.GetModelSpace();
//   modelspace->PreCalculateMoshinsky();
   for ( int ch : modelspace->SortedTwoBodyChannels )
   {
      TwoBodyChannel& tbc = modelspace->GetTwoBodyChannel(ch);
      int J = tbc.J;
      int parity = tbc.parity;
//      int Tz = tbc.Tz;
      arma::mat& MatJJ = OpIn.TwoBody.GetMatrix(ch);
      int nkets_JJ = tbc.GetNumberKets();

      // Find the maximum oscillator energy for this channel
      Ket& ketlast = tbc.GetKet( tbc.GetNumberKets()-1 );
      int emax_ket = 2*ketlast.op->n + 2*ketlast.oq->n + ketlast.op->l + ketlast.oq->l;

      vector<array<int,6>> JacobiBasis;  // L,S,N,Lambda,n,lambda
      for (int L=max(J-1,0); L<=J+1; ++L)
      {
       for ( int S=abs(J-L); S<=1; ++S)
       {
        for ( int N=0; N<=emax_ket/2; ++N )
        {
         for ( int Lambda=0; Lambda<=(emax_ket-2*N); ++Lambda)
         {
          for ( int lambda=abs(L-Lambda)+(L+parity)%2; lambda<=min(Lambda+L,emax_ket-2*N-Lambda); lambda+=2)
          {
           for ( int n =0; n<=(emax_ket-2*N-Lambda-lambda)/2; ++n)
           {
             JacobiBasis.push_back({L,S,N,Lambda,n,lambda});
           }
          }
         }
        }
       }
      }


      int nkets_Jacobi = JacobiBasis.size();
      arma::mat MatJacobi(nkets_Jacobi,nkets_Jacobi);
      arma::mat Trans(nkets_Jacobi,nkets_JJ);
      int n_nonzero = 0;
      for (int iJJ=0; iJJ<nkets_JJ; ++iJJ)
      {
        Ket & ket = tbc.GetKet(iJJ);
        int la = ket.op->l;
        int lb = ket.oq->l;
        int na = ket.op->n;
        int nb = ket.oq->n;
        double ja = ket.op->j2*0.5;
        double jb = ket.oq->j2*0.5;
//        int ta = ket.op->n;
//        int tb = ket.oq->n;
        for (int iJac=0; iJac<nkets_Jacobi; ++iJac)
        {
          int L      = JacobiBasis[iJac][0];
          int S      = JacobiBasis[iJac][1];
          int N      = JacobiBasis[iJac][2];
          int Lambda = JacobiBasis[iJac][3];
          int n      = JacobiBasis[iJac][4];
          int lambda = JacobiBasis[iJac][5];

//          int Asym = 1; // Fix this...
          double ninej = NormNineJ(la,0.5,ja,lb,0.5,jb,L,S,J);
          if (abs(ninej)<1e-6) continue;
          double mosh = modelspace->GetMoshinsky(N,Lambda,n,lambda,na,la,nb,lb,L);
          if (abs(mosh)<1e-6) continue;
          Trans(iJac,iJJ) = ninej * mosh;
          n_nonzero += 1;

        }
      }
      for (int i=0; i<nkets_Jacobi; ++i)
      {
        int Li      = JacobiBasis[i][0];
        int Si      = JacobiBasis[i][1];
        int Ni      = JacobiBasis[i][2];
        int Lambdai = JacobiBasis[i][3];
        int ni      = JacobiBasis[i][4];
        int lambdai = JacobiBasis[i][5];
        for (int j=0; j<nkets_Jacobi; ++j)
        {
          int Lj      = JacobiBasis[j][0];
          int Sj      = JacobiBasis[j][1];
          int Nj      = JacobiBasis[j][2];
          int Lambdaj = JacobiBasis[j][3];
          int nj      = JacobiBasis[j][4];
          int lambdaj = JacobiBasis[j][5];
          if(Li!=Lj or Si!=Sj or Lambdai!=Lambdaj or lambdai!=lambdaj) continue;
          double tcm = 0;
          double trel = 0;
          if (ni == nj)
          {
            if      (Ni == Nj)   tcm = (2*Ni+Lambdai+1.5);
            else if (Ni == Nj+1) tcm = sqrt(Ni*( Ni+Lambdai+0.5));
            else if (Ni == Nj-1) tcm = sqrt(Nj*( Nj+Lambdai+0.5));
          }
          if (Ni == Nj)
          {
            if      (ni == nj)   tcm = (2*ni+lambdai+1.5);
            else if (ni == nj+1) tcm = sqrt(ni*( ni+lambdai+0.5));
            else if (ni == nj-1) tcm = sqrt(nj*( nj+lambdai+0.5));
          }
          MatJacobi(i,j) = tcm - trel;
        }
      }


      MatJJ = Trans.t() * MatJacobi * Trans;
      cout << "ch = " << ch << "   size of JJ basis = " << nkets_JJ << "  size of Jacobi Basis = " << nkets_Jacobi << "   nonzero matrix elements = " << n_nonzero << endl;

   }
 }

////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////





// Center of mass R^2, with the hw/A factor
/// Returns
/// \f[
/// R^{2}_{CM} = \left( \frac{1}{A}\sum_{i}\vec{r}_{i}\right)^2 =
/// \frac{1}{A^2} \left( \sum_{i}r_{i}^{2} + \sum_{i\neq j}\vec{r}_i\cdot\vec{r}_j  \right)
/// \f]
/// evaluated in the oscillator basis.
 Operator R2CM_Op(ModelSpace& modelspace)
 {
   Operator R2cmOp = Operator(modelspace);

   unsigned int norb = modelspace.GetNumberOrbits();
   for (unsigned int i=0; i<norb; ++i)
   {
      Orbit & oi = modelspace.GetOrbit(i);
      for (auto j : R2cmOp.OneBodyChannels.at({oi.l, oi.j2, oi.tz2}) )
      {
         if (j<i) continue;
         Orbit & oj = modelspace.GetOrbit(j);
         double rij = 0;
         if (oi.n == oj.n)        rij = (2*oi.n+oi.l + 1.5);
         else if (oi.n == oj.n-1) rij = -sqrt(oj.n*(oj.n+oj.l + 0.5));
         R2cmOp.OneBody(i,j) = rij;
         R2cmOp.OneBody(j,i) = rij;
      }
   }

   int nchan = modelspace.GetNumberTwoBodyChannels();
   modelspace.PreCalculateMoshinsky();
   #pragma omp parallel for schedule(dynamic,1)
   for (int ch=0; ch<nchan; ++ch)
   {
      TwoBodyChannel& tbc = modelspace.GetTwoBodyChannel(ch);
      int nkets = tbc.GetNumberKets();
      for (int ibra=0;ibra<nkets;++ibra)
      {
         Ket & bra = tbc.GetKet(ibra);
         for (int iket=ibra;iket<nkets;++iket)
         {
            Ket & ket = tbc.GetKet(iket);
            double mat_el = Calculate_r1r2(modelspace,bra,ket,tbc.J);

            R2cmOp.TwoBody.SetTBME(ch,ibra,iket,mat_el);
            R2cmOp.TwoBody.SetTBME(ch,iket,ibra,mat_el);
         }
      }
   }
   double hw = modelspace.GetHbarOmega();
   int A = modelspace.GetTargetMass();
   return R2cmOp * (HBARC*HBARC/M_NUCLEON/hw)/(A*A);
 }



// Center of mass R^2, with the hw/A factor
/// Returns
/// \f[
/// R^{2}_{CM} = \left( \frac{1}{A}\sum_{i}\vec{r}_{i}\right)^2 =
/// \frac{1}{A^2} \left( \sum_{i}r_{i}^{2} + 2\sum_{i<j}\vec{r}_i\cdot\vec{r}_j  \right)
/// \f]
/// evaluated in the oscillator basis.
 Operator Rp2_corrected_Op(ModelSpace& modelspace, int A, int Z)
 {
   return R2CM_Op(modelspace) + (A-2.0)/(A*Z)*R2_1body_Op(modelspace,"proton") - 2./(A*Z)*R2_2body_Op(modelspace,"proton");
 }

 Operator Rn2_corrected_Op(ModelSpace& modelspace, int A, int Z)
 {
   return R2CM_Op(modelspace) + (A-2.0)/(A*(A-Z))*R2_1body_Op(modelspace,"neutron") - 2./(A*(A-Z))*R2_2body_Op(modelspace,"neutron");
 }

 Operator Rm2_corrected_Op(ModelSpace& modelspace, int A, int Z)
 {
   return (1./A)*RSquaredOp(modelspace) - R2CM_Op(modelspace)  ;
 }



 // Evaluate <bra | r1*r2 | ket>, omitting the factor (hbar * omega) /(m * omega^2)
/// Returns the normalized, anti-symmetrized, J-coupled, two-body matrix element of \f$ \frac{m\omega^2}{\hbar \omega} \vec{r}_1\cdot\vec{r}_2 \f$.
/// Calculational details are similar to Calculate_p1p2().
 double Calculate_r1r2(ModelSpace& modelspace, Ket & bra, Ket & ket, int J)
 {
   Orbit & oa = modelspace.GetOrbit(bra.p);
   Orbit & ob = modelspace.GetOrbit(bra.q);
   Orbit & oc = modelspace.GetOrbit(ket.p);
   Orbit & od = modelspace.GetOrbit(ket.q);

   int na = oa.n;
   int nb = ob.n;
   int nc = oc.n;
   int nd = od.n;

   int la = oa.l;
   int lb = ob.l;
   int lc = oc.l;
   int ld = od.l;

   double ja = oa.j2/2.0;
   double jb = ob.j2/2.0;
   double jc = oc.j2/2.0;
   double jd = od.j2/2.0;

   int fab = 2*na + 2*nb + la + lb;
   int fcd = 2*nc + 2*nd + lc + ld;
   if (abs(fab-fcd)%2 >0) return 0; // p1*p2 only connects kets with delta N = 0,1
   if (abs(fab-fcd)>2) return 0; // p1*p2 only connects kets with delta N = 0,1

   double sa,sb,sc,sd;
   sa=sb=sc=sd=0.5;

   double r1r2=0;

   // First, transform to LS coupling using 9j coefficients
   for (int Lab=abs(la-lb); Lab<= la+lb; ++Lab)
   {
     for (int Sab=0; Sab<=1; ++Sab)
     {
       if ( abs(Lab-Sab)>J or Lab+Sab<J) continue;

       double njab = NormNineJ(la,sa,ja, lb,sb,jb, Lab,Sab,J);
       if (njab == 0) continue;
       int Scd = Sab;
       int Lcd = Lab;
       double njcd = NormNineJ(lc,sc,jc, ld,sd,jd, Lcd,Scd,J);
       if (njcd == 0) continue;

       // Next, transform to rel / com coordinates with Moshinsky tranformation
       for (int N_ab=0; N_ab<=fab/2; ++N_ab)  // N_ab = CoM n for a,b
       {
         for (int Lam_ab=0; Lam_ab<= fab-2*N_ab; ++Lam_ab) // Lam_ab = CoM l for a,b
         {
           int Lam_cd = Lam_ab; // tcm and trel conserve lam and Lam, ie relative and com orbital angular momentum
           for (int lam_ab=(fab-2*N_ab-Lam_ab)%2; lam_ab<= (fab-2*N_ab-Lam_ab); lam_ab+=2) // lam_ab = relative l for a,b
           {
              if (Lab<abs(Lam_ab-lam_ab) or Lab>(Lam_ab+lam_ab) ) continue;
              // factor to account for antisymmetrization

              int asymm_factor = (abs(bra.op->tz2+ket.op->tz2) + abs(bra.op->tz2+ket.oq->tz2)*modelspace.phase( lam_ab + Sab ))/ 2;
              if ( asymm_factor ==0 ) continue;

              int lam_cd = lam_ab; // tcm and trel conserve lam and Lam
              int n_ab = (fab - 2*N_ab-Lam_ab-lam_ab)/2; // n_ab is determined by energy conservation

              double mosh_ab = modelspace.GetMoshinsky(N_ab,Lam_ab,n_ab,lam_ab,na,la,nb,lb,Lab);

              if (abs(mosh_ab)<1e-8) continue;

              for (int N_cd=max(0,N_ab-1); N_cd<=N_ab+1; ++N_cd) // N_cd = CoM n for c,d
              {
                int n_cd = (fcd - 2*N_cd-Lam_cd-lam_cd)/2; // n_cd is determined by energy conservation
                if (n_cd < 0) continue;
                if  (n_ab != n_cd and N_ab != N_cd) continue;

                double mosh_cd = modelspace.GetMoshinsky(N_cd,Lam_cd,n_cd,lam_cd,nc,lc,nd,ld,Lcd);
                if (abs(mosh_cd)<1e-8) continue;

                double r2cm = 0;
                double r2rel = 0;
                if (n_ab == n_cd)
                {
                  if      (N_ab == N_cd)   r2cm = (2*N_ab+Lam_ab+1.5);
                  else if (N_ab == N_cd+1) r2cm = -sqrt(N_ab*( N_ab+Lam_ab+0.5));
                  else if (N_ab == N_cd-1) r2cm = -sqrt(N_cd*( N_cd+Lam_ab+0.5));
                }
                if (N_ab == N_cd)
                {
                  if      (n_ab == n_cd)   r2rel = (2*n_ab+lam_ab+1.5);
                  else if (n_ab == n_cd+1) r2rel = -sqrt(n_ab*( n_ab+lam_ab+0.5));
                  else if (n_ab == n_cd-1) r2rel = -sqrt(n_cd*( n_cd+lam_cd+0.5));
                }
                double prefactor = njab * njcd * mosh_ab * mosh_cd * asymm_factor;
                r1r2 += (r2cm-r2rel) * prefactor;

              } // N_cd
           } // lam_ab
         } // Lam_ab
       } // N_ab

     } // Sab
   } // Lab

   // normalize.
   r1r2 *=  sqrt((1.0+bra.delta_pq())*(1.0+ket.delta_pq()));
   return r1r2 ;

 }

 int k_fact(int n, int l, int j2)
 {
	return ( (2*n+l)*(2*n+l+3) - j2 +3 );
 }

 /* Copied from other operator */
 Operator CorrE2b(ModelSpace& modelspace)
 {
   double t_start = omp_get_wtime();
   Operator E = Operator(modelspace);
   E.SetHermitian();

   int nchan = modelspace.GetNumberTwoBodyChannels();
   modelspace.PreCalculateMoshinsky();
   #pragma omp parallel for
   for (int ch=0; ch<nchan; ++ch)
   {
      TwoBodyChannel& tbc = modelspace.GetTwoBodyChannel(ch);
      int nkets = tbc.GetNumberKets();
      for (int ibra=0;ibra<nkets;++ibra)
      {
         Ket & bra = tbc.GetKet(ibra);
	 Orbit oa = modelspace.GetOrbit(bra.p);
         Orbit ob = modelspace.GetOrbit(bra.q);
         for (int iket=ibra;iket<nkets;++iket)
         {
            Ket & ket = tbc.GetKet(iket);
	    Orbit oc = modelspace.GetOrbit(ket.p);
	    Orbit od = modelspace.GetOrbit(ket.q);
	    if (E.TwoBody.GetTBME(ch,ch,bra,ket) != 0 or E.TwoBody.GetTBME(ch,ch,ket,bra) != 0) continue;

	    if ( oa.l+ob.l!=oc.l+od.l &&  oa.l+ob.l!=oc.l+od.l+1 && oa.l+ob.l!=oc.l+od.l-1 ) continue; // Total angular momentum ought to be conserved, I think.
            double mat_el = Corr_Invr(modelspace,bra,ket,tbc.J, modelspace.systemBasis);

	    //Ket ketp = Ket(od,oc);
	    //double mat_el_asym = pow(-1,(oc.j2+od.j2)/2 - tbc.J) * Corr_Invr(modelspace,bra,ketp,tbc.J,modelspace.systemBasis);
	    //mat_el -= mat_el_asym;
	    //if (abs(mat_el) >= 10) cout << "abs(mat_el) = " << mat_el << " at ibra=" << ibra << " iket=" << iket << " in ch=" << ch << endl;
            E.TwoBody.SetTBME(ch,ch,ibra,iket,mat_el);
            E.TwoBody.SetTBME(ch,ch,iket,ibra,mat_el);
         }
      }
   }
   E.profiler.timer["CorrE2b"] += omp_get_wtime() - t_start;
   return E;
 }

 /* Copied from r1r2 calculation */
 // Evaluate <bra | 1/|r1-r2| | ket>, including the factors of hw, etc.
/// Calculational details are similar to Calculate_p1p2().
 double Corr_Invr(ModelSpace& modelspace, Ket & bra, Ket & ket, int J, string systemBasis)
 {
   if (systemBasis == "hydrogen") return Corr_Invr_Hydrogen(modelspace, bra, ket, J);
   Orbit & oa = modelspace.GetOrbit(bra.p);
   Orbit & ob = modelspace.GetOrbit(bra.q);
   Orbit & oc = modelspace.GetOrbit(ket.p);
   Orbit & od = modelspace.GetOrbit(ket.q);

   int na = oa.n;
   int nb = ob.n;
   int nc = oc.n;
   int nd = od.n;

   int la = oa.l;
   int lb = ob.l;
   int lc = oc.l;
   int ld = od.l;

   double ja = oa.j2/2.0;
   double jb = ob.j2/2.0;
   double jc = oc.j2/2.0;
   double jd = od.j2/2.0;

   int fab = 2*na + 2*nb + la + lb;
   int fcd = 2*nc + 2*nd + lc + ld;
   //if (abs(fab-fcd)%2 >0) return 0; // p1*p2 only connects kets with delta N = 0,1
   //if (abs(fab-fcd)>2) return 0; // p1*p2 only connects kets with delta N = 0,1

   double sa,sb,sc,sd;
   sa=sb=sc=sd=0.5;

   double invr=0;
   double tol=1e-6;

   double b = HBARC/sqrt( 511000 * modelspace.GetHbarOmega()/2 ); // 511 from electron mass in eV, same units as InverseR_Op

   // First, transform to LS coupling using 9j coefficients
   for (int Lab=abs(la-lb); Lab<= la+lb; ++Lab)
   {
     for (int Sab=0; Sab<=1; ++Sab)
     {
       if ( abs(Lab-Sab)>J or Lab+Sab<J) continue;

       double njab = NormNineJ(la,sa,ja, lb,sb,jb, Lab,Sab,J);
       //if (njab < 0) cout << "njab=" << njab << " la=" << la << " sa=" << sa << " ja=" << ja << " lb=" << lb << " sb=" << sb << " jb=" << jb << " Lab=" << Lab << " Sab=" << Sab << " J=" << J << endl;
       if (abs(njab) <= tol) continue;
       int Scd = Sab;
       int Lcd = Lab; 
       if ( abs(Lcd-Scd)>J or Lcd+Scd<J) continue;
       double njcd = NormNineJ(lc,sc,jc, ld,sd,jd, Lcd,Scd,J);
       //if (njcd < 0) cout << "njcd=" << njcd << " lc=" << lc << " sa=" << sc << " jc=" << jc << " ld=" << ld << " sd=" << sd << " jd=" << jd << " Lcd=" << Lcd << " Scd=" << Scd << " J=" << J << endl;
       if (abs(njcd) <= tol) continue;

       // Next, transform to rel / com coordinates with Moshinsky tranformation
       for (int N_ab=0; N_ab<=fab/2; ++N_ab)  // N_ab = CoM n for a,b
       {
	// int L_lim = 0;
	// if (modelspace.Lmax <0) L_lim = fab-2*N_ab;
	// else L_lim = min(modelspace.Lmax,fab-2*N_ab);
         for (int Lam_ab=0; Lam_ab<=fab-2*N_ab; ++Lam_ab) // Lam_ab = CoM l for a,b
	// for (int Lam_ab=0; Lam_ab<=L_lim; ++Lam_ab) // Lam_ab = CoM l for a,b
         {
           int Lam_cd = Lam_ab; // Orthogonality in final term
 	  // int l_lim = 0;
	  // if (modelspace.Lmax<0) l_lim = (fab-2*N_ab-Lam_ab)%2;
	  // else l_lim = min(modelspace.Lmax, (fab-2*N_ab-Lam_ab)%2);
           for (int lam_ab=(fab-2*N_ab-Lam_ab)%2; lam_ab<= fab-2*N_ab-Lam_ab; lam_ab+=2) // lam_ab = relative l for a,b; %2 necessary? no, but faster
	  // for (int lam_ab=l_lim%2; lam_ab <= l_lim; lam_ab+= 2) // I think this needs to be more complicated
           {
              if (Lab<abs(Lam_ab-lam_ab) or Lab>(Lam_ab+lam_ab) ) continue;

              int lam_cd = lam_ab; // Orthogonality later
              int n_ab = (fab - 2*N_ab-Lam_ab-lam_ab)/2; // n_ab is determined by energy conservation
	      if (n_ab < 0) continue;

              double mosh_ab = modelspace.GetMoshinsky(N_ab,Lam_ab,n_ab,lam_ab,na,la,nb,lb,Lab);
              if (abs(mosh_ab)<=tol) continue;

		int N_cd = N_ab; // Orthogonality; A. Brown 2005 pg 186
                int n_cd = (fcd - 2*N_cd-Lam_cd-lam_cd)/2; // n_cd is determined by energy conservation
                if (n_cd < 0) continue;
                double mosh_cd = modelspace.GetMoshinsky(N_cd,Lam_cd,n_cd,lam_cd,nc,lc,nd,ld,Lcd);
                if (abs(mosh_cd)<=tol) continue;
		

		// double rad_sym = getRadialIntegral(n_ab, lam_ab, n_cd, lam_cd, modelspace);
		double rad_sym = RadialIntegral(n_ab,lam_ab, n_cd,lam_cd, -1, modelspace);
		if (abs(rad_sym) <= tol) continue;

		// invr += njab * njcd * mosh_ab * mosh_cd * rad_sym ; // Eqn 17.36 in Alex Brown 2005
		invr += njab * njcd * mosh_ab * mosh_cd * rad_sym * ( 1 + pow(-1, lam_cd+Scd)); // Symmetry term from R. Stroberg
           } // lam_ab
         } // Lam_ab
       } // N_ab

     } // Sab
   } // Lab
   // if (invr < 0) cout << "Invr=" << invr << " oa.index=" << oa.index << " ob.index=" << ob.index << " oc.index=" << oc.index << " od.index=" << od.index << endl;
   return invr / b * (HBARC / (137.035999139)) * 1./sqrt(1+ket.delta_pq()) * 1./sqrt(1+bra.delta_pq());
}

void PrecalculateRad_fromList( vector<unsigned int>& rad_list, ModelSpace& modelspace)
{
    //#pragma omp parallel for // Program stalls when running in parallel, but doesn't throw an error or eat more comp time.
    for ( unsigned int it=0; it < rad_list.size(); it++ )
    {
	auto iter = modelspace.radList.find(rad_list[it]);
	if (iter != modelspace.radList.end() ) continue;
	int temp = rad_list[it];
	//cout << "temp=" << temp << endl;
	int lb = temp%100;
	temp -= lb;
	temp /= 100;
	int la = temp%100;
	temp -= la;
	temp /= 100;
	int nb = temp%100;
	temp -= nb;
	temp /= 100;
	int na = temp%100;
	//cout << "About to calculate rad with na=" << na << " la=" << la << " nb=" << nb << " lb=" << lb << endl;
	//cout << "temp=" << temp << endl;
	//cout << "it=" << it << endl;
	//#pragma omp critical
	double rad = RadialIntegral(na, la, nb, lb, -1, modelspace);
	modelspace.radList[rad_list[it]] = rad;
	temp = 1e6*nb + 1e2*na + 1e4*lb + la;
	modelspace.radList[ temp ] = rad;
    } // it
}

unsigned long long int getNineJkey(double j1, double j2, double J12, double j3, double j4, double J34, double J13, double J24, double J)
{
/*
	unsigned long long int key = 0;
	key += pow(100,8)*J;
	key += pow(100,7)*J24;
	key += pow(100,6)*J13;
	key += pow(100,5)*J34;
	key += pow(100,4)*j4;
	key += pow(100,3)*j3;
	key += pow(100,2)*J12;
	key += pow(100,1)*j2;
	key += pow(100,0)*j1;
*/
   int k1 = 2*j1;
   int k2 = 2*j2;
   int K12 = 2*J12;
   int k3 = 2*j3;
   int k4 = 2*j4;
   int K34 = 2*J34;
   int K13 = 2*J13;
   int K24 = 2*J24;
   int K = 2*J;

   array<int,9> klist = {k1,k2,K12,k3,k4,K34,K13,K24,K};
   array<double,9> jlist = {j1,j2,J12,j3,j4,J34,J13,J24,J};
   int imin = min_element(klist.begin(),klist.end()) - klist.begin();
   switch (imin)
   {
      case 0:
       klist = {k4,K34,k3,K24,K,K13,k2,K12,k1};
       jlist = {j4,J34,j3,J24,J,J13,j2,J12,j1};
       break;
      case 1:
       klist = {K13,K,K24,k3,K34,k4,k1,K12,k2};
       jlist = {J13,J,J24,j3,J34,j4,j1,J12,j2};
       break;
      case 2:
       klist = {k3,k4,K34,K13,K24,K,k1,k2,K12};
       jlist = {j3,j4,J34,J13,J24,J,j1,j2,J12};
       break;
      case 3:
       klist = {K12,k2,k1,K,K24,K13,K34,k4,k3};
       jlist = {J12,j2,j1,J,J24,J13,J34,j4,j3};
       break;
      case 4:
       klist = {k1,K12,k2,K13,K,K24,k3,K34,k4};
       jlist = {j1,J12,j2,J13,J,J24,j3,J34,j4};
       break;
      case 5:
       klist = {K13,K24,K,k1,k2,K12,k3,k4,K34};
       jlist = {J13,J24,J,j1,j2,J12,j3,j4,J34};
       break;
      case 6:
       klist = {k2,K12,k1,k4,K34,k3,K24,K,K13};
       jlist = {j2,J12,j1,j4,J34,j3,J24,J,J13};
       break;
      case 7:
       klist = {K12,k1,k2,K34,k3,k4,K,K13,K24};
       jlist = {J12,j1,j2,J34,j3,j4,J,J13,J24};
       break;
      case 8:
       break;
   }

   unsigned long long int key =   klist[0];
   unsigned long long int factor = 100;
   for (int i=1; i<9; ++i)
   {
      key += klist[i]*factor;
      factor *=100;
   }
   return key;
}


unsigned long long int getMoshkey( int N, int Lam, int n, int lam, int n1, int l1, int n2, int l2, int L )
{
    	int phase_mosh = 1;
	int switches = 10;

	while (switches > 0)
	{
	    switches = 0;
	    if (n2>n1 or (n2==n1 and l2>l1))
   	    {
		swap(n1,n2);
		swap(l1,l2);
		phase_mosh *= phase(Lam+L);
		++switches;
	    }
	    if (n>N or (n==N and lam>Lam))
	    {
		swap(n,N);
		swap(lam,Lam);
		phase_mosh *= phase(l1 +L);
		++switches;
	    }
	    if (n1>N or (n1==N and l1>Lam) or (n1==N and l1==Lam and n2>n) or (n1==N and l1==Lam and n2==n and l2>lam) )
	    {
		swap(n1,N);
		swap(l1,Lam);
		swap(n2,n);
		swap(l2,lam);
		++switches;
//      phase_mosh *= phase(l2+lam); // This phase is given in Moshinsky and Brody, but with the current algorithm, it appears not to be required.
	    }
	}
	//cout << "N=" << N << " Lam=" << Lam << " n=" << n << " lam=" << lam << " n1=" << n1 << " l1=" << l1 << " n2=" << n2 << " l2=" << l2 << " L=" << L << endl;
	unsigned long long int key = 0;
			key += pow(100,8)*N;
			key += pow(100,7)*Lam;
			key += pow(100,6)*n;
			key += pow(100,5)*lam;
			key += pow(100,4)*n1;
			key += pow(100,3)*l1;
	 		key += pow(100,2)*n2;
			key += 100*l2;
			key += L;
	//cout << "moshkey=" << key << endl;
	return key;
}

struct e2b_params { int n1; int l1; int n2; int l2; int n3; int l3; int n4; int l4; int Z;};

/*
Integral of <12,J|1/abs(r3-r4)|34,J>
*/
double
e2b (double *k, size_t dim, void * p)
{
  (void)(dim); // avoid unused parameter warnings
  struct e2b_params * params = (struct e2b_params *)p;

  double x3 = k[0]/(1-k[0]);
  double x4 = k[1]/(1-k[1]);

  int Z = (params->Z);
  double a = BOHR_RADIUS; // Bohr Radius 0.53 A, 0.0529 nm

  int n1 = (params->n1);
  int l1 = (params->l1);
  double c1 = Z / (n1 * a);
  double h1 = pow(2*c1,3) * gsl_sf_fact(n1-l1-1) / ( 2*n1*gsl_sf_fact(n1+l1) );
  double hy1 = sqrt( h1 ) * pow(2*c1*x3,l1) * exp( -c1*x3 ) * gsl_sf_laguerre_n(n1-l1-1, 2*l1+1, 2*x3*c1);

  int n2 = (params->n2);
  int l2 = (params->l2);
  double c2 = Z / (n2 * a);
  double h2 = pow(2*c2,3) * gsl_sf_fact(n2-l2-1) / ( 2*n2*gsl_sf_fact(n2+l2) );
  double hy2 = sqrt( h2 ) * pow(2*c2*x4,l2) * exp( -c2*x4 ) * gsl_sf_laguerre_n(n2-l2-1, 2*l2+1, 2*x4*c2);

  int n3 = (params->n3);
  int l3 = (params->l3);
  double c3 = Z / (n3 * a);
  double h3 = pow(2*c3,3) * gsl_sf_fact(n3-l3-1) / ( 2*n3*gsl_sf_fact(n3+l3) );
  double hy3 = sqrt( h3 ) * pow(2*c3*x3,l3) * exp( -c3*x3 ) * gsl_sf_laguerre_n(n3-l3-1, 2*l3+1, 2*x3*c3);

  int n4 = (params->n4);
  int l4 = (params->l4);
  double c4 = Z / (n4 * a);
  double h4 = pow(2*c4,3) * gsl_sf_fact(n4-l4-1) / ( 2*n4*gsl_sf_fact(n4+l4) );
  double hy4 = sqrt( h4 ) * pow(2*c4*x4,l4) * exp( -c4*x4 ) * gsl_sf_laguerre_n(n4-l4-1, 2*l4+1, 2*x4*c4);

  double A = hy1 * hy2 * 1/fabs(x3-x4) * hy3 * hy4 * 1/pow(1-k[0],2) * 1/pow(1-k[1],2) * x3*x3 * x4*x4 * HBARC/137; // hbarc*alpha
  return A;
}

/*
struct my_f_params { int n1;int l1;int m1;
		int n2;int l2;int m2;
		int n3;int l3;int m3;
		int n4;int l4;int m4; int Z;};
*/
double
Yml ( double t, int l, int m)
{
  if (l+m>170) cout << "Yml throws gamma overflow." << endl;
  if (l-m<0) cout << "Yml has negative factorial." << endl;
  double temp=pow(-1,m) * sqrt( (2*l+1) * gsl_sf_fact(l-m) / (4*3.141592 * gsl_sf_fact(l+m)));
  if (m < 0) return temp * pow(-1,-m) * gsl_sf_fact(l+m) * gsl_sf_legendre_Plm( l, -m, cos(t)
);
  return temp*gsl_sf_legendre_Plm( l, m, cos(t) );
  //return gsl_sf_legendre_sphPlm( l, m, cos(t) );
}

double hydrogenWF(double x, double theta, int n, int l, int m, int Z, int A)
{
        double a = BOHR_RADIUS * (A*M_NUCLEON + M_ELECTRON) / (A*M_NUCLEON);
        double c = Z/(n * a);
	if (n-l-1<0) cout << "hydrogenWF has factorial <0" << endl;
	if (n+l>170) cout << "hydrogenWF overflows gamma" << endl;
	if (2*l+1<=-1) cout << "hydrogenWF has a <= -1" << endl;
        double h = pow(2*c, 3) * gsl_sf_fact(n-l-1) / (2*n*gsl_sf_fact(n+l) );
        double hy = sqrt(h) * pow(2*c*x, l) * exp(-c*x) * gsl_sf_laguerre_n(n-l-1, 2*l+1, 2*x*c) * Yml(theta, l, m);

        return hy;
}

int f(unsigned ndim, const double *x, void *fdata, unsigned fdim, double *fval) {
        (void)(ndim); // Avoid unused parameter warnings;
        (void)(fval);
        struct my_f_params * params = (struct my_f_params *)fdata;
        int lim = 1;
        float a = BOHR_RADIUS; // Bohr Radius
        double x3 = x[0]/(1-x[0]);
        double x4 = x[2]/(1-x[2]);
        double theta3 = x[1];
        double theta4 = x[3];
        int Z = (params->Z);
	int A = (params->A);
        double del = 1e-9;
        double PI = 3.141592;

        // First function
        int n1 = (params->n1);
        int l1 = (params->l1);
	int m1 = (params->m1);

        // Second Function
        int n2 = (params->n2);
        int l2 = (params->l2);
	int m2 = (params->m2);

        // Third function
        int n3 = (params->n3);
        int l3 = (params->l3);
	int m3 = (params->m3);

        // Fourth Function
        int n4 = (params->n4);
        int l4 = (params->l4);
	int m4 = (params->m4);

        // The phi components are orth
        if (m1 != m3 || m2 != m4) {
		fval[0] = 0;
                return 0;
	}
        // l < n
        if (l1 >= n1 || l2 >= n2 || l3 >= n3 || l4 >= n4) {
		fval[0] = 0;
                return 0;
	}

        // -l <= m <= l
        if (m1 > l1 || m2 > l2 || m3 > l3 || m4 > l4 || m1 < -l1 || m2 < -l2 || m3 < -l3 || m4 < -l4) {
		fval[0] = 0;
        	return 0;
	}

        double h1 = hydrogenWF(x3, theta3, n1, l1, m1, Z, A);
        double h2 = hydrogenWF(x4, theta4, n2, l2, m2, Z, A);
        double h3 = hydrogenWF(x3, theta3, n3, l3, m3, Z, A);
        double h4 = hydrogenWF(x4, theta4, n4, l4, m4, Z, A);

        fval[0] = h1 * h3 * x3*x3 * 1/pow(1-x[0],2);
        fval[0]*= h2 * h4 * x4*x4 * 1/pow(1-x[2],2);
        fval[0]*= sin(theta3) * sin(theta4) * 4*PI*PI;
        fval[0]*= 1/sqrt( x3*x3 + x4*x4 - 2*x3*x4* cos(theta3) );

        return 0;
}


/*
int
f(unsigned ndim, const double *x, void *fdata, unsigned fdim, double *fval) {
  (void)(ndim); // avoid unused parameter warnings
  struct my_f_params * params = (struct my_f_params *)fdata;
  //(void)(fdata);
  (void)(fval);
  int lim = 1;
  float a = 0.0529;
  //float HBARC = 197.3;
  double x3 = x[0]/(1-x[0]);
  double x4 = x[2]/(1-x[2]);
  double t3 = x[1];
  double t4 = x[3];
  //double p3 = x[2];
  //double p4 = x[5];
  int Z = (params->Z);
  double del = 1e-9;
  double PI = 3.141592;
  //int Z = (params->Z);
  //printf ("x3=%.18f\n",x3);
  //printf ("x4=%.18f\n",x4);
  int n1 = (params->n1);
  int l1 = (params->l1);
  int m1 = (params->m1);
  double c1 = Z / (n1 * a);
  double h1 = pow(2*c1,3) * gsl_sf_fact(n1-l1-1) / ( 2*n1*gsl_sf_fact(n1+l1) );
  //printf ("h1=%.18f\n",h1);
  double hy1 = sqrt( h1 ) * pow(2*c1*x3,l1) * exp( -c1*x3 ) * gsl_sf_laguerre_n(n1-l1-1, 2*l1+1, 2*x3*c1) * Yml (t3, l1, m1);
  //printf ("hy1=%.18f\n",hy1);
  int n2 = (params->n2);
  int l2 = (params->l2);
  int m2 = (params->m2);
  double c2 = Z / (n2 * a);
  double h2 = pow(2*c2,3) * gsl_sf_fact(n2-l2-1) / ( 2*n2*gsl_sf_fact(n2+l2) );
  //printf ("h2=%.18f\n",h2);
  double hy2 = sqrt( h2 ) * pow(2*c2*x4,l2) * exp( -c2*x4 ) * gsl_sf_laguerre_n(n2-l2-1, 2*l2+1, 2*x4*c2) * Yml (t4, l2, m2);
  //printf ("hy2=%.18f\n",hy2);
  int n3 = (params->n3);
  int l3 = (params->l3);
  int m3 = (params->m3);
  double c3 = Z / (n3 * a);
  double h3 = pow(2*c3,3) * gsl_sf_fact(n3-l3-1) / ( 2*n3*gsl_sf_fact(n3+l3) );
  //printf ("h3=%.18f\n",h3);
  double hy3 = sqrt( h3 ) * pow(2*c3*x3,l3) * exp( -c3*x3 ) * gsl_sf_laguerre_n(n3-l3-1, 2*l3+1, 2*x3*c3) * Yml (t3, l3, m3);
  //printf ("hy3=%.18f\n",hy3);
  int n4 = (params->n4);
  int l4 = (params->l4);
  int m4 = (params->m4);
  double c4 = Z / (n4 * a);
  double h4 = pow(2*c4,3) * gsl_sf_fact(n4-l4-1) / ( 2*n4*gsl_sf_fact(n4+l4) );
  //printf ("h4=%.18f\n",h4);
  double hy4 = sqrt( h4 ) * pow(2*c4*x4,l4) * exp( -c4*x4 ) * gsl_sf_laguerre_n(n4-l4-1, 2*l4+1, 2*x4*c4) * Yml (t4, l4, m4);
  //printf ("hy4=%.18f\n",hy4);
  double den = sqrt( x4*x4 + x3*x3 - 2*x3*x4*cos( t3 ) );
  //fval[0] = hy1 * hy2 * 1/fabs(x4-x3) * hy3 * hy4 * 1/pow(lim-x[0],2) * 1/pow(lim-x[1],2) * x3*x3 * x4*x4;// * HBARC/137; // hbarc*alpha
  fval[0] = hy1 * hy3 * x3*x3 * 1/pow(1-x[0],2);// * sin(t3);
  fval[0]*= hy2 * hy3 * x4*x4 * 1/pow(1-x[2],2);
  fval[0]*= sin(t3) * sin(t4) * 4*PI*PI;
  //fval[0] = hy1 * hy2 * hy3 * hy4 * x3*x3 * x4*x4 * 1/pow(lim-x[0],2) * 1/pow(lim-x[2],2) * sin(t3) * sin(t4);// * 1/fabs(den) * HBARC/137 * 4*PI*PI;
  //printf ("fabs=%.18f\n", 1/fabs(x4 - x3 + del) );
  return 0;
} */

double
numerical_tb_md2 ( int n1, int l1, int m1, int n2, int l2, int m2, int n3, int l3, int m3, int n4, int l4, int m4, int Z )
{
  double xmin[4] = {0,0,0,0};
  double pi = 3.141592;
  double xmax[4] = {1, pi, 1, pi};
  double val=0;
  double err=0;
  struct my_f_params alpha = { n1,l1,m1, n2,l2,m2, n3,l3,m3, n4,l4,m4, Z };
  hcubature(1, &f, &alpha, 4, xmin, xmax, 1e5, 0, 1e-4, ERROR_INDIVIDUAL, &val, &err);
  return val;
}

//extern "C" {
//    double cube_test( int n1,int l1,int j1, int n2,int l2,int j2, int n3,int l3,int j3, int n4,int l4,int j4, int Z);
//}



double
numerical_tb ( int n1, int l1, int n2, int l2, int n3, int l3, int n4, int l4, int Z )
{
  double res, err;
  cout << "n1=" << n1 << " l1=" << l1 << " n2=" << n2 << " l2=" << l2 << " n3=" << n3 << " l3=" << l3 << " n4=" << n4 << endl;

  double xl[2] = { 0, 0 }; // Lower limit 0; [0,1)
  double xu[2] = { 1, 1 } ; // Upper limit 1;

  const gsl_rng_type *T;
  gsl_rng *r;
  struct e2b_params alpha = { n1,l1, n2,l2, n3,l3, n4,l4, Z };
  gsl_monte_function G = { &e2b, 2, &alpha };

  size_t calls = 1000000;

  gsl_rng_env_setup ();

  T = gsl_rng_default;
  r = gsl_rng_alloc (T);

  {
    gsl_monte_vegas_state *s = gsl_monte_vegas_alloc (2);

    gsl_monte_vegas_integrate (&G, xl, xu, 2, 10000, r, s,
                               &res, &err);
    cout << "Vegas converging..." << endl;

    do
      {
        gsl_monte_vegas_integrate (&G, xl, xu, 2, calls/5, r, s,
                                   &res, &err);
        printf ("result = % .6f sigma = % .6f "
                "chisq/dof = %.1f\n", res, err, gsl_monte_vegas_chisq (s));
      }
    while (fabs (gsl_monte_vegas_chisq (s) - 1.0) > 0.5);

    gsl_monte_vegas_free (s);
  }

  gsl_rng_free (r);

  return res;
}

// Numerica eval of 1/|r1-r2|
Operator NumericalE2b(ModelSpace& modelspace)
{
    double t_start = omp_get_wtime();
    Operator E(modelspace);
    E.SetHermitian();

    int nchan = modelspace.GetNumberTwoBodyChannels();
    for (int ch=0; ch<nchan; ++ch)
    {
	TwoBodyChannel& tbc = modelspace.GetTwoBodyChannel(ch);
	int nkets = tbc.GetNumberKets();
	//#pragma omp parallel for
	for (int ibra=0;ibra<nkets;++ibra)
	{
	    Ket & bra = tbc.GetKet(ibra);
	    for (int iket=ibra;iket<nkets;++iket)
	    {
		Ket& ket = tbc.GetKet(iket);
		if (E.TwoBody.GetTBME(ch,ch,bra,ket) != 0 or E.TwoBody.GetTBME(ch,ch,ket,bra) != 0) continue;
		//double mat_el = numerical_tb( bra.op->n,bra.op->l, bra.oq->n,bra.oq->l, ket.op->n,ket.op->l, ket.oq->n,ket.oq->l, modelspace.GetTargetZ() );
		int ml1 = min(bra.op->l,ket.op->l);
		double temp=0;
		double dtemp=0;
		//cout << "
		//int res = fcube( bra.op->n, bra.op->l, bra.op->j2,
		//			bra.oq->n, bra.oq->l, bra.oq->j2,
		//			ket.op->n, ket.op->l, ket.op->j2,
		//			ket.op->n, ket.op->l, ket.op->j2, modelspace.GetTargetZ());

		for (int m1=0; m1 <= ml1; m1++)
		{
		    int ml2 = min(bra.oq->l,ket.oq->l);
		    for(int m2 = 0; m2 <= ml2; m2++)
		    {
			cout << "tbc.J=" << tbc.J << endl;
			cout << "n1=" << bra.op->n << " l1=" << bra.op->l << " m1=" << m1 << " j1=" << bra.op->j2 << endl;
			cout << "n2=" << bra.oq->n << " l2=" << bra.oq->l << " m2=" << m2 << " j2=" << bra.oq->j2 << endl;
			cout << "n3=" << ket.op->n << " l3=" << ket.op->l << " m3=" << m1 << " j3=" << ket.op->j2 << endl;
			cout << "n4=" << ket.oq->n << " l3=" << ket.oq->l << " m4=" << m2 << " j4=" << ket.oq->j2 << endl;
			double that = 0; //fcube( bra.op->n, bra.op->l, m1,
					//	 bra.oq->n, bra.oq->l, m2,
					//	 ket.op->n, bra.op->l, m1,
					//	 ket.oq->n, bra.oq->l, m2,
//modelspace.GetTargetZ() );
			cout << "this is " << that << endl;
			temp += that;
		    }
		    dtemp += 2*ml2;
		}
		dtemp += 2*ml1;
		if(dtemp != 0) temp /= dtemp;
		E.TwoBody.SetTBME(ch,ch,ibra,iket,temp);
		E.TwoBody.SetTBME(ch,ch,iket,ibra,temp);
	    }
	}
    }
    E.profiler.timer["NumericalE2b"] += omp_get_wtime() - t_start;
    return E;
}

 /* Copied from other operator */
 Operator CorrE2b_Hydrogen(ModelSpace& modelspace)
 {
   cout << "Entering Hydrogen two body." << endl;
   double t_start = omp_get_wtime();
   Operator E = Operator(modelspace);
   E.SetHermitian();

   int nmax = 12; //min(4*modelspace.Emax,32); // 46 because integral returns NaN for n > 46, 32 because gamma overflows for nmax > 32
		  // Est. Error at nmax=46 ~ 0.28% Est. Error at nmax=32 ~ 1%
   cout << "Setting nmax=" << nmax << endl;
   int maxFact = 0;
   int n1max = 0;
   int l1max = 0;
   int n2max = 0;
   int l2max = 0;
   double tol = 1e-9;
   double suma = 0;
   double sumb = 0;
   double sumc = 0;
   double sumd = 0;
   bool trunca = false;
   bool truncb = false;
   bool truncc = false;
   bool truncd = false;
   int nchan = modelspace.GetNumberTwoBodyChannels();
   //modelspace.PreCalculateMoshinsky();
   static map<unsigned long int,double> Mat_El_List;
   //vector<unsigned long long int> local_mosh_list;
   vector<int> local_D_list; // Int should be long enough
   //vector<unsigned long long int> local_nineJ_list;
   //vector<unsigned int> local_rad_list;
   vector<unsigned long long int> local_mat_list;


   cout << "About to estimate which constants are needed." << endl;

   // First loop; determines which constants to calculate
   for (int ch=0; ch<nchan; ++ch)
   {
      //cout << "In channel " << ch << endl;
      TwoBodyChannel& tbc = modelspace.GetTwoBodyChannel(ch);
      int nkets = tbc.GetNumberKets();
      for (int ibra=0;ibra<nkets;++ibra)
      {
	 //cout << "In bra " << ibra << endl;
         Ket & bra = tbc.GetKet(ibra);
	 //if ( bra.op->n == bra.oq->n and bra.op->l == bra.oq->l and bra.op->j2 == bra.oq->j2 ) continue; // Pauli!
         for (int iket=ibra;iket<nkets;++iket)
         {
	    //cout << "In ket " << iket << endl;
            Ket & ket = tbc.GetKet(iket);
	    //if ( ket.op->n == ket.oq->n and ket.op->l == ket.oq->l and ket.op->j2 == ket.oq->j2 ) continue; // Pauli!
	    if (E.TwoBody.GetTBME(ch,ch,bra,ket) != 0 or E.TwoBody.GetTBME(ch,ch,ket,bra) != 0) continue;

	    //cout << "About to start big loop." << endl;

	    //#pragma omp parallel for // should check out generateMoshinsky to see how it is done.
	    for (int Na = 0; Na <= nmax; Na++) // 46 because integral returns NaN for n > 46
	    {
		int ia = 1000*Na + 10*bra.op->n + bra.op->l;
		#pragma omp critical
		if ( std::find(local_D_list.begin(), local_D_list.end(), ia) == local_D_list.end() ) {
		    //cout << "Setting Dna at " << ia << " total elements=" << local_D_list.size() << endl;

		    local_D_list.emplace_back (ia);
		}

		//cout << "Getting Dna at " << ia << "=" << Dna << endl;
		Orbit oa = Orbit(Na, bra.op->l, bra.op->j2, bra.op->tz2, bra.op->occ, bra.op->cvq, bra.op->index);
		//if ( 2*Na + bra.op->l + 3./2 >= modelspace.GetTargetZ() * modelspace.GetTargetZ() * modelspace.GetHbarOmega()/(min(bra.oq->n,bra.op->n)^2)/2 ) continue; // Crazy energy. / from 2*(2N + l+...)=27.211 eV Z^2

		for (int Nb = 0; Nb <= nmax; Nb++) // 46 because integral returns NaN for n > 46
		{
		    int ib = 1000*Nb + 10*bra.oq->n + bra.oq->l;
		    #pragma omp critical
		    if ( std::find(local_D_list.begin(), local_D_list.end(), ib) == local_D_list.end() ) {
			//cout << "Setting Dnb at " << ib << " total elements=" << local_D_list.size() << endl;
			local_D_list.emplace_back (ib);
		    }

		    Orbit ob = Orbit(Nb, bra.oq->l, bra.oq->j2, bra.oq->tz2, bra.oq->occ, bra.oq->cvq, bra.oq->index);
		    //if ( 2*Na + bra.op->l + 3./2 + 2*Nb + bra.oq->l + 3./2 >= modelspace.GetTargetZ() * modelspace.GetTargetZ() * modelspace.GetHbarOmega()/(min(bra.oq->n,bra.op->n)^2)/2 ) continue; // Crazy energy.
		    Ket brap = Ket(oa, ob);


		    for (int Nc = 0; Nc <= nmax; Nc++) // 46 because integral returns NaN for n > 46
		    {
			int ic = 1000*Nc + 10*ket.op->n + ket.op->l;
			#pragma omp critical
			if ( std::find(local_D_list.begin(), local_D_list.end(), ic) == local_D_list.end() ) {
			    //cout << "Setting Dnc at " << ic << " total elements=" << local_D_list.size() << endl;
			    local_D_list.emplace_back (ic);
			}

			Orbit oc = Orbit(Nc, ket.op->l, ket.op->j2, ket.op->tz2, ket.op->occ, ket.op->cvq, ket.op->index);

			//if ( 2*Nc + ket.op->l + 3./2 >= modelspace.GetTargetZ() * modelspace.GetTargetZ() * modelspace.GetHbarOmega()/(min(ket.oq->n,ket.op->n)^2)/2 ) continue; // Crazy energy.

			for (int Nd = 0; Nd <= nmax; Nd++) // 46 because integral returns NaN for n > 46
			{
			    Orbit od = Orbit(Nd, ket.oq->l, ket.oq->j2, ket.oq->tz2, ket.oq->occ, ket.oq->cvq, ket.oq->index);
			    int id = 1000*Nd + 10*ket.oq->n + ket.oq->l;
			    //#pragma omp critical
			    if ( std::find(local_D_list.begin(), local_D_list.end(), id) == local_D_list.end() ) {
				//cout << "Setting Dnd at " << id << " total elements=" << local_D_list.size() << endl;
				local_D_list.emplace_back (id);
			    }

			    //if ( od.j2 + oc.j2 != oa.j2 + ob.j2 ) continue; // cons of angular momentum.
			    int n1, n2, n3, n4, l1, l2, l3, l4, j1, j2, j3, j4;
			    int p = 1;

			    if ( Na <= Nb ) {
				n1 = Na;
				n2 = Nb;
				l1 = bra.op->l;
				l2 = bra.oq->l;
				j1 = bra.op->j2;
				j2 = bra.oq->j2;
			    } else {
				n2 = Na;
				n1 = Nb;
				l2 = bra.op->l;
				l1 = bra.oq->l;
				j2 = bra.op->j2;
				j1 = bra.oq->j2;
			    }
			    if ( Nc <= Nd ) {
				n3 = Nc;
				n4 = Nd;
				l3 = ket.op->l;
				l4 = ket.oq->l;
				j3 = ket.op->j2;
				j4 = ket.oq->j2;
			    } else {
				n4 = Nc;
				n3 = Nd;
				l4 = ket.op->l;
				l3 = ket.oq->l;
				j4 = ket.op->j2;
				j3 = ket.oq->j2;
			    }

			    long long unsigned int index = n1*1 +
							   n2*100 +
							   n3*10000 +
							   n4*1000000 +
							   l1*100000000 +
							   l2*1000000000 +
							   l3*10000000000 +
							   l4*100000000000 +
							   j1*1000000000000 +
							   j2*10000000000000 +
							   j3*100000000000000 +
							   j4*1000000000000000 + // Could probably figure out a better indexing function.
							tbc.J*10000000000000000;

			  /*  long long unsigned int index = Na*1 +
							   Nb*100 +
							   Nc*10000 +
							   Nd*1000000 +
						    bra.op->l*100000000 +
						   bra.op->j2*1000000000 +
						    bra.oq->l*10000000000 +
						   bra.oq->j2*100000000000 +
						    ket.op->l*1000000000000 +
						   ket.op->j2*10000000000000 +
						    ket.oq->l*100000000000000 +
						   ket.oq->j2*1000000000000000 +// Could probably figure out a better indexing function.
							tbc.J*10000000000000000; */

			    //if ( index > local_mat_list.size() ) local_mat_list.resize(index);
			    //if ( local_mat_list[index] != 0 ) continue;
			    //if ( local_mat_list.find(index) == local_mat_list.end() ) continue;
			    //#pragma omp critical
			    if ( std::find(local_mat_list.begin(), local_mat_list.end(), index) == local_mat_list.end() ) {
				//cout << "Lacking mat_el at " << index << " total elements=" << local_mat_list.size() << endl;
				local_mat_list.emplace_back (index);
			    } else {
				continue;
			    }

			    //cout << "New Element, simulating Corr_Invr_Hydrogen." << endl;

			    // About to start the effective Corr_Invr_Hydrogen function
			    int la = bra.op->l;
			    int lb = bra.oq->l;
			    int lc = ket.op->l;
			    int ld = ket.oq->l;

			    double ja = bra.op->j2/2.0;
			    double jb = bra.oq->j2/2.0;
			    double jc = ket.op->j2/2.0;
			    double jd = ket.oq->j2/2.0;

			    int fab = 2*Na + 2*Nb + la + lb;
			    int fcd = 2*Nc + 2*Nd + lc + ld;

			    double sa,sb,sc,sd;
			    sa=sb=sc=sd=0.5;
			    int J = tbc.J;
			    // First, transform to LS coupling using 9j coefficients
			    for (int Lab=abs(la-lb); Lab<= la+lb; ++Lab)
			    {
				for (int Sab=0; Sab<=1; ++Sab)
				{
				    if ( abs(Lab-Sab)>J or Lab+Sab<J) continue;
				    /* unsigned long long int ninejkeyab = pow(100,8)*J
								      + pow(100,7)*Sab
								      + pow(100,6)*Lab
								      + pow(100,5)*jb
								      + pow(100,4)*sb
								      + pow(100,3)*lb
								      + pow(100,2)*ja
								      + pow(100,1)*sa
								      + pow(100,0)*la; */
				    /* unsigned long long int ninejkeyab = getNineJkey( la,sa,ja, lb,sb,jb, Lab,Sab,J );
				    if ( std::find(local_nineJ_list.begin(), local_nineJ_list.end(), ninejkeyab) == local_nineJ_list.end() ) {
					//cout << "Setting 9-J with key=" << ninejkeyab << " total elements=" << local_nineJ_list.size() << endl;
					#pragma omp critical
					local_nineJ_list.emplace_back( ninejkeyab );
				    } */
				    //#pragma omp critical
				    double nin1 = modelspace.GetNineJ(la, sa, ja, lb, sb, jb, Lab, Sab, J);
				    if ( nin1 == 0 ) continue;
       				    int Scd = Sab;
				    int Lcd = Lab;
				    /* unsigned long long int ninejkeycd = pow(100,8)*J
								      + pow(100,7)*Scd
								      + pow(100,6)*Lcd
								      + pow(100,5)*jd
								      + pow(100,4)*sd
								      + pow(100,3)*ld
								      + pow(100,2)*jc
								      + pow(100,1)*sc
								      + pow(100,0)*lc; */
				    /* unsigned long long int ninejkeycd = getNineJkey( lc,sc,jc, ld,sd,jd, Lcd,Scd,J );
				    if ( std::find(local_nineJ_list.begin(), local_nineJ_list.end(), ninejkeycd) == local_nineJ_list.end() ) {
					//cout << "Setting 9-J with key=" << ninejkeycd << " total elements=" << local_nineJ_list.size() << endl;
					#pragma omp critical
					local_nineJ_list.emplace_back( ninejkeycd );
				    } */
				    //#pragma omp critical
				    double nin2 = modelspace.GetNineJ(lc, sc, jc, ld, sd, jd, Lcd, Scd, J);
				    if ( nin2 == 0 ) continue;
				    for (int N_ab=1; N_ab<=fab/2; ++N_ab)  // N_ab = CoM n for a,b
				    {
					for (int Lam_ab=0; Lam_ab<= fab-2*N_ab; ++Lam_ab) // Lam_ab = CoM l for a,b
					{
					    int Lam_cd = Lam_ab; // tcm and trel conserve lam and Lam, ie relative and com orbital angular momentum
					    for (int lam_ab=(fab-2*N_ab-Lam_ab)%2; lam_ab<= (fab-2*N_ab-Lam_ab); lam_ab+=2) // lam_ab = relative l for a,b
					    {
						if (Lab<abs(Lam_ab-lam_ab) or Lab>(Lam_ab+lam_ab) ) continue;
						int lam_cd = lam_ab; // tcm and trel conserve lam and Lam
						int n_ab = (fab - 2*N_ab-Lam_ab-lam_ab)/2; // n_ab is determined by energy conservation
						if (n_ab < 0 or N_ab < 0) continue; // or lam_ab >= n_ab
						/* unsigned long long int moshkey1 = pow(100,8)*N_ab
										+ pow(100,7)*Lam_ab
										+ pow(100,6)*n_ab
										+ pow(100,5)*lam_ab
										+ pow(100,4)*Na
										+ pow(100,3)*la
										+ pow(100,2)*Nb
										+ pow(100,1)*lb
										+ pow(100,0)*Lab; */
						//cout << "N=" << N_ab << " Lam=" << Lam_ab << " n=" << n_ab << " lam=" << lam_ab << " n1=" << Na << " l1=" << la << " n2=" << Nb << " l2=" << lb << " L=" << Lab << endl;
						//unsigned long long int moshkey1 = getMoshkey( N_ab,Lam_ab, n_ab,lam_ab, Na,la, Nb,lb, Lab );
						//#pragma omp critical
						double mosh_ab = modelspace.GetMoshinsky( N_ab,Lam_ab, n_ab,lam_ab, Na,la, Nb,lb, Lab );
				                if ( abs(mosh_ab) < tol ) {
						     //cout << "Mosh_ab too small, mosh_ab=" << mosh_ab << endl;
						     continue;
					        }
						/* if ( std::find(local_mosh_list.begin(), local_mosh_list.end(), moshkey1 ) == local_mosh_list.end() ) {
						    //cout << "Setting mosh with key=" << moshkey1 << " total elements=" << local_mosh_list.size() << endl;
						    #pragma omp critical
						    local_mosh_list.emplace_back( moshkey1 );
						} */
						//double mosh1 = modelspace.GetMoshinsky( N_ab, Lam_ab, n_ab, lam_ab, Na, la, Nb, lb, Lab );
						for (int N_cd=max(0,N_ab-1); N_cd<=N_ab+1; ++N_cd) // N_cd = CoM n for c,d
						{
						    int n_cd = (fcd - 2*N_cd-Lam_cd-lam_cd)/2; // n_cd is determined by energy conservation
						    //if ( n_ab == 14 and lam_ab == 3 ) cout << "n_ab=" << n_ab << endl;
						    //if ( n_cd == 14 and lam_cd == 3 ) cout << "n_cd=" << n_cd << endl;
						    if (n_cd < 0 or N_cd < 0) continue; // or lam_cd >= n_cd or
						    /* unsigned long long int moshkey2 = pow(100,8)*N_cd
										    + pow(100,7)*Lam_cd
										    + pow(100,6)*n_cd
										    + pow(100,5)*lam_cd
										    + pow(100,4)*Nc
										    + pow(100,3)*lc
										    + pow(100,2)*Nd
										    + pow(100,1)*ld
										    + pow(100,0)*Lcd; */
						    //cout << "N=" << N_cd << " Lam=" << Lam_cd << " n=" << n_cd << " lam=" << lam_cd << " n1=" << Nc << " l1=" << lc << " n2=" << Nd << " l2=" << ld << " L=" << Lcd << endl;
						    //unsigned long long int moshkey2 = getMoshkey( N_cd,Lam_cd, n_cd,lam_cd, Nc,lc, Nd,ld, Lcd );
						    //#pragma omp critical
						    double mosh_cd = modelspace.GetMoshinsky( N_cd,Lam_cd, n_cd,lam_cd, Nc,lc, Nd,ld, Lcd);
					            if ( abs(mosh_ab) < tol ) {
							 //cout << "Mosh_ab too small, mosh_ab=" << mosh_ab << endl;
							 continue;
						    }
						    /* if ( std::find(local_mosh_list.begin(), local_mosh_list.end(), moshkey2 ) == local_mosh_list.end() ) {
							//cout << "Setting mosh with key=" << moshkey2 << " total elements=" << local_mosh_list.size() << endl;
							#pragma omp critical
							local_mosh_list.emplace_back( moshkey2 );
						    } */
						    //double mosh2 = modelspace.GetMoshinsky( N_cd, Lam_cd, n_cd, lam_cd, Nc, lc, Nd, ld, Lcd );
						    //if ( 2*n_ab + lam_ab + 3./2 >= modelspace.GetTargetZ() * modelspace.GetTargetZ() * modelspace.GetHbarOmega()/2 ) continue; // Crazy energy.
						    //if ( 2*n_cd + lam_cd + 3./2 >= modelspace.GetTargetZ() * modelspace.GetTargetZ() * modelspace.GetHbarOmega()/2 ) continue; // Crazy energy.
						    //#pragma omp critical
						    {
						    unsigned long long int radkey = 0;
							/* radkey = 1e6*n_ab
								+ 1e2*lam_ab
								+ 1e4*n_cd
								+ lam_cd; */
						    if ( n_ab < n_cd) {
							radkey = 1e6*n_ab
								+ 1e2*lam_ab
								+ 1e4*n_cd
								+ lam_cd;
							n1max = max( n1max, n_ab );
							l1max = max( l1max, lam_ab );
							n2max = max( n2max, n_cd );
							l2max = max( l2max, lam_cd );
						    } else {
							radkey = 1e6*n_cd
								+ 1e2*lam_cd
								+ 1e4*n_ab
								+ lam_ab;
							n2max = max( n2max, n_ab );
							l2max = max( l2max, lam_ab );
							n1max = max( n1max, n_cd );
							l1max = max( l1max, lam_cd );
						    }

							int pmax = (lam_ab + lam_cd)/2 + n_ab + n_cd;
							int q = (lam_ab + lam_cd)/2;
							int kmax = min(max(n_ab,n_cd),pmax-q);
							maxFact = max( maxFact, max( 2*pmax+1, max( n_ab, max( n_cd, max( n_cd, max( 2*n_ab + 2*lam_ab + 1, max( n_ab + lam_ab, n_cd + lam_cd ) ) ) ) ) ) );
							maxFact = max( maxFact, max( kmax, max( 2*lam_ab+2*kmax+1, max( n_ab-kmax, max( n_cd, max( 2*pmax - lam_ab + lam_cd - 2*kmax + 1, pmax-q-kmax ) ) ) ) ) );
						    }
						    //long double rad = RadialIntegral(n_ab, lam_ab, n_cd, lam_cd, -1, modelspace);
						    //modelspace.radList[ radkey ] = rad;
						    /* if ( std::find(local_rad_list.begin(), local_rad_list.end(), radkey) == local_rad_list.end() ) {
							//cout << "About to place radkey=" << radkey << " total elements=" << local_rad_list.size() << endl;
							#pragma omp critical
							local_rad_list.emplace_back( radkey );
						    } // find rad */
						} // N_cd
					    } // lam_ab
					} // Lam_ab
				    } // N_ab
				} // Sab
			    } // Lab
            		} // Nd
		    } // Nc
		} // Nb
	    } // Na
         } // iket
      } // ibra
   } // ch

   cout << "Constants estimated, calculating needed factorials." << endl;

   modelspace.GenerateFactorialList( maxFact );

   cout << "Factorials calculated about to calculate mosh, D_coeff, 9-J, and Radial integrals." << endl;
   //cout << "Number of Moshinsky = " << local_mosh_list.size() << endl;
   cout << "Number of D_coeff = " << local_D_list.size() << endl;
   //cout << "Number of 9-J = " << local_nineJ_list.size() << endl;
   //cout << "Number of Radial Integrals = " << local_rad_list.size() << endl;
   cout << "n1max=" << n1max << " n2max=" << n2max << " l1max=" << l1max << " l2max=" << l2max << endl;
   // Could set factorial list to have 9 threads, and rest to have 1, to ensure thread safety.
  // #pragma omp parallel sections num_threads(4)
   {
	//#pragma omp section
	//modelspace.PreCalculateMoshinsky_FromList( local_mosh_list );
	// Should now destroy local_mosh_list to save memory

	//#pragma omp section
	modelspace.GenerateOsToHydroCoeff_fromlist( local_D_list );
	local_D_list.resize(0);
	// Should now destroy local_D_list to save memory

	//#pragma omp section
	//modelspace.PrecalculateNineJ( local_nineJ_list );
	// Should now destroy local_ninej_list to save memory


	cout << "About to Calculate Radial Intergrals." << endl;
	//#pragma omp section
	n1max = max(n1max, n2max)+1;
	l1max = max(l1max, l2max)+2;
	GenerateRadialIntegrals(modelspace,1e6*n1max+1e4*n1max+1e2*l1max+l1max);
	//PrecalculateRad_fromList( local_rad_list, modelspace );
	// Should now destroy local_rad_list to save memory
	cout << "Radial integrals calculated, calculating matrix elements." << endl;

	/* //#pragma omp section
	for ( unsigned long long int i=0; i < local_mat_list.size(); i++ )
	{
	    unsigned long long int temp = local_mat_list[i];
	    int na = temp%100;
	    temp -= na;
	    temp /= 100;
	    int nb = temp%100;
	    temp -= nb;
	    temp /= 100;
	    int nc = temp%100;
	    temp -= nc;
	    temp /= 100;
	    int nd = temp%100;
	    temp -= nd;
	    temp /= 100;
	    int la = temp%10;
	    temp -= la;
	    temp /= 10;
	    int ja = temp%10;
	    temp -= ja;
	    temp /= 10;
	    int lb = temp%10;
	    temp -= lb;
	    temp /= 10;
	    int jb = temp%10;
	    temp -= jb;
	    temp /= 10;
	    int lc = temp%10;
	    temp -= lc;
	    temp /= 10;
	    int jc = temp%10;
	    temp -= jc;
	    temp /= 10;
	    int ld = temp%10;
	    temp -= ld;
	    temp /= 10;
	    int jd = temp%10;
	    temp -= jd;
	    temp /= 10;
	    int J = temp%10;
	   Orbit oa = Orbit( na, la, ja, -1, 0, 0, 0);
	   Orbit ob = Orbit( nb, lb, jb, -1, 0, 0, 0);
	   Orbit oc = Orbit( nc, lc, jc, -1, 0, 0, 0);
	   Orbit od = Orbit( nd, ld, jd, -1, 0, 0, 0);
	   Ket bra_ab = Ket( oa, ob );
	   Ket ket_cd = Ket( oc, od );
	   #pragma omp critical
	   Mat_El_List[ local_mat_list[i] ] = Corr_Invr_Hydrogen(modelspace, bra_ab, ket_cd, J);
	} */
   }

   cout << "Lists generated, moving to calculation." << endl;

   // Actual Calculation.
   //#pragma omp parallel for  //not thread safe if using Mat_El_List
   for (int ch=0; ch<nchan; ++ch)
   {
      //cout << "In channel " << ch << endl;
      TwoBodyChannel& tbc = modelspace.GetTwoBodyChannel(ch);
      int nkets = tbc.GetNumberKets();
      for (int ibra=0;ibra<nkets;++ibra)
      {
	 //cout << "In bra " << ibra << endl;
         Ket & bra = tbc.GetKet(ibra);
	 //if ( bra.op->n == bra.oq->n and bra.op->l == bra.oq->l and bra.op->j2 == bra.oq->j2 ) continue; // Pauli!
         for (int iket=ibra;iket<nkets;++iket)
         {
	    //cout << "In ket " << iket << endl;
            Ket & ket = tbc.GetKet(iket);
	    //if ( ket.op->n == ket.oq->n and ket.op->l == ket.oq->l and ket.op->j2 == ket.oq->j2 ) continue; // Pauli!
	    if (E.TwoBody.GetTBME(ch,ch,bra,ket) != 0 or E.TwoBody.GetTBME(ch,ch,ket,bra) != 0) continue;
	    double mat_el = 0;
	    double result = 0;
	    //cout << "About to start big loop." << endl;
	    suma = 0;
	    trunca = false;
	    #pragma omp parallel for  // should check out generateMoshinsky to see how it is done.
	    for (int Na = 0; Na <= nmax; Na++) // 46 because integral returns NaN for n > 46
	    {
		static unordered_map<unsigned long int,double> local_List;
		int ia = 1000*Na + 10*bra.op->n + bra.op->l;
		//cout << "Getting Dna@" << ia << endl;
		double Dna = modelspace.OsToHydroCoeffList[ia];

		Orbit oa = Orbit(Na, bra.op->l, bra.op->j2, bra.op->tz2, bra.op->occ, bra.op->cvq, bra.op->index);
		//if ( 2*Na + bra.op->l + 3./2 >= modelspace.GetTargetZ() * modelspace.GetTargetZ() * modelspace.GetHbarOmega()/(min(bra.oq->n,bra.op->n)^2)/2 ) continue; // Crazy energy. /4 from 2*(2N + l+...)=27.211 eV Z^2/2
		if ( abs(Dna) < tol ) {
		    cout << "Dna too small; Dna=" << Dna << " at ia=" << ia << endl;
		    continue;
		}
		suma += (2*Na + bra.op->l + 3/2)*pow(Dna,2);
		if ( (suma > pow(modelspace.GetTargetZ(),1)/pow(bra.op->n,2) *0.5 ) ) {
		    trunca = true;
		    continue; // truncation boolean?
		}
		sumb = 0;
		truncb = false;
		for (int Nb = 0; Nb <= nmax and truncb == true ; Nb++) // 46 because integral returns NaN for n > 46
		{
		    int ib = 1000*Nb + 10*bra.oq->n + bra.oq->l;
		    //cout << "Getting Dnb@" << ib << endl;
		    double Dnb = modelspace.OsToHydroCoeffList[ib];

		    Orbit ob = Orbit(Nb, bra.oq->l, bra.oq->j2, bra.oq->tz2, bra.oq->occ, bra.oq->cvq, bra.oq->index);
		    //if ( 2*Na + bra.op->l + 3./2 + 2*Nb + bra.oq->l + 3./2 >= modelspace.GetTargetZ() * modelspace.GetTargetZ() * modelspace.GetHbarOmega()/(min(bra.oq->n,bra.op->n)^2)/2 ) continue; // Crazy energy.
		    Ket brap = Ket(oa, ob);
		    if ( abs(Dnb) < tol ) {
			cout << "Dnb too small; Dnb=" << Dnb << " at ib=" << ib << endl;
			continue;
		    }
		    sumb += (2*Nb + bra.oq->l + 3/2)*pow(Dnb,2);
		    if ( (sumb > pow(modelspace.GetTargetZ(),1)/pow(bra.oq->n,2) *0.5 ) ) {
			truncb = true;
			continue; // truncation boolean?
		    }
		    sumc = 0;
		    truncc = false;
		    for (int Nc = 0; Nc <= nmax and truncc == true; Nc++) // 46 because integral returns NaN for n > 46
		    {
			int ic = 1000*Nc + 10*ket.op->n + ket.op->l;
			//cout << "Getting Dnc@" << ic << endl;
			double Dnc = modelspace.OsToHydroCoeffList[ic];

			Orbit oc = Orbit(Nc, ket.op->l, ket.op->j2, ket.op->tz2, ket.op->occ, ket.op->cvq, ket.op->index);

			//if ( 2*Nc + ket.op->l + 3./2 >= modelspace.GetTargetZ() * modelspace.GetTargetZ() * modelspace.GetHbarOmega()/(min(ket.oq->n,ket.op->n)^2)/2 ) continue; // Crazy energy.
			if ( abs(Dnc) < tol ) {
			    cout << "Dnc too small; Dnc=" << Dnc << " at ic=" << ic << endl;
			    continue;
			}
			sumc += (2*Nc + ket.op->l + 3/2)*pow(Dnc,2);
			if ( (sumc > pow(modelspace.GetTargetZ(),1)/pow(ket.op->n,2) *0.5 ) ) {
			    truncc = true;
			    continue; // truncation boolean?
			}
			sumd = 0;
			truncd = false;
			for (int Nd = 0; Nd <= nmax; Nd++) // 46 because integral returns NaN for n > 46
			{
			    int id = 1000*Nd + 10*ket.oq->n + ket.oq->l;
			    //cout << "Getting Dnd@" << id << endl;
			    double Dnd = modelspace.OsToHydroCoeffList[id];
			    //cout << "Getting Dnd at " << id << "=" << Dnd << " Dnc@" << ic << "=" << Dnc << " Dnb@" << ib << "=" << Dnb << " Dna@" << ia << "=" << Dna << endl;
			    if ( abs(Dnd) < tol ) {
				cout << "Dnd too small; Dnd=" << Dnd << " at id=" << id << endl;
				continue;
			    }
			    sumd += (2*Nd + ket.oq->l + 3/2)*pow(Dnd,2);
			    if ( (sumd > pow(modelspace.GetTargetZ(),2)/pow(ket.oq->n,2) *0.5 ) ) {
				truncd = true;
				continue; // truncation boolean?
			    }

			    int n1, n2, n3, n4, l1, l2, l3, l4, j1, j2, j3, j4;
			    if ( Na <= Nb ) {
				n1 = Na;
				n2 = Nb;
				l1 = bra.op->l;
				l2 = bra.oq->l;
				j1 = bra.op->j2;
				j2 = bra.oq->j2;
			    } else {
				n2 = Na;
				n1 = Nb;
				l2 = bra.op->l;
				l1 = bra.oq->l;
				j2 = bra.op->j2;
				j1 = bra.oq->j2;
			    }
			    if ( Nc <= Nd ) {
				n3 = Nc;
				n4 = Nd;
				l3 = ket.op->l;
				l4 = ket.oq->l;
				j3 = ket.op->j2;
				j4 = ket.oq->j2;
			    } else {
				n4 = Nc;
				n3 = Nd;
				l4 = ket.op->l;
				l3 = ket.oq->l;
				j4 = ket.op->j2;
				j3 = ket.oq->j2;
			    }
			   /* if ( n1 > n3 ) {
				swap( n1, n3 );
				swap( n2, n4 );
				swap( l1, l3 );
				swap( l2, l4 );
				swap( j1, j3 );
				swap( j2, j4 );
			    } */
			    long long unsigned int index = n1*1 +
							   n2*100 +
							   n3*10000 +
							   n4*1000000 +
							   l1*100000000 +
							   l2*1000000000 +
							   l3*10000000000 +
							   l4*100000000000 +
							   j1*1000000000000 +
							   j2*10000000000000 +
							   j3*100000000000000 +
							   j4*1000000000000000 + // Could probably figure out a better indexing function.
							tbc.J*10000000000000000;

			  /*  long long unsigned int index = Na*1 +
							   Nb*100 +
							   Nc*10000 +
							   Nd*1000000 +
						    bra.op->l*100000000 +
						   bra.op->j2*1000000000 +
						    bra.oq->l*10000000000 +
						   bra.oq->j2*100000000000 +
						    ket.op->l*1000000000000 +
						   ket.op->j2*10000000000000 +
						    ket.oq->l*100000000000000 +
						   ket.oq->j2*1000000000000000 +// Could probably figure out a better indexing function.
							tbc.J*10000000000000000; */

			    if ( Mat_El_List.find(index) != Mat_El_List.end() ) {
				//cout << "Already found element, getting from list" << endl;
				result = Mat_El_List[index];
			    } else {
				//cout << "Not in list, calculating." << endl;
				Orbit od = Orbit(Nd, ket.oq->l, ket.oq->j2, ket.oq->tz2, ket.oq->occ, ket.oq->cvq, ket.oq->index);
				//if ( od.j2 + oc.j2 != oa.j2 + ob.j2 ) continue; // cons of angular momentum.
				//int osLmax = oc.l + od.l;
				//int osLmin = abs(oc.l-od.l);
				//if ( oa.l + ob.l > osLmax or abs(oa.l - ob.l) < osLmin ) continue; // Cons of Ang Mom. Should be taken care of in Kets, I think
				//if ( 2*Nc + ket.op->l + 3./2 + 2*Nd + ket.oq->l + 3./2 >= modelspace.GetTargetZ() * modelspace.GetTargetZ() * modelspace.GetHbarOmega()/(min(ket.oq->n,ket.op->n)^2) )continue; // Crazy energy.
			        Ket ketp = Ket(oc, od);
				result = Corr_Invr_Hydrogen(modelspace, brap, ketp, tbc.J);
				if ( std::isnan( result ) ) continue; // Should find a better way of dealing/avoiding with NaN results.
				if ( abs(result) < tol ) {
				    //cout << "Result too small, result = " << result << endl;
				    continue;
				}
				#pragma omp critical
				local_List[index] = result;
			    }

			    //result = Corr_Invr_Hydrogen(modelspace, brap, ketp, tbc.J);//Need to cache these.
			    //cout << "Result=" << result << endl;
			    /* if ( Dna*Dnb*Dnc*Dnd*result < 0 ) {
				cout << "Negative result=" << result << endl;
				cout << "Na=" << Na << " bra.op->n=" << bra.op->n << " bra.op->l=" << bra.op->l << " bra.op->j2=" << bra.op->j2 << endl;
				cout << "Nb=" << Nb << " bra.oq->n=" << bra.oq->n << " bra.oq->l=" << bra.oq->l << " bra.oq->j2=" << bra.oq->j2 << endl;
				cout << "Nc=" << Nc << " ket.op->n=" << ket.op->n << " ket.op->l=" << ket.op->l << " ket.op->j2=" << ket.op->j2 << endl;
				cout << "Nd=" << Nd << " ket.oq->n=" << ket.oq->n << " ket.oq->l=" << ket.oq->l << " ket.oq->j2=" << ket.oq->j2 << endl;
				cout << "Dna=" << Dna << " Dnb=" << Dnb << " Dnc=" << Dnc << " Dnd=" << Dnd << endl;
			    } */
			    mat_el += Dna*Dnb*Dnc*Dnd*result;
			    //cout << "Mat_el=" << mat_el << endl;
            		} // Nd
		    } // Nc
		} // Nb
		#pragma omp critical
		Mat_El_List.insert( local_List.begin(), local_List.end() );
	    } // Na
	    //if (abs(mat_el) >= 10) cout << "abs(mat_el) = " << mat_el << " at ibra=" << ibra << " iket=" << iket << " in ch=" << ch << endl;
	    /* #pragma omp critical
	    if ( mat_el < 0 ) {
		cout << "Negative Mat_el for ibra=" << ibra << " iket=" << iket << " tbc.J=" << tbc.J << endl;
		cout << "bra.op->n=" << bra.op->n << " bra.op->l=" << bra.op->l << " bra.op->j2=" << bra.op->j2 << endl;
		cout << "bra.oq->n=" << bra.oq->n << " bra.oq->l=" << bra.oq->l << " bra.oq->j2=" << bra.oq->j2 << endl;
		cout << "ket.op->n=" << ket.op->n << " ket.op->l=" << ket.op->l << " ket.op->j2=" << ket.op->j2 << endl;
		cout << "ket.oq->n=" << ket.oq->n << " ket.oq->l=" << ket.oq->l << " ket.oq->j2=" << ket.oq->j2 << endl;
	    } */
	    //cout << "Mat_el=" << mat_el << endl;
            E.TwoBody.SetTBME(ch,ch,ibra,iket,mat_el);
            E.TwoBody.SetTBME(ch,ch,iket,ibra,mat_el);
         }
      }
   }
   E.profiler.timer["CorrE2b_Hydrogen"] += omp_get_wtime() - t_start;
   cout << "Exiting Hydrogen two body." << endl;
   return E;
 }




double Corr_Invr_Hydrogen(ModelSpace& modelspace, Ket & bra, Ket & ket, int J)
{
   //Orbit & oa = bra.p;
   //Orbit & ob = bra.q;
   //Orbit & oc = ket.p;
   //Orbit & od = ket.q;

   int na = bra.op->n;
   int nb = bra.oq->n;
   int nc = ket.op->n;
   int nd = ket.oq->n;

   //cout << "na=" << na << " nb=" << nb << " nc=" << nc << " nd=" << nd << endl;

   int la = bra.op->l;
   int lb = bra.oq->l;
   int lc = ket.op->l;
   int ld = ket.oq->l;

   //cout << "la=" << la << " lb=" << lb << " lc=" << lc << " ld=" << ld << endl;

   double ja = bra.op->j2/2.0;
   double jb = bra.oq->j2/2.0;
   double jc = ket.op->j2/2.0;
   double jd = ket.oq->j2/2.0;

   //cout << "ja=" << ja << " jb=" << jb << " jc=" << jc << " jd=" << jd << endl;

   int fab = 2*na + 2*nb + la + lb;
   int fcd = 2*nc + 2*nd + lc + ld;
   //if (abs(fab-fcd)%2 >0) return 0; // p1*p2 only connects kets with delta N = 0,1
   //if (abs(fab-fcd)>2) return 0; // p1*p2 only connects kets with delta N = 0,1

   double sa,sb,sc,sd;
   sa=sb=sc=sd=0.5;

   double invr=0;

   // First, transform to LS coupling using 9j coefficients
   for (int Lab=abs(la-lb); Lab<= la+lb; ++Lab)
   {
     for (int Sab=0; Sab<=1; ++Sab)
     {
       if ( abs(Lab-Sab)>J or Lab+Sab<J) continue;

       double njab = modelspace.GetNineJ(la,sa,ja, lb,sb,jb, Lab,Sab,J);
       //if (njab < 0) cout << "njab=" << njab << " la=" << la << " sa=" << sa << " ja=" << ja << " lb=" << lb << " sb=" << sb << " jb=" << jb << " Lab=" << Lab << " Sab=" << Sab << " J=" << J << endl;
       if (njab == 0) {
	   //cout << "njab is zero." << endl;
	   continue;
       }
       int Scd = Sab;
       int Lcd = Lab;
       double njcd = modelspace.GetNineJ(lc,sc,jc, ld,sd,jd, Lcd,Scd,J);
       //if (njcd < 0) cout << "njcd=" << njcd << " lc=" << lc << " sa=" << sc << " jc=" << jc << " ld=" << ld << " sd=" << sd << " jd=" << jd << " Lcd=" << Lcd << " Scd=" << Scd << " J=" << J << endl;
       if (njcd == 0) {
	   //cout << "njcd is zero." << endl;
	   continue;
       }
       // Next, transform to rel / com coordinates with Moshinsky tranformation
       for (int N_ab=0; N_ab<=fab/2; ++N_ab)  // N_ab = CoM n for a,b
       {
         for (int Lam_ab=0; Lam_ab<= fab-2*N_ab; ++Lam_ab) // Lam_ab = CoM l for a,b
         {
           int Lam_cd = Lam_ab; // tcm and trel conserve lam and Lam, ie relative and com orbital angular momentum
           for (int lam_ab=(fab-2*N_ab-Lam_ab)%2; lam_ab<= (fab-2*N_ab-Lam_ab); lam_ab+=2) // lam_ab = relative l for a,b
           {
              if (Lab<abs(Lam_ab-lam_ab) or Lab>(Lam_ab+lam_ab) ) continue;
              // factor to account for antisymmetrization

              //int asymm_factor = (abs(bra.op->tz2+ket.op->tz2) + abs(bra.op->tz2+ket.oq->tz2)*modelspace.phase( lam_ab + Sab ))/ 2; // Shouldn't need this ?
              //if ( asymm_factor ==0 ) continue;

              int lam_cd = lam_ab; // tcm and trel conserve lam and Lam
              int n_ab = (fab - 2*N_ab-Lam_ab-lam_ab)/2; // n_ab is determined by energy conservation
	      if (n_ab < 0 or N_ab < 0) continue; // lam_ab >= n_ab or

              double mosh_ab = modelspace.GetMoshinsky(N_ab,Lam_ab,n_ab,lam_ab,na,la,nb,lb,Lab);
	      //if (mosh_ab < 0) cout << "Mosh_ab=" << mosh_ab << " N_ab=" << N_ab << " Lam_ab=" << Lam_ab << " n_ab=" << n_ab << " lam_ab=" << lam_ab << " na=" << na << " la=" << la << " nb=" << nb << " lb=" << lb << " Lab=" << Lab;
              if (abs(mosh_ab)<1e-9) {
		   //cout << "Mosh_ab too small, mosh_ab=" << mosh_ab << endl;
		   continue;
	      }
              for (int N_cd=max(0,N_ab-1); N_cd<=N_ab+1; ++N_cd) // N_cd = CoM n for c,d
              {
                int n_cd = (fcd - 2*N_cd-Lam_cd-lam_cd)/2; // n_cd is determined by energy conservation
                if (n_cd < 0 or N_cd < 0) continue; // or lam_cd >= n_cd or
                //if  (n_ab != n_cd and N_ab != N_cd) continue;

                double mosh_cd = modelspace.GetMoshinsky(N_cd,Lam_cd,n_cd,lam_cd,nc,lc,nd,ld,Lcd);
		//if (mosh_cd < 0) cout << "Mosh_cd=" << mosh_cd << " N_cd=" << N_cd << " Lam_cd=" << Lam_cd << " n_cd=" << n_cd << " lam_cd=" << lam_cd << " nc=" << nc << " lc=" << lc << " nd=" << nd << " ld=" << ld << " Lcd=" << Lcd;
                if (abs(mosh_cd)<1e-9) {
		    //cout << "Mosh_cd too small, mosh_cd=" << mosh_cd << endl;
		    continue;
		}
                //double rad = RadialIntegral(n_ab, lam_ab, n_cd, lam_cd, -1, modelspace); // Not valid for atomic systems.
		//if ( 2*n_ab + lam_ab + 3./2 >= modelspace.GetTargetZ() * modelspace.GetTargetZ() * modelspace.GetHbarOmega()/2 ) continue; // Crazy energy.
		//if ( 2*n_cd + lam_cd + 3./2 >= modelspace.GetTargetZ() * modelspace.GetTargetZ() * modelspace.GetHbarOmega()/2 ) continue; // Crazy energy.
		/* unsigned long long int radkey = 0;
		if ( n_ab < n_cd) {
		    radkey = 1e6*n_ab
			+ 1e2*lam_ab
			+ 1e4*n_cd
			+ lam_cd;
		} else {
		    radkey = 1e6*n_cd
			+ 1e2*lam_cd
			+ 1e4*n_ab
			+ lam_ab;
		} */
		//double rad = modelspace.radList[radkey];
		//cout << "n_ab=" << n_ab << " lam_ab=" << lam_ab << " n_cd=" << n_cd << " lam_cd=" << lam_cd << endl;
		double rad = getRadialIntegral(n_ab, lam_ab, n_cd, lam_cd, modelspace);
		//cout << "Rad=" << rad << endl;
		if (abs(rad) < 1e-9) {
		    //cout << "Rad is too small Rad=" << rad << endl;
		    continue;
		}
		/* if (abs(rad) > 1) {
		    cout << "n_ab=" << n_ab << " lam_ab=" << lam_ab << " n_cd=" << n_cd << " lam_cd=" << lam_cd << endl;
		    cout << "Rad=" << rad << endl;
		} */

		invr += njab * njcd * mosh_ab * mosh_cd * rad / sqrt(2) * HBARC / 137. / BOHR_RADIUS;

              } // N_cd
           } // lam_ab
         } // Lam_ab
       } // N_ab

     } // Sab
   } // Lab
   // if (invr < 0) cout << "Invr=" << invr << " oa.index=" << oa.index << " ob.index=" << ob.index << " oc.index=" << oc.index << " od.index=" << od.index << endl;
   return invr ;
}






/// Center of mass Hamiltonian
/// \f[
/// \begin{align}
/// H_{CM} &= T_{CM} + \frac{1}{2} Am\omega^2 R^2 \\
///        &= T_{CM} + \frac{1}{2b^2} AR^2 \hbar\omega
/// \end{align}
/// \f]
 Operator HCM_Op(ModelSpace& modelspace)
 {
   double hw = modelspace.GetHbarOmega();
   int A = modelspace.GetTargetMass();
//   double oscillator_b2 = HBARC*HBARC/M_NUCLEON/hw;
//   Operator HcmOp = TCM_Op(modelspace) + R2CM_Op(modelspace) * (0.5*A * hw / oscillator_b2);
   Operator HcmOp = TCM_Op(modelspace) + 0.5*A*M_NUCLEON*hw*hw/HBARC/HBARC * R2CM_Op(modelspace) ;
   return HcmOp;
 }



/// Returns
/// \f[ r^2 = \sum_{i} r_{i}^2 \f]
///
Operator RSquaredOp(ModelSpace& modelspace)
{
   Operator r2 = Operator(modelspace);
   r2.OneBody.zeros();
   unsigned int norbits = modelspace.GetNumberOrbits();
   double hw = modelspace.GetHbarOmega();
   for (unsigned int a=0;a<norbits;++a)
   {
      Orbit & oa = modelspace.GetOrbit(a);
      r2.OneBody(a,a) = (2*oa.n + oa.l +1.5);
      for ( unsigned int b : r2.OneBodyChannels.at({oa.l, oa.j2, oa.tz2}) )
      {
        if ( b < a ) continue;
        Orbit & ob = modelspace.GetOrbit(b);
        {
           if (oa.n == ob.n+1)
              r2.OneBody(a,b) = -sqrt( (oa.n)*(oa.n + oa.l +0.5));
           else if (oa.n == ob.n-1)
              r2.OneBody(a,b) = -sqrt( (ob.n)*(ob.n + ob.l +0.5));
           r2.OneBody(b,a) = r2.OneBody(a,b);
        }
      }
   }
   r2.OneBody *= (HBARC*HBARC/M_NUCLEON/hw);
   return r2;
}


 Operator R2_p1_Op(ModelSpace& modelspace)
 {
   return R2_1body_Op(modelspace,"proton");
 }

/// One-body part of the proton charge radius operator.
/// Returns
/// \f[
/// \hat{R}^{2}_{p1} = \sum_{i} e_{i}{r}_i^2
/// \f]
 Operator R2_1body_Op(ModelSpace& modelspace,string option)
 {
   Operator r2(modelspace);
   double oscillator_b = (HBARC*HBARC/M_NUCLEON/modelspace.GetHbarOmega());

   auto orbitlist = modelspace.proton_orbits;
   if (option == "neutron") orbitlist = modelspace.neutron_orbits;
   else if (option == "matter")  orbitlist.insert(orbitlist.end(),modelspace.neutron_orbits.begin(),modelspace.neutron_orbits.end());
   else if (option != "proton") cout << "!!! WARNING. BAD OPTION "  << option << " FOR imsrg_util::R2_p1_Op !!!" << endl;

   for (unsigned int a : orbitlist )
   {
      Orbit & oa = modelspace.GetOrbit(a);
      r2.OneBody(a,a) = (2*oa.n + oa.l +1.5);
      for ( unsigned int b : r2.OneBodyChannels.at({oa.l, oa.j2, oa.tz2}) )
      {
        if ( b < a ) continue;
        Orbit & ob = modelspace.GetOrbit(b);
        {
           if (oa.n == ob.n+1)
              r2.OneBody(a,b) = -sqrt( (oa.n)*(oa.n + oa.l +0.5));
           else if (oa.n == ob.n-1)
              r2.OneBody(a,b) = -sqrt( (ob.n)*(ob.n + ob.l +0.5));
           r2.OneBody(b,a) = r2.OneBody(a,b);
        }
      }
   }
   r2.OneBody *= oscillator_b;
   return r2;
 }

 Operator R2_p2_Op(ModelSpace& modelspace)
 {
   return R2_2body_Op(modelspace,"proton");
 }

/// Two-body part of the proton charge radius operator.
/// Returns
/// \f[
/// \hat{R}^{2}_{p2} = \sum_{i\neq j} e_{i}\vec{r}_i\cdot\vec{r}_j
/// \f]
/// evaluated in the oscillator basis.
 Operator R2_2body_Op(ModelSpace& modelspace,string option)
 {
   Operator Rp2Op(modelspace,0,0,0,2);
   double oscillator_b = (HBARC*HBARC/M_NUCLEON/modelspace.GetHbarOmega());

   int nchan = modelspace.GetNumberTwoBodyChannels();
//   modelspace.PreCalculateMoshinsky();
   #pragma omp parallel for schedule(dynamic,1)
   for (int ch=0; ch<nchan; ++ch)
   {
      TwoBodyChannel& tbc = modelspace.GetTwoBodyChannel(ch);
      int nkets = tbc.GetNumberKets();
      for (int ibra=0;ibra<nkets;++ibra)
      {
         Ket & bra = tbc.GetKet(ibra);
         if (option=="proton" and bra.op->tz2>0) continue;
         else if (option=="neutron" and bra.op->tz2<0) continue;
         else if (option!="matter" and option!="proton" and option!="neutron") cout << "!!! WARNING. BAD OPTION "  << option << " FOR imsrg_util::R2_p2_Op !!!" << endl;
         for (int iket=ibra;iket<nkets;++iket)
         {
            Ket & ket = tbc.GetKet(iket);
            double mat_el = Calculate_r1r2(modelspace,bra,ket,tbc.J) * oscillator_b ;
            Rp2Op.TwoBody.SetTBME(ch,ibra,iket,mat_el);
            Rp2Op.TwoBody.SetTBME(ch,iket,ibra,mat_el);
         }
      }
   }
   return Rp2Op;
 }


Operator ProtonDensityAtR(ModelSpace& modelspace, double R)
{
  Operator Rho(modelspace,0,0,0,2);
  for ( auto i : modelspace.proton_orbits)
  {
    Orbit& oi = modelspace.GetOrbit(i);
    Rho.OneBody(i,i) = HO_density(oi.n,oi.l,modelspace.GetHbarOmega(),R);
  }
  return Rho;
}

Operator NeutronDensityAtR(ModelSpace& modelspace, double R)
{
  Operator Rho(modelspace,0,0,0,2);
  for ( auto i : modelspace.neutron_orbits)
  {
    Orbit& oi = modelspace.GetOrbit(i);
    Rho.OneBody(i,i) = HO_density(oi.n,oi.l,modelspace.GetHbarOmega(),R);
  }
  return Rho;
}


Operator RpSpinOrbitCorrection(ModelSpace& modelspace)
{
  Operator dr_so(modelspace,0,0,0,2);
  double M2 = M_NUCLEON*M_NUCLEON/(HBARC*HBARC);
  int norb = modelspace.GetNumberOrbits();
  for (int i=0;i<norb;i++)
  {
    Orbit& oi = modelspace.GetOrbit(i);
    double mu_i = oi.tz2<0 ? 1.79 : -1.91;
    int kappa = oi.j2 < 2*oi.l ? oi.l : -(oi.l+1);
    dr_so.OneBody(i,i) = -mu_i/M2*(kappa+1);
  }
  return dr_so;
}

// Electric monopole operator
/// Returns
/// \f[ r_{e}^2 = \sum_{i} e_{i} r_{i}^2 \f]
///
Operator E0Op(ModelSpace& modelspace)
{
   Operator e0(modelspace);
   e0.EraseZeroBody();
   e0.OneBody.zeros();
//   unsigned int norbits = modelspace.GetNumberOrbits();
   double hw = modelspace.GetHbarOmega();
   for (unsigned int a : modelspace.proton_orbits)
   {
      Orbit & oa = modelspace.GetOrbit(a);
      e0.OneBody(a,a) = (2*oa.n + oa.l +1.5);
      for (unsigned int b : e0.OneBodyChannels.at({oa.l,oa.j2,oa.tz2}) )
      {
        if (b<=a) continue;
        Orbit & ob = modelspace.GetOrbit(b);
        {
           if (oa.n == ob.n+1)
              e0.OneBody(a,b) = -sqrt( (oa.n)*(oa.n + oa.l +0.5));
           else if (oa.n == ob.n-1)
              e0.OneBody(a,b) = -sqrt( (ob.n)*(ob.n + ob.l +0.5));
           e0.OneBody(b,a) = e0.OneBody(a,b);
        }
      }
   }
   e0.OneBody *= (HBARC*HBARC/M_NUCLEON/hw);
   return e0;
}

struct FBCIntegrandParameters{int n; int l; double hw;};

double FBCIntegrand(double x, void *p)
{
  struct FBCIntegrandParameters * params = (struct FBCIntegrandParameters *)p;
  return x*HO_density(params->n, params->l, params->hw, x);
}

Operator FourierBesselCoeff(ModelSpace& modelspace, int nu, double R, vector<index_t> index_list)
{
  Operator a_nu(modelspace,0,0,0,2);
  double omega = nu * M_PI / R; // coefficient of sine function, i.e. sin(omega*x)
  double L = R; // range of integration
  size_t n = 20; // number of bisections
  gsl_integration_qawo_table * table = gsl_integration_qawo_table_alloc (omega, L, GSL_INTEG_SINE, n);
  gsl_integration_workspace * workspace = gsl_integration_workspace_alloc ( n );

  const double epsabs = 1e-5; // absolute error
  const double epsrel = 1e-5; // relative error
  const size_t limit = n; // maximum number of subintervals (maybe should be different?)
  const double start = 0.0; // lower limit on integration range
  double result;
  double  abserr;
  gsl_function F;
  F.function = &FBCIntegrand;

  for (auto i : index_list )
  {
    Orbit& oi = modelspace.GetOrbit(i);
    struct FBCIntegrandParameters params = {oi.n, oi.l, modelspace.GetHbarOmega()};
    F.params = &params;
    //int status = gsl_integration_qawo (&F, start, epsabs, epsrel, limit, workspace, table, &result, &abserr);
    gsl_integration_qawo (&F, start, epsabs, epsrel, limit, workspace, table, &result, &abserr);
    a_nu.OneBody(i,i) = M_PI*M_PI/R/R/R * R/nu/M_PI*(result);
    cout << "orbit,nu = " << i << "," << nu << "  => " << a_nu.OneBody(i,i) << "  from " << result << " (" << abserr << ")" << endl;
  }
  return a_nu;
}



/// Returns the \f$ T^{2} \f$ operator
 Operator Isospin2_Op(ModelSpace& modelspace)
 {
   Operator T2 = Operator(modelspace,0,0,0,2);
   T2.OneBody.diag().fill(0.75);

   for (int ch=0; ch<T2.nChannels; ++ch)
   {
     TwoBodyChannel& tbc = modelspace.GetTwoBodyChannel(ch);
     arma::mat& TB = T2.TwoBody.GetMatrix(ch);
     // pp,nn:  2<t2.t1> = 1/(2(1+delta_ab)) along diagonal
     if (abs(tbc.Tz) == 1)
     {
        TB.diag().fill(0.5); // pp,nn TBME's
        for (int ibra=0;ibra<tbc.GetNumberKets(); ++ibra)
        {
           Ket& bra = tbc.GetKet(ibra);
           if (bra.p == bra.q)
           {
             TB(ibra,ibra) /= 2.;
           }
        }
     }
     else if (tbc.Tz == 0)
     {
        for (int ibra=0;ibra<tbc.GetNumberKets(); ++ibra)
        {
           Ket& bra = tbc.GetKet(ibra);
           Orbit& oa = modelspace.GetOrbit(bra.p);
           Orbit& ob = modelspace.GetOrbit(bra.q);
           for (int iket=ibra;iket<tbc.GetNumberKets(); ++iket)
           {
             Ket& ket = tbc.GetKet(iket);
             Orbit& oc = modelspace.GetOrbit(ket.p);
             Orbit& od = modelspace.GetOrbit(ket.q);
             if (oa.j2==oc.j2 and oa.n==oc.n and oa.l==oc.l
               and ob.j2==od.j2 and ob.n==od.n and ob.l==od.l )
             {
               // tz1 tz2 case
               if( oa.tz2 == oc.tz2 and ob.tz2==od.tz2)
               {
                 TB(ibra,iket) -= 0.5;
               }
               // t+ t- case
               if( oa.tz2 == od.tz2 and ob.tz2==oc.tz2)
               {
                 TB(ibra,iket) += 1.0;
               }
               // if a==b==c==d, we need to consider the exchange term
               if (oa.j2==ob.j2 and oa.n==ob.n and oa.l==ob.l)
               {
                  int phase = bra.Phase(tbc.J);
                  // tz1 tz2 case
                  if( oa.tz2 == oc.tz2 and ob.tz2==od.tz2)
                  {
                    TB(ibra,iket) += phase * 1.0;
                  }
                  // t+ t- case
                  if( oa.tz2 == od.tz2 and ob.tz2==oc.tz2)
                  {
                    TB(ibra,iket) -= phase * 0.5;
                  }
               }
               TB(iket,ibra) = TB(ibra,iket); // hermitian
             }

           }
        }
     }
   }
   return T2;
 }



  /// Returns a reduced electric multipole operator with units \f$ e\f$ fm\f$^{\lambda} \f$
  /// See Suhonen eq. (6.23)
  Operator ElectricMultipoleOp(ModelSpace& modelspace, int L)
  {
    Operator EL(modelspace, L,0,L%2,2);
    double bL = pow( HBARC*HBARC/M_NUCLEON/modelspace.GetHbarOmega(),0.5*L); // b^L where b=sqrt(hbar/mw)
    for (int i : modelspace.proton_orbits)
    {
      Orbit& oi = modelspace.GetOrbit(i);
      double ji = 0.5*oi.j2;
      for ( int j : EL.OneBodyChannels.at({oi.l, oi.j2, oi.tz2}) )
      {
        if (j<i) continue;
        Orbit& oj = modelspace.GetOrbit(j);
        double jj = 0.5*oj.j2;
        double r2int = RadialIntegral(oi.n,oi.l,oj.n,oj.l,L,modelspace) * bL ;
        EL.OneBody(i,j) = modelspace.phase(jj+L-0.5) * sqrt( (2*ji+1)*(2*jj+1)*(2*L+1)/4./3.1415926) * AngMom::ThreeJ(ji,jj, L, 0.5, -0.5,0) * r2int;
        EL.OneBody(j,i) = modelspace.phase((oi.j2-oj.j2)/2) * EL.OneBody(i,j);
      }
    }
    return EL;
  }

  /// Returns a reduced magnetic multipole operator with units \f$ \mu_{N}\f$ fm\f$ ^{\lambda-1} \f$
  Operator MagneticMultipoleOp(ModelSpace& modelspace, int L)
  {
    return MagneticMultipoleOp_pn(modelspace,L,"both");
  }

  /// Returns a reduced magnetic multipole operator with units \f$ \mu_{N}\f$ fm\f$ ^{\lambda-1} \f$
  /// This version allows for the selection of just proton or just neutron contributions, or both.
  /// See Suhonen eq. (6.24)
  Operator MagneticMultipoleOp_pn(ModelSpace& modelspace, int L, string pn)
  {
    double bL = pow( HBARC*HBARC/M_NUCLEON/modelspace.GetHbarOmega(),0.5*(L-1));
    Operator ML(modelspace, L,0,(L+1)%2,2);
    if (L<1)
    {
      cout << "A magnetic monopole operator??? Setting it to zero..." << endl;
      return ML;
    }
    int norbits = modelspace.GetNumberOrbits();
    for (int i=0; i<norbits; ++i)
    {
      Orbit& oi = modelspace.GetOrbit(i);
      if (pn=="proton" and oi.tz2>0) continue;
      if (pn=="neutron" and oi.tz2<0) continue;
      double gl = oi.tz2<0 ? 1.0 : 0.0;
      double gs = oi.tz2<0 ? 5.586 : -3.826;
      double ji = 0.5*oi.j2;
      for ( int j : ML.OneBodyChannels.at({oi.l, oi.j2, oi.tz2}) )
      {
        if (j<i) continue;
        Orbit& oj = modelspace.GetOrbit(j);
        double jj = 0.5*oj.j2;
        // multiply radial integral by b^L-1 = (hbar/mw)^L-1/2
        double r2int = RadialIntegral(oi.n,oi.l,oj.n,oj.l,L-1,modelspace) * bL;
        double kappa =  modelspace.phase(oi.l+ji+0.5) * (ji+0.5)  +  modelspace.phase(oj.l+jj+0.5) * (jj+0.5);
        ML.OneBody(i,j) = modelspace.phase(jj+L-0.5) * sqrt( (2*ji+1)*(2*jj+1)*(2*L+1)/4./3.1415926) * AngMom::ThreeJ(ji, jj,L, 0.5,-0.5,0)
                        * (L - kappa) *(gl*(1+kappa/(L+1.))-0.5*gs )  * r2int ;
        ML.OneBody(j,i) = modelspace.phase((oi.j2-oj.j2)/2) * ML.OneBody(i,j);
      }
    }
    return ML;
  }

/// Evaluate the radial integral \f[
/// \tilde{\mathcal{R}}^{\lambda}_{ab} = \int_{0}^{\infty} dx \tilde{g}_{n_a\ell_a}(x)x^{\lambda+2}\tilde{g}_{n_b\ell_b}(x)
/// \f]
/// where \f$ \tilde{g}(x) \f$ is the radial part of the harmonic oscillator wave function with unit oscillator length \f$ b=1 \f$
/// and \f$ x = r/b \f$.
/// To obtain the radial integral for some other oscillator length, multiply by \f$ b^{\lambda} \f$.
/// This implementation uses eq (6.41) from Suhonen.
/// Note this is only valid for \f$ \ell_a+\ell_b+\lambda\f$ = even.
/// If \f$ \ell_a+\ell_b+\lambda\f$ is odd, RadialIntegral_RpowK() is called.
  double RadialIntegral(int na, int la, int nb, int lb, int L, ModelSpace& ms)
  {
    double rad = RadialIntegral_RpowK(na,la,nb,lb,L,ms);
    if ( na == 25 and nb == 25 and la == 0 and lb == 0 and L == -1 ) cout << "At point, Rad=" << rad << " should be -4207096.773437500000000000." << endl;
    if ((la+lb+L)%2!=0) return RadialIntegral_RpowK(na,la,nb,lb,L,ms);
    int tau_a = max((lb-la+L)/2,0);
    int tau_b = max((la-lb+L)/2,0);
    int sigma_min = max(max(na-tau_a,nb-tau_b),0);
    int sigma_max = min(na,nb);

    double term1 = AngMom::phase(na+nb) * gsl_sf_fact(tau_a)*gsl_sf_fact(tau_b) * sqrt(gsl_sf_fact(na)*gsl_sf_fact(nb)
                   / (gsl_sf_gamma(na+la+1.5)*gsl_sf_gamma(nb+lb+1.5) ) );
    double term2 = 0;
    for (int sigma=sigma_min; sigma<=sigma_max; ++sigma)
    {
      term2 += gsl_sf_gamma(0.5*(la+lb+L)+sigma+1.5) / (gsl_sf_fact(sigma)*gsl_sf_fact(na-sigma)*gsl_sf_fact(nb-sigma)*gsl_sf_fact(sigma+tau_a-na)*gsl_sf_fact(sigma+tau_b-nb) );
    }
    return term1*term2;

  }


 double RadialIntegral_RpowK(int na, int la, int nb, int lb, int k, ModelSpace& ms)
 {
   double I = 0;
   int pmin = (la+lb)/2;
   int pmax = pmin + na + nb;
   for (int p=pmin;p<=pmax;++p)
   {
      I += TalmiB(na,la,nb,lb,p,ms) * TalmiI(p,k);
   }
   return I;
 }

/// General Talmi integral for a potential r**k
/// 1/gamma(p+3/2) * 2*INT dr r**2 r**2p r**k exp(-r**2/b**2)
/// This is valid for (2p+3+k) > 0. The Gamma function diverges for non-positive integers.
 double TalmiI(int p, double k)
 {
   return gsl_sf_gamma(p+1.5+0.5*k) / gsl_sf_gamma(p+1.5);
 }

 vector<double> ser(double j) // takes j, creates {1,2,...,j}
 {
    //cout << "Generating vector for j=" << j << endl;
    if (j <= 0){
	vector<double> s = {1};
	return s;
    }
    vector<double> s;
    for (double i=1; i <= j; i++)
    {
	s.emplace_back(i);
    }
    return s;
 }

 bool isOnes(vector<double> a)
 {
    for (double i : a)
    {
	if (i != 1) return false;
    }
    return true;
 }


 // Takes in a vector numerator and vector denominator of the form {1,2,..,a,1,2,...b,1,2,..,c,...} for a!b!c! and removes like terms that occur in both numerator and denominator.
 //boost::multiprecision::cpp_bin_float_100 simplefact(vector<double> n, vector<double> d, bool isSquare)
 double simplefact(vector<double> n, vector<double> d, bool isSquare) // removes like terms in the factorials, returns n!/d!
 {
    vector<double> a = n;
    vector<double> b = d;

    //if (n.size() < d.size()) // I forgot why I put this in here, but I think I had a good reason.
    //{
	//a.swap(b);
    //}
    bool done = false;
    int bailout = a.size() * b.size(); // Arbitrary choice, number of terms in N * number of terms in D
    while (!done and bailout >= 0)
    {
        for (double& i : a)
        {
	    for (double& j : b)
	    {
		if (i == j and i > 1 and j > 1)
		{
		    i = 1; // set value to 1; ideally, should erase to save space/comp time, but it's throwing an error.
		    j = 1; // set value to 1; ideally, should erase to save space/comp time, but it's throwing an error.
		}
	    }
	}
	bool a1 = isOnes(a);
        bool b1 = isOnes(b);
	bailout--; // just in case we go too many times.
	if (a1 or b1) done = true;
    }
    a.erase(std::remove(a.begin()+1, a.end(), 1), a.end()); // Erase all but the first "1"
    b.erase(std::remove(b.begin()+1, b.end(), 1), b.end()); // If the first "1" is erase as well, can end up with empty array.

    double t1 = 1;
    double t2 = 1;
    #pragma omp parallel
    {
    for (double i : a)
    {
	if(isSquare){
	    t1 *= sqrt(i);
	} else {
	    t1 *= i;
    	}
    }
    for (double j : b)
    {
	if(isSquare){
	    t2 *= sqrt(j);
	} else {
	    t2 *= j;
    	}
    }
    }
    return double (t1 / t2);
 }

/// Calculate B coefficient for Talmi integral. Formula given in Brody and Moshinsky
/// "Tables of Transformation Brackets for Nuclear Shell-Model Calculations"
 double TalmiB(int na, int la, int nb, int lb, int p, ModelSpace& ms)
 {
   if ( (la+lb)%2>0 ) return 0;

   int q = (la+lb)/2;

   double lna = (2*p+1);
   double lnb = (p);
   double lnc = (na);
   double lnd = (nb);
   double lne = (2*na+2*la+1);
   double lnf = (2*nb+2*lb+1);
   double lng = (na+la);
   double lnh = (nb+lb);
   double lMax = max(lna, max(lnb, max(lnc, max(lnd, max(lne, max(lnf, max(lng, lnh)))))));

   //boost::multiprecision::float128 B1 = AngMom::phase(p-q);
   double B1 = AngMom::phase(p-q);
   B1 /= pow(2,(na+nb));

   /* if (lMax > 160){
   	vector<double> al = ser(lna);
   	vector<double> bl = ser(lnb);
   	vector<double> cl = ser(lnc);
   	vector<double> dl = ser(lnd);
   	vector<double> el = ser(lne);
   	vector<double> fl = ser(lnf);
   	vector<double> gl = ser(lng);
   	vector<double> hl = ser(lnh);
   	vector<double> N;
   	N.insert(N.end(), cl.begin(), cl.end());
   	N.insert(N.end(), dl.begin(), dl.end());
   	N.insert(N.end(), el.begin(), el.end());
   	N.insert(N.end(), fl.begin(), fl.end());
   	vector<double> D;
   	D.insert(D.end(), gl.begin(), gl.end());
   	D.insert(D.end(), hl.begin(), hl.end());
   	//cout << "About to calc B1" << endl;
   	//cout << "B1 = " << B1 << endl;
   	//cout << "B1 = " << B1 << endl;
   	B1 *= simplefact(al,bl,false);
   	//cout << "B1 = " << B1 << endl;
   	B1 *= sqrt(simplefact(N,D));//true);
   } else {
	B1 *= gsl_sf_fact(lna);
	B1 /= gsl_sf_fact(lnb);
	B1 *= sqrt(gsl_sf_fact(lnc));
	B1 *= sqrt(gsl_sf_fact(lnd));
	B1 *= sqrt(gsl_sf_fact(lne));
	B1 *= sqrt(gsl_sf_fact(lnf));
	B1 /= sqrt(gsl_sf_fact(lng));
	B1 /= sqrt(gsl_sf_fact(lnh)); */
	B1 *= boost::math::tgamma_ratio( lna+1, lnb+1 );
	B1 *= sqrt( boost::math::tgamma_ratio( lne+1, lng+1 ) );
	B1 *= sqrt( boost::math::tgamma_ratio( lnf+1, lnh+1 ) );
	B1 *= sqrt(gsl_sf_fact(lnc));
	B1 *= sqrt(gsl_sf_fact(lnd));
   //}

/*
   B1 *= ms.GetFactorial(lna);
   B1 /= ms.GetFactorial(lnb);
   B1 *= sqrt(ms.GetFactorial(lnc));
   B1 *= sqrt(ms.GetFactorial(lnd));
   B1 /= sqrt(ms.GetFactorial(lng));
   B1 *= sqrt(ms.GetFactorial(lne));
   B1 *= sqrt(ms.GetFactorial(lnf));
   B1 /= sqrt(ms.GetFactorial(lnh));
*/
   //boost::multiprecision::float128 B2 = 0;
   double B2 = 0;
   int kmin = max(0, p-q-nb);
   int kmax = min(na, p-q);
   for (int k=kmin;k<=kmax;++k)
   {/*
      long double temp = gsl_sf_fact(la+k);
      temp *= gsl_sf_fact(p-int((la-lb)/2)-k);
      temp /= gsl_sf_fact(k);
      temp /= gsl_sf_fact(2*la+2*k+1);
      temp /= gsl_sf_fact(na-k);
      temp /= gsl_sf_fact(2*p-la+lb-2*k+1);
      temp /= gsl_sf_fact(nb - p + q + k);
      temp /= gsl_sf_fact(p-q-k);
    */ /*
      vector<double> t;
      vector<double> ta = ser(p-int((la-lb)/2)-k);
      vector<double> tb = ser(k);
      vector<double> tc = ser(2*la+2*k+1);
      vector<double> td = ser(na-k);
      vector<double> te = ser(2*p-la+lb-2*k+1);
      vector<double> tf = ser(nb - p + q + k);
      vector<double> tg = ser(p-q-k);
      t.insert( t.end(), tb.begin(), tb.end() );
      t.insert( t.end(), tc.begin(), tc.end() );
      t.insert( t.end(), td.begin(), td.end() );
      t.insert( t.end(), te.begin(), te.end() );
      t.insert( t.end(), tf.begin(), tf.end() );
      t.insert( t.end(), tg.begin(), tg.end() );
      B2 += simplefact(ta, t); */

      long double temp = 1;
      //temp *= ms.GetFactorial(la+k);
      //temp *= ms.GetFactorial(p-int((la-lb)/2)-k);
      temp /= ms.GetFactorial(k);
      //temp /= ms.GetFactorial(2*la+2*k+1);
      temp /= ms.GetFactorial(na-k);
      //temp /= ms.GetFactorial(2*p-la+lb-2*k+1);
      temp /= ms.GetFactorial(nb - p + q + k);
      temp /= ms.GetFactorial(p-q-k);
      temp *= boost::math::tgamma_ratio( la+k +1, 2*la+2*k+1 +1);
      temp *= boost::math::tgamma_ratio( p-int((la-lb)/2)-k +1, 2*p-la+lb-2*k+1 +1);
      B2 += temp;
   }
    double retB = (B1 * B2);//.convert_to<double>();
    return retB; //B1 * B2;
 }

  Operator AllowedFermi_Op(ModelSpace& modelspace)
  {
    Operator Fermi(modelspace,0,1,0,2);
    Fermi.SetHermitian();
    int norbits = modelspace.GetNumberOrbits();
    for (int i=0; i<norbits; ++i)
    {
      Orbit& oi = modelspace.GetOrbit(i);
      for (int j : Fermi.OneBodyChannels[{oi.l,oi.j2,oi.tz2}] )
      {
        Orbit& oj = modelspace.GetOrbit(j);
        if (oi.n!=oj.n or oi.tz2 == oj.tz2) continue;
        Fermi.OneBody(i,j) = sqrt(oi.j2+1.0);  // Reduced matrix element
      }
    }
    return Fermi;
  }

/// Note that there is a literature convention to include the 1/sqrt(Lambda) factor
/// in the reduced matrix element rather than in the expression involving the sum
/// over one-body densities (see footnote on pg 165 of Suhonen).
/// I do not follow this convention, and instead produce the reduced matrix element
///  \f[ \langle f \| \sigma \tau_{\pm} \| i \rangle \f]
///
  Operator AllowedGamowTeller_Op(ModelSpace& modelspace)
  {
    Operator GT(modelspace,1,1,0,2);
    GT.SetHermitian();
    int norbits = modelspace.GetNumberOrbits();
    for (int i=0; i<norbits; ++i)
    {
      Orbit& oi = modelspace.GetOrbit(i);
      for (int j : GT.OneBodyChannels[{oi.l,oi.j2,oi.tz2}] )
      {
        Orbit& oj = modelspace.GetOrbit(j);
        if (oi.n!=oj.n or oi.l != oj.l or oi.tz2==oj.tz2) continue;
        double sixj = modelspace.GetSixJ(0.5,0.5,1.0,oj.j2/2.,oi.j2/2.,oi.l);
        double M_gt = 2 * modelspace.phase(oi.l+oi.j2/2.0+1.5) * sqrt((oi.j2+1)*(oj.j2+1)) * sqrt(1.5) * sixj;
        GT.OneBody(i,j) = M_gt;
      }
    }
    return GT;
  }



 /// Pauli spin operator \f[ \langle f \| \sigma \| i \rangle \f]
 Operator Sigma_Op(ModelSpace& modelspace)
 {
   return Sigma_Op_pn(modelspace,"both");
 }

 /// Pauli spin operator \f[ \langle f \| \sigma \| i \rangle \f]
 Operator Sigma_Op_pn(ModelSpace& modelspace, string pn)
 {
   Operator Sig(modelspace,1,0,0,2);
   Sig.SetHermitian();
   int norbits = modelspace.GetNumberOrbits();
   for (int i=0; i<norbits; ++i)
   {
     Orbit& oi = modelspace.GetOrbit(i);
      if (pn=="proton" and oi.tz2>0) continue;
      if (pn=="neutron" and oi.tz2<0) continue;
      for (int j : Sig.OneBodyChannels[{oi.l,oi.j2,oi.tz2}] )
      {
        Orbit& oj = modelspace.GetOrbit(j);
        if ((oi.n!=oj.n) or (oi.l != oj.l) or (oi.tz2!=oj.tz2)) continue;
        double sixj = modelspace.GetSixJ(0.5,0.5,1.0,oj.j2/2.,oi.j2/2.,oi.l);
        double M_sig = 2 * modelspace.phase(oi.l+oi.j2/2.0+1.5) * sqrt((oi.j2+1)*(oj.j2+1)) * sqrt(1.5) * sixj;
        Sig.OneBody(i,j) = M_sig;
      }
   }
   return Sig;
 }

  void Reduce(Operator& X)
  {
    ModelSpace* modelspace = X.GetModelSpace();
    int norbits = modelspace->GetNumberOrbits();
    for (int i=0; i<norbits; ++i)
    {
      Orbit& oi = modelspace->GetOrbit(i);
      for ( int j : X.OneBodyChannels.at({oi.l, oi.j2, oi.tz2}) )
      {
         X.OneBody(i,j) *= sqrt(oi.j2+1.);
      }
    }

    for ( auto& itmat : X.TwoBody.MatEl )
    {
      int ch_bra = itmat.first[0];
      int J = modelspace->GetTwoBodyChannel(ch_bra).J;
      itmat.second *= sqrt(2*J+1.);
    }
  }

  void UnReduce(Operator& X)
  {
    ModelSpace* modelspace = X.GetModelSpace();
    int norbits = modelspace->GetNumberOrbits();
    for (int i=0; i<norbits; ++i)
    {
      Orbit& oi = modelspace->GetOrbit(i);
      for ( int j : X.OneBodyChannels.at({oi.l, oi.j2, oi.tz2}) )
      {
         X.OneBody(i,j) /= sqrt(oi.j2+1.);
      }
    }

    for ( auto& itmat : X.TwoBody.MatEl )
    {
      int ch_bra = itmat.first[0];
      int J = modelspace->GetTwoBodyChannel(ch_bra).J;
      itmat.second /= sqrt(2*J+1.);
    }

  }

  /* // Simple Hydrogen-like one-body bound energy operator
  Operator HydrogenEnergy(Modelspace& modelspace)
  {
  } */

  Operator RadialOverlap(ModelSpace& modelspace)
  {
     Operator OVL(modelspace,0,1,0,1);
     index_t norb = modelspace.GetNumberOrbits();
     for (index_t i=0; i<norb; ++i)
     {
       Orbit& oi = modelspace.GetOrbit(i);
       for (index_t j=0; j<norb; ++j)
       {
         Orbit& oj = modelspace.GetOrbit(j);
         OVL.OneBody(i,j) = RadialIntegral(oi.n, oi.l, oj.n, oj.l, 0, modelspace); // This is not quite right. Only works for li+lj=even.
       }
     }
     return OVL;
  }


  Operator LdotS_Op(ModelSpace& modelspace)
  {
    Operator LdS(modelspace,0,0,0,2);
    index_t norbits = modelspace.GetNumberOrbits();
    for (index_t i=0;i<norbits;++i)
    {
      Orbit& oi = modelspace.GetOrbit(i);
      double ji = 0.5*oi.j2;
      int li = oi.l;
      LdS.OneBody(i,i) = 0.5* (ji*(ji+1) -li*(li+1)-0.75);
    }
    return LdS;
  }


  map<index_t,double> GetSecondOrderOccupations(Operator& H, int emax)
  {
//    ModelSpace* modelspace = H.GetModelSpace();
    map<index_t,double> hole_list;
    cout << "GetSecondOrderOccupations : Not yet implemented" << endl;
    return hole_list;
  }

  double ReducedFactorial(int n, int d)
  {
     double temp = 1;
     if((n-d) == 0) return 1;
     for (int x = min(n,d); x <= max(n,d); x++)
     {
	temp *= x;
     }
     if((n-d) > 0) return temp;
     else return 1/temp;
  }


  /// Embeds the one-body operator of op1 in the two-body part, using mass number A in the embedding.
  /// Note that the embedded operator is added to the two-body part, rather than overwriting.
  /// The one-body part is left as-is.
  void Embed1BodyIn2Body(Operator& op1, int A)
  {
    if (A<2)
    {
      cout << "Embed1BodyIn2Body: A = " << A << ". You clearly didn't mean to do that..." << endl;
      return;
    }
    ModelSpace* modelspace = op1.GetModelSpace();
    int Lambda = op1.GetJRank();
    for (auto& itmat : op1.TwoBody.MatEl)
    {
      index_t ch_bra = itmat.first[0];
      index_t ch_ket = itmat.first[1];
      arma::mat& TBME = itmat.second;

      TwoBodyChannel& tbc_bra = modelspace->GetTwoBodyChannel(ch_bra);
      TwoBodyChannel& tbc_ket = modelspace->GetTwoBodyChannel(ch_ket);
      index_t nbras = tbc_bra.GetNumberKets();
      index_t nkets = tbc_ket.GetNumberKets();
      int Jbra = tbc_bra.J;
      int Jket = tbc_ket.J;
      for (index_t ibra=0;ibra<nbras;++ibra)
      {
        Ket& bra = tbc_bra.GetKet(ibra);
        index_t i = bra.p;
        index_t j = bra.q;
        for (index_t iket=0;iket<nkets;++iket)
        {
          Ket& ket = tbc_ket.GetKet(iket);
          index_t k = ket.p;
          index_t l = ket.q;
          double embedded_tbme = GetEmbeddedTBME(op1,i,j,k,l,Jbra,Jket,Lambda) / (A-1.0);
          TBME(ibra,iket) += embedded_tbme;
        }
      }
    }
  }

  /// Returns a normalized TBME formed by embedding the one body part of op1 in a two body operator. Assumes A=2 in the formula.
  /// For other values of A, divide by (A-1).
  double GetEmbeddedTBME(Operator& op1, index_t i, index_t j, index_t k, index_t l, int Jbra,int Jket, int Lambda)
  {
    double embedded_tbme = 0;
    ModelSpace* modelspace = op1.GetModelSpace();
    double ji = modelspace->GetOrbit(i).j2 * 0.5;
    double jj = modelspace->GetOrbit(j).j2 * 0.5;
    double jk = modelspace->GetOrbit(k).j2 * 0.5;
    double jl = modelspace->GetOrbit(l).j2 * 0.5;
    arma::mat& OB = op1.OneBody;
    if (Lambda==0) // scalar => no sixJs, tbmes are not reduced.
    {
       if (j==l)  embedded_tbme += OB(i,k);
       if (i==k)  embedded_tbme += OB(j,l);
       if (i==l)  embedded_tbme -= OB(j,k) * modelspace->phase(ji+jj-Jbra);
       if (j==k)  embedded_tbme -= OB(i,l) * modelspace->phase(ji+jj-Jbra);
    }
    else // Tensor => slightly more complicated, tbmes are reduced.
    {
       if (j==l)  embedded_tbme += OB(i,k) * modelspace->phase(ji+jj+Jket) * SixJ(Jbra,Jket,Lambda,jk,ji,jj);
       if (i==k)  embedded_tbme += OB(j,l) * modelspace->phase(jk+jl-Jbra) * SixJ(Jbra,Jket,Lambda,jl,jj,ji);
       if (i==l)  embedded_tbme -= OB(j,k) * modelspace->phase(ji+jj+jk+jl)* SixJ(Jbra,Jket,Lambda,jl,ji,jj);
       if (j==k)  embedded_tbme -= OB(i,l) * modelspace->phase(Jket-Jbra)  * SixJ(Jbra,Jket,Lambda,jk,jj,ji);
       embedded_tbme *= sqrt((2*Jbra+1)*(2*Jket+1)) * modelspace->phase(Lambda);
    }
    if (i==j) embedded_tbme /=SQRT2;
    if (k==l) embedded_tbme /=SQRT2;
    return embedded_tbme;
  }



  void CommutatorTest(Operator& X, Operator& Y)
  {
    Operator Zscalar(X);
    if ( (X.IsHermitian() and Y.IsHermitian()) or (X.IsAntiHermitian() and Y.IsAntiHermitian()) ) Zscalar.SetAntiHermitian();
    if ( (X.IsHermitian() and Y.IsAntiHermitian()) or (X.IsAntiHermitian() and Y.IsHermitian()) ) Zscalar.SetHermitian();
    Zscalar.Erase();
    Operator Ztensor(Zscalar);
    Operator Yred = Y;
    Reduce(Yred);

    cout << "operator norms: " << X.Norm() << "  " << Y.Norm() << endl;
//    X.comm111ss(Y,Zscalar);
//    X.comm111st(Yred,Ztensor);
    Zscalar.comm111ss(X,Y);
    Ztensor.comm111st(X,Yred);
    Zscalar.Symmetrize();
    Ztensor.Symmetrize();
    UnReduce(Ztensor);
    cout << "comm111 norm = " << Zscalar.OneBodyNorm() << " " << Zscalar.TwoBodyNorm() << ",   " << Ztensor.OneBodyNorm() << " " << Ztensor.TwoBodyNorm() << endl;
    Zscalar -= Ztensor;
    cout << "comm111 diff = " << Zscalar.OneBodyNorm() << " " << Zscalar.TwoBodyNorm() << endl;

    Zscalar.Erase();
    Ztensor.Erase();
    cout << "121ss" << endl;
    Zscalar.comm121ss(X,Y);
    cout << "121st" << endl;
    Ztensor.comm121st(X,Yred);
    Zscalar.Symmetrize();
    Ztensor.Symmetrize();
    UnReduce(Ztensor);
    cout << "comm121 norm = " << Zscalar.OneBodyNorm() << " " << Zscalar.TwoBodyNorm() << ",   " << Ztensor.OneBodyNorm() << " " << Ztensor.TwoBodyNorm() << endl;
    Zscalar -= Ztensor;
    cout << "comm121 diff = " << Zscalar.OneBodyNorm() << " " << Zscalar.TwoBodyNorm() << endl;

    Zscalar.Erase();
    Ztensor.Erase();
    Zscalar.comm122ss(X,Y);
    Ztensor.comm122st(X,Yred);
    Zscalar.Symmetrize();
    Ztensor.Symmetrize();
    UnReduce(Ztensor);
    cout << "comm122 norm = " << Zscalar.OneBodyNorm() << " " << Zscalar.TwoBodyNorm() << ",   " << Ztensor.OneBodyNorm() << " " << Ztensor.TwoBodyNorm() << endl;
    Zscalar -= Ztensor;
    cout << "comm122 diff = " << Zscalar.OneBodyNorm() << " " << Zscalar.TwoBodyNorm() << endl;

    Zscalar.Erase();
    Ztensor.Erase();
    Zscalar.comm222_pp_hh_221ss(X,Y);
    Ztensor.comm222_pp_hh_221st(X,Yred);
    Zscalar.Symmetrize();
    Ztensor.Symmetrize();
    UnReduce(Ztensor);
    cout << "comm222_pp_hh_221 norm = " << Zscalar.OneBodyNorm() << " " << Zscalar.TwoBodyNorm() << ",   " << Ztensor.OneBodyNorm() << " " << Ztensor.TwoBodyNorm() << endl;
    Zscalar -= Ztensor;
    cout << "comm222_pp_hh_221 diff = " << Zscalar.OneBodyNorm() << " " << Zscalar.TwoBodyNorm() << endl;

    Zscalar.Erase();
    Ztensor.Erase();
    X.comm222_phss(Y,Zscalar);
//    Reduce(Y); // Not sure why I can't use Yred...
    Zscalar.comm222_phss(X,Y);
    Ztensor.comm222_phst(X,Yred);
    Zscalar.Symmetrize();
    Ztensor.Symmetrize();
    UnReduce(Ztensor);
    cout << "comm222_ph norm = " << Zscalar.OneBodyNorm() << " " << Zscalar.TwoBodyNorm() << ",   " << Ztensor.OneBodyNorm() << " " << Ztensor.TwoBodyNorm() << endl;
    Zscalar -= Ztensor;
    cout << "comm222_ph diff = " << Zscalar.OneBodyNorm() << " " << Zscalar.TwoBodyNorm() << endl;


  }



/*
  void CommutatorTest(Operator& X, Operator& Y)
  {
    Operator Zscalar(X);
    if ( (X.IsHermitian() and Y.IsHermitian()) or (X.IsAntiHermitian() and Y.IsAntiHermitian()) ) Zscalar.SetAntiHermitian();
    if ( (X.IsHermitian() and Y.IsAntiHermitian()) or (X.IsAntiHermitian() and Y.IsHermitian()) ) Zscalar.SetHermitian();
    Zscalar.Erase();
    Operator Ztensor(Zscalar);
    Operator Yred = Y;
    Reduce(Yred);
    cout << "operator norms: " << X.Norm() << "  " << Y.Norm() << endl;
//    X.comm111ss(Y,Zscalar);
//    X.comm111st(Yred,Ztensor);
    Zscalar.comm111ss(X,Y);
    Ztensor.comm111st(X,Yred);
    Zscalar.Symmetrize();
    Ztensor.Symmetrize();
    UnReduce(Ztensor);
    cout << "comm111 norm = " << Zscalar.OneBodyNorm() << " " << Zscalar.TwoBodyNorm() << ",   " << Ztensor.OneBodyNorm() << " " << Ztensor.TwoBodyNorm() << endl;
    Zscalar -= Ztensor;
    cout << "comm111 diff = " << Zscalar.OneBodyNorm() << " " << Zscalar.TwoBodyNorm() << endl;
    Zscalar.Erase();
    Ztensor.Erase();
    X.comm121ss(Y,Zscalar);
    X.comm121st(Yred,Ztensor);
    Zscalar.Symmetrize();
    Ztensor.Symmetrize();
    UnReduce(Ztensor);
    cout << "comm121 norm = " << Zscalar.OneBodyNorm() << " " << Zscalar.TwoBodyNorm() << ",   " << Ztensor.OneBodyNorm() << " " << Ztensor.TwoBodyNorm() << endl;
    Zscalar -= Ztensor;
    cout << "comm121 diff = " << Zscalar.OneBodyNorm() << " " << Zscalar.TwoBodyNorm() << endl;
    Zscalar.Erase();
    Ztensor.Erase();
    X.comm122ss(Y,Zscalar);
    X.comm122st(Yred,Ztensor);
    Zscalar.Symmetrize();
    Ztensor.Symmetrize();
    UnReduce(Ztensor);
    cout << "comm122 norm = " << Zscalar.OneBodyNorm() << " " << Zscalar.TwoBodyNorm() << ",   " << Ztensor.OneBodyNorm() << " " << Ztensor.TwoBodyNorm() << endl;
    Zscalar -= Ztensor;
    cout << "comm122 diff = " << Zscalar.OneBodyNorm() << " " << Zscalar.TwoBodyNorm() << endl;
    Zscalar.Erase();
    Ztensor.Erase();
    X.comm222_pp_hh_221ss(Y,Zscalar);
    X.comm222_pp_hh_221st(Yred,Ztensor);
    Zscalar.Symmetrize();
    Ztensor.Symmetrize();
    UnReduce(Ztensor);
    cout << "comm222_pp_hh_221 norm = " << Zscalar.OneBodyNorm() << " " << Zscalar.TwoBodyNorm() << ",   " << Ztensor.OneBodyNorm() << " " << Ztensor.TwoBodyNorm() << endl;
    Zscalar -= Ztensor;
    cout << "comm222_pp_hh_221 diff = " << Zscalar.OneBodyNorm() << " " << Zscalar.TwoBodyNorm() << endl;
    Zscalar.Erase();
    Ztensor.Erase();
    X.comm222_phss(Y,Zscalar);
    Reduce(Y); // Not sure why I can't use Yred...
    X.comm222_phst(Y,Ztensor);
    Zscalar.Symmetrize();
    Ztensor.Symmetrize();
    UnReduce(Ztensor);
    cout << "comm222_ph norm = " << Zscalar.OneBodyNorm() << " " << Zscalar.TwoBodyNorm() << ",   " << Ztensor.OneBodyNorm() << " " << Ztensor.TwoBodyNorm() << endl;
    Zscalar -= Ztensor;
    cout << "comm222_ph diff = " << Zscalar.OneBodyNorm() << " " << Zscalar.TwoBodyNorm() << endl;
  }
*/




// template <typename T>
// T VectorUnion(T& v1)
// {
//   return v1;
// }
//
// template <typename T, typename... Args>
// T VectorUnion(T& v1, T& v2, Args... args)
// {
//   T vec(v1.size()+v2.size());
//   copy(v1.begin(),v1.end(),vec.begin());
//   copy(v2.begin(),v2.end(),vec.begin()+v1.size());
//   return VectorUnion(vec, args...);
// }


}// namespace imsrg_util
