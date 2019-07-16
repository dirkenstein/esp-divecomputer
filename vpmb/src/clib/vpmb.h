/* Copyright 2010, Bryan Waite, Erik C. Baker. All rights reserved.
 * Redistribution and use in source and binary forms, with or without modification, are
 * permitted provided that the following conditions are met:
 *
 *    1. Redistributions of source code must retain the above copyright notice, this list of
 *       conditions and the following disclaimer.
 *
 *    2. Redistributions in binary form must reproduce the above copyright notice, this list
 *       of conditions and the following disclaimer in the documentation and/or other materials
 *       provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY Bryan Waite, Erik C. Baker ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL Bryan Waite, or Erik C. Baker OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * The views and conclusions contained in the software and documentation are those of the
 * authors and should not be interpreted as representing official policies, either expressed
 * or implied, of Bryan Waite, or Erik C. Baker.
 */

#ifndef VPMB_H
#define VPMB_H

#include <Arduino.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include <new_yield.h>
#include <dirkutil.h>

//#include <mcheck.h>

/* #include "strlcpy.h" */
#include "utils.h"

/* structs */

/*==========================================*/
/* C++ compatible */

#ifdef __cplusplus
extern "C" {
#endif
	
extern void new_yield(const char *);
extern int dbprintf(const char * funcn, char * str, ...);
extern int lprintf (char * str, ...);
extern   uint32_t getFreeHeap(void);
extern   void lflush(void);


/*
#define DBG_PRINT(...) dbprintf(__func__,__VA_ARGS__) 
#define TRC_PRINT(...) dbprintf(__func__,__VA_ARGS__) 
#define ERR_PRINT(...) dbprintf(__func__,__VA_ARGS__) 

#define YIELD() new_yield(__func__)
*/

//typedef int BOOL;
//typedef short bool;

#ifndef _NDS_BUILD_
//extern const int TRUE;
//extern const int FALSE;
//extern const short true;
//extern const short false;
#endif

/* errors */
extern const int ALLGOOD;
//extern const int BADJSON;
//extern const int GOODJSON;
extern const int INVALIDDATA;
extern const int VALIDDATA;

extern const int ROOTERROR;
extern const int ROOTFOUND;
extern const int BADALTITUDE;

extern const int OFFGASSINGERROR;
extern const int BADDECOSTOP;
//extern const int NOFILE;

/* Constants */
extern const double ATM;
extern const double fraction_inert_gas;
extern const double Helium_Half_Time[];
extern const double Nitrogen_Half_Time[];
extern const int Buhlmann_Compartments;

extern const int DONE_PROFILE;
extern const int NOT_DONE_PROFILE;
extern const int DEPTH_CHANGED;

typedef enum  {ASCENT, DESCENT, CONSTANT,  ERROR, DONE} direction;
extern const char * const direction_labels[];
/* Structs */

/**
 * \struct decompression_stops
 * \brief Holds the time, depth and whether the diver is ascending or at constant depth
 */
typedef struct {
    double time; /**< Run time that the decompression stop should take place at */
    double depth; /**< Depth that the decompression stop should take place at */
    direction ascent_or_const; /**< Either ASCENT or CONSTANT, based on whether the diver should be ascending or at constant depth */
} decompression_stops;

/**
 * \struct dive_state
 * \brief Contains all the state needed to calculate the VPM-B algorithm
 */
typedef struct {
    //char Units[5];
    //char Units_Word1[10];
    //char Units_Word2[10];

    double Water_Vapor_Pressure;
    double Surface_Tension_Gamma;
    double Skin_Compression_GammaC;
    double Crit_Volume_Parameter_Lambda;
    double Minimum_Deco_Stop_Time;
    double Regeneration_Time_Constant;
    double Constant_Pressure_Other_Gases;
    double Gradient_Onset_of_Imperm_Atm;

    int Number_of_Changes;
    int Segment_Number_Start_of_Ascent;
    int Repetitive_Dive_Flag;
    bool Schedule_Converged;
    bool Critical_Volume_Algorithm_Off;
    bool Altitude_Dive_Algorithm_Off;
    double Ascent_Ceiling_Depth;
    double Deco_Stop_Depth;
    double Step_Size;
    double Depth;
    double Last_Depth;
    double Ending_Depth;
    double Starting_Depth;
    double Rate;
    double Run_Time_End_of_Segment;
    double Last_Run_Time;
    double Stop_Time;
    double Depth_Start_of_Deco_Zone;
    double Deepest_Possible_Stop_Depth;
    double First_Stop_Depth;
    double Critical_Volume_Comparison;
    double Next_Stop;
    double Run_Time_Start_of_Deco_Zone;
    double Critical_Radius_N2_Microns;
    double Critical_Radius_He_Microns;
    double Run_Time_Start_of_Ascent;
    double Altitude_of_Dive;
    double Deco_Phase_Volume_Time;
    double Surface_Interval_Time;
    double Regenerated_Radius_He[16];
    double Regenerated_Radius_N2[16];

    int Mix_Change[16];
    int Depth_Change[16];
    int Rate_Change[16];
    int Step_Size_Change[16];
    double He_Pressure_Start_of_Ascent[16];
    double N2_Pressure_Start_of_Ascent[16];
    double He_Pressure_Start_of_Deco_Zone[16];
    double N2_Pressure_Start_of_Deco_Zone[16];
    double Phase_Volume_Time[16];
    double Last_Phase_Volume_Time[16];
    double Allowable_Gradient_He[16];
    double Allowable_Gradient_N2[16];
    double Adjusted_Crushing_Pressure_He[16];
    double Adjusted_Crushing_Pressure_N2[16];
    double Initial_Allowable_Gradient_N2[16];
    double Initial_Allowable_Gradient_He[16];
    double Deco_Gradient_He[16];
    double Deco_Gradient_N2[16];
    int Segment_Number;
    double Run_Time;
    double Segment_Time;
    double Ending_Ambient_Pressure;
    int Mix_Number;
    double Barometric_Pressure;
    bool units_fsw;
    double Units_Factor;
    double Helium_Time_Constant[16];
    double Nitrogen_Time_Constant[16];
    double Helium_Pressure[16];
    double Nitrogen_Pressure[16];
    double Initial_Helium_Pressure[16];
    double Initial_Nitrogen_Pressure[16];
    double *Fraction_Helium;
    double *Fraction_Nitrogen;
    double Initial_Critical_Radius_He[16];
    double Initial_Critical_Radius_N2[16];
    double Adjusted_Critical_Radius_He[16];
    double Adjusted_Critical_Radius_N2[16];
    double Max_Crushing_Pressure_He[16];
    double Max_Crushing_Pressure_N2[16];
    double Surface_Phase_Volume_Time[16];
    double Max_Actual_Gradient[16];
    double Amb_Pressure_Onset_of_Imperm[16];
    double Gas_Tension_Onset_of_Imperm[16];
    bool Diver_Acclimatized;

    double Last_Direction_Depth;
    double Last_Direction_Time;

    double Start_of_Decompression_Zone;
    bool Decompressing;

    decompression_stops *decomp_stops;
	int ndecomp_stops;

    int decomp_stop_index;

    bool Real_Time_Decompression;
    double Wait_Time;
	bool CCR_Mode;
	double PpO2;
	double * SetPoints;
	int Number_Of_SetPoints;
	int SetPoint_Number;
	double Default_Ascent_Rate; //used during descent for bounce dive prediction
	double OTU_Sum;
	double CNS_Sum;
	
	bool dive_finished;
	bool calc_decompression_stop;
	bool did_loop_init;

} dive_state;



/* Function Prototypes */

//void vpmb_failure(void);

double vpmb_schreiner_equation(double Initial_Inspired_Gas_Pressure, double Rate_Change_Insp_Gas_Pressure, double Interval_Time, double Gas_Time_Constant, double Initial_Gas_Pressure);
double vpmb_haldane_equation(double Initial_Gas_Pressure, double Inspired_Gas_Pressure, double Gas_Time_Constant, double Interval_Time);
int vpmb_radius_root_finder(double A, double B, double C, double Low_Bound, double High_Bound, double *result);
double vpmb_calc_barometric_pressure(double Altitude, bool units_fsw);
double vpmb_calc_deco_ceiling(dive_state *dive);
void vpmb_gas_loadings_constant_depth(dive_state *dive, double Depth, double Run_Time_End_of_Segment);
void vpmb_nuclear_regeneration(dive_state *dive, double Dive_Time);
double vpmb_crushing_pressure_helper(dive_state *dive, double Radius_Onset_of_Imperm_Molecule, double Ending_Ambient_Pressure_Pa, double Amb_Press_Onset_of_Imperm_Pa, double Gas_Tension_Onset_of_Imperm_Pa, double Gradient_Onset_of_Imperm_Pa);
int vpmb_onset_of_impermeability(dive_state *dive, double Starting_Ambient_Pressure, double Ending_Ambient_Pressure, double Rate, int i);
bool vpmb_calc_crushing_pressure(dive_state *dive, double Starting_Depth, double Ending_Depth, double Rate);
void vpmb_gas_loadings_ascent_descent(dive_state *dive, double Starting_Depth, double Ending_Depth, double Rate);
int vpmb_decompression_stop(dive_state *dive, double Deco_Stop_Depth, double Step_Size);
double vpmb_calculate_deco_gradient(dive_state *dive, double Allowable_Gradient_Molecule, double Amb_Press_First_Stop_Pascals, double Amb_Press_Next_Stop_Pascals);
bool vpmb_boyles_law_compensation(dive_state *dive, double First_Stop_Depth, double Deco_Stop_Depth, double Step_Size);
void vpmb_calc_max_actual_gradient(dive_state *dive, double Deco_Stop_Depth);
void vpmb_projected_ascent(dive_state *dive, double Starting_Depth, double Rate, double Step_Size);
void vpmb_calc_ascent_ceiling(dive_state *dive);
void vpmb_calc_surface_phase_volume_time(dive_state *dive);
void vpmb_critical_volume(dive_state *dive, double Deco_Phase_Volume_Time);
int vpmb_calc_start_of_deco_zone(dive_state *dive, double Starting_Depth, double Rate);
void vpmb_calc_initial_allowable_gradient(dive_state *dive);
void vpmb_gas_loadings_surface_interval(dive_state *dive, double Surface_Interval_Time);
double vpmb_new_critical_radius(dive_state *dive, double Max_Actual_Gradient_Pascals, double Adj_Crush_Pressure_Pascals);
void vpmb_vpm_repetitive_algorithm(dive_state *dive, double Surface_Interval_Time);

bool vpmb_deco_stop_loop_block_within_critical_volume_loop(dive_state *dive);
int vpmb_critical_volume_loop(dive_state *dive, bool init);

direction vpmb_current_direction(dive_state *dive, double increment_time);
bool vpmb_critical_volume_decision_tree(dive_state *dive, double stop_depth, bool addstops);
void vpmb_init_decomp_stop_table(dive_state *dive);

void vpmb_free_dive_state(dive_state *dive);

	
	/*==========================================*/
	/* C++ compatible */
	
#ifdef __cplusplus
}
#endif

#endif
