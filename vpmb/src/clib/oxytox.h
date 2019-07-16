/*  oxytox.h
 *  
 *
 *  Created by Dirk Niggemann on 03/02/2018.
 *  Copyright 2018 __MyCompanyName__. All rights reserved.
 *
 */

#ifndef __OXYTOX_H__
#define __OXYTOX_H__

#include "vpmb.h"

/*==========================================*/
/* C++ compatible */

#ifdef __cplusplus
extern "C" {
#endif
	
	
	double toxotu_const(dive_state * dive, double depth, double dtime);

	double toxotu_ascdesc(dive_state * dive, double sdepth, double fdepth, double rate);
	double toxcns_const(dive_state * dive, double depth, double dtime);
	double toxcns_ascdesc(dive_state * dive,double sdepth, double fdepth, double rate);


/*==========================================*/
/* C++ compatible */

#ifdef __cplusplus
}
#endif

#endif
