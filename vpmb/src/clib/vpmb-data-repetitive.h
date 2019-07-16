/*
 *  vpmb-data-repetitive.h
 *  
 *
 *  Created by Dirk Niggemann on 16/03/2018.
 *  Copyright 2018 __MyCompanyName__. All rights reserved.
 *
 */



#ifndef __VPMB_DATA_REP_H__
#define __VPMB_DATA_REP_H__

#include "vpmb.h"
#include "vpmb-data.h"

/*==========================================*/
/* C++ compatible */

#ifdef __cplusplus
extern "C" {
#endif



	
	void vpmb_repetitive_dive_loop(dive_state *dive, vpmb_input *input);
	bool vpmb_profile_code_loop(dive_state *dive, single_dive *current_dive);
	void vpmb_decompression_loop(dive_state *dive, single_dive *current_dive);
 	
/*==========================================*/
/* C++ compatible */

#ifdef __cplusplus
}
#endif

#endif