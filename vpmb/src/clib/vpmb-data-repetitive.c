/*
 *  vpmb-data-repetitive.c
 *  
 *
 *  Created by Dirk Niggemann on 16/03/2018.
 *  Copyright 2018 __MyCompanyName__. All rights reserved.
 *
 */

#include "vpmb-data-repetitive.h"



int vpmb_set_gas_mixes(dive_state *dive, single_dive *current_dive)
{
    int i,j, num_setpoints;
    int num_gas_mixes = current_dive->num_gas_mixes;
	int num_profile = current_dive->num_profile_codes;
    double Fraction_Oxygen;
    gasmix_summary *current_gasmix;
	dive_profile * current_profile;
	
	if(dive->Fraction_Helium != NULL) {
        free(dive->Fraction_Helium);
		dive->Fraction_Helium = NULL;
    }
    if(dive->Fraction_Nitrogen != NULL) {
        free(dive->Fraction_Nitrogen);
		dive->Fraction_Nitrogen = NULL;
		
    }
	if(dive->SetPoints != NULL) {
        free(dive->SetPoints);
		dive->SetPoints = NULL;
    }
	
    dive->Fraction_Helium = calloc(num_gas_mixes, sizeof(double));
    dive->Fraction_Nitrogen = calloc(num_gas_mixes, sizeof(double));
		if (!dive->Fraction_Nitrogen || !dive->Fraction_Helium){
			lprintf("M,%s,no memory\n", __func__);
      return INVALIDDATA;

		}
    for(i = 0; i < num_gas_mixes; i++) {
		
        current_gasmix = &(current_dive->gasmixes[i]);
        Fraction_Oxygen = current_gasmix->fraction_O2;
        dive->Fraction_Nitrogen[i] = current_gasmix->fraction_N2;
        dive->Fraction_Helium[i] = current_gasmix->fraction_He;
		
        /* make sure the fractions add up */
        if((Fraction_Oxygen + dive->Fraction_Nitrogen[i] + dive->Fraction_Helium[i]) != 1.0) {
            return INVALIDDATA;
        }
		
        //for i in range(num_gas_mixes):
        //  dive->output_object.add_gasmix(Fraction_Oxygen[i], self.Fraction_Nitrogen[i], self.Fraction_Helium[i])
    }
	num_setpoints = num_profile;
	for (i = 0; i < num_profile; i++) {
		current_profile = &(current_dive->dive_profiles[i]);
		if(current_profile->profile_code == 99) {
			num_setpoints += current_profile->number_of_ascent_parameter_changes;
		}
	}
	dive->SetPoints = calloc(num_setpoints, sizeof(double));
	if (!dive->SetPoints ){
			lprintf("M,%s,no memory\n", __func__);
      return INVALIDDATA;

	}
	dive->Number_Of_SetPoints = num_setpoints;
	for (i = 0; i < num_profile; i++) {
		current_profile = &(current_dive->dive_profiles[i]);
		dive->SetPoints[i] = current_profile->setpoint;
	}
	for (i = num_profile; i < num_setpoints; i++) {
		current_profile = &(current_dive->dive_profiles[i]);
		if(current_profile->profile_code == 99) {
			for (j = 0; j < current_profile->number_of_ascent_parameter_changes; j++)
				 dive->SetPoints[i++] = current_profile->ascents[j].setpoint;
		}
	}
    return VALIDDATA;
}


bool vpmb_profile_code_loop(dive_state *dive, single_dive *current_dive)
{
    int i;
	
    for(i = 0; i < current_dive->num_profile_codes; i++) {
        dive_profile *current_profile = &(current_dive->dive_profiles[i]);
        int Profile_Code = current_profile->profile_code;
		
        if(Profile_Code == 1) {
            char Word[10] = "TEMP";
            dive->Starting_Depth = current_profile->starting_depth;
            dive->Ending_Depth = current_profile->ending_depth;
            dive->Rate = current_profile->rate;
            dive->Mix_Number = current_profile->gasmix;
			
            vpmb_gas_loadings_ascent_descent(dive, dive->Starting_Depth, dive->Ending_Depth, dive->Rate);
            if(dive->Ending_Depth > dive->Starting_Depth) {
                bool res = vpmb_calc_crushing_pressure(dive, dive->Starting_Depth, dive->Ending_Depth, dive->Rate);
				if (!res) return FALSE;
            }
            /* the error seems unnecessary */
            if( dive->Ending_Depth > dive->Starting_Depth) {
                strlcpy(Word, "Descent", strlen(Word));
            } else if (dive->Starting_Depth <  dive->Ending_Depth) {
                strlcpy(Word, "Ascent", strlen(Word));
            } else {
                strlcpy(Word, "ERROR", strlen(Word));
            }
			
            /* dive->output_object.add_dive_profile_entry_descent(dive->Segment_Number, dive->Segment_Time, dive->Run_Time, dive->Mix_Number, Word, dive->Starting_Depth, dive->Ending_Depth, dive->Rate) */
        } else if( Profile_Code == 2) {
            dive->Depth = current_profile->depth;
            dive->Run_Time_End_of_Segment = current_profile->run_time_at_end_of_segment;
            dive->Mix_Number = current_profile->gasmix;
            vpmb_gas_loadings_constant_depth(dive, dive->Depth, dive->Run_Time_End_of_Segment);
			
            /* dive->output_object.add_dive_profile_entry_ascent(dive->Segment_Number, dive->Segment_Time, dive->Run_Time, dive->Mix_Number, dive->Depth) */
        } else if(Profile_Code == 99) {
            break;
        }
		
    }
	return TRUE;
}

void vpmb_decompression_loop(dive_state *dive, single_dive *current_dive)
{
    int i, j;
    vpmb_nuclear_regeneration(dive, dive->Run_Time);
	
    vpmb_calc_initial_allowable_gradient(dive);
	
    for(i = 0; i < 16; i++) {
        dive->He_Pressure_Start_of_Ascent[i] = dive->Helium_Pressure[i];
        dive->N2_Pressure_Start_of_Ascent[i] = dive->Nitrogen_Pressure[i];
    }
	
    dive->Run_Time_Start_of_Ascent = dive->Run_Time;
    dive->Segment_Number_Start_of_Ascent = dive->Segment_Number;
	
    for(i = 0; i < current_dive->num_profile_codes; i++) {
        dive_profile *current_profile = &(current_dive->dive_profiles[i]);
		
        int Profile_Code = current_profile->profile_code;
		
        if(Profile_Code == 99) {
            dive->Number_of_Changes = current_profile->number_of_ascent_parameter_changes;
			
            // TODO remove hard coding of 16 and malloc this
            //dive->Depth_Change = [0.0 for i in range(dive->Number_of_Changes)]
            //        dive->Mix_Change = [0.0 for i in range(dive->Number_of_Changes)]
            //dive->Rate_Change = [0.0 for i in range(dive->Number_of_Changes)]
            //dive->Step_Size_Change = [0.0 for i in range(dive->Number_of_Changes)]
			
            for(j = 0; j < current_profile->number_of_ascent_parameter_changes; j++) {
                ascent_summary *current_ascent = &(current_profile->ascents[j]);
                dive->Depth_Change[j] = current_ascent->starting_depth;
                dive->Mix_Change[j] = current_ascent->gasmix;
                dive->Rate_Change[j] = current_ascent->rate;
                dive->Step_Size_Change[j] = current_ascent->step_size;
				YIELD();
				
            }
        }
    }
	
    dive->Starting_Depth = dive->Depth_Change[0];
    dive->Mix_Number = dive->Mix_Change[0];
    dive->Rate = dive->Rate_Change[0];
    dive->Step_Size = dive->Step_Size_Change[0];
	
    vpmb_calc_start_of_deco_zone(dive, dive->Starting_Depth, dive->Rate);
	
    if(dive->units_fsw == TRUE) {
        if (dive->Step_Size < 10.0) {
            double rounding_op = (dive->Depth_Start_of_Deco_Zone / dive->Step_Size) - 0.5;
            dive->Deepest_Possible_Stop_Depth = round(rounding_op) * dive->Step_Size;
        } else {
            double rounding_op = (dive->Depth_Start_of_Deco_Zone / 10.0) - 0.5;
            dive->Deepest_Possible_Stop_Depth = round(rounding_op) * 10.0;
        }
    } else {
        if(dive->Step_Size < 3.0) {
            double rounding_op = (dive->Depth_Start_of_Deco_Zone / dive->Step_Size) - 0.5;
            dive->Deepest_Possible_Stop_Depth = round(rounding_op) * dive->Step_Size;
        } else {
            double rounding_op = (dive->Depth_Start_of_Deco_Zone / 3.0)  - 0.5;
            dive->Deepest_Possible_Stop_Depth = round(rounding_op) * 3.0;
        }
    }
	
    vpmb_gas_loadings_ascent_descent(dive, dive->Starting_Depth, dive->Depth_Start_of_Deco_Zone, dive->Rate);
    dive->Run_Time_Start_of_Deco_Zone = dive->Run_Time;
    dive->Deco_Phase_Volume_Time = 0.0;
    dive->Last_Run_Time = 0.0;
    dive->Schedule_Converged = FALSE;
	
    for(i = 0; i < 16; i++) {
        dive->Last_Phase_Volume_Time[i] = 0.0;
        dive->He_Pressure_Start_of_Deco_Zone[i] = dive->Helium_Pressure[i];
        dive->N2_Pressure_Start_of_Deco_Zone[i] = dive->Nitrogen_Pressure[i];
        dive->Max_Actual_Gradient[i] = 0.0;
    }
	
    vpmb_critical_volume_loop(dive, false);
}


void vpmb_repetitive_dive_loop(dive_state *dive, vpmb_input *input)
{
    int i, j;
    single_dive *current_dive;
    dive->Real_Time_Decompression = FALSE;
    for(i = 0; i < input->number_of_dives; i++) {
        int Repetitive_Dive_Flag;
		
        current_dive = &(input->dives[i]);
        //self.output_object.new_dive(dive["desc"])
		
        vpmb_set_gas_mixes(dive, current_dive);
        vpmb_profile_code_loop(dive, current_dive);
        vpmb_decompression_loop(dive, current_dive);
		
        Repetitive_Dive_Flag = current_dive->repetitive_code;
		
        if(Repetitive_Dive_Flag == 0) {
            continue;
        }
		
        else if (Repetitive_Dive_Flag == 1) {
            dive->Surface_Interval_Time = current_dive->surface_interval_time_minutes;
			
            vpmb_gas_loadings_surface_interval(dive, dive->Surface_Interval_Time);
            vpmb_vpm_repetitive_algorithm(dive, dive->Surface_Interval_Time);
			
            for(j = 0; j < 16; j++) {
                dive->Max_Crushing_Pressure_He[j] = 0.0;
                dive->Max_Crushing_Pressure_N2[j] = 0.0;
                dive->Max_Actual_Gradient[j] = 0.0;
            }
            dive->Run_Time = 0.0;
            dive->Segment_Number = 0;
			
            /* may not be needed anymore */
            continue;
        }
    }
}
