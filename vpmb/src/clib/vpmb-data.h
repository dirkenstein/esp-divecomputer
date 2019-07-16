/*
 *  vpmb-data.h
 *  
 *
 *  Created by Dirk Niggemann on 03/02/2018.
 *  Copyright 2018 __MyCompanyName__. All rights reserved.
 *
 */

#ifndef __VPMB_DATA_H__
#define __VPMB_DATA_H__

#include "vpmb.h"

/*==========================================*/
/* C++ compatible */

#ifdef __cplusplus
extern "C" {
#endif
	
//typedef int BOOL;
	
	typedef struct {
		double starting_depth;
		int gasmix;
		double rate;
		double step_size;
		double setpoint;
	} ascent_summary;
	
	typedef struct {
		int profile_code;
		double starting_depth;
		double ending_depth;
		double rate;
		int gasmix;
		double setpoint;
		double depth;
		double run_time_at_end_of_segment;
		int number_of_ascent_parameter_changes;
		ascent_summary *ascents;
	} dive_profile;
	
	typedef struct {
		double fraction_O2;
		double fraction_He;
		double fraction_N2;
	} gasmix_summary;
	
	typedef struct {
		char desc[20];
		int num_gas_mixes;
		int repetitive_code;
		gasmix_summary *gasmixes;
		int num_profile_codes;
		dive_profile *dive_profiles;
		double surface_interval_time_minutes;
	} single_dive;
	
	
	typedef struct _gases {
		char name[20];
		bool active;
		gasmix_summary gasmix;
		double MOD;
		double ceiling;
	} gases;
	
typedef struct {
    /* input*/
    single_dive *dives;
    int number_of_dives;
	
    /* altitude */
    double Altitude_of_Dive;
    bool Diver_Acclimatized_at_Altitude;
    double Starting_Acclimatized_Altitude;
    double Ascent_to_Altitude_Hours;
    double Hours_at_Altitude_Before_Dive;
	
    /* settings */
    char Units[4]; /* "fsw" or "msw" */
    bool SetPoint_Is_Bar;
    bool Altitude_Dive_Algorithm; /* "OFF" or "ON" */
    double Minimum_Deco_Stop_Time;
    double Critical_Radius_N2_Microns;
    double Critical_Radius_He_Microns;
    bool Critical_Volume_Algorithm;
    double Crit_Volume_Parameter_Lambda;
    double Gradient_Onset_of_Imperm_Atm;
    double Surface_Tension_Gamma;
    double Skin_Compression_GammaC;
    double Regeneration_Time_Constant;
    double Pressure_Other_Gases_mmHg;
	gases * gaslist;
	int number_of_gases;
	bool CCR_Mode;
	double * setpoints;
	int number_of_setpoints;
	double default_step_size; //for realtime use
	double default_ascent_rate; //for realtime use

} vpmb_input;

/* Function Prototypes */


void vpmb_free_dives(vpmb_input *in);
int vpmb_validate_data(vpmb_input *input, dive_state *dive);
int vpmb_initialize_data(vpmb_input *input, dive_state *dive);
	
int vpmb_vpm_altitude_dive_algorithm(vpmb_input *input, dive_state *dive);
void vpmb_free_dives(vpmb_input *in);

/*==========================================*/
/* C++ compatible */

#ifdef __cplusplus
}
#endif

#endif
