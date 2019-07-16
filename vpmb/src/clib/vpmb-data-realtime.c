/*
 *  vpmb-data-realtime.c
 *  
 *
 *  Created by Dirk Niggemann on 17/03/2018.
 *  Copyright 2018 __MyCompanyName__. All rights reserved.
 *
 */

#include "vpmb-data-realtime.h"


#define TRC_PRINT(...)
#define DBG_PRINT(...)

//#define LOOPLOG

int vpmb_finished_constant_depth_profile(dive_state *dive, dive_profile *current_profile)
{
	
    if(dive->Run_Time >= current_profile->run_time_at_end_of_segment) {
        return DONE_PROFILE;
    }
    return NOT_DONE_PROFILE;
}
//unused??
double vpmb_find_start_of_decompression_zone(dive_state *dive, dive_profile *current_profile)
{
    int i;//, j;
	
    ascent_summary *current_ascent;
    dive_state *dive_cpy = malloc(sizeof(dive_state));
    if (dive_cpy == NULL) {
    	lprintf("M,%s,no memory\n", __func__);

    	ERR_PRINT("Aaargh- no memory!");	
    }
    memcpy (dive_cpy, dive, sizeof(dive_state));
	 dive_cpy->decomp_stops =NULL;
	  dive_cpy->ndecomp_stops = 0;
    vpmb_nuclear_regeneration(dive_cpy, dive_cpy->Run_Time);
    vpmb_calc_initial_allowable_gradient(dive_cpy);
	
    for(i = 0; i < 16; i++) {
        dive_cpy->He_Pressure_Start_of_Ascent[i] = dive_cpy->Helium_Pressure[i];
        dive_cpy->N2_Pressure_Start_of_Ascent[i] = dive_cpy->Nitrogen_Pressure[i];
    }
	
    dive_cpy->Run_Time_Start_of_Ascent = dive_cpy->Run_Time;
    dive_cpy->Segment_Number_Start_of_Ascent = dive_cpy->Segment_Number;
	
    /* for(i=0; i < current_dive->num_profile_codes; i++) { */
    /*         dive_cpy->Number_of_Changes = current_profile->number_of_ascent_parameter_changes; */
	
    /*         for(j=0; j < current_profile->number_of_ascent_parameter_changes; j++){ */
    /*                 ascent_summary *current_ascent = &(current_profile->ascents[j]); */
    /*                 dive_cpy->Depth_Change[j] = current_ascent->starting_depth; */
    /*                 dive_cpy->Mix_Change[j] = current_ascent->gasmix; */
    /*                 dive_cpy->Rate_Change[j] = current_ascent->rate; */
    /*                 dive_cpy->Step_Size_Change[j] = current_ascent->step_size; */
    /*         } */
    /* } */
	
    //dive_profile *current_profile = &(current_dive->dive_profiles[2]);
    current_ascent = &(current_profile->ascents[0]);
	
    dive_cpy->Starting_Depth = dive_cpy->Depth; //dive_cpy->Depth_Change[0];
    dive_cpy->Mix_Number = current_ascent->gasmix; //dive_cpy->Mix_Change[0];
    dive_cpy->Rate = current_ascent->rate;//dive_cpy->Rate_Change[0];
    dive_cpy->Step_Size = current_ascent->step_size;//dive_cpy->Step_Size_Change[0];
	
    vpmb_calc_start_of_deco_zone(dive_cpy, dive_cpy->Depth, dive_cpy->Rate);
	
    if(dive_cpy->units_fsw == false) {
        if (dive_cpy->Step_Size < 10.0) {
            double rounding_op = (dive_cpy->Depth_Start_of_Deco_Zone / dive_cpy->Step_Size) - 0.5;
            dive_cpy->Deepest_Possible_Stop_Depth = round(rounding_op) * dive_cpy->Step_Size;
        } else {
            double rounding_op = (dive_cpy->Depth_Start_of_Deco_Zone / 10.0) - 0.5;
            dive_cpy->Deepest_Possible_Stop_Depth = round(rounding_op) * 10.0;
        }
    } else {
        if(dive_cpy->Step_Size < 3.0) {
            double rounding_op = (dive_cpy->Depth_Start_of_Deco_Zone / dive_cpy->Step_Size) - 0.5;
            dive_cpy->Deepest_Possible_Stop_Depth = round(rounding_op) * dive_cpy->Step_Size;
        } else {
            double rounding_op = (dive_cpy->Depth_Start_of_Deco_Zone / 3.0)  - 0.5;
            dive_cpy->Deepest_Possible_Stop_Depth = round(rounding_op) * 3.0;
        }
    }
	
    double deepest_depth = dive_cpy->Deepest_Possible_Stop_Depth;
        if(dive_cpy->decomp_stops) free(dive_cpy->decomp_stops);
		free(dive_cpy);
    if(deepest_depth < 0) {
        deepest_depth = 0;
    }
    return deepest_depth;
}

double vpmb_nonprofile_find_start_of_decompression_zone(dive_state *dive)
{
    int i;//, j;
	
    ascent_summary *current_ascent;
    dive_state *dive_cpy = malloc(sizeof(dive_state));
    if (dive_cpy == NULL) {
    	lprintf("M,%s,no memory\n", __func__);

    	ERR_PRINT("Aaargh- no memory!");	
    }
    memcpy (dive_cpy, dive, sizeof(dive_state));
		dive_cpy->decomp_stops =NULL;
	  dive_cpy->ndecomp_stops = 0;
    vpmb_nuclear_regeneration(dive_cpy, dive_cpy->Run_Time);
    vpmb_calc_initial_allowable_gradient(dive_cpy);
	
    for(i = 0; i < 16; i++) {
        dive_cpy->He_Pressure_Start_of_Ascent[i] = dive_cpy->Helium_Pressure[i];
        dive_cpy->N2_Pressure_Start_of_Ascent[i] = dive_cpy->Nitrogen_Pressure[i];
    }
	
    dive_cpy->Run_Time_Start_of_Ascent = dive_cpy->Run_Time;
    dive_cpy->Segment_Number_Start_of_Ascent = dive_cpy->Segment_Number;
	
    /* for(i=0; i < current_dive->num_profile_codes; i++) { */
    /*         dive_cpy->Number_of_Changes = current_profile->number_of_ascent_parameter_changes; */
	
    /*         for(j=0; j < current_profile->number_of_ascent_parameter_changes; j++){ */
    /*                 ascent_summary *current_ascent = &(current_profile->ascents[j]); */
    /*                 dive_cpy->Depth_Change[j] = current_ascent->starting_depth; */
    /*                 dive_cpy->Mix_Change[j] = current_ascent->gasmix; */
    /*                 dive_cpy->Rate_Change[j] = current_ascent->rate; */
    /*                 dive_cpy->Step_Size_Change[j] = current_ascent->step_size; */
    /*         } */
    /* } */
	
    //dive_profile *current_profile = &(current_dive->dive_profiles[2]);
    //current_ascent = &(current_profile->ascents[0]);
	
    //dive_cpy->Starting_Depth = dive_cpy->Depth; //dive_cpy->Depth_Change[0];
    //dive_cpy->Mix_Number = current_ascent->gasmix; //dive_cpy->Mix_Change[0];
    //dive_cpy->Rate = current_ascent->rate;//dive_cpy->Rate_Change[0];
    //dive_cpy->Step_Size = current_ascent->step_size;//dive_cpy->Step_Size_Change[0];
		dive_cpy->Starting_Depth = dive_cpy->Depth; //dive_cpy->Depth_Change[0];
    //dive_cpy->Mix_Number = current_ascent->gasmix; //dive_cpy->Mix_Change[0];
    //dive_cpy->Rate = current_ascent->rate;//dive_cpy->Rate_Change[0];
    if (dive_cpy->Rate >= 0.0 /* && !init*/ ) {
			dive_cpy->Rate = dive->Default_Ascent_Rate;

		} else {
			dive_cpy->Rate = dive->Rate;
		}
    //dive_cpy->Step_Size = dive->Default_Step_Size
	
	if (dive->Rate >= 0.0 /* && !init*/ ) {
		dive->Rate_Change[0] = dive->Default_Ascent_Rate;

	} else {
		dive->Rate_Change[0] = dive->Rate;
	}
	dive->Step_Size_Change[0] = dive->Step_Size;
	
    vpmb_calc_start_of_deco_zone(dive_cpy, dive_cpy->Depth, dive_cpy->Rate);
	
    if(dive_cpy->units_fsw == false) {
        if (dive_cpy->Step_Size < 10.0) {
            double rounding_op = (dive_cpy->Depth_Start_of_Deco_Zone / dive_cpy->Step_Size) - 0.5;
            dive_cpy->Deepest_Possible_Stop_Depth = round(rounding_op) * dive_cpy->Step_Size;
        } else {
            double rounding_op = (dive_cpy->Depth_Start_of_Deco_Zone / 10.0) - 0.5;
            dive_cpy->Deepest_Possible_Stop_Depth = round(rounding_op) * 10.0;
        }
    } else {
        if(dive_cpy->Step_Size < 3.0) {
            double rounding_op = (dive_cpy->Depth_Start_of_Deco_Zone / dive_cpy->Step_Size) - 0.5;
            dive_cpy->Deepest_Possible_Stop_Depth = round(rounding_op) * dive_cpy->Step_Size;
        } else {
            double rounding_op = (dive_cpy->Depth_Start_of_Deco_Zone / 3.0)  - 0.5;
            dive_cpy->Deepest_Possible_Stop_Depth = round(rounding_op) * 3.0;
        }
    }
	
    double deepest_depth = dive_cpy->Deepest_Possible_Stop_Depth;
    if(dive_cpy->decomp_stops) free(dive_cpy->decomp_stops);
    free(dive_cpy);
    if(deepest_depth < 0) {
        deepest_depth = 0;
    }
    return deepest_depth;
}

bool vpmb_calculate_decompression_stops(dive_state *dive, dive_profile *current_profile)
{
    int i, j;
    dive_state *dive_cpy = malloc(sizeof(dive_state));
    if (dive_cpy == NULL) {
      lprintf("M,%s,no memory\n", __func__);

    	ERR_PRINT("Aaargh- no memory!");	
    }
    memcpy(dive_cpy, dive, sizeof(dive_state));
		dive_cpy->decomp_stops =NULL;
	  dive_cpy->ndecomp_stops = 0;
	vpmb_decompress(dive_cpy, current_profile, false);
	
	if (dive->decomp_stops) free (dive->decomp_stops);
	dive->decomp_stops = (decompression_stops*) calloc(dive_cpy->decomp_stop_index, sizeof(decompression_stops));
	if (!dive->decomp_stops) {
		lprintf("M,%s,no memory\n", __func__);
		if(dive_cpy->decomp_stops) free(dive_cpy->decomp_stops);

    free(dive_cpy);
		return;
  }
    //int i;
    for(i = 0; i < dive_cpy->decomp_stop_index; i++) {
        dive->decomp_stops[i] = dive_cpy->decomp_stops[i];
    }
	dive->ndecomp_stops = dive_cpy->ndecomp_stops;
    dive->decomp_stop_index = 0;
    if(dive_cpy->decomp_stops) free(dive_cpy->decomp_stops);
    free(dive_cpy);
}


void vpmb_decompress(dive_state *dive, dive_profile *current_profile, bool init)
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
	
    dive->Number_of_Changes = current_profile->number_of_ascent_parameter_changes;
	
    for(j = 0; j < current_profile->number_of_ascent_parameter_changes; j++) {
        ascent_summary *current_ascent = &(current_profile->ascents[j]);
        dive->Depth_Change[j] = current_ascent->starting_depth;
        dive->Mix_Change[j] = current_ascent->gasmix;
        dive->Rate_Change[j] = current_ascent->rate;
        dive->Step_Size_Change[j] = current_ascent->step_size;
    }
	
    dive->Starting_Depth = dive->Depth_Change[0];
    dive->Mix_Number = dive->Mix_Change[0];
    dive->Rate = dive->Rate_Change[0];
    dive->Step_Size = dive->Step_Size_Change[0];
	
    vpmb_calc_start_of_deco_zone(dive, dive->Starting_Depth, dive->Rate);
	
    if(dive->units_fsw == false) {
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
    vpmb_critical_volume_loop(dive, init);
}


//DIRK

int vpmb_set_active_gas_mixes(dive_state *dive, vpmb_input * in)
{
    int i;
    int num_gas_mixes = in->number_of_gases;
	int num_setpoints = in->number_of_setpoints;

    double Fraction_Oxygen;
    gasmix_summary *current_gasmix;
	
	//TRC_PRINT("florp ");
    if(dive->Fraction_Helium != NULL) {
		//TRC_PRINT("eeep ");

        free(dive->Fraction_Helium);
		dive->Fraction_Helium = NULL;
    }
    if(dive->Fraction_Nitrogen != NULL) {
		//TRC_PRINT("eeep2 ");

        free(dive->Fraction_Nitrogen);
		dive->Fraction_Nitrogen = NULL;

    }
	if(dive->SetPoints != NULL) {
		//TRC_PRINT("eeep3 ");

        free(dive->SetPoints);
		dive->SetPoints = NULL;
    }
	//TRC_PRINT("postflorp ");

    dive->Fraction_Helium = calloc(num_gas_mixes, sizeof(double));
    dive->Fraction_Nitrogen = calloc(num_gas_mixes, sizeof(double));
		if (!dive->Fraction_Nitrogen || !dive->Fraction_Helium){
			lprintf("M,%s,no memory\n", __func__);
      return INVALIDDATA;

		}
    for(i = 0; i < num_gas_mixes; i++) {
		//TRC_PRINT("flarp %d %d ", i, num_gas_mixes);

        current_gasmix = &(in->gaslist[i].gasmix);
		//TRC_PRINT("florpl ");
		//TRC_PRINT("Gas: %s\n", in->gaslist[i].name);
		//TRC_PRINT("  O2 ->%f\n", in->gaslist[i].gasmix.fraction_O2);
		
		
        Fraction_Oxygen = current_gasmix->fraction_O2;
        dive->Fraction_Nitrogen[i] = current_gasmix->fraction_N2;
        dive->Fraction_Helium[i] = current_gasmix->fraction_He;
		//Fraction_Oxygen = in->gaslist[i].gasmix.fraction_O2;
        //dive->Fraction_Nitrogen[i] = in->gaslist[i].gasmix.fraction_N2;
        //dive->Fraction_Helium[i] = in->gaslist[i].gasmix.fraction_He;
		
		
        /* make sure the fractions add up */
        if((Fraction_Oxygen + dive->Fraction_Nitrogen[i] + dive->Fraction_Helium[i]) != 1.0) {
			ERR_PRINT("baddata/\n");

            return INVALIDDATA;
        }
		//TRC_PRINT("florpl2 ");

        //for i in range(num_gas_mixes):
        //  dive->output_object.add_gasmix(Fraction_Oxygen[i], self.Fraction_Nitrogen[i], self.Fraction_Helium[i])
    }
	//TRC_PRINT("nsetpoints %d\n", num_setpoints);
	dive->SetPoints = calloc(num_setpoints, sizeof(double));
	if (!dive->SetPoints ){
			lprintf("M,%s,no memory\n", __func__);
      return INVALIDDATA;

	}
	for(i = 0; i < num_setpoints; i++) {
		dive->SetPoints[i] = in->setpoints[i];
	}
	//TRC_PRINT("flipple ");

    return VALIDDATA;
}




int update_depth(dive_state *dive, double increment_time, double rate, dive_profile *current_profile)
{
    /* Currently this is just part of a driver to test the real time dive loop
     *  this would be changed to just read the current depth from a sensor and then update the rate
     * in a real dive computer
     */
    dive->Last_Depth = dive->Depth;
	
    /* ascent or descent */
    if(current_profile->profile_code == 1) {
        /* descending */
        if(rate > 0 && dive->Depth < current_profile->ending_depth ||
		   rate < 0 && dive->Depth > current_profile->ending_depth) {
            dive->Depth = dive->Depth + (rate * increment_time);
        } else {
            return DONE_PROFILE;
        }
		
    } else if(current_profile->profile_code == 99) {
        if(dive->decomp_stops != NULL) {
            if(dive->decomp_stops[dive->decomp_stop_index].ascent_or_const == ASCENT) {
                dive->Depth = dive->Depth + (rate * increment_time);
				
                /* adjust if we go over */
                if(dive->Depth < dive->decomp_stops[dive->decomp_stop_index].depth) {
                    dive->Depth = dive->decomp_stops[dive->decomp_stop_index].depth;
                }
				
                /* reached the depth */
                if(dive->Depth == dive->decomp_stops[dive->decomp_stop_index].depth) {
                    dive->decomp_stop_index++;
                }
				
            } else {
                if(dive->Run_Time >= dive->Wait_Time) {
                    dive->decomp_stop_index++;
                } else {
                    dive->Run_Time += increment_time;
                }
            }
        }
    }
    return DEPTH_CHANGED;
}


//Not used?
void calc_current_deco_zone(dive_state * dive, single_dive *current_dive)
{
    dive_profile *current_profile = NULL;
    int j;
    double decomp_zone;
    for(j = 0; j < current_dive->num_profile_codes; j++) {
        current_profile = &(current_dive->dive_profiles[j]);
        if (current_profile->profile_code == 99) {
            break;
        }
    }
    decomp_zone = vpmb_find_start_of_decompression_zone(dive, current_profile);
    DBG_PRINT("Deepest possible decompression stop is %f\n", decomp_zone);
    dive->Start_of_Decompression_Zone = decomp_zone;
}

void vpmb_real_time_dive(dive_state *dive, vpmb_input *input)
{
    int i;
    single_dive *current_dive;
    for(i = 0; i < input->number_of_dives; i++){
		YIELD();
		TRC_PRINT("dive %d\n", i);
		vpmb_real_time_single_dive(dive, input, &(input->dives[i]));
		TRC_PRINT("dive done %d\n", i);

	}
}

							 
void vpmb_real_time_single_dive(dive_state *dive, vpmb_input *input, single_dive *current_dive)
{
	int j;
	dive->Real_Time_Decompression = true;

	dive_profile *current_profile;
	bool dive_finished = false;
	bool calc_decompression_stop = false;
	direction cur_dir;
	const double increment_time = 0.1; /* minutes */

	//current_dive = &(input->dives[i]);
	TRC_PRINT("pregas ");
	vpmb_set_gas_mixes(dive, current_dive);

	//vpmb_set_active_gas_mixes(dive, input);
	TRC_PRINT("postgas ");

	TRC_PRINT("profiles %d\n", current_dive->num_profile_codes);

	current_profile = &(current_dive->dive_profiles[0]);
	TRC_PRINT("profile2 ");

	dive->Starting_Depth = current_profile->starting_depth;
	dive->Ending_Depth = current_profile->ending_depth;
	dive->Rate = current_profile->rate;
	dive->Mix_Number = current_profile->gasmix;
	TRC_PRINT("profile3 ");

	for(j = 0; j < current_dive->num_profile_codes; j++) {
		//TRC_PRINT("prof %d %d ", j);

		YIELD();
		dive_profile *current_profile = &(current_dive->dive_profiles[j]);
		double current_rate = current_profile->rate;
		/* TRC_PRINT("current_rate: %f",current_rate); */
		TRC_PRINT("profile4 ");

		if(current_profile->profile_code == 99) {
			dive->Decompressing = true;
			current_rate = current_profile->ascents[0].rate;
		}
		while(!dive_finished) {
			YIELD();
			/* change direction or profiles */
				TRC_PRINT("profile5 ");

			if(update_depth(dive, increment_time, current_rate, current_profile) == DONE_PROFILE) {
				break;
			}
							TRC_PRINT("profile6 ");

			
			/* double rate; */
			/* dive->Rate = current_profile->rate; */
			if(dive->Decompressing == false) {
				cur_dir = vpmb_current_direction(dive, increment_time);
											TRC_PRINT("profile7 %d %s\n ", cur_dir,  direction_labels[cur_dir]);

				/* int check = msw_driver(dive); */
				
				/* if (check == 1 || check ==2) { */
				
				/* regular ascent, or ascent to start of decompression zone */
#ifdef LOOPLOG

				lprintf("{\"state\": \"%s\", \"Depth\": %f, \"Run_Time\": %f},\n",
						 direction_labels[cur_dir], dive->Depth, dive->Run_Time);
#endif
											TRC_PRINT("profile8 ");

				if(cur_dir == ASCENT) {
					/* else{ */
					vpmb_gas_loadings_ascent_descent(dive, dive->Last_Depth, dive->Depth, dive->Rate);
					YIELD();
					
					continue;
					/* } */
				} else if(cur_dir == DESCENT) {
					vpmb_gas_loadings_ascent_descent(dive, dive->Last_Depth, dive->Depth, dive->Rate);
					YIELD();
					
					vpmb_calc_crushing_pressure(dive, dive->Last_Depth, dive->Depth, dive->Rate);
					YIELD();
				} else if (cur_dir == CONSTANT) {
					/* else{ */
					if(vpmb_finished_constant_depth_profile(dive, current_profile) == DONE_PROFILE) {
						break;
					}
					YIELD();
					
					vpmb_gas_loadings_constant_depth(dive, dive->Depth, dive->Run_Time + increment_time);
					/* } */
				}
			} else {
				if(dive->decomp_stops == NULL) {
												TRC_PRINT("profile9");

					double run_time = dive->Run_Time;
					/* double depth = dive->Depth; */
					YIELD();
					
					vpmb_calculate_decompression_stops(dive, current_profile);
					YIELD();
					
					vpmb_decompress(dive, current_profile, true);
					YIELD();
					
					dive->Run_Time = run_time;
					/* dive->Depth = depth; */
				} else {					
					if(dive->Depth <= 0) {
						dive_finished = true;
#ifdef LOOPLOG

						lprintf("{\"stop: %d, \"depth\": %f, \"time\": %f, \"state\": \"%s\"},\n ", 
								 dive->decomp_stop_index, dive->Depth, 
								 dive->Run_Time, direction_labels[DONE]);			
#endif
					} else if (dive->decomp_stops[dive->decomp_stop_index].ascent_or_const == ASCENT) {
#ifdef LOOPLOG
						lprintf("{\"stop: %d, \"depth\": %f, \"time\": %f, \"state\": \"%s\"},\n ", 
								 dive->decomp_stop_index, dive->Depth, 
								 dive->Run_Time, direction_labels[dive->decomp_stops[dive->decomp_stop_index].ascent_or_const]);			
#endif
						vpmb_gas_loadings_ascent_descent(dive, dive->Last_Depth, dive->Depth, dive->Rate);
						YIELD();
						
						vpmb_calc_max_actual_gradient(dive, dive->Deco_Stop_Depth);
						YIELD();
						
						calc_decompression_stop = true;
					} else if (dive->decomp_stops[dive->decomp_stop_index].ascent_or_const == CONSTANT){
#ifdef LOOPLOG
						lprintf("{\"stop: %d, \"depth\": %f, \"time\": %f, \"state\": \"%s\"},\n ",
								 dive->decomp_stop_index, dive->Depth, 
								 dive->Run_Time, direction_labels[dive->decomp_stops[dive->decomp_stop_index].ascent_or_const]);			
#endif
						if(calc_decompression_stop == true) {
							double run_time = dive->Run_Time;
							YIELD();
							
							vpmb_critical_volume_decision_tree(dive, dive->decomp_stops[dive->decomp_stop_index].depth - 0.1, false);
							YIELD();
							
							dive->Wait_Time = dive->Run_Time;
							dive->Run_Time = run_time;
							calc_decompression_stop = false;
							YIELD();
							
						} else {
#ifdef LOOPLOG

							lprintf("{\"stop: %d, \"depth\": %f, \"time\": %f, \"state\": \"%s\"},\n ", 
									 dive->decomp_stop_index, dive->Depth, dive->Run_Time, 
									 direction_labels[dive->decomp_stops[dive->decomp_stop_index].ascent_or_const]);			
#endif
						}
						/* dive->decomp_stop_index++; */
					}
					
				}
				
			}
			
		}
	}
	TRC_PRINT("RTDone ");
}



bool vpmb_nonprofile_calculate_decompression_stops(dive_state *dive)
{
    int i;
    bool ret;
	//TRC_PRINT("morp ");
    dive_state *dive_cpy = malloc(sizeof(dive_state));
    if (dive_cpy == NULL) {
    	lprintf("M,%s,no memory\n", __func__);

    	ERR_PRINT("Aaargh- no memory, need %d have%d\n", sizeof (dive_state), getFreeHeap());	
    	return false;
    }
    memcpy(dive_cpy, dive, sizeof(dive_state));
		TRC_PRINT("morp1 ");
	  dive_cpy->decomp_stops =NULL;
	  dive_cpy->ndecomp_stops = 0;
	
		ret = vpmb_nonprofile_decompress(dive_cpy, false);
		if (ret) {
			if (dive->decomp_stops) free (dive->decomp_stops);

    	dive->decomp_stops = (decompression_stops*) calloc(dive_cpy->decomp_stop_index, sizeof(decompression_stops));
    
    	if (!dive->decomp_stops) {
				lprintf("M,%s,no memory\n", __func__);
				if(dive_cpy->decomp_stops) free(dive_cpy->decomp_stops);

   		 free(dive_cpy);
			return false;
  		}//int i;
    	for(i = 0; i < dive_cpy->decomp_stop_index; i++) {
      	  dive->decomp_stops[i] = dive_cpy->decomp_stops[i];
    	}
			dive->ndecomp_stops = dive_cpy->ndecomp_stops;
		  dive->decomp_stop_index = 0;

			TRC_PRINT("morp98 ");
		}
    if(dive_cpy->decomp_stops) free(dive_cpy->decomp_stops);
    free(dive_cpy);
		TRC_PRINT("morp99 ");
		return ret;
}

void set_default_change(dive_state * dive)
{
	
    if (dive->Number_of_Changes == 0) dive->Number_of_Changes = 1;
	//dive->Number_of_Changes = current_profile->number_of_ascent_parameter_changes;
	dive->Depth_Change[0] = dive->Depth;
	dive->Mix_Change[0] = dive->Mix_Number;
	if (dive->Rate >= 0.0 /* && !init*/ ) {
		dive->Rate_Change[0] = dive->Default_Ascent_Rate;

	} else {
		dive->Rate_Change[0] = dive->Rate;
	}
	dive->Step_Size_Change[0] = dive->Step_Size;
}

bool vpmb_nonprofile_decompress(dive_state *dive, bool init)
{
	//TRC_PRINT("blorp0 ");

    int i;
	TRC_PRINT("blorp00 ");

    vpmb_nuclear_regeneration(dive, dive->Run_Time);
	TRC_PRINT("blorp01 ");

    vpmb_calc_initial_allowable_gradient(dive);
	TRC_PRINT("blorp02 ");

    for(i = 0; i < 16; i++) {
        dive->He_Pressure_Start_of_Ascent[i] = dive->Helium_Pressure[i];
        dive->N2_Pressure_Start_of_Ascent[i] = dive->Nitrogen_Pressure[i];
    }
	
    dive->Run_Time_Start_of_Ascent = dive->Run_Time;
    dive->Segment_Number_Start_of_Ascent = dive->Segment_Number;

	//                for(i=0; i < current_dive_cpy->num_profile_codes; i++) {
    //dive_cpy_profile *current_profile = &(current_dive_cpy->dive_cpy_profiles[i]);
	
    //int Profile_Code = current_profile->profile_code;
	
    //if(Profile_Code == 99){
    //dive_cpy->Number_of_Changes = current_profile->number_of_ascent_parameter_changes;
	
    // TODO remove hard coding of 16 and malloc this
    //dive->Depth_Change = [0.0 for i in range(dive->Number_of_Changes)]
    //        dive->Mix_Change = [0.0 for i in range(dive->Number_of_Changes)]
    //dive->Rate_Change = [0.0 for i in range(dive->Number_of_Changes)]
    //dive->Step_Size_Change = [0.0 for i in range(dive->Number_of_Changes)]
	
		set_default_change(dive);
    //for(j = 0; j < current_profile->number_of_ascent_parameter_changes; j++) {
    //    ascent_summary *current_ascent = &(current_profile->ascents[j]);
    //    dive->Depth_Change[j] = current_ascent->starting_depth;
    //    dive->Mix_Change[j] = current_ascent->gasmix;
    //    dive->Rate_Change[j] = current_ascent->rate;
    //    dive->Step_Size_Change[j] = current_ascent->step_size;
    //}
	
    dive->Starting_Depth = dive->Depth_Change[0];
    dive->Mix_Number = dive->Mix_Change[0];
    dive->Rate = dive->Rate_Change[0];
    dive->Step_Size = dive->Step_Size_Change[0];
	//TRC_PRINT("blorp1 ");
	TRC_PRINT("blorp03 ");

    vpmb_calc_start_of_deco_zone(dive, dive->Starting_Depth, dive->Rate);
	TRC_PRINT("blorp04 ");

    if(dive->units_fsw == false) {
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
	TRC_PRINT("blorp05 ");

    vpmb_gas_loadings_ascent_descent(dive, dive->Starting_Depth, dive->Depth_Start_of_Deco_Zone, dive->Rate);
   	TRC_PRINT("blorp06 ");

	dive->Run_Time_Start_of_Deco_Zone = dive->Run_Time;
    dive->Deco_Phase_Volume_Time = 0.0;
    dive->Last_Run_Time = 0.0;
    dive->Schedule_Converged = false;
	
    for(i = 0; i < 16; i++) {
        dive->Last_Phase_Volume_Time[i] = 0.0;
        dive->He_Pressure_Start_of_Deco_Zone[i] = dive->Helium_Pressure[i];
        dive->N2_Pressure_Start_of_Deco_Zone[i] = dive->Nitrogen_Pressure[i];
        dive->Max_Actual_Gradient[i] = 0.0;
    }
	if (vpmb_critical_volume_loop(dive, init) != ALLGOOD) {
		DBG_PRINT ("baddecostops\n");
				return false;
  }
	TRC_PRINT("blorp97 stops %d ", dive->decomp_stop_index);
			return true;

}


int update_depth_real(dive_state *dive, double increment_time, double (*depthfunc)(void), double (*po2func)(void))
{
    /* Currently this is just part of a driver to test the real time dive loop
     *  this would be changed to just read the current depth from a sensor and then update the rate
     * in a real dive computer
     */
    dive->Last_Depth = dive->Depth;
	dive->Depth = depthfunc();
	dive->Rate = (dive->Depth - dive->Last_Depth) / increment_time;
	if (dive->CCR_Mode) dive->PpO2 = po2func();
    /* ascent or descent */
	
}
						 
int find_active_gasmix(vpmb_input *input) 
{
	gases * gas = input->gaslist;
	for (int i = 0; i < input->number_of_gases; i++) {
		if(gas[i].active) return i +1;
	}
}
				
void  add_gas_change (dive_state *dive, vpmb_input *input)
{
	 int i = dive->Number_of_Changes;
   if (i == 0) {
				set_default_change(dive);
				i = 1;
	}
	//dive->Number_of_Changes = current_profile->number_of_ascent_parameter_changes;
	dive->Depth_Change[i] = dive->Depth;
	dive->Mix_Change[i] = find_active_gasmix(input);
	if (dive->Rate >= 0.0 /* && !init*/ ) {
		dive->Rate_Change[i] = dive->Default_Ascent_Rate;

	} else {
		dive->Rate_Change[i] = dive->Rate;
	}
	dive->Step_Size_Change[i] = dive->Step_Size;
	dive->Number_of_Changes++;
}	
		
double millis2minutes(unsigned long m)
{
	return ((double)m)/60000.0;
}

void vpmb_real_time_nonprofile_dive_init(dive_state *dive, vpmb_input *input, 
						double init_depth, double starting_depth,
										 double planned_depth, double intvl)
{
    int i;
    dive->Real_Time_Decompression = true;
		
	//dive_profile *current_profile;
	 dive->dive_finished = false;
	 dive->calc_decompression_stop = false;
    dive->did_loop_init = false;

	direction cur_dir;
	//const double increment_time = 0.1; /* minutes */
	
	//current_dive = &(input->dives[i]);
	vpmb_set_active_gas_mixes(dive, input);
	
	//current_profile = &(current_dive->dive_profiles[0]);
	dive->Starting_Depth = starting_depth;
	dive->Ending_Depth = planned_depth;
	dive->Rate = (starting_depth - init_depth)/intvl;
	dive->Mix_Number = find_active_gasmix(input);
	dive->dive_finished = false;
	dive->calc_decompression_stop = false;

} 

double calculate_nd_time (dive_state *dive) {
	int nstops = 0;
	const double start_value = 0.5; //30s immediately reduces to 1m
	const double final_value = 256.0 ; //4h, 16m
	double remaining = start_value;
	double bracket;
	do {
		remaining = remaining*  2.0;
		nstops = calculate_remaining_nd_time_trial(dive, remaining);
		//DBG_PRINT("Phase 1 Depth %f Stops %d Time %f\n", dive->Depth, nstops, remaining);
		YIELD();
	} while(nstops == 0 && remaining < final_value);
			DBG_PRINT("Phase 1 end Stops %d Time %f\n",nstops, remaining);
  if (nstops == -1) return 0.0;

	if (remaining >= final_value) return remaining;
	bracket = remaining;
	do {
		if (bracket > 1.0) bracket = bracket / 2.0;
		if (nstops > 0) remaining = remaining - bracket;
		else remaining = remaining + bracket;
		nstops = calculate_remaining_nd_time_trial(dive, remaining);
    if (nstops == -1) return 0.0;
		YIELD();
				DBG_PRINT("Phase 2 Depth %f Stops %d Rem  %f Bra %f\n",dive->Depth, nstops, remaining, bracket);

	} while ((nstops > 0|| bracket > 1.0)&& remaining >0.0);
				DBG_PRINT("Phase 2 end Stops %d Time %f Rem  %f Bra %f\n",nstops, remaining,bracket);

	return remaining; 
}

int calculate_remaining_nd_time_trial (dive_state *dive,double remaining) 
{
	 int i, len,n;
	 bool ret;
	//DBG_PRINT("flurp ");
    dive_state *dive_cpy = malloc(sizeof(dive_state));
    if (dive_cpy == NULL) {
      lprintf("M,%s,no memory\n", __func__);

    	ERR_PRINT("Aaargh- no memory!");
    	return -1;	
    }
    memcpy(dive_cpy, dive, sizeof(dive_state));
		DBG_PRINT("flurp1 ");
		dive_cpy->decomp_stops = NULL;
		dive_cpy->ndecomp_stops = 0;
	  vpmb_gas_loadings_constant_depth(dive_cpy, dive_cpy->Depth, dive_cpy->Run_Time + remaining);

		ret = vpmb_nonprofile_decompress(dive_cpy, false);
		n = 0;
		if (ret) {
			len = dive_cpy->ndecomp_stops;
		//lprintf("\t\"Decompression_Stops\":[\n");
		
			for (int i = 0; i < len; i++) {
				decompression_stops * d = &(dive_cpy->decomp_stops[i]);  
				// DBG_PRINT("ND deco stop %d  depth %f time %f\n", n, d->depth, d->time);   
				if (d->depth <=0.0) break;
				if (d->ascent_or_const == CONSTANT) n++;
			}
		} else n = -1;
//    dive->decomp_stops = (decompression_stops*) calloc(dive_cpy->decomp_stop_index, sizeof(decompression_stops));
//    //int i;
//    for(i = 0; i < dive_cpy->decomp_stop_index; i++) {
//        dive->decomp_stops[i] = dive_cpy->decomp_stops[i];
//    }
//	dive->ndecomp_stops = dive_cpy->ndecomp_stops;
	//DBG_PRINT("flurp ");

    //dive->decomp_stop_index = 0;
    if(dive_cpy->decomp_stops) free(dive_cpy->decomp_stops);
    free(dive_cpy);
	//DBG_PRINT("flurp99 ");
	return n;
}

						 
void vpmb_real_time_nonprofile_dive_loop(dive_state *dive, vpmb_input *input, 
							 double increment_time,  double (*depthfunc)(void), double (*po2func)(void) )
{
	direction curr_dir;
	YIELD();
	bool did_loadings = false;
	if (dive->dive_finished) return;	
	update_depth_real(dive, increment_time, depthfunc, po2func);
	curr_dir = vpmb_current_direction(dive, increment_time);
	if(curr_dir == ASCENT||curr_dir == DESCENT) {
		vpmb_gas_loadings_ascent_descent(dive, dive->Last_Depth, dive->Depth, dive->Rate);
		did_loadings = true;
		double curr_otu = toxotu_ascdesc(dive, dive->Last_Depth, dive->Depth, dive->Rate);
		double curr_cns =  toxcns_ascdesc(dive, dive->Last_Depth, dive->Depth, dive->Rate); 
		dive->OTU_Sum += isnan(curr_otu) ? 0.0 : curr_otu;
		dive->CNS_Sum +=isnan(curr_cns) ? 0.0 : curr_cns;

		YIELD();
	} else if (curr_dir == CONSTANT) {
		/* else{ */
		//if(vpmb_finished_constant_depth_nonprofile(dive, current_profile) == DONE_PROFILE) {
		//	break;
		//}
		//YIELD();
		
		vpmb_gas_loadings_constant_depth(dive, dive->Depth, dive->Run_Time + increment_time);
		double curr_otu = toxotu_const(dive, dive->Depth, increment_time);
		double curr_cns =  toxcns_const(dive, dive->Depth, increment_time);
		dive->OTU_Sum += isnan(curr_otu) ? 0.0 : curr_otu;
		dive->CNS_Sum += isnan(curr_cns) ? 0.0 : curr_cns;
		did_loadings = true;
		/* } */
	}
#ifdef LOOPLOG

	lprintf("\"loop\": {\"state\": \"%s\", \"Depth\": %f, \"Rate\":%f, \"Run_Time\": %f},\n", 
			 direction_labels[curr_dir], dive->Depth, dive->Rate, dive->Run_Time);
#endif
  if(curr_dir == DESCENT) {
		dive->did_loop_init = false;
		vpmb_calc_crushing_pressure(dive, dive->Last_Depth, dive->Depth, dive->Rate);		
		dive->Decompressing = false;
	} 
	//if(dive->decomp_stops == NULL) {
	DBG_PRINT("trydeco ");

	double run_time = dive->Run_Time;
	/* double depth = dive->Depth; */
	YIELD();
	DBG_PRINT("fluble ");
	if (curr_dir == ASCENT) {
		dive->Decompressing = true;
  }
	if (dive->Decompressing == false) {
		vpmb_nonprofile_calculate_decompression_stops(dive);
	}
	
  if (curr_dir == ASCENT && dive->did_loop_init == false&&dive->Decompressing) {
  	if(dive->decomp_stops == NULL) vpmb_nonprofile_calculate_decompression_stops(dive);
		vpmb_nonprofile_decompress(dive, true);
		dive->did_loop_init = true;
		YIELD();
	}	
	dive->Run_Time = run_time;

	if (curr_dir == ASCENT && dive->Decompressing) {
		vpmb_calc_max_actual_gradient(dive, dive->Deco_Stop_Depth);
		dive->calc_decompression_stop = true;
	}
	if (curr_dir == CONSTANT && dive->calc_decompression_stop&& dive->Decompressing){	
				double run_time = dive->Run_Time;
				YIELD();	
				vpmb_critical_volume_decision_tree(dive, dive->Depth - 0.1, false);
				YIELD();
				
				dive->Wait_Time = dive->Run_Time;
				dive->Run_Time = run_time;
				dive->calc_decompression_stop = false;
	}
	YIELD();
	DBG_PRINT("fleeble ");

/*
  
	*/
	// dive->Depth = depth; 
	/*if(dive->decomp_stops != NULL) {
	//} else {
		if(dive->Depth <= 0) {
			dive->dive_finished = true;
		} else if (curr_dir == ASCENT) {
			YIELD();
		
			vpmb_calc_max_actual_gradient(dive, dive->Deco_Stop_Depth);
			YIELD();
			dive->calc_decompression_stop = true;
		} else if (curr_dir == CONSTANT){		
			if(dive->calc_decompression_stop) {
				double run_time = dive->Run_Time;
				YIELD();
				
				vpmb_critical_volume_decision_tree(dive, dive->Depth - 0.1, false);
				YIELD();
				
				dive->Wait_Time = dive->Run_Time;
				dive->Run_Time = run_time;
				dive->calc_decompression_stop = false;
				YIELD();
				
			}
		//dive->decomp_stop_index++; 
		
		}
	
	}
	*/
}
						 
	
	/*
		if(dive->decomp_stops != NULL) {
	//} else {
		if(dive->Depth <= 0) {
		dive->dive_finished = true;
#ifdef LOOPLOG
		lprintf("\"deco\": {\"stop: %d, \"depth\": %f, \"time\": %f, \"state\": %s},\n ", 
				 dive->decomp_stop_index, dive->Depth, 
				 dive->Run_Time, direction_labels[DONE]);			
#endif 

		} else if (dive->decomp_stops[dive->decomp_stop_index].ascent_or_const == ASCENT) {
#ifdef LOOPLOG
			lprintf("\"deco\": {\"stop: %d, \"depth\": %f, \"time\": %f, \"state\": %s},\n ", 
				 dive->decomp_stop_index, dive->Depth, dive->Run_Time, 
				 direction_labels[dive->decomp_stops[dive->decomp_stop_index].ascent_or_const]);
#endif
			if (!did_loadings) vpmb_gas_loadings_ascent_descent(dive, dive->Last_Depth, dive->Depth, dive->Rate);
			YIELD();
		
			vpmb_calc_max_actual_gradient(dive, dive->Deco_Stop_Depth);
			YIELD();
		// reached the depth 
			if(dive->Depth - dive->decomp_stops[dive->decomp_stop_index].depth < 0.1) {		
				dive->decomp_stop_index++;
#ifdef LOOPLOG
				lprintf("next stop: %d\n", dive->decomp_stop_index);
#endif
			}
			dive->calc_decompression_stop = true;
		} else if(dive->Run_Time >= dive->Wait_Time) {
			dive->decomp_stop_index++;
#ifdef LOOPLOG

			lprintf("new_stop: %d\n", dive->decomp_stop_index);
#endif
		} else if (dive->decomp_stops[dive->decomp_stop_index].ascent_or_const == CONSTANT){
#ifdef LOOPLOG

			lprintf("{\"stop: %d, \"depth\": %f, \"time\": %f, \"state\": %s},\n ", dive->decomp_stop_index, dive->Depth, 
				 dive->Run_Time, direction_labels[dive->decomp_stops[dive->decomp_stop_index].ascent_or_const]);	
#endif		
			if(dive->calc_decompression_stop) {
				double run_time = dive->Run_Time;
				YIELD();
				
				vpmb_critical_volume_decision_tree(dive, dive->decomp_stops[dive->decomp_stop_index].depth - 0.1, false);
				YIELD();
				
				dive->Wait_Time = dive->Run_Time;
				dive->Run_Time = run_time;
				dive->calc_decompression_stop = false;
				YIELD();
				
			} else {
#ifdef LOOPLOG
				lprintf("{\"stop: %d, \"depth\": %f, \"time\": %f, \"state\": %s},\n ", dive->decomp_stop_index, dive->Depth, 
						 dive->Run_Time, direction_labels[dive->decomp_stops[dive->decomp_stop_index].ascent_or_const]);			
#endif
			}
		// dive->decomp_stop_index++;
		}
	
	}
	
	
	*/					
		//dive_profile *current_profile = &(current_dive->dive_profiles[j]);
		//double current_rate = current_profile->rate;
		/* DBG_PRINT("current_rate: %f",current_rate); */
		//if(current_profile->profile_code == 99) {
		//dive->Decompressing = false;
		
			/* dive->Rate = current_profile->rate; */
			//if(dive->Decompressing == false) {
				/* int check = msw_driver(dive); */
				
				/* if (check == 1 || check ==2) { */
				
				/* regular ascent, or ascent to start of decompression zone */
			//} else {
			//	}

							 
