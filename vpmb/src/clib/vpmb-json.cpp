/*
 *  vpm-json.c
 *  
 *
 *  Created by Dirk Niggemann on 03/02/2018.
 *  Copyright 2018 __MyCompanyName__. All rights reserved.
 *
 */
#include <Arduino.h>
#include "vpmb.h"
#include "vpmb-json.h"
#include "vpmb-data.h"
//#include "cJSON.h"
#include <ArduinoJson.h>


//const int BADJSON = -1;
//const int GOODJSON = 1;
static bool vpmb_input_add_dive_profiles(dive_profile *in, JsonObject &profile);
static bool vpmb_input_add_dive(single_dive *in, JsonObject &dive);
//static double vpmb_get_setpoint_or_null(const JsonObject &setpoint);

#define vpmb_get_setpoint_or_null(a) a
static bool vpmb_input_add_dive_profiles(dive_profile *in, JsonObject &profile)
{
    int i;
	
    in->profile_code = profile["profile_code"];
    if (in->profile_code == 1) {
        in->starting_depth = profile[ "starting_depth"];
        in->ending_depth  = profile [ "ending_depth" ];
        in->rate  = profile [ "rate"];
        in->gasmix  = profile [ "gasmix" ];
        in->setpoint = vpmb_get_setpoint_or_null(profile[ "setpoint"]);
    } else if (in->profile_code == 2) {
        in->depth = profile [ "depth"];
        in->run_time_at_end_of_segment  = profile [ "run_time_at_end_of_segment"];
        in->gasmix = profile [ "gasmix"];
        in->setpoint  = vpmb_get_setpoint_or_null(profile[  "setpoint"]);
		
    } else if (in->profile_code == 99) {
		JsonArray &ascent_summaries = profile["ascent_summary"];
		in->number_of_ascent_parameter_changes = ascent_summaries.size();
		
        in->ascents = (ascent_summary*)calloc(in->number_of_ascent_parameter_changes, sizeof(ascent_summary));
		
        if(in->ascents == NULL) {
            //vpmb_failure();
			return false;
        }
		
        for(i = 0; i < in->number_of_ascent_parameter_changes; i++) {
            JsonObject &current_ascent = ascent_summaries[i];
            in->ascents[i].starting_depth = current_ascent["starting_depth"];
            in->ascents[i].gasmix = current_ascent["gasmix"];
            in->ascents[i].rate = current_ascent["rate"];
            in->ascents[i].step_size = current_ascent["step_size"];
            in->ascents[i].setpoint = vpmb_get_setpoint_or_null(current_ascent["setpoint"]);
        }
    } else {
        return false;
    }
    return true;
}

static bool vpmb_input_add_dive(single_dive *in, JsonObject &dive)
{
    int i;
	
    strlcpy(in->desc, dive[ "desc"], sizeof(in->desc));
    in->repetitive_code = dive[ "repetitive_code"];
	
    if(in->repetitive_code == 1) {
        in->surface_interval_time_minutes = dive[ "surface_interval_time_minutes"];
    }
	JsonArray &gasmix_summary_fields = dive[ "gasmix_summary"];
	in->num_gas_mixes = gasmix_summary_fields.size();
    in->gasmixes = (gasmix_summary *)calloc(in->num_gas_mixes, sizeof(gasmix_summary));
    for(i = 0; i < in->num_gas_mixes; i++) {
		
        JsonObject &current_gasmix = gasmix_summary_fields[i];
        in->gasmixes[i].fraction_O2 = current_gasmix[ "fraction_O2"];
        in->gasmixes[i].fraction_He = current_gasmix[ "fraction_He"];
        in->gasmixes[i].fraction_N2 = current_gasmix[ "fraction_N2"];
    }
	
	JsonArray &profile_code_fields = dive["profile_codes"];
	in->num_profile_codes = profile_code_fields.size(); 
    in->dive_profiles = (dive_profile *)calloc(in->num_profile_codes, sizeof(dive_profile));
    for(i = 0; i < in->num_profile_codes; i++) {
		
        JsonObject &current_profile = profile_code_fields[i];
		
        vpmb_input_add_dive_profiles(&(in->dive_profiles[i]), current_profile);
    }
    return true;
}


bool vpmb_load_dives_from_json(vpmb_input *in, JsonObject &json)
{
    int i;
    size_t len;
    //char *data;
	bool setpoint_is_bar;
    /* find out the size of the file and allocate enough memory to hold it */
   
    JsonArray  &dives = json["input"];
	
    in->number_of_dives = dives.size();
    in->dives = (single_dive*)calloc(in->number_of_dives, sizeof(single_dive));
	
    for (i = 0; i < dives.size(); i++) {
        JsonObject &current_dive = dives[i];
		vpmb_input_add_dive(&(in->dives[i]), current_dive);

//        if (current_dive != NULL) {
//        } else {
            //free(data);
//            return false;
//        }
    }
	
    /* copy altitude */	
    return true;
}


static bool vpmb_input_add_gas(gases *in, JsonObject &gas)
{
    int i;	
    strlcpy(in->name, gas[ "name"], 20);
    in->active = gas[ "active"];
	in->MOD = gas["MOD"];
	JsonObject &current_gasmix = gas[ "gasmix_summary"];
    in->gasmix.fraction_O2 = current_gasmix[ "fraction_O2"];
	in->gasmix.fraction_He = current_gasmix[ "fraction_He"];
	in->gasmix.fraction_N2 = current_gasmix[ "fraction_N2"];	
	return true;
}


bool vpmb_load_config_from_json(vpmb_input *in, JsonObject &json)
{
    int i;
    size_t len;
    //char *data;
	//bool setpoint_is_bar;
    /* find out the size of the file and allocate enough memory to hold it */
	
    JsonObject  &config = json["config"];
	
    //in->number_of_dives = dives.size();
    //in->dives = (single_dive*)calloc(in->number_of_dives, sizeof(single_dive));
	//
    //for (i = 0; i < dives.size(); i++) {
    //    JsonObject &current_dive = dives[i];
	//	vpmb_input_add_dive(&(in->dives[i]), current_dive);
		
		////        if (current_dive != NULL) {
		////        } else {
		////free(data);
		////            return false;
		////        }
    //}
	
    /* copy altitude */
    JsonObject &altitude = config[ "altitude"];
	
    in->Altitude_of_Dive = altitude["Altitude_of_Dive"];
		in->Diver_Acclimatized_at_Altitude = altitude["Diver_Acclimatized_at_Altitude"];
    //strlcpy(in->Diver_Acclimatized_at_Altitude, altitude["Diver_Acclimatized_at_Altitude"], sizeof(in->Diver_Acclimatized_at_Altitude));
	
    in->Starting_Acclimatized_Altitude = altitude["Starting_Acclimatized_Altitude"];
    in->Ascent_to_Altitude_Hours = altitude["Ascent_to_Altitude_Hours"];
    in->Hours_at_Altitude_Before_Dive = altitude["Hours_at_Altitude_Before_Dive"];
	
    /* copy settings */
    JsonObject &settings = config["settings"];
	
    strlcpy(in->Units, settings["Units"], sizeof(in->Units));
	
    //setpoint_is_bar = 
		in->SetPoint_Is_Bar = settings["SetPoint_Is_Bar"];
    //if (setpoint_is_bar != NULL) {
    //    if (cJSON_true == setpoint_is_bar->type) {
    //        in->SetPoint_Is_Bar = true;
    //    } else if (cJSON_False == setpoint_is_bar->type) {
    //        in->SetPoint_Is_Bar = false;
    //    } else {
    //        free(data);
    //        return BADJSON;
    //    }
    //}
    //strlcpy(, , sizeof(in->Altitude_Dive_Algorithm));
		in->Altitude_Dive_Algorithm = settings["Altitude_Dive_Algorithm"];
    in->Minimum_Deco_Stop_Time = settings["Minimum_Deco_Stop_Time"];
    in->Critical_Radius_N2_Microns = settings["Critical_Radius_N2_Microns"];
    in->Critical_Radius_He_Microns = settings["Critical_Radius_He_Microns"];
    //strlcpy(in->Critical_Volume_Algorithm, settings["Critical_Volume_Algorithm"], sizeof(in->Critical_Volume_Algorithm));
    in->Critical_Volume_Algorithm = settings["Critical_Volume_Algorithm"];
    in->Crit_Volume_Parameter_Lambda = settings["Crit_Volume_Parameter_Lambda"];
    in->Gradient_Onset_of_Imperm_Atm = settings["Gradient_Onset_of_Imperm_Atm"];
    in->Surface_Tension_Gamma = settings["Surface_Tension_Gamma"];
    in->Skin_Compression_GammaC = settings["Skin_Compression_GammaC"];
    in->Regeneration_Time_Constant = settings["Regeneration_Time_Constant"];
    in->Pressure_Other_Gases_mmHg = settings["Pressure_Other_Gases_mmHg"];
		in->CCR_Mode = settings["CCR_Mode"];
		in->default_step_size = settings["Default_Step_Size"];
		in->default_ascent_rate = settings["Default_Ascent_Rate"]; //Should be -ve
	
		/* copy settings */
    JsonArray &gaslist = config["gases"];
	
	
		in->number_of_gases = gaslist.size();
    in->gaslist = (gases*)calloc(in->number_of_gases, sizeof(gases));
	//
    for (i = 0; i < in->number_of_gases; i++) {
        JsonObject &current_gas = gaslist[i];
	
		vpmb_input_add_gas(&(in->gaslist[i]), current_gas);
		//DBG_PRINT("Gas: %s\n", in->gaslist[i].name);
		//DBG_PRINT("  O2 ->%f\n", in->gaslist[i].gasmix.fraction_O2);

		////        if (current_dive != NULL) {
	////        } else {
	////free(data);
	////            return false;
	////        }
	}
	JsonArray &setpoints = config["setpoints"];
	in->number_of_setpoints = setpoints.size();
	in->setpoints = (double *)calloc(in->number_of_setpoints, sizeof(double));

	for (i = 0; i < in->number_of_setpoints; i++) {
		in->setpoints[i] = setpoints[i];
		//DBG_PRINT("SP: %f\n", in->setpoints[i]);

	}
		
    //cJSON_Delete(json);
    //free(data);
	
    return true;
}



static bool vpmb_output_add_gas(gases *in, JsonObject &gas)
{
  gas[ "name"] = in->name;
  gas[ "active"] = in->active ;
 	gas["MOD"] = in->MOD;
	JsonObject &current_gasmix = gas.createNestedObject("gasmix_summary");
	current_gasmix[ "fraction_O2"] = in->gasmix.fraction_O2;
	current_gasmix[ "fraction_He"] = in->gasmix.fraction_He;
 	current_gasmix[ "fraction_N2"] = in->gasmix.fraction_N2;	
	return true;
}

bool vpmb_save_config_to_json(vpmb_input *in, JsonObject &config)
{
    int i;
    size_t len;
    //char *data;
		bool setpoint_is_bar;
    /* find out the size of the file and allocate enough memory to hold it */
	
    //JsonObject  &config = json.createNestedObject("config");
	
    //in->number_of_dives = dives.size();
    //in->dives = (single_dive*)calloc(in->number_of_dives, sizeof(single_dive));
	//
    //for (i = 0; i < dives.size(); i++) {
    //    JsonObject &current_dive = dives[i];
	//	vpmb_input_add_dive(&(in->dives[i]), current_dive);
		
		////        if (current_dive != NULL) {
		////        } else {
		////free(data);
		////            return false;
		////        }
    //}
	
    /* copy altitude */
    JsonObject &altitude = config.createNestedObject("altitude");
    altitude["Altitude_of_Dive"]  = in->Altitude_of_Dive;
	
		altitude["Diver_Acclimatized_at_Altitude"] = in->Diver_Acclimatized_at_Altitude;
    altitude["Starting_Acclimatized_Altitude"] = in->Starting_Acclimatized_Altitude;
    altitude["Ascent_to_Altitude_Hours"] = in->Ascent_to_Altitude_Hours;
    altitude["Hours_at_Altitude_Before_Dive"] = in->Hours_at_Altitude_Before_Dive;
	
    /* copy settings */
    JsonObject &settings =  config.createNestedObject("settings");
	
		settings["Units"] = in->Units;
	
    settings["SetPoint_Is_Bar"] = in->SetPoint_Is_Bar;
		//in->SetPoint_Is_Bar = setpoint_is_bar ? true : false;
    //if (setpoint_is_bar != NULL) {
    //    if (cJSON_true == setpoint_is_bar->type) {
    //        in->SetPoint_Is_Bar = true;
    //    } else if (cJSON_False == setpoint_is_bar->type) {
    //        in->SetPoint_Is_Bar = false;
    //    } else {
    //        free(data);
    //        return BADJSON;
    //    }
    //}
		settings["Altitude_Dive_Algorithm"] = in->Altitude_Dive_Algorithm;
    settings["Minimum_Deco_Stop_Time"] = in->Minimum_Deco_Stop_Time;
    settings["Critical_Radius_N2_Microns"] =in->Critical_Radius_N2_Microns;
    settings["Critical_Radius_He_Microns"] = in->Critical_Radius_He_Microns;
    settings["Critical_Volume_Algorithm"] = in->Critical_Volume_Algorithm;
    settings["Crit_Volume_Parameter_Lambda"] = in->Crit_Volume_Parameter_Lambda;
    settings["Gradient_Onset_of_Imperm_Atm"] = in->Gradient_Onset_of_Imperm_Atm;
	  settings["Surface_Tension_Gamma"] = in->Surface_Tension_Gamma ;
	  settings["Skin_Compression_GammaC"] = in->Skin_Compression_GammaC;
	  settings["Regeneration_Time_Constant"] = in->Regeneration_Time_Constant ;
	  settings["Pressure_Other_Gases_mmHg"] = in->Pressure_Other_Gases_mmHg ;
		settings["CCR_Mode"] = in->CCR_Mode;
		settings["Default_Step_Size"] = in->default_step_size;
		settings["Default_Ascent_Rate"] = in->default_ascent_rate; //Should be -ve

	/* copy settings */
   JsonArray &gaslist = config.createNestedArray("gases");
	
	
    for (i = 0; i < in->number_of_gases; i++) {
        JsonObject &current_gas = gaslist.createNestedObject();
	
			vpmb_output_add_gas(&(in->gaslist[i]), current_gas);
	  }
		//DBG_PRINT("Gas: %s\n", in->gaslist[i].name);
		//DBG_PRINT("  O2 ->%f\n", in->gaslist[i].gasmix.fraction_O2);

		////        if (current_dive != NULL) {
	////        } else {
	////free(data);
	////            return false;
	////        }
	//}
	JsonArray &setpoints = config.createNestedArray("setpoints");

	for (i = 0; i < in->number_of_setpoints; i++) {
		setpoints.add(in->setpoints[i]);
		//DBG_PRINT("SP: %f\n", in->setpoints[i]);

	}
		
    //cJSON_Delete(json);
    //free(data);
	
    return true;
}




//double vpmb_get_setpoint_or_null(JsonObject &setpoint)
//{
//    if (setpoint == NULL) {
//        return 0.0;
//    }
//    return (double)setpoint;
//}
