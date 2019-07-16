/*
 *  vpmb-data.c
 *  
 *
 *  Created by Dirk Niggemann on 03/02/2018.
 *  Copyright 2018 __MyCompanyName__. All rights reserved.
 *
 */
#include <Arduino.h>
#include "vpmb.h"
#include "vpmb-data.h"

#define TRC_PRINT(...) 

const int INVALIDDATA = -2;
const int VALIDDATA = 2;


void vpmb_free_dives(vpmb_input *in)
{
	int i, j;
	for(i = 0; i < in->number_of_dives; i++) {
		for (j = 0; j < in->dives[i].num_profile_codes; j++) {
			if(in->dives[i].dive_profiles[j].profile_code == 99) {
				free(in->dives[i].dive_profiles[j].ascents);
			}
		}
		free(in->dives[i].gasmixes);
		free(in->dives[i].dive_profiles);
	}
	free(in->dives);
}



int vpmb_validate_data(vpmb_input *input, dive_state *dive)
{
		TRC_PRINT("val1\n");

    lowercase_string(input->Units);
    if (strcmp(input->Units, "fsw") == 0) {
        dive->units_fsw = TRUE;
    } else if (strcmp(input->Units, "msw") == 0) {
        dive->units_fsw = FALSE;
    } else {
        return INVALIDDATA;
    }
			TRC_PRINT("val2\n");

    if (input->Regeneration_Time_Constant <= 0) {
        return INVALIDDATA;
    }
				TRC_PRINT("val3\n");

    if ((dive->units_fsw == TRUE) && (input->Altitude_of_Dive > 30000.0)) {
        return INVALIDDATA;
    }
					TRC_PRINT("val4\n");

    if ((dive->units_fsw == FALSE) && (input->Altitude_of_Dive > 9144.0)) {
        return INVALIDDATA;
    }
						TRC_PRINT("val5\n");

    //lowercase_string();
		dive->Diver_Acclimatized = input->Diver_Acclimatized_at_Altitude;
    //if (strcmp(input->Diver_Acclimatized_at_Altitude, "yes") == 0) {
     //   d = TRUE;
    //} else if (strcmp(input->Diver_Acclimatized_at_Altitude, "no") == 0) {
      //  dive->Diver_Acclimatized = FALSE;
    //} else {
    //    return INVALIDDATA;
    //}
	
    dive->Critical_Radius_N2_Microns = input->Critical_Radius_N2_Microns;
    dive->Critical_Radius_He_Microns = input->Critical_Radius_He_Microns;
	
    /* nitrogen */
    if ((input->Critical_Radius_N2_Microns < 0.2) || (input->Critical_Radius_N2_Microns) > 1.35) {
        return INVALIDDATA;
    }
						TRC_PRINT("val6\n");

    /* helium */
    if ((input->Critical_Radius_He_Microns < 0.2) || (input->Critical_Radius_He_Microns > 1.35)) {
        return INVALIDDATA;
    }
							TRC_PRINT("val7\n");

	//Step size
	if (input->default_step_size < 0.01) {
		return INVALIDDATA;
    }
	if (input->default_ascent_rate > -0.1 || input->default_ascent_rate < -25.0) {
		return INVALIDDATA;
    }
							TRC_PRINT("val8\n");
  if(input->number_of_setpoints <= 0 && input->CCR_Mode) {
  			return INVALIDDATA;
  }
	return VALIDDATA;
}



int vpmb_initialize_data(vpmb_input *input, dive_state *dive)
{
    int i;
	
    dive->Depth = 0;
    dive->Decompressing = FALSE;
    dive->decomp_stops = NULL;
	dive->decomp_stop_index = 0;

    dive->Start_of_Decompression_Zone = 0;
    dive->Wait_Time = 0;
    /* This is to make sure the values are initialized before they get used for anything */
    dive->Fraction_Helium = NULL;
    dive->Fraction_Nitrogen = NULL;
	dive->SetPoints = NULL;
    //strlcpy(dive->Units, "" , sizeof(dive->Units));
    //strlcpy(dive->Units_Word1, "" , sizeof(dive->Units_Word1));
    //strlcpy(dive->Units_Word2, "" , sizeof(dive->Units_Word2));
	
    dive->Surface_Tension_Gamma = input->Surface_Tension_Gamma;
    dive->Skin_Compression_GammaC = input->Skin_Compression_GammaC;
    dive->Crit_Volume_Parameter_Lambda = input->Crit_Volume_Parameter_Lambda;
    dive->Gradient_Onset_of_Imperm_Atm = input->Gradient_Onset_of_Imperm_Atm;
    dive->Minimum_Deco_Stop_Time = input->Minimum_Deco_Stop_Time;
    dive->Critical_Radius_N2_Microns = input->Critical_Radius_N2_Microns;
    dive->Critical_Radius_He_Microns = input->Critical_Radius_He_Microns;
    dive->Regeneration_Time_Constant = input->Regeneration_Time_Constant;
    dive->Minimum_Deco_Stop_Time = input->Minimum_Deco_Stop_Time;
	dive->CCR_Mode = input->CCR_Mode;
    /* INITIALIZE CONSTANTS/VARIABLES BASED ON SELECTION OF UNITS - FSW OR MSW */
    /* fsw = feet of seawater, a unit of pressure */
    /* msw = meters of seawater, a unit of pressure */
	
    if(dive->units_fsw == TRUE) {
        //strlcpy(dive->Units_Word1, "fswg", strlen(dive->Units_Word1));
        //strlcpy(dive->Units_Word2, "fsw/min", strlen(dive->Units_Word2));
        dive->Units_Factor = 33.0;
        dive->Water_Vapor_Pressure = 1.607; /* based on respiratory quotient of 0.8  (Schreiner value) */
    } else {
        //size_t t = strlen(dive->Units_Word1);
        //strlcpy(dive->Units_Word1, "mswg", t);
        //strlcpy(dive->Units_Word2, "msw/min", strlen(dive->Units_Word2));
        dive->Units_Factor = 10.1325;
        dive->Water_Vapor_Pressure = 0.493; /* based on respiratory quotient of 0.8  (Schreiner value) */
    }
	
    /* INITIALIZE CONSTANTS/VARIABLES */
    dive->Constant_Pressure_Other_Gases = (input->Pressure_Other_Gases_mmHg / 760.0) * dive->Units_Factor;
    dive->Run_Time = 0.0;
    dive->Segment_Number = 0;
    dive->Last_Direction_Depth = dive->Depth;
    dive->Last_Direction_Time = dive->Run_Time;
    if (dive->CCR_Mode) {
    	dive->Number_Of_SetPoints = input->number_of_setpoints;
    	dive->SetPoints = calloc(dive->Number_Of_SetPoints, sizeof(double));
    	for (int i = 0; i <  dive->Number_Of_SetPoints; i++) {
    		dive->SetPoints[i] = input->setpoints[i];
    		
    	}
    	dive->SetPoint_Number = 0;
			dive->PpO2 = 0.0; //use actual ppo2 not setpoint value
		} else {
			dive->PpO2 = 0.0;
			dive->SetPoint_Number = 0;
		}
	
	dive->Step_Size = input->default_step_size;
	dive->Default_Ascent_Rate = input->default_ascent_rate;
  dive->OTU_Sum = 0.0;
  dive->CNS_Sum = 0.0;
    for (i = 0; i < Buhlmann_Compartments; i++) {
        dive->Helium_Time_Constant[i] = log(2.0) / Helium_Half_Time[i];
        dive->Nitrogen_Time_Constant[i] = log(2.0) / Nitrogen_Half_Time[i];
        dive->Max_Crushing_Pressure_He[i] = 0.0;
        dive->Max_Crushing_Pressure_N2[i] = 0.0;
        dive->Max_Actual_Gradient[i] = 0.0;
        dive->Surface_Phase_Volume_Time[i] = 0.0;
        dive->Amb_Pressure_Onset_of_Imperm[i] = 0.0;
        dive->Gas_Tension_Onset_of_Imperm[i] = 0.0;
        dive->Initial_Critical_Radius_N2[i] = input->Critical_Radius_N2_Microns * 1.0E-6;
        dive->Initial_Critical_Radius_He[i] = input->Critical_Radius_He_Microns * 1.0E-6;
    }
    //lowercase_string(input->Critical_Volume_Algorithm);
		dive->Critical_Volume_Algorithm_Off = !input->Critical_Volume_Algorithm;

	  
    //lowercase_string(input->Altitude_Dive_Algorithm);
	dive->Altitude_Dive_Algorithm_Off = !input->Altitude_Dive_Algorithm;
	
    /*INITIALIZE VARIABLES FOR SEA LEVEL OR ALTITUDE DIVE
	 See subroutines for explanation of altitude calculations.  Purposes are
	 1) to determine barometric pressure and 2) set or adjust the VPM critical
	 radius variables and gas loadings, as applicable, based on altitude,
	 ascent to altitude before the dive, and time at altitude before the dive*/
	
    if (dive->Altitude_Dive_Algorithm_Off == TRUE) {
        dive->Altitude_of_Dive = 0.0;
        dive->Barometric_Pressure = vpmb_calc_barometric_pressure(dive->Altitude_of_Dive, dive->units_fsw);
		
        for (i = 0 ; i < Buhlmann_Compartments; i++) {
            dive->Adjusted_Critical_Radius_N2[i] = dive->Initial_Critical_Radius_N2[i];
            dive->Adjusted_Critical_Radius_He[i] = dive->Initial_Critical_Radius_He[i];
            dive->Helium_Pressure[i] = 0.0;
            dive->Nitrogen_Pressure[i] = (dive->Barometric_Pressure - dive->Water_Vapor_Pressure) * fraction_inert_gas;
        }
    } else {
        vpmb_vpm_altitude_dive_algorithm(input, dive);
    }
	
    return VALIDDATA;
}

int vpmb_vpm_altitude_dive_algorithm(vpmb_input *input, dive_state *dive)
{
    int i;
    double Ascent_to_Altitude_Time = input->Ascent_to_Altitude_Hours * 60.0;
    double Time_at_Altitude_Before_Dive = input->Hours_at_Altitude_Before_Dive * 60.0;
	
    if (dive->Diver_Acclimatized == TRUE) {
        dive->Barometric_Pressure = vpmb_calc_barometric_pressure(dive->Altitude_of_Dive, dive->units_fsw);
		
        for(i = 0; i < 16; i++) {
            dive->Adjusted_Critical_Radius_N2[i] = dive->Initial_Critical_Radius_N2[i];
            dive->Adjusted_Critical_Radius_He[i] = dive->Initial_Critical_Radius_He[i];
            dive->Helium_Pressure[i] = 0.0;
            dive->Nitrogen_Pressure[i] = (dive->Barometric_Pressure - dive->Water_Vapor_Pressure) * fraction_inert_gas;
        }
    } else {
        double Starting_Ambient_Pressure;
        double Ending_Ambient_Pressure;
        double Initial_Inspired_N2_Pressure;
        double Rate;
        double Nitrogen_Rate;
        double Inspired_Nitrogen_Pressure;
		
        if ((input->Starting_Acclimatized_Altitude >= dive->Altitude_of_Dive) || (input->Starting_Acclimatized_Altitude < 0.0)) {
            return BADALTITUDE;
        }
		
        dive->Barometric_Pressure = vpmb_calc_barometric_pressure(input->Starting_Acclimatized_Altitude, dive->units_fsw);
		
        Starting_Ambient_Pressure = dive->Barometric_Pressure;
		
        for(i = 0; i < 16; i++) {
            dive->Helium_Pressure[i] = 0.0;
            dive->Nitrogen_Pressure[i] = (dive->Barometric_Pressure - dive->Water_Vapor_Pressure) * fraction_inert_gas;
        }
		
        dive->Barometric_Pressure = vpmb_calc_barometric_pressure(dive->Altitude_of_Dive, dive->units_fsw);
		
        Ending_Ambient_Pressure = dive->Barometric_Pressure;
        Initial_Inspired_N2_Pressure = (Starting_Ambient_Pressure - dive->Water_Vapor_Pressure) * fraction_inert_gas;
        Rate = (Ending_Ambient_Pressure - Starting_Ambient_Pressure) / Ascent_to_Altitude_Time;
        Nitrogen_Rate = Rate * fraction_inert_gas;
		
        for (i = 0; i < 16; i++) {
            double Initial_Nitrogen_Pressure = dive->Nitrogen_Pressure[i];
			
            dive->Nitrogen_Pressure[i] = vpmb_schreiner_equation(Initial_Inspired_N2_Pressure, Nitrogen_Rate, Ascent_to_Altitude_Time, dive->Nitrogen_Time_Constant[i], Initial_Nitrogen_Pressure);
			
            double Compartment_Gradient = (dive->Nitrogen_Pressure[i] + dive->Constant_Pressure_Other_Gases) - Ending_Ambient_Pressure;
			
            double Compartment_Gradient_Pascals = (Compartment_Gradient / dive->Units_Factor) * ATM;
			
            double Gradient_He_Bubble_Formation = ((2.0 * dive->Surface_Tension_Gamma * (dive->Skin_Compression_GammaC - dive->Surface_Tension_Gamma)) / (dive->Initial_Critical_Radius_He[i] * dive->Skin_Compression_GammaC));
			
            if (Compartment_Gradient_Pascals > Gradient_He_Bubble_Formation) {
				
                double New_Critical_Radius_He = ((2.0 * dive->Surface_Tension_Gamma * (dive->Skin_Compression_GammaC - dive->Surface_Tension_Gamma))) / (Compartment_Gradient_Pascals * dive->Skin_Compression_GammaC);
				
                dive->Adjusted_Critical_Radius_He[i] = dive->Initial_Critical_Radius_He[i] + (dive->Initial_Critical_Radius_He[i] -  New_Critical_Radius_He) * exp(-Time_at_Altitude_Before_Dive / dive->Regeneration_Time_Constant);
				
                dive->Initial_Critical_Radius_He[i] = dive->Adjusted_Critical_Radius_He[i];
            } else {
                double Ending_Radius_He = 1.0 / (Compartment_Gradient_Pascals / (2.0 * (dive->Surface_Tension_Gamma - dive->Skin_Compression_GammaC))  + 1.0 / dive->Initial_Critical_Radius_He[i]);
				
                double Regenerated_Radius_He = dive->Initial_Critical_Radius_He[i] + (Ending_Radius_He - dive->Initial_Critical_Radius_He[i]) * exp(-Time_at_Altitude_Before_Dive / dive->Regeneration_Time_Constant);
				
                dive->Initial_Critical_Radius_He[i] = Regenerated_Radius_He;
                dive->Adjusted_Critical_Radius_He[i] = dive->Initial_Critical_Radius_He[i];
            }
            double Gradient_N2_Bubble_Formation = ((2.0 * dive->Surface_Tension_Gamma * (dive->Skin_Compression_GammaC - dive->Surface_Tension_Gamma)) / (dive->Initial_Critical_Radius_N2[i] * dive->Skin_Compression_GammaC));
			
            if (Compartment_Gradient_Pascals > Gradient_N2_Bubble_Formation) {
				
                double New_Critical_Radius_N2 = ((2.0 * dive->Surface_Tension_Gamma * (dive->Skin_Compression_GammaC - dive->Surface_Tension_Gamma))) / (Compartment_Gradient_Pascals * dive->Skin_Compression_GammaC);
				
                dive->Adjusted_Critical_Radius_N2[i] = dive->Initial_Critical_Radius_N2[i] + (dive->Initial_Critical_Radius_N2[i] - New_Critical_Radius_N2) * exp(-Time_at_Altitude_Before_Dive / dive->Regeneration_Time_Constant);
				
                dive->Initial_Critical_Radius_N2[i] = dive->Adjusted_Critical_Radius_N2[i];
            } else {
                double Ending_Radius_N2 = 1.0 / (Compartment_Gradient_Pascals / (2.0 * (dive->Surface_Tension_Gamma - dive->Skin_Compression_GammaC)) + 1.0 / dive->Initial_Critical_Radius_N2[i]);
				
                double Regenerated_Radius_N2 = dive->Initial_Critical_Radius_N2[i] + (Ending_Radius_N2 - dive->Initial_Critical_Radius_N2[i]) * exp(-Time_at_Altitude_Before_Dive / dive->Regeneration_Time_Constant);
				
                dive->Initial_Critical_Radius_N2[i] = Regenerated_Radius_N2;
                dive->Adjusted_Critical_Radius_N2[i] = dive->Initial_Critical_Radius_N2[i];
            }
			
        }
        Inspired_Nitrogen_Pressure = (dive->Barometric_Pressure - dive->Water_Vapor_Pressure) * fraction_inert_gas;
		
        for(i = 0; i < 16; i++) {
            double Initial_Nitrogen_Pressure = dive->Nitrogen_Pressure[i];
			
            dive->Nitrogen_Pressure[i] = vpmb_haldane_equation(Initial_Nitrogen_Pressure, Inspired_Nitrogen_Pressure, dive->Nitrogen_Time_Constant[i], Time_at_Altitude_Before_Dive);
        }
    }
    return VALIDDATA;
}






