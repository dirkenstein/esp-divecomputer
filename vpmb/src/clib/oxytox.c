/* oxytox.f -- translated by f2c (version 20100827).
   You must link the resulting object file with libf2c:
	on Microsoft Windows system, link with libf2c.lib;
	on Linux or Unix systems, link with .../path/to/libf2c.a -lm
	or, if you install libf2c.a in a standard place, with -lf2c -lm
	-- in that order, at the end of the command line, as in
		cc *.o -lf2c -lm
	Source for libf2c is in /netlib/f2c/libf2c.zip, e.g.,

		http://www.netlib.org/f2c/libf2c.zip
*/

//#include "f2c.h"
//#include "vpmb.h"
#include "oxytox.h"
#include <math.h>
#include<stdio.h>
#include <stdlib.h>
#include <sys/param.h>

/* Common Block Declarations */

typedef struct a_1_ {
    double po2lo[7], po2hi[7], limslp[7], limint[7];
} cnslim;

cnslim cnsl =  { .5f, .6f, .7f, .8f, .9f, 1.1f, 1.5f, .6f, .7f, .8f, .9f, 1.1f, 
	    1.5f, 1.6f, -1800.f, -1500.f, -1200.f, -900.f, -600.f, -300.f, 
	    -750.f, 1800.f, 1620.f, 1410.f, 1170.f, 900.f, 570.f, 1245.f };

//#define cns (*(struct a_1_ *) &a_)

//typedef struct c_1_ {
//    int gasmix;
//    double depthata, fo2[3];
//} config;

//config conf =  { 2, 33.f, .21f, .32f, .36f };
//#define c_1 (*(struct c_1_ *) &c_)

/* Initialized data */

//struct {
//    double e_1[28];
//    } a_ =;

//struct {
 //   int e_1;
 //   double e_2[4];
 //   } c_ = { 2, 33.f, .21f, .32f, .36f };


/* Table of constant values */


static double c_b32 = -.83333333333333337;
static double c_b35 = 1.8333333333333333;


/* for meters */
/*      DATA DEPTHATA /10.0/ */
/*  for feet */

double toxotu_const(dive_state * dive, double depth, double dtime)
{
    /* Local variables */
    double po2, otu, pata, fo2;
	  fo2 = 1.0-  (dive->Fraction_Helium[dive->Mix_Number -1] + dive->Fraction_Nitrogen[dive->Mix_Number -1] );
    pata = (depth + dive->Barometric_Pressure)/dive->Units_Factor;
    if (dive->CCR_Mode) {
    	po2 = dive->PpO2;
    } else {
    		po2 = pata * fo2;
    }
    otu = dtime * pow(.5f / (po2 - .5f), c_b32);
/*        WRITE(*,*) GASMIX, DEPTHATA, FO2(1) */
    return otu;
} /* toxotu_const */

double toxotu_ascdesc(dive_state * dive, double sdepth, double fdepth, double rate)
{


    /* Local variables */
    double otu, po2f, po2i, fpata, ipata, o2time, minpo2, maxpo2, lowpo2,
	     minata, maxata, sgtime, fo2;
sgtime = (fdepth - sdepth) / rate;
    ipata = (sdepth + dive->Barometric_Pressure - dive->Water_Vapor_Pressure) / dive->Units_Factor;
    fpata = (fdepth +dive->Barometric_Pressure - dive->Water_Vapor_Pressure) / dive->Units_Factor;
    po2i = ipata * fo2;
    po2f = fpata * fo2;
    maxata = MAX(ipata,fpata);
    minata = MIN(ipata,fpata);
    if (dive->CCR_Mode) {
	  	fo2 = dive->PpO2/(ipata - dive->PpO2); 
	  } else {
			fo2 = 1.0-  (dive->Fraction_Helium[dive->Mix_Number -1] + dive->Fraction_Nitrogen[dive->Mix_Number -1] );
		}
    
    maxpo2 = maxata * fo2;
    minpo2 = minata * fo2;
    if (maxpo2 <= .5f) {
			otu = 0.f;
	   	return otu;
    }
    if (minpo2 < .5f) {
	lowpo2 = .5f;
    } else {
	lowpo2 = minpo2;
    }
    if (po2i <= lowpo2) {
	po2i = lowpo2;
    }
    if (po2f <= lowpo2) {
	po2f = lowpo2;
    }
    o2time = sgtime * (maxpo2 - lowpo2) / (maxpo2 - minpo2);
   
    otu = o2time * .27272727272727271f / (po2f - po2i) * (pow(((po2f - .5f) / .5f), 
	    c_b35) - pow(((po2i - .5f) / .5f), c_b35));
    return otu;
} /* toxotu_ascdesc__ */

double toxcns_const(dive_state * dive, double depth, double dtime)
{
    
    /* Local variables */
    int i;
    double po2, cns, pata, tlim, fo2;
	  fo2 = 1.0-  (dive->Fraction_Helium[dive->Mix_Number -1] + dive->Fraction_Nitrogen[dive->Mix_Number -1] );

    pata = (depth + dive->Barometric_Pressure - dive->Water_Vapor_Pressure) / dive->Units_Factor;
    if (dive->CCR_Mode) {
    	po2 = dive->PpO2;
    } else {
    		po2 = pata * fo2;
    }    
    if (po2 <= .5f) {
			return 0.f;
    }
    for (i = 0; i < 7; i++) {
			if (po2 > cnsl.po2lo[i] && po2 <= cnsl.po2hi[i]) {
			    tlim = cnsl.limslp[i] * po2 + cnsl.limint[i];
				  break;
					}
/* L10: */
    }
    cns = dtime / tlim;
    return cns;
} /* toxcns_const__ */

double toxcns_ascdesc(dive_state * dive,double sdepth, double fdepth, double rate)
{
   
    double mk[7], cns[7], po2f[7], po2o[7], fpata, ipata, otime[7], 
	    tlimi[7], segpo2[7], o2time, minpo2, maxpo2, lowpo2, minata, 
	    maxata, sgtime, tmpcns, sumcns, fo2;
	  
    sgtime = (fdepth - sdepth) / rate;
    ipata = (sdepth + dive->Barometric_Pressure - dive->Water_Vapor_Pressure) / dive->Units_Factor;
    fpata = (fdepth + dive->Barometric_Pressure - dive->Water_Vapor_Pressure) / dive->Units_Factor;
    maxata = MAX(ipata,fpata);
    minata = MIN(ipata,fpata);
    if (dive->CCR_Mode) {
	  	fo2 = dive->PpO2/(ipata - dive->PpO2); 
	  } else {
			fo2 = 1.0-  (dive->Fraction_Helium[dive->Mix_Number -1] + dive->Fraction_Nitrogen[dive->Mix_Number -1] );
		}
    
    maxpo2 = maxata * fo2;
    minpo2 = minata * fo2;
    if (maxpo2 <= .5f) {
			return  0.f;
    }
    if (minpo2 < .5f) {
			lowpo2 = .5f;
    } else {
			lowpo2 = minpo2;
    }
    o2time = sgtime * (maxpo2 - lowpo2) / (maxpo2 - minpo2);
    for (int i  = 1; i < 7; i++) {
	if (maxpo2 > cnsl.po2lo[i] && lowpo2 <= cnsl.po2hi[i]) {
	    if (maxpo2 >= cnsl.po2hi[i] && lowpo2 < cnsl.po2lo[i]) {
		if (sdepth > fdepth) {
		    po2o[i] = cnsl.po2hi[i];
		    po2f[i] = cnsl.po2lo[i];
		} else {
		    po2o[i] = cnsl.po2lo[i];
		    po2f[i] = cnsl.po2hi[i];
		}
		segpo2[i] = po2f[i] - po2o[i];
	    } else if (maxpo2 < cnsl.po2hi[i] && lowpo2 <= cnsl.po2lo[i]) {
		if (sdepth > fdepth) {
		    po2o[i] = maxpo2;
		    po2f[i] = cnsl.po2lo[i];
		} else {
		    po2o[i] = cnsl.po2lo[i];
		    po2f[i] = maxpo2;
		}
		segpo2[i] = po2f[i] - po2o[i];
	    } else if (lowpo2 > cnsl.po2lo[i] && maxpo2 >= cnsl.po2hi[i]) {
		if (sdepth > fdepth) {
		    po2o[i] = cnsl.po2hi[i];
		    po2f[i] = lowpo2;
		} else {
		    po2o[i] = lowpo2;
		    po2f[i] = cnsl.po2hi[i];
		}
		segpo2[i] = po2f[i] - po2o[i];
	    } else {
		if (sdepth > fdepth) {
		    po2o[i] = maxpo2;
		    po2f[i] = lowpo2;
		} else {
		    po2o[i] = lowpo2;
		    po2f[i] = maxpo2;
		}
		segpo2[i] = po2f[i] - po2o[i];
	    }
	    otime[i] = o2time * (fabs(segpo2[i])) / (maxpo2 - lowpo2);
	} else {
	    otime[i] = 0.f;
	}
    }
    for (int i = 1; i < 7; i++) {
	if (otime[i] == 0.f) {
	    cns[i] = 0.f;
	    continue;
	} else {
	    tlimi[i] = cnsl.limslp[i] * po2o[i] + cnsl.limint[i];
	    mk[i] = cnsl.limslp[i] * (segpo2[i] / otime[i]);
	    cns[i] = 1.f / mk[i] * (log( fabs(tlimi[i] + mk[i] * otime[i])) - log(fabs(tlimi[i])));
	}
    }
    sumcns = 0.f;
    for (int i = 1; i < 7; i++) {
			tmpcns = sumcns;
			sumcns = tmpcns + cns[i];
    }
    return sumcns;

} /* toxcns_ascdesc__ */


#ifdef TEST

int main(void)
{
    /* System generated locals */
    

/*       WRITE(*,*) TOXCNS_CONST(20.0, 25.0) */
/*       WRITE(*,*) TOXOTU_CONST(20.0, 25.0) */
 
   printf("%f\n", toxotu_ascdesc(0.0, 120.0, 40.0));
   
    printf("%f\n", toxotu_const(120.0, 22.0));
   
    printf("%f\n", toxotu_ascdesc(120.0, 0.0, -4.0));
   
   printf("%f\n", toxcns_ascdesc(0.0, 120.0, 40.0));
 
    printf("%f\n",  toxcns_const(120.0, 22.0));
   
    printf("%f\n", toxcns_ascdesc(120.0, 0.0, -4.0));
    return 0;
} /* MAIN__ */

#endif