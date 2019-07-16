/*
 *  vpmb-data-realtime.h
 *  
 *
 *  Created by Dirk Niggemann on 17/03/2018.
 *  Copyright 2018 __MyCompanyName__. All rights reserved.
 *
 */
#ifndef __VPMB_DATA_REAL_H__
#define __VPMB_DATA_REAL_H__

#include "vpmb.h"
#include "vpmb-data.h"
#include "oxytox.h"

/*==========================================*/
/* C++ compatible */

#ifdef __cplusplus
extern "C" {
#endif
	
	

double vpmb_find_start_of_decompression_zone(dive_state *dive, dive_profile *current_profile);
double vpmb_nonprofile_find_start_of_decompression_zone(dive_state *dive);

int vpmb_finished_constant_depth_profile(dive_state *dive, dive_profile *current_profile);
bool vpmb_calculate_decompression_stops(dive_state *dive, dive_profile *current_profile);
void vpmb_decompress(dive_state *dive, dive_profile *current_profile, bool init);
int vpmb_set_active_gas_mixes(dive_state *dive, vpmb_input * in);
int update_depth(dive_state *dive, double increment_time, double rate, dive_profile *current_profile);
void calc_current_deco_zone(dive_state * dive, single_dive *current_dive);
void vpmb_real_time_dive(dive_state *dive, vpmb_input *input);
//not implemented
void vpmb_real_time_dive_loop(dive_state *dive, single_dive *current_dive);

//New
int update_depth_real(dive_state *dive, double increment_time, double (*depthfunc)(void), double (*po2func)(void));
int find_active_gasmix(vpmb_input *input);
void add_gas_change (dive_state *dive, vpmb_input *input);
void set_default_change(dive_state * dive);
double calculate_nd_time (dive_state *dive);

double millis2minutes(unsigned long m);
	
bool vpmb_nonprofile_calculate_decompression_stops(dive_state *dive);

bool vpmb_nonprofile_decompress(dive_state *dive, bool init);

void vpmb_real_time_nonprofile_dive_init(dive_state *dive, vpmb_input *input, 
										 double init_depth, double starting_depth,
										 double planned_depth, double intvl);
void vpmb_real_time_nonprofile_dive_loop(dive_state *dive, vpmb_input *input, 
										 double increment_time,  double (*depthfunc)(void), double (*po2func)(void));	


/*==========================================*/
/* C++ compatible */
	
#ifdef __cplusplus
}
#endif

#endif
