/*
 *  vpmb.cpp
 *  
 *
 *  Created by Dirk Niggemann on 10/03/2018.
 *  Copyright 2018 __MyCompanyName__. All rights reserved.
 *
***********************************************************************************/

#if ARDUINO >= 100
#include "Arduino.h"
#else
#include "WProgram.h"
#endif


#include "VPMB.h"


#define TRC_PRINT(...)

VPMB::VPMB(void) 
{
	initDiveState();
}

VPMB::VPMB(double(*depthf)(void) )
{
	depthfunc = depthf;
	initDiveState();

}


VPMB::VPMB(double(*depthf)(void), double(*po2f)(void))
{
	depthfunc = depthf;
		po2func = po2f;

	initDiveState();

}

VPMB::VPMB(JsonObject &json, double(*depthf)(void))
{
	initDiveState();

	depthfunc = depthf;
	vpmb_load_config_from_json(&input, json);
	vpmb_load_dives_from_json(&input, json); //== BADJSON	
}



VPMB::VPMB(JsonObject &json, double(*depthf)(void), double(*po2f)(void))
{
	initDiveState();

	depthfunc = depthf;
	po2func = po2f;
	vpmb_load_config_from_json(&input, json);
	vpmb_load_dives_from_json(&input, json); //== BADJSON	
}

bool VPMB::loadconfig(JsonObject &json) 
{
	return vpmb_load_config_from_json(&input, json); //== BADJSON	

}

bool VPMB::saveconfig(JsonObject &json) 
{
	return vpmb_save_config_to_json(&input, json); //== BADJSON	
	
}

bool VPMB::loaddives(JsonObject &json) 
{
	if(&input.dives)  vpmb_free_dives(&input);
	return vpmb_load_dives_from_json(&input, json); //== BADJSON	
	
}
bool VPMB::validate(void) 
{
	
	vpmb_free_dive_state(dive);
	initDiveState();
	if(vpmb_validate_data(&input, dive) == INVALIDDATA){
		DBG_PRINT("INVALID DATA IN INPUT FILE. PLEASE CHECK IT AND RUN THIS AGAIN\n");
		return false;
	}
	if (vpmb_initialize_data(&input, dive) == INVALIDDATA) {
		DBG_PRINT("DATA INITIALIZATION FAILED INPUT FILE. PLEASE CHECK IT AND RUN THIS AGAIN\n");
		
		return false;
	}
	return true;
}
void VPMB::real_time_dive(void) 
{
	vpmb_real_time_dive(dive, &input);
}

void VPMB::repetitive_dive_loop(void)
{
	vpmb_repetitive_dive_loop(dive, &input);

}

void VPMB::setPlannedDepth(double dp) 
{
	planned_depth = dp;	
}
double VPMB::getPlannedDepth() 
{
	return planned_depth;
}

bool VPMB::getDiveStarted()
{
	return dive_started;
}

double VPMB::getDiveTime(void)
{
	return millis2minutes(millis() - dive_start_millis);
}

bool VPMB::real_time_dive_loop(void) {
	dive_last_millis = dive_intvl_millis;
	dive_intvl_millis = millis();
	double d = 0.0;
	unsigned long millintvl = dive_intvl_millis - dive_last_millis;
	if (updating) {
		DBG_PRINT("aready updating\n");
		return dive_started;	
	}
	if (!dive_started) {
		loop_call_count = 0;
		d = depthfunc();
		DBG_PRINT ("Dive Check- Depth: %f\n", d);
		if(d > trigger_depth ) {
			DBG_PRINT("dive triggered: %f\n", d);

			updating = true;
			vpmb_real_time_nonprofile_dive_init(dive,  &input, 
											 0.0, d,
											 planned_depth, millis2minutes(millintvl));
	      	updating = false;
	      	
	      	  dive_started = true;
	    		

			dive_start_millis = dive_intvl_millis;
			last_depth_window = dive_intvl_millis;
			//DBG_PRINT("Steps: %f\n", dive->Step_Size);
			recent_max_depth = d;
			
		}
	} else {
		DBG_PRINT("dive running\n");
			updating = true;

		vpmb_real_time_nonprofile_dive_loop(dive,  &input,
								millis2minutes(millintvl),  depthfunc, po2func);	
					updating = false;
		loop_call_count++;
		if (dive_intvl_millis - last_depth_window > depth_window) {
			recent_max_depth = 0.0;
			last_depth_window = dive_intvl_millis;
		}
		if (dive->Depth > recent_max_depth)
			recent_max_depth = dive->Depth;
	 if (recent_max_depth < trigger_depth) dive_started = false;
	
		//lprintf("\"dive\": {\"Time\": %f, \"Intvl:\": %f, \"Depth\": %f, \"Step\": %f, \"Rate\": %f, \"NDTime\": %f  \"Deepest\": %f},\n",
				lprintf("D,%d,%f,%f,%f,%f,%f,%f,%f\n",
	
				 dive_started, dive->Run_Time, millis2minutes(millintvl),  dive->Depth, dive->Step_Size, dive->Rate, 
				  nd_time, deepest_stop);
				 
		//\"DecoDepth\": %f, \"Ceiling\": %f, \"Deepest\": %f \"AscentTime\": %f \"DecoStartTime\": %f},\n", 
		//dive->Depth_Start_of_Deco_Zone, dive->Ascent_Ceiling_Depth, 
				 	 
				 //dive->Run_Time_Start_of_Ascent, dive->Run_Time_Start_of_Deco_Zone);
		if(loop_call_count % 20 == 0) output_dive_state_short();
		else output_deco_stops();
		lflush();
		YIELD();

	}
	return dive_started;
}

VPMB::~VPMB() 
{
	
    vpmb_free_dives(&input);
    vpmb_free_dive_state(dive);
	//free(dive);
}
void VPMB::initDiveState() 
{
	dive = (dive_state*)malloc (sizeof(dive_state));
	if (!dive) {
	  lprintf("M,%s,no memory\n", __func__);

		return;
	} 
	memset(dive, 0x0, sizeof(dive_state));
}	

int VPMB::getGasListLength() 
{
	return input.number_of_gases;
}

gases * VPMB::getGasList() 
{
	return input.gaslist;
}

void VPMB::gasChange()
{
		if (!dive_started) return;
		add_gas_change (dive, &input);
}	

 void VPMB::output_array_double(char *name, double arr[], int len)
{
    int i = 0;
    lprintf("%s: [", name);
    for(i = 0; i < len; i++) {
         if (i < (len -1)) lprintf("%f, ", arr[i]);
		 else  lprintf("%f", arr[i]);

    }
    lprintf("],\n");
}

void VPMB::output_array_double_short(char *name, double arr[], int len)
{
    int i = 0;
    lprintf("%s: [", name);
    for(i = 0; i < len; i++) {
         if (i < (len -1)) lprintf("%3.4f, ", arr[i]);
		 else  lprintf("%3.4f", arr[i]);

    }
    lprintf("],\n");
}

 void VPMB::output_array_int(char *name, int arr[], int len)
{
    int i = 0;
    lprintf("%s: [", name);
    for(i = 0; i < len; i++) {
        if (i < (len -1)) lprintf("%d, ", arr[i]);
		else  lprintf("%d", arr[i]);
    }
    lprintf("],\n");
}

double VPMB::realtime_find_deepest_stop(void)
{
	double d;
	if (!dive_started) return 0.0;
	if (updating) return deepest_stop;
	updating = true;

	d = vpmb_nonprofile_find_start_of_decompression_zone(dive);
	updating = false;
	deepest_stop = d;
	return d;
}

void VPMB::output_deco_stops(void)
{
	int len = dive->ndecomp_stops;
	//lprintf("\t\"Decompression_Stops\":[\n");
	for (int i = 0; i < len; i++) {
		decompression_stops * d = &(dive->decomp_stops[i]);
		//lprintf("\t\t{ \"Stop\": %d, \"Depth\": %f, \"Time\": %f, \"State\": \"%s\"}", 
	  lprintf("S,%d,%f,%f,%s\n", 
				 i, d->depth, 
				 d->time, direction_labels[d->ascent_or_const]);
		YIELD();
		//if (i < (len -1) && d->depth > 0.0)lprintf(",\n");
		if (d->depth <=0.0) break;
	}
	//lprintf("\n\t]\n");
	lflush();

}

void VPMB::decostopcalc(void) 
{
	int len = dive->ndecomp_stops;
	int n = 0, n2 = 0;
	//lprintf("\t\"Decompression_Stops\":[\n");
	
	for (int i = 0; i < len; i++) {
		decompression_stops * d = &(dive->decomp_stops[i]);     
		if (d->depth <=0.0) break;
		if (d->ascent_or_const == CONSTANT) n2++;
		n++;
	}
	nlast_decostops = n;
	nlast_decocount = n2;
}

int VPMB::getDecoStops(void)
{
	if (!dive_started) return 0;
	if (updating) return nlast_decostops;
	decostopcalc();
	return nlast_decostops;
}


int VPMB::getActualDecoStops(void)
{
	if (!dive_started) return 0;
	if (updating) return nlast_decocount;
	decostopcalc();

	return nlast_decocount;
}

double VPMB::getNDTime() 
{	
	double t;
	if (!dive_started || nlast_decocount > 0) return 0.0;
	
	if(updating) return nd_time;
	updating = true;
	TRC_PRINT("NDTime pre\n");
	t = calculate_nd_time(dive);
		TRC_PRINT("NDTime post\n");
	updating = false;
	nd_time = t;
	
	return t;
}

void VPMB::calc_nextstop(float depth)
{
		int len = 0, nl = 0; 
	//lprintf("\t\"Decompression_Stops\":[\n");
		int lastIdx = 0, lastCount = 0, lastNum = 0;
	double lastdepth = 0.0, lasttime = 0.0, stopstarttime = 0.0;
		nlast_decostops = len = getDecoStops();
	//if (len > 0) {
		lastdepth = 0; //dive->decomp_stops[len-1].depth;
		lasttime = 0; //dive->decomp_stops[len-1].time;
		lastIdx = len;
		lastCount = len / 2;
		lastNum  = 0;
  //} else {
  //	lastdepth = 0.0;
  //	lasttime = 0.0;
  //	lastIdx = 0;
  //	lastCount= 0;
  //	lastNum = 0;
  //}
	for (int i = len-1; i >= 0; i--) {
		decompression_stops * d = &(dive->decomp_stops[i]);
		if (d->depth <=0.0 || d->ascent_or_const == ASCENT) {
			stopstarttime = d->time;
			continue;
		}

    if (d->depth > depth) {
    	last_nextstopdepth = lastdepth;
    	last_nextstopidx = lastIdx;
    	last_nextstopcount = lastCount;
    	last_nextstopnum = lastNum;

			last_nextstoptime = lasttime -stopstarttime;
    	return; 
		}
		lastdepth = d->depth;
		lasttime = d->time;
		lastIdx = i;
		lastCount--;
		lastNum++;
	}
	//lprintf("\n\t]\n");
	 last_nextstopdepth = lastdepth;
	 last_nextstopidx = lastIdx;
	 last_nextstopcount = lastCount;
	 last_nextstopnum = lastNum;

	 last_nextstoptime = lasttime - stopstarttime;

	 //next_stop = lastdepth;
}

double VPMB::getNextStopDepth (float depth) 
{
	
	if (updating) return last_nextstopdepth;
	if (!dive_started) return 0.0;
	calc_nextstop(depth);
	return last_nextstopdepth;
}


double VPMB::getNextStopTime (float depth) 
{
	
	if (updating) return last_nextstoptime;
	if (!dive_started) return 0.0;
	calc_nextstop(depth);
	return last_nextstoptime;
}


int VPMB::getNextStopCount (float depth) 
{
	
	if (updating) return last_nextstopcount;
	if (!dive_started) return 0;
	calc_nextstop(depth);
	return last_nextstopcount;
}


int VPMB::getNextStopNum (float depth) 
{
	
	if (updating) return last_nextstopnum;
	if (!dive_started) return 0;
	calc_nextstop(depth);
	return last_nextstopnum;
}

double VPMB::getOTU() 
{
	return dive->OTU_Sum;	
}

double VPMB::getCNS()
{
	return dive->CNS_Sum;	
}

int VPMB::getNumberOfTestDiveProfiles() 
{
	if (input.number_of_dives > 0) {
  	int n = 0;
  	while (input.dives[0].dive_profiles[n].profile_code != 99) n++;
  	n++;
  	return n;
	} else {
		return 0;
	}
}

dive_profile * VPMB::getTestDiveProfiles()
{
		if (input.number_of_dives > 0) {

	    return input.dives[0].dive_profiles;		
		} else {
			return NULL;	
	  }
}
void VPMB::output_dive_state(void)
{
	lprintf("\"state\":{\n");

    //lprintf("\t\"Units\": \"%s\",\n", dive->Units);
    //lprintf("\t\"Units_Word1\": \"%s\",\n", dive->Units_Word1);
    //lprintf("\t\"Units_Word2\": \"%s\",\n", dive->Units_Word2);
	
    lprintf("\t\"Water_Vapor_Pressure\": %f,\n", dive->Water_Vapor_Pressure);
    lprintf("\t\"Surface_Tension_Gamma\": %f,\n", dive->Surface_Tension_Gamma);
    lprintf("\t\"Skin_Compression_GammaC\": %f,\n", dive->Skin_Compression_GammaC);
    lprintf("\t\"Crit_Volume_Parameter_Lambda\": %f,\n", dive->Crit_Volume_Parameter_Lambda);
    lprintf("\t\"Minimum_Deco_Stop_Time\": %f,\n", dive->Minimum_Deco_Stop_Time);
    lprintf("\t\"Regeneration_Time_Constant\": %f,\n", dive->Regeneration_Time_Constant);
    lprintf("\t\"Constant_Pressure_Other_Gases\": %f,\n", dive->Constant_Pressure_Other_Gases);
    lprintf("\t\"Gradient_Onset_of_Imperm_Atm\": %f,\n", dive->Gradient_Onset_of_Imperm_Atm);
	
    lprintf("\t\"Number_of_Changes\": %d,\n", dive->Number_of_Changes);
    lprintf("\t\"Segment_Number_Start_of_Ascent\": %d,\n", dive->Segment_Number_Start_of_Ascent);
    lprintf("\t\"Repetitive_Dive_Flag\": %d,\n", dive->Repetitive_Dive_Flag);
    lprintf("\t\"Schedule_Converged\": %d,\n", dive->Schedule_Converged);
    lprintf("\t\"Critical_Volume_Algorithm_Off\": %d,\n", dive->Critical_Volume_Algorithm_Off);
    lprintf("\t\"Altitude_Dive_Algorithm_Off\": %d,\n", dive->Altitude_Dive_Algorithm_Off);
		//	YIELD();
    lprintf("\t\"Ascent_Ceiling_Depth\": %f,\n", dive->Ascent_Ceiling_Depth);
    lprintf("\t\"Deco_Stop_Depth\": %f,\n", dive->Deco_Stop_Depth);
    lprintf("\t\"Step_Size\": %f,\n", dive->Step_Size);
    lprintf("\t\"Depth\": %f,\n", dive->Depth);
    lprintf("\t\"Last_Depth\": %f,\n", dive->Last_Depth);
    lprintf("\t\"Ending_Depth\": %f,\n", dive->Ending_Depth);
    lprintf("\t\"Starting_Depth\": %f,\n", dive->Starting_Depth);
    lprintf("\t\"Rate\": %f,\n", dive->Rate);
    lprintf("\t\"Run_Time_End_of_Segment\": %f,\n", dive->Run_Time_End_of_Segment);
    lprintf("\t\"Last_Run_Time\": %f,\n", dive->Last_Run_Time);
    lprintf("\t\"Stop_Time\": %f,\n", dive->Stop_Time);
    lprintf("\t\"Depth_Start_of_Deco_Zone\": %f,\n", dive->Depth_Start_of_Deco_Zone);
    lprintf("\t\"Deepest_Possible_Stop_Depth\": %f,\n", dive->Deepest_Possible_Stop_Depth);
    lprintf("\t\"First_Stop_Depth\": %f,\n", dive->First_Stop_Depth);
    lprintf("\t\"Critical_Volume_Comparison\": %f,\n", dive->Critical_Volume_Comparison);
    lprintf("\t\"Next_Stop\": %f,\n", dive->Next_Stop);
    lprintf("\t\"Run_Time_Start_of_Deco_Zone\": %f,\n", dive->Run_Time_Start_of_Deco_Zone);
    lprintf("\t\"Critical_Radius_N2_Microns\": %f,\n", dive->Critical_Radius_N2_Microns);
    lprintf("\t\"Critical_Radius_He_Microns\": %f,\n", dive->Critical_Radius_He_Microns);
    lprintf("\t\"Run_Time_Start_of_Ascent\": %f,\n", dive->Run_Time_Start_of_Ascent);
    lprintf("\t\"Altitude_of_Dive\": %f,\n", dive->Altitude_of_Dive);
    lprintf("\t\"Deco_Phase_Volume_Time\": %f,\n", dive->Deco_Phase_Volume_Time);
    lprintf("\t\"Surface_Interval_Time\": %f,\n", dive->Surface_Interval_Time);
		//		YIELD();

    output_array_int("\t\"Mix_Change\"", dive->Mix_Change, 16);
    output_array_int("\t\"Depth_Change\"", dive->Depth_Change, 16);
    	//		YIELD();
		output_array_int("\t\"Rate_Change\"", dive->Rate_Change, 16);
    output_array_int("\t\"Step_Size_Change\"", dive->Step_Size_Change, 16);
	
    output_array_double("\t\"Regenerated_Radius_He\"", dive->Regenerated_Radius_He, 16);
    output_array_double("\t\"Regenerated_Radius_N2\"", dive->Regenerated_Radius_N2, 16);
    output_array_double("\t\"He_Pressure_Start_of_Ascent\"", dive->He_Pressure_Start_of_Ascent, 16);
    output_array_double("\t\"N2_Pressure_Start_of_Ascent\"", dive->N2_Pressure_Start_of_Ascent, 16);
    output_array_double("\t\"He_Pressure_Start_of_Deco_Zone\"", dive->He_Pressure_Start_of_Deco_Zone, 16);
    output_array_double("\t\"N2_Pressure_Start_of_Deco_Zone\"", dive->N2_Pressure_Start_of_Deco_Zone, 16);
    output_array_double("\t\"Phase_Volume_Time\"", dive->Phase_Volume_Time, 16);
    		//	YIELD();
		output_array_double("\t\"Last_Phase_Volume_Time\"", dive->Last_Phase_Volume_Time, 16);
    output_array_double("\t\"Allowable_Gradient_He\"", dive->Allowable_Gradient_He, 16);
    output_array_double("\t\"Allowable_Gradient_N2\"", dive->Allowable_Gradient_N2, 16);
    output_array_double("\t\"Adjusted_Crushing_Pressure_He\"", dive->Adjusted_Crushing_Pressure_He, 16);
    output_array_double("\t\"Adjusted_Crushing_Pressure_N2\"", dive->Adjusted_Crushing_Pressure_N2, 16);
    output_array_double("\t\"Initial_Allowable_Gradient_N2\"", dive->Initial_Allowable_Gradient_N2, 16);
    output_array_double("\t\"Initial_Allowable_Gradient_He\"", dive->Initial_Allowable_Gradient_He, 16);
    output_array_double("\t\"Deco_Gradient_He\"", dive->Deco_Gradient_He, 16);
    output_array_double("\t\"Deco_Gradient_N2\"", dive->Deco_Gradient_N2, 16);
	
    lprintf("\t\"Segment_Number\": %d,\n", dive->Segment_Number);
    lprintf("\t\"Mix_Number\": %d,\n", dive->Mix_Number);
    lprintf("\t\"units_fsw\": %d,\n", dive->units_fsw);
	
    lprintf("\t\"Run_Time\": %f,\n", dive->Run_Time);
    lprintf("\t\"Segment_Time\": %f,\n", dive->Segment_Time);
    lprintf("\t\"Ending_Ambient_Pressure\": %f,\n", dive->Ending_Ambient_Pressure);
    lprintf("\t\"Barometric_Pressure\": %f,\n", dive->Barometric_Pressure);
    lprintf("\t\"Units_Factor\": %f,\n", dive->Units_Factor);
				//YIELD();

    output_array_double("\t\"Helium_Time_Constant\"", dive->Helium_Time_Constant, 16);
    output_array_double("\t\"Nitrogen_Time_Constant\"", dive->Nitrogen_Time_Constant, 16);
    output_array_double("\t\"Helium_Pressure\"", dive->Helium_Pressure, 16);
    output_array_double("\t\"Nitrogen_Pressure\"", dive->Nitrogen_Pressure, 16);
    output_array_double("\t\"Initial_Helium_Pressure\"", dive->Initial_Helium_Pressure, 16);
    output_array_double("\t\"Initial_Nitrogen_Pressure\"", dive->Initial_Nitrogen_Pressure, 16);
    output_array_double("\t\"Initial_Critical_Radius_He\"", dive->Initial_Critical_Radius_He, 16);
    output_array_double("\t\"Initial_Critical_Radius_N2\"", dive->Initial_Critical_Radius_N2, 16);
    output_array_double("\t\"Adjusted_Critical_Radius_He\"", dive->Adjusted_Critical_Radius_He, 16);
    output_array_double("\t\"Adjusted_Critical_Radius_N2\"", dive->Adjusted_Critical_Radius_N2, 16);
    output_array_double("\t\"Max_Crushing_Pressure_He\"", dive->Max_Crushing_Pressure_He, 16);
    output_array_double("\t\"Max_Crushing_Pressure_N2\"", dive->Max_Crushing_Pressure_N2, 16);
    output_array_double("\t\"Surface_Phase_Volume_Time\"", dive->Surface_Phase_Volume_Time, 16);
    output_array_double("\t\"Max_Actual_Gradient\"", dive->Max_Actual_Gradient, 16);
    output_array_double("\t\"Amb_Pressure_Onset_of_Imperm\"", dive->Amb_Pressure_Onset_of_Imperm, 16);
    output_array_double("\t\"Gas_Tension_Onset_of_Imperm\"", dive->Gas_Tension_Onset_of_Imperm, 16);
	
    /* double *Fraction_Helium; */
    /* double *Fraction_Nitrogen; */
	
    lprintf("\t\"Diver_Acclimatized\": %d,\n", dive->Diver_Acclimatized);
    lprintf("\t\"Decompressing\": %d,\n", dive->Decompressing);
    lprintf("\t\"Real_Time_Decompression\": %d,\n", dive->Real_Time_Decompression);
	
    lprintf("\t\"Last_Direction_Depth\": %f,\n", dive->Last_Direction_Depth);
    lprintf("\t\"Last_Direction_Time\": %f,\n", dive->Last_Direction_Time);
	
    lprintf("\t\"Start_of_Decompression_Zone\": %f,\n", dive->Start_of_Decompression_Zone);
	
    /* decompression_stops *decomp_stops; */
    lprintf("\t\"decomp_stop_index\": %d,\n", dive->decomp_stop_index);
	lprintf("\t\"ndecomp_stops\": %d,\n", dive->ndecomp_stops);

    lprintf("\t\"Wait_Time\": %f,\n", dive->Wait_Time);
    lprintf("\t\"OTU_Sum\": %f,\n", dive->OTU_Sum);
    lprintf("\t\"CNS_Sum\": %f,\n", dive->CNS_Sum);
    lprintf(			"}\n");
		YIELD();
		output_deco_stops();
		YIELD();

	lflush();
}



void VPMB::output_dive_state_short(void)
{
	lprintf("state:{\n");

    //lprintf("\tUnits: %s,\n", dive->Units);
    //lprintf("\tUnits_Word1: %s,\n", dive->Units_Word1);
    //lprintf("\tUnits_Word2: %s,\n", dive->Units_Word2);
	
    lprintf("wvp: %3.4f,\n", dive->Water_Vapor_Pressure);
    lprintf("stg: %3.4f,\n", dive->Surface_Tension_Gamma);
    lprintf("scg: %3.4f,\n", dive->Skin_Compression_GammaC);
    lprintf("cvpl: %3.4f,\n", dive->Crit_Volume_Parameter_Lambda);
    lprintf("mdst: %3.4f,\n", dive->Minimum_Deco_Stop_Time);
    lprintf("rtc: %3.4f,\n", dive->Regeneration_Time_Constant);
    lprintf("cpog: %3.4f,\n", dive->Constant_Pressure_Other_Gases);
    lprintf("gooia: %3.4f,\n", dive->Gradient_Onset_of_Imperm_Atm);
	
    lprintf("noc: %d,\n", dive->Number_of_Changes);
    lprintf("snsoa: %d,\n", dive->Segment_Number_Start_of_Ascent);
    lprintf("rdf: %d,\n", dive->Repetitive_Dive_Flag);
    lprintf("sc: %d,\n", dive->Schedule_Converged);
    lprintf("cvao: %d,\n", dive->Critical_Volume_Algorithm_Off);
    lprintf("adao: %d,\n", dive->Altitude_Dive_Algorithm_Off);
		//	YIELD();
    lprintf("acd: %3.4f,\n", dive->Ascent_Ceiling_Depth);
    lprintf("dcd: %3.4f,\n", dive->Deco_Stop_Depth);
    lprintf("ss: %3.4f,\n", dive->Step_Size);
    lprintf("d: %3.4f,\n", dive->Depth);
    lprintf("ld: %3.4f,\n", dive->Last_Depth);
    lprintf("ed: %3.4f,\n", dive->Ending_Depth);
    lprintf("sd: %3.4f,\n", dive->Starting_Depth);
    lprintf("r: %3.4f,\n", dive->Rate);
    lprintf("rteos: %3.4f,\n", dive->Run_Time_End_of_Segment);
    lprintf("lrt: %3.4f,\n", dive->Last_Run_Time);
    lprintf("st: %3.4f,\n", dive->Stop_Time);
    lprintf("dosdz: %3.4f,\n", dive->Depth_Start_of_Deco_Zone);
    lprintf("dpsd: %3.4f,\n", dive->Deepest_Possible_Stop_Depth);
    lprintf("fsd: %3.4f,\n", dive->First_Stop_Depth);
    lprintf("cvc: %3.4f,\n", dive->Critical_Volume_Comparison);
    lprintf("Next_Stop: %3.4f,\n", dive->Next_Stop);
    lprintf("rtdz: %3.4f,\n", dive->Run_Time_Start_of_Deco_Zone);
    lprintf("crn2m: %3.4f,\n", dive->Critical_Radius_N2_Microns);
    lprintf("crhem: %3.4f,\n", dive->Critical_Radius_He_Microns);
    lprintf("rtsoa: %3.4f,\n", dive->Run_Time_Start_of_Ascent);
    lprintf("aod: %3.4f,\n", dive->Altitude_of_Dive);
    lprintf("dpvt: %3.4f,\n", dive->Deco_Phase_Volume_Time);
    lprintf("sit: %3.4f,\n", dive->Surface_Interval_Time);
		//		YIELD();

    output_array_int("mc", dive->Mix_Change, 16);
    output_array_int("dc", dive->Depth_Change, 16);
    	//		YIELD();
		output_array_int("rc", dive->Rate_Change, 16);
    output_array_int("ssc", dive->Step_Size_Change, 16);
	
    output_array_double_short("rrhe", dive->Regenerated_Radius_He, 16);
    output_array_double_short("rrn2", dive->Regenerated_Radius_N2, 16);
    output_array_double_short("hepsoa", dive->He_Pressure_Start_of_Ascent, 16);
    output_array_double_short("n2psoa", dive->N2_Pressure_Start_of_Ascent, 16);
    output_array_double_short("hepsodz", dive->He_Pressure_Start_of_Deco_Zone, 16);
    output_array_double_short("n2psodz", dive->N2_Pressure_Start_of_Deco_Zone, 16);
    output_array_double_short("pvt", dive->Phase_Volume_Time, 16);
    		//	YIELD();
		output_array_double_short("lpvt", dive->Last_Phase_Volume_Time, 16);
    output_array_double_short("aghe", dive->Allowable_Gradient_He, 16);
    output_array_double_short("agn2", dive->Allowable_Gradient_N2, 16);
    output_array_double_short("acphe", dive->Adjusted_Crushing_Pressure_He, 16);
    output_array_double_short("acpn2", dive->Adjusted_Crushing_Pressure_N2, 16);
    output_array_double_short("iagn2", dive->Initial_Allowable_Gradient_N2, 16);
    output_array_double_short("iaghe", dive->Initial_Allowable_Gradient_He, 16);
    output_array_double_short("dghe", dive->Deco_Gradient_He, 16);
    output_array_double_short("dgn2", dive->Deco_Gradient_N2, 16);
	
    lprintf("sn: %d,\n", dive->Segment_Number);
    lprintf("mn: %d,\n", dive->Mix_Number);
    lprintf("ufsw: %d,\n", dive->units_fsw);
	
    lprintf("rt: %3.4f,\n", dive->Run_Time);
    lprintf("sgt: %3.4f,\n", dive->Segment_Time);
    lprintf("eap: %3.4f,\n", dive->Ending_Ambient_Pressure);
    lprintf("bp: %3.4f,\n", dive->Barometric_Pressure);
    lprintf("uf: %3.4f,\n", dive->Units_Factor);
				//YIELD();

    output_array_double_short("hetc", dive->Helium_Time_Constant, 16);
    output_array_double_short("n2tc", dive->Nitrogen_Time_Constant, 16);
    output_array_double_short("hep", dive->Helium_Pressure, 16);
    output_array_double_short("n2p", dive->Nitrogen_Pressure, 16);
    output_array_double_short("ihep", dive->Initial_Helium_Pressure, 16);
    output_array_double_short("in2p", dive->Initial_Nitrogen_Pressure, 16);
    output_array_double_short("icrhe", dive->Initial_Critical_Radius_He, 16);
    output_array_double_short("icrn2", dive->Initial_Critical_Radius_N2, 16);
    output_array_double_short("acrhe", dive->Adjusted_Critical_Radius_He, 16);
    output_array_double_short("acrn2", dive->Adjusted_Critical_Radius_N2, 16);
    output_array_double_short("mcphe", dive->Max_Crushing_Pressure_He, 16);
    output_array_double_short("mcpn2", dive->Max_Crushing_Pressure_N2, 16);
    output_array_double_short("spvt", dive->Surface_Phase_Volume_Time, 16);
    output_array_double_short("mag", dive->Max_Actual_Gradient, 16);
    output_array_double_short("apooi", dive->Amb_Pressure_Onset_of_Imperm, 16);
    output_array_double_short("gtooi", dive->Gas_Tension_Onset_of_Imperm, 16);
	
    /* double *Fraction_Helium; */
    /* double *Fraction_Nitrogen; */
	
    lprintf("da: %d,\n", dive->Diver_Acclimatized);
    lprintf("dg: %d,\n", dive->Decompressing);
    lprintf("rtd: %d,\n", dive->Real_Time_Decompression);
	
    lprintf("ldd: %3.4f,\n", dive->Last_Direction_Depth);
    lprintf("ldt: %3.4f,\n", dive->Last_Direction_Time);
	
    lprintf("sodz: %3.4f,\n", dive->Start_of_Decompression_Zone);
	
    /* decompression_stops *decomp_stops; */
    lprintf("dsi: %d,\n", dive->decomp_stop_index);
	lprintf("nds: %d,\n", dive->ndecomp_stops);

    lprintf("wt: %3.4f,\n", dive->Wait_Time);
    lprintf("otus: %3.4f,\n", dive->OTU_Sum);
    lprintf("cnss:%3.4f,\n", dive->CNS_Sum);
    lprintf(			"}\n");
		YIELD();
		output_deco_stops();
		YIELD();

	lflush();
}

