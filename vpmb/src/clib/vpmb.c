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
 *
 */

#include <Arduino.h>

#include "vpmb.h"


//const int TRUE = 1;
//const int FALSE = 0;

//const short true = 1;

//const short false = 0;

//#define DBG(a) DBG_PRINT(a)
#define DBG_PRINT(...) 
#define TRC_PRINT(...) 


/* errors */
const int ALLGOOD = 256; /* My Lucky Number */


const int ROOTERROR = -3;
const int ROOTFOUND = 3;
const int BADALTITUDE = -4;

const int OFFGASSINGERROR = -5;
const int BADDECOSTOP = -6;
//const int NOFILE = -7;

/* Constants */
const double ATM = 101325.0;
const double fraction_inert_gas = 0.79;
const double Helium_Half_Time[] = {1.88, 3.02, 4.72, 6.99, 10.21, 14.48, 20.53, 29.11, 41.20, 55.19, 70.69, 90.34, 115.29, 147.42, 188.24, 240.03};
const double Nitrogen_Half_Time[] = {5.0, 8.0, 12.5, 18.5, 27.0, 38.3, 54.3, 77.0, 109.0, 146.0, 187.0, 239.0, 305.0, 390.0, 498.0, 635.0};
const int Buhlmann_Compartments = 16;

const int DONE_PROFILE = 1;
const int NOT_DONE_PROFILE = -1;
const int DEPTH_CHANGED = 0;

const double meters_per_minute_change = 2.5; /* when deciding whether to ascend or descend (schreiner_equation) or staying at a constant depth (haldane_equation), see if the depth change over the last minute is +- this value */

const char  * const direction_labels[5] = {"ASCENT", "DESCENT", "CONSTANT", "ERROR", "DONE"};

//void vpmb_failure()
//{
//    /* turn this into a better way to handle failures for a real program */
//    exit(EXIT_FAILURE);
//}
/* VPM RELATED STUFF */

/**
 * \brief Function for ascent and descent gas loading calculations.
 *
 * Based on the derivations done by Erik Baker (Note: ascents must be given in negative numbers (ie: -10 for 10 feet up), as descents use positive values).
 *
 * Calculates
 * \f$P(t) = P_{Io} + R(t - 1 / k) - (P_{Io} − P_O − R / k) e^{kt}\f$
 * \return Pressure at interval time
 */
double vpmb_schreiner_equation(double Initial_Inspired_Gas_Pressure, double Rate_Change_Insp_Gas_Pressure,
                               double Interval_Time, double Gas_Time_Constant, double Initial_Gas_Pressure)
{

    return Initial_Inspired_Gas_Pressure + Rate_Change_Insp_Gas_Pressure * (Interval_Time - 1.0 / Gas_Time_Constant) - (Initial_Inspired_Gas_Pressure - Initial_Gas_Pressure - Rate_Change_Insp_Gas_Pressure / Gas_Time_Constant) * exp (-Gas_Time_Constant * Interval_Time);
}

/**
 * \brief Function for gas loading calculations at a constant depth.
 *
 * Based on the derivations by Erik Baker.
 *
 * Calculates
 * \f$P = P_0 + (P_I - P_0)(1 - e^{kt})\f$
 * \return Pressure at interval time.
 */
double vpmb_haldane_equation(double Initial_Gas_Pressure, double Inspired_Gas_Pressure, double Gas_Time_Constant, double Interval_Time)
{

    return Initial_Gas_Pressure + (Inspired_Gas_Pressure - Initial_Gas_Pressure) * (1.0 - exp(-Gas_Time_Constant * Interval_Time));
}



int vpmb_radius_root_finder(double A, double B, double C, double Low_Bound, double High_Bound, double *result)
{
    int i;
    const double iterations = 100;
    double Function_at_Low_Bound = Low_Bound * (Low_Bound * (A * Low_Bound - B)) - C;
    double Function_at_High_Bound = High_Bound * (High_Bound * (A * High_Bound - B)) - C;
    double Radius_at_Low_Bound, Radius_at_High_Bound;
    double Ending_Radius, Last_Diff_Change, Differential_Change;
    double Function, Derivative_of_Function, Last_Ending_Radius;

    if ((Function_at_Low_Bound > 0.0) && (Function_at_High_Bound > 0.0)) {
        return ROOTERROR;
    }

    if ((Function_at_Low_Bound < 0.0) && (Function_at_High_Bound < 0.0)) {
        return ROOTERROR;
    }
    if (Function_at_Low_Bound == 0.0) {
        *result = Low_Bound;
        return ROOTFOUND;
    } else if (Function_at_High_Bound == 0.0) {
        *result = High_Bound;
        return ROOTFOUND;
    } else if (Function_at_Low_Bound < 0.0) {
        Radius_at_Low_Bound = Low_Bound;
        Radius_at_High_Bound = High_Bound;
    } else {
        Radius_at_High_Bound = Low_Bound;
        Radius_at_Low_Bound = High_Bound;
    }

    Ending_Radius = 0.5 * (Low_Bound + High_Bound);
    Last_Diff_Change = fabs(High_Bound - Low_Bound);
    Differential_Change = Last_Diff_Change;

    /*At this point, the Newton-Raphson Method is applied which uses a function
      and its first derivative to rapidly converge upon a solution.
      Note: the program allows for up to 100 iterations.  Normally an exit will
      be made from the loop well before that number.  If, for some reason, the
      program exceeds 100 iterations, there will be a pause to alert the user.
      When a solution with the desired accuracy is found, exit is made from the
      loop by returning to the calling program.  The last value of ending
      radius has been assigned as the solution.*/

    Function = Ending_Radius * (Ending_Radius * (A * Ending_Radius - B)) - C;
    Derivative_of_Function = Ending_Radius * (Ending_Radius * 3.0 * A - 2.0 * B);

    for(i = 0; i < iterations; i++) {
        if((((Ending_Radius - Radius_at_High_Bound) * Derivative_of_Function - Function) *
            ((Ending_Radius - Radius_at_Low_Bound) * Derivative_of_Function - Function) >= 0.0)
           || (fabs(2.0 * Function) > (fabs(Last_Diff_Change * Derivative_of_Function)))) {

            Last_Diff_Change = Differential_Change;
            Differential_Change = 0.5 * (Radius_at_High_Bound - Radius_at_Low_Bound);

            Ending_Radius = Radius_at_Low_Bound + Differential_Change;
            if (Radius_at_Low_Bound == Ending_Radius) {
                *result = Ending_Radius;
                return ROOTFOUND;
            }
        }

        else {
            Last_Diff_Change = Differential_Change;
            Differential_Change = Function / Derivative_of_Function;
            Last_Ending_Radius = Ending_Radius;
            Ending_Radius = Ending_Radius - Differential_Change;
            if (Last_Ending_Radius == Ending_Radius) {
                *result = Ending_Radius;
                return ROOTFOUND;
            }
            if (fabs(Differential_Change) < 1.0E-12) {
                *result = Ending_Radius;
                return ROOTFOUND;
            }
        }

        Function = Ending_Radius * (Ending_Radius * (A * Ending_Radius - B)) - C;
        Derivative_of_Function = Ending_Radius * (Ending_Radius * 3.0 * A - 2.0 * B);

        if (Function < 0.0) {
            Radius_at_Low_Bound = Ending_Radius;
        } else {
            Radius_at_High_Bound = Ending_Radius;
        }
    }
    return ROOTERROR;
}

/** \brief Calculate the barometric pressure at the current altitude
 *
 * Based on the FORTRAN code by Ralph L. Carmichael available at http://www.pdas.com/atmos.html ,
 * and the version provided by Erik Baker.
 * \returns The barometric pressure for the given altitude, in the units specified by the user.
 */
double vpmb_calc_barometric_pressure(double Altitude, bool units_fsw)
{
    double Radius_of_Earth = 6369.0;
    double Acceleration_of_Gravity = 9.80665;
    double Molecular_weight_of_Air = 28.9644;
    double Gas_Constant_R = 8.31432;
    double Temp_at_Sea_Level = 288.15;
    double Pressure_at_Sea_Level_Fsw = 33.0;
    double Pressure_at_Sea_Level_Msw = 10.0;
    double Temp_Gradient = -6.5;
    double GMR_Factor = Acceleration_of_Gravity * Molecular_weight_of_Air / Gas_Constant_R;
    double Pressure_at_Sea_Level, Geopotential_Altitude, Temp_at_Geopotential_Altitude, Barometric_Pressure;
    double Altitude_Kilometers;

    if (units_fsw == TRUE) {
        double Altitude_Feet = Altitude;
        Altitude_Kilometers = Altitude_Feet / 3280.839895;
        Pressure_at_Sea_Level = Pressure_at_Sea_Level_Fsw;
    } else {
        double Altitude_Meters = Altitude;
        Altitude_Kilometers = Altitude_Meters / 1000.0;
        Pressure_at_Sea_Level = Pressure_at_Sea_Level_Msw;
    }

    Geopotential_Altitude =  (Altitude_Kilometers * Radius_of_Earth) / (Altitude_Kilometers + Radius_of_Earth);
    Temp_at_Geopotential_Altitude = Temp_at_Sea_Level + Temp_Gradient * Geopotential_Altitude;

    Barometric_Pressure = Pressure_at_Sea_Level * exp(log(Temp_at_Sea_Level / Temp_at_Geopotential_Altitude) * GMR_Factor / Temp_Gradient);
    return Barometric_Pressure;
}

double vpmb_calc_deco_ceiling(dive_state *dive)
{
    int i;
    double Compartment_Deco_Ceiling[Buhlmann_Compartments];
    double Gas_Loading, Tolerated_Ambient_Pressure, Deco_Ceiling_Depth;

    for(i = 0; i < Buhlmann_Compartments; i++) {
        Gas_Loading = dive->Helium_Pressure[i] + dive->Nitrogen_Pressure[i];

        if (Gas_Loading > 0.0) {
            double Weighted_Allowable_Gradient = (dive->Deco_Gradient_He[i] * dive->Helium_Pressure[i] + dive->Deco_Gradient_N2[i] * dive->Nitrogen_Pressure[i]) / (dive->Helium_Pressure[i] + dive->Nitrogen_Pressure[i]);

            Tolerated_Ambient_Pressure = (Gas_Loading + dive->Constant_Pressure_Other_Gases) - Weighted_Allowable_Gradient;
        } else {
            double Weighted_Allowable_Gradient = min(dive->Deco_Gradient_He[i], dive->Deco_Gradient_N2[i]);
            Tolerated_Ambient_Pressure = dive->Constant_Pressure_Other_Gases - Weighted_Allowable_Gradient;
        }
        if(Tolerated_Ambient_Pressure < 0.0) {
            Tolerated_Ambient_Pressure = 0.0;
        }


        Compartment_Deco_Ceiling[i] = Tolerated_Ambient_Pressure - dive->Barometric_Pressure;
    }

    Deco_Ceiling_Depth = Compartment_Deco_Ceiling[0];

    for(i = 1; i < Buhlmann_Compartments; i++) {
        Deco_Ceiling_Depth = max(Deco_Ceiling_Depth, Compartment_Deco_Ceiling[i]);
    }

    return Deco_Ceiling_Depth;
}


/** \brief Updates the gas pressures when staying at a constant depth.
 *
 * Uses the ::HALDANE_EQUATION to update the gas pressures (dive_state.Helium_Pressure and dive_state.Nitrogen_Pressure)
 * for a time segment at constant depth.
 *
 * Side Effects: Sets
 * - dive_state.Ending_Ambient_Pressure
 * - dive_state.Helium_Pressure
 * - dive_state.Nitrogen_Pressure
 * - dive_state.Run_Time
 * - dive_state.Segment_Number
 * - dive_state.Segment_Time
 *
 */
void vpmb_gas_loadings_constant_depth(dive_state *dive, double Depth, double Run_Time_End_of_Segment)
{
    int i;
    double Ambient_Pressure = Depth + dive->Barometric_Pressure;

    dive->Segment_Time = Run_Time_End_of_Segment - dive->Run_Time;
    dive->Run_Time = Run_Time_End_of_Segment;
    dive->Segment_Number++;

    double Inspired_Helium_Pressure = (Ambient_Pressure - dive->Water_Vapor_Pressure - (dive->PpO2 *dive->Units_Factor)) * dive->Fraction_Helium[dive->Mix_Number - 1];

    double Inspired_Nitrogen_Pressure = (Ambient_Pressure - dive->Water_Vapor_Pressure - (dive->PpO2 *dive->Units_Factor)) * dive->Fraction_Nitrogen[dive->Mix_Number - 1];

    dive->Ending_Ambient_Pressure = Ambient_Pressure;

    double Temp_Helium_Pressure = 0.0;
    double Temp_Nitrogen_Pressure = 0.0;

    for(i = 0; i < Buhlmann_Compartments; i++) {
        Temp_Helium_Pressure = dive->Helium_Pressure[i];
        Temp_Nitrogen_Pressure = dive->Nitrogen_Pressure[i];

        dive->Helium_Pressure[i] = vpmb_haldane_equation(Temp_Helium_Pressure, Inspired_Helium_Pressure, dive->Helium_Time_Constant[i], dive->Segment_Time);

        dive->Nitrogen_Pressure[i] = vpmb_haldane_equation(Temp_Nitrogen_Pressure, Inspired_Nitrogen_Pressure, dive->Nitrogen_Time_Constant[i], dive->Segment_Time);
    }
}

void vpmb_nuclear_regeneration(dive_state *dive, double Dive_Time)
{
    int i;

    for (i = 0; i < Buhlmann_Compartments; i++) {
        double Crushing_Pressure_Pascals_He = (dive->Max_Crushing_Pressure_He[i] / dive->Units_Factor) * ATM;
        double Crushing_Pressure_Pascals_N2 = (dive->Max_Crushing_Pressure_N2[i] / dive->Units_Factor) * ATM;

        double Ending_Radius_He = 1.0 / (Crushing_Pressure_Pascals_He / (2.0 * (dive->Skin_Compression_GammaC - dive->Surface_Tension_Gamma)) + 1.0 / dive->Adjusted_Critical_Radius_He[i]);

        double Ending_Radius_N2 = 1.0 / (Crushing_Pressure_Pascals_N2 / (2.0 * (dive->Skin_Compression_GammaC - dive->Surface_Tension_Gamma)) + 1.0 / dive->Adjusted_Critical_Radius_N2[i]);

        dive->Regenerated_Radius_He[i] = dive->Adjusted_Critical_Radius_He[i] + (Ending_Radius_He - dive->Adjusted_Critical_Radius_He[i]) * exp(-Dive_Time / dive->Regeneration_Time_Constant);

        dive->Regenerated_Radius_N2[i] = dive->Adjusted_Critical_Radius_N2[i] + (Ending_Radius_N2 - dive->Adjusted_Critical_Radius_N2[i]) * exp(-Dive_Time / dive->Regeneration_Time_Constant);


        double Crush_Pressure_Adjust_Ratio_He = (Ending_Radius_He * (dive->Adjusted_Critical_Radius_He[i] - dive->Regenerated_Radius_He[i])) / (dive->Regenerated_Radius_He[i] * (dive->Adjusted_Critical_Radius_He[i] - Ending_Radius_He));

        double Crush_Pressure_Adjust_Ratio_N2 = (Ending_Radius_N2 * (dive->Adjusted_Critical_Radius_N2[i] - dive->Regenerated_Radius_N2[i])) / (dive->Regenerated_Radius_N2[i] * (dive->Adjusted_Critical_Radius_N2[i] - Ending_Radius_N2));

        double Adj_Crush_Pressure_He_Pascals = Crushing_Pressure_Pascals_He * Crush_Pressure_Adjust_Ratio_He;
        double Adj_Crush_Pressure_N2_Pascals = Crushing_Pressure_Pascals_N2 * Crush_Pressure_Adjust_Ratio_N2;

        dive->Adjusted_Crushing_Pressure_He[i] = (Adj_Crush_Pressure_He_Pascals / ATM) * dive->Units_Factor;
        dive->Adjusted_Crushing_Pressure_N2[i] = (Adj_Crush_Pressure_N2_Pascals / ATM) * dive->Units_Factor;
    }
}


double vpmb_crushing_pressure_helper(dive_state *dive, double Radius_Onset_of_Imperm_Molecule, double Ending_Ambient_Pressure_Pa, double Amb_Press_Onset_of_Imperm_Pa, double Gas_Tension_Onset_of_Imperm_Pa, double Gradient_Onset_of_Imperm_Pa)
{

    double A = Ending_Ambient_Pressure_Pa - Amb_Press_Onset_of_Imperm_Pa + Gas_Tension_Onset_of_Imperm_Pa + (2.0 * (dive->Skin_Compression_GammaC - dive->Surface_Tension_Gamma)) / Radius_Onset_of_Imperm_Molecule;
    double B = 2.0 * (dive->Skin_Compression_GammaC - dive->Surface_Tension_Gamma);
    double C = Gas_Tension_Onset_of_Imperm_Pa * pow(Radius_Onset_of_Imperm_Molecule, 3);

    double High_Bound = Radius_Onset_of_Imperm_Molecule;
    double Low_Bound = B / A;

    double Ending_Radius;
    double Crushing_Pressure_Pascals;

    if (vpmb_radius_root_finder(A, B, C, Low_Bound, High_Bound, &Ending_Radius) < 0) {
        //vpmb_failure();
		return NAN;
    }
    Crushing_Pressure_Pascals = Gradient_Onset_of_Imperm_Pa + Ending_Ambient_Pressure_Pa - Amb_Press_Onset_of_Imperm_Pa +  Gas_Tension_Onset_of_Imperm_Pa * (1.0 - pow(Radius_Onset_of_Imperm_Molecule, 3) / pow(Ending_Radius, 3));

    return (Crushing_Pressure_Pascals / ATM) * dive->Units_Factor;
}
int vpmb_onset_of_impermeability(dive_state *dive, double Starting_Ambient_Pressure, double Ending_Ambient_Pressure, double Rate, int i)
{
    int j;

    double Gradient_Onset_of_Imperm = dive->Gradient_Onset_of_Imperm_Atm * dive->Units_Factor;

    double Initial_Inspired_He_Pressure = (Starting_Ambient_Pressure - dive->Water_Vapor_Pressure)- (dive->PpO2 *dive->Units_Factor) * dive->Fraction_Helium[dive->Mix_Number - 1];

    double Initial_Inspired_N2_Pressure = (Starting_Ambient_Pressure - dive->Water_Vapor_Pressure- (dive->PpO2 *dive->Units_Factor)) * dive->Fraction_Nitrogen[dive->Mix_Number - 1];

    double Helium_Rate = Rate * dive->Fraction_Helium[dive->Mix_Number - 1];
    double Nitrogen_Rate = Rate * dive->Fraction_Nitrogen[dive->Mix_Number - 1];
    double Low_Bound = 0.0;

    double High_Bound = (Ending_Ambient_Pressure - Starting_Ambient_Pressure) / Rate;

    double Starting_Gas_Tension = dive->Initial_Helium_Pressure[i] + dive->Initial_Nitrogen_Pressure[i] + dive->Constant_Pressure_Other_Gases;

    double Function_at_Low_Bound = Starting_Ambient_Pressure - Starting_Gas_Tension - Gradient_Onset_of_Imperm;

    double High_Bound_Helium_Pressure = vpmb_schreiner_equation(Initial_Inspired_He_Pressure, Helium_Rate, High_Bound, dive->Helium_Time_Constant[i], dive->Initial_Helium_Pressure[i]);

    double High_Bound_Nitrogen_Pressure = vpmb_schreiner_equation(Initial_Inspired_N2_Pressure, Nitrogen_Rate, High_Bound, dive->Nitrogen_Time_Constant[i], dive->Initial_Nitrogen_Pressure[i]);

    double Ending_Gas_Tension = High_Bound_Helium_Pressure + High_Bound_Nitrogen_Pressure + dive->Constant_Pressure_Other_Gases;

    double Function_at_High_Bound = Ending_Ambient_Pressure -  Ending_Gas_Tension - Gradient_Onset_of_Imperm;

    double Time, Differential_Change;
    double Mid_Range_Ambient_Pressure, Gas_Tension_at_Mid_Range;

    if((Function_at_High_Bound * Function_at_Low_Bound) >= 0.0) {
        return  ROOTERROR;
    }


    if( Function_at_Low_Bound < 0.0) {
        Time = Low_Bound;
        Differential_Change = High_Bound - Low_Bound;
    } else {
        Time = High_Bound;
        Differential_Change = Low_Bound - High_Bound;
    }

    for(j = 0; j < 100; j++) {
        double Last_Diff_Change = Differential_Change;
        Differential_Change = Last_Diff_Change * 0.5;
        double Mid_Range_Time = Time + Differential_Change;

        Mid_Range_Ambient_Pressure = (Starting_Ambient_Pressure + Rate * Mid_Range_Time);

        double Mid_Range_Helium_Pressure = vpmb_schreiner_equation(Initial_Inspired_He_Pressure, Helium_Rate, Mid_Range_Time, dive->Helium_Time_Constant[i], dive->Initial_Helium_Pressure[i]);

        double Mid_Range_Nitrogen_Pressure = vpmb_schreiner_equation(Initial_Inspired_N2_Pressure, Nitrogen_Rate, Mid_Range_Time, dive->Nitrogen_Time_Constant[i], dive->Initial_Nitrogen_Pressure[i]);

        Gas_Tension_at_Mid_Range = Mid_Range_Helium_Pressure +  Mid_Range_Nitrogen_Pressure + dive->Constant_Pressure_Other_Gases;

        double Function_at_Mid_Range = Mid_Range_Ambient_Pressure - Gas_Tension_at_Mid_Range - Gradient_Onset_of_Imperm;

        if(Function_at_Mid_Range <= 0.0) {
            Time = Mid_Range_Time;
        }

        if ((fabs(Differential_Change) < 1.0E-3) || (Function_at_Mid_Range == 0.0)) {
            break;
        }

        if(j == 100) {
            return ROOTERROR;
        }
    }

    dive->Amb_Pressure_Onset_of_Imperm[i] = Mid_Range_Ambient_Pressure;
    dive->Gas_Tension_Onset_of_Imperm[i] = Gas_Tension_at_Mid_Range;

    return ROOTFOUND;
}

//DIRK this can fail hard- extern only
bool vpmb_calc_crushing_pressure(dive_state *dive, double Starting_Depth, double Ending_Depth, double Rate)
{
    int i;

    double Gradient_Onset_of_Imperm = dive->Gradient_Onset_of_Imperm_Atm * dive->Units_Factor;
    double Gradient_Onset_of_Imperm_Pa = dive->Gradient_Onset_of_Imperm_Atm * ATM;

    double Starting_Ambient_Pressure = Starting_Depth + dive->Barometric_Pressure;
    double Ending_Ambient_Pressure = Ending_Depth + dive->Barometric_Pressure;

    for(i = 0; i < Buhlmann_Compartments; i++) {
        double Starting_Gas_Tension = dive->Initial_Helium_Pressure[i] + dive->Initial_Nitrogen_Pressure[i] + dive->Constant_Pressure_Other_Gases;

        double Starting_Gradient = Starting_Ambient_Pressure - Starting_Gas_Tension;

        double Ending_Gas_Tension = dive->Helium_Pressure[i] + dive->Nitrogen_Pressure[i] + dive->Constant_Pressure_Other_Gases;

        double Ending_Gradient = Ending_Ambient_Pressure - Ending_Gas_Tension;

        double Radius_Onset_of_Imperm_He = 1.0 / (Gradient_Onset_of_Imperm_Pa / (2.0 * (dive->Skin_Compression_GammaC - dive->Surface_Tension_Gamma)) + 1.0 / dive->Adjusted_Critical_Radius_He[i]);

        double Radius_Onset_of_Imperm_N2 = 1.0 / (Gradient_Onset_of_Imperm_Pa / (2.0 * (dive->Skin_Compression_GammaC - dive->Surface_Tension_Gamma)) + 1.0 / dive->Adjusted_Critical_Radius_N2[i]);

        double Crushing_Pressure_He;
        double Crushing_Pressure_N2;
        if(Ending_Gradient <= Gradient_Onset_of_Imperm) {
            Crushing_Pressure_He = Ending_Ambient_Pressure - Ending_Gas_Tension;
            Crushing_Pressure_N2 = Ending_Ambient_Pressure - Ending_Gas_Tension;
        } else {
            double Ending_Ambient_Pressure_Pa;
            double Amb_Press_Onset_of_Imperm_Pa;
            double Gas_Tension_Onset_of_Imperm_Pa;

            if (Starting_Gradient == Gradient_Onset_of_Imperm) {
                dive->Amb_Pressure_Onset_of_Imperm[i] = Starting_Ambient_Pressure;
                dive->Gas_Tension_Onset_of_Imperm[i] = Starting_Gas_Tension;
            }

            if (Starting_Gradient < Gradient_Onset_of_Imperm) {
                vpmb_onset_of_impermeability(dive, Starting_Ambient_Pressure, Ending_Ambient_Pressure, Rate, i);
            }

            Ending_Ambient_Pressure_Pa = (Ending_Ambient_Pressure / dive->Units_Factor) * ATM;

            Amb_Press_Onset_of_Imperm_Pa = (dive->Amb_Pressure_Onset_of_Imperm[i] / dive->Units_Factor) * ATM;

            Gas_Tension_Onset_of_Imperm_Pa = (dive->Gas_Tension_Onset_of_Imperm[i] / dive->Units_Factor) * ATM;

            Crushing_Pressure_He = vpmb_crushing_pressure_helper(dive, Radius_Onset_of_Imperm_He, Ending_Ambient_Pressure_Pa, Amb_Press_Onset_of_Imperm_Pa, Gas_Tension_Onset_of_Imperm_Pa, Gradient_Onset_of_Imperm_Pa);

            Crushing_Pressure_N2 = vpmb_crushing_pressure_helper(dive, Radius_Onset_of_Imperm_N2, Ending_Ambient_Pressure_Pa, Amb_Press_Onset_of_Imperm_Pa, Gas_Tension_Onset_of_Imperm_Pa, Gradient_Onset_of_Imperm_Pa);
			if (isnan(Crushing_Pressure_He) || isnan (Crushing_Pressure_N2)) return FALSE;
       
		}

        dive->Max_Crushing_Pressure_He[i] = max(dive->Max_Crushing_Pressure_He[i], Crushing_Pressure_He);
        dive->Max_Crushing_Pressure_N2[i] = max(dive->Max_Crushing_Pressure_N2[i], Crushing_Pressure_N2);
    }
	return TRUE;
}

void vpmb_gas_loadings_ascent_descent(dive_state *dive, double Starting_Depth, double Ending_Depth, double Rate)
{
    int i;
    double Starting_Ambient_Pressure;
    double Initial_Inspired_He_Pressure;
    double Initial_Inspired_N2_Pressure;
    double Helium_Rate;
    double Nitrogen_Rate;

    dive->Segment_Time = (Ending_Depth - Starting_Depth) / Rate;

    dive->Run_Time = dive->Run_Time + dive->Segment_Time;
    dive->Segment_Number++;
    dive->Ending_Ambient_Pressure = Ending_Depth + dive->Barometric_Pressure;

    Starting_Ambient_Pressure = Starting_Depth + dive->Barometric_Pressure;
    Initial_Inspired_He_Pressure = (Starting_Ambient_Pressure - dive->Water_Vapor_Pressure - (dive->PpO2 *dive->Units_Factor)) * dive->Fraction_Helium[dive->Mix_Number - 1];

    Initial_Inspired_N2_Pressure = (Starting_Ambient_Pressure - dive->Water_Vapor_Pressure - (dive->PpO2 *dive->Units_Factor)) * dive->Fraction_Nitrogen[dive->Mix_Number - 1];
    Helium_Rate = Rate * dive->Fraction_Helium[dive->Mix_Number - 1];
    Nitrogen_Rate = Rate * dive->Fraction_Nitrogen[dive->Mix_Number - 1];

    for(i = 0; i < Buhlmann_Compartments; i++) {
        dive->Initial_Helium_Pressure[i] = dive->Helium_Pressure[i];
        dive->Initial_Nitrogen_Pressure[i] = dive->Nitrogen_Pressure[i];

        dive->Helium_Pressure[i] = vpmb_schreiner_equation(Initial_Inspired_He_Pressure, Helium_Rate, dive->Segment_Time, dive->Helium_Time_Constant[i], dive->Initial_Helium_Pressure[i]);

        dive->Nitrogen_Pressure[i] = vpmb_schreiner_equation(Initial_Inspired_N2_Pressure, Nitrogen_Rate, dive->Segment_Time, dive->Nitrogen_Time_Constant[i], dive->Initial_Nitrogen_Pressure[i]);
    }
}

int vpmb_decompression_stop(dive_state *dive, double Deco_Stop_Depth, double Step_Size)
{
    int i;
    double Deco_Ceiling_Depth = 0.0;
    double Last_Run_Time = dive->Run_Time;
    double Round_Up_Operation = round((Last_Run_Time / dive->Minimum_Deco_Stop_Time) + 0.5) * dive->Minimum_Deco_Stop_Time;
    dive->Segment_Time = Round_Up_Operation - dive->Run_Time;
    dive->Run_Time = Round_Up_Operation;
    double Temp_Segment_Time = dive->Segment_Time;
    dive->Segment_Number++;
    double Ambient_Pressure = Deco_Stop_Depth + dive->Barometric_Pressure;
    dive->Ending_Ambient_Pressure = Ambient_Pressure;
    double Next_Stop = Deco_Stop_Depth - Step_Size;

    double Inspired_Helium_Pressure = (Ambient_Pressure - dive->Water_Vapor_Pressure - (dive->PpO2 *dive->Units_Factor)) * dive->Fraction_Helium[dive->Mix_Number - 1];

    double Inspired_Nitrogen_Pressure = (Ambient_Pressure - dive->Water_Vapor_Pressure- (dive->PpO2 *dive->Units_Factor)) * dive->Fraction_Nitrogen[dive->Mix_Number - 1];


    for(i = 0; i < Buhlmann_Compartments; i++) {
        if((Inspired_Helium_Pressure + Inspired_Nitrogen_Pressure) > 0.0) {
            double Weighted_Allowable_Gradient = (dive->Deco_Gradient_He[i] * Inspired_Helium_Pressure + dive->Deco_Gradient_N2[i] * Inspired_Nitrogen_Pressure) / (Inspired_Helium_Pressure + Inspired_Nitrogen_Pressure);

            if ((Inspired_Helium_Pressure + Inspired_Nitrogen_Pressure + dive->Constant_Pressure_Other_Gases - Weighted_Allowable_Gradient) > (Next_Stop + dive->Barometric_Pressure)) {
                return OFFGASSINGERROR;
            }
        }
    }
    while(TRUE) {
        for(i = 0; i < Buhlmann_Compartments; i++) {
            double Initial_Helium_Pressure = dive->Helium_Pressure[i];
            double Initial_Nitrogen_Pressure = dive->Nitrogen_Pressure[i];

            dive->Helium_Pressure[i] = vpmb_haldane_equation(Initial_Helium_Pressure, Inspired_Helium_Pressure, dive->Helium_Time_Constant[i], dive->Segment_Time);

            dive->Nitrogen_Pressure[i] = vpmb_haldane_equation(Initial_Nitrogen_Pressure, Inspired_Nitrogen_Pressure, dive->Nitrogen_Time_Constant[i], dive->Segment_Time);
        }
				YIELD();
        Deco_Ceiling_Depth = vpmb_calc_deco_ceiling(dive);
        if (Deco_Ceiling_Depth > Next_Stop) {
            dive->Segment_Time = dive->Minimum_Deco_Stop_Time;
            double Time_Counter = Temp_Segment_Time;
            Temp_Segment_Time =  Time_Counter + dive->Minimum_Deco_Stop_Time;
            double Last_Run_Time = dive->Run_Time;
            dive->Run_Time = Last_Run_Time + dive->Minimum_Deco_Stop_Time;
            continue;
        }
        break;
    }
    dive->Segment_Time = Temp_Segment_Time;
    return ALLGOOD;
}

double vpmb_calculate_deco_gradient(dive_state *dive, double Allowable_Gradient_Molecule, double Amb_Press_First_Stop_Pascals, double Amb_Press_Next_Stop_Pascals)
{

    double Allow_Grad_First_Stop_Pa = (Allowable_Gradient_Molecule / dive->Units_Factor) * ATM;
    double Radius_First_Stop = (2.0 * dive->Surface_Tension_Gamma) / Allow_Grad_First_Stop_Pa;

    double A = Amb_Press_Next_Stop_Pascals;
    double B = -2.0 * dive->Surface_Tension_Gamma;
    double C = (Amb_Press_First_Stop_Pascals + (2.0 * dive->Surface_Tension_Gamma) / Radius_First_Stop) * Radius_First_Stop * (Radius_First_Stop * (Radius_First_Stop));

    double Low_Bound = Radius_First_Stop;
    double High_Bound = Radius_First_Stop * pow((Amb_Press_First_Stop_Pascals / Amb_Press_Next_Stop_Pascals), (1.0 / 3.0));

    double Ending_Radius;
    double Deco_Gradient_Pascals;
    if(vpmb_radius_root_finder(A, B, C, Low_Bound, High_Bound, &Ending_Radius) < 0) {
       // vpmb_failure();
		return NAN;
    }

    Deco_Gradient_Pascals = (2.0 * dive->Surface_Tension_Gamma) / Ending_Radius;
    return (Deco_Gradient_Pascals / ATM) * dive->Units_Factor;
}

bool vpmb_boyles_law_compensation(dive_state *dive, double First_Stop_Depth, double Deco_Stop_Depth, double Step_Size)
{

    int i;
    double Next_Stop = Deco_Stop_Depth - Step_Size;
    double Ambient_Pressure_First_Stop = First_Stop_Depth + dive->Barometric_Pressure;
    double Ambient_Pressure_Next_Stop = Next_Stop + dive->Barometric_Pressure;

    double Amb_Press_First_Stop_Pascals = (Ambient_Pressure_First_Stop / dive->Units_Factor) * ATM;
    double Amb_Press_Next_Stop_Pascals = (Ambient_Pressure_Next_Stop / dive->Units_Factor) * ATM;

    for(i = 0; i < Buhlmann_Compartments; i++) {
        dive->Deco_Gradient_He[i] = vpmb_calculate_deco_gradient(dive, dive->Allowable_Gradient_He[i], Amb_Press_First_Stop_Pascals, Amb_Press_Next_Stop_Pascals);

        dive->Deco_Gradient_N2[i] = vpmb_calculate_deco_gradient(dive, dive->Allowable_Gradient_N2[i], Amb_Press_First_Stop_Pascals, Amb_Press_Next_Stop_Pascals);
				if (isnan(dive->Deco_Gradient_He[i]) || isnan (dive->Deco_Gradient_N2[i])) return FALSE;
	}
	return TRUE;
}

void vpmb_calc_max_actual_gradient(dive_state *dive, double Deco_Stop_Depth)
{
    int i;
    for(i = 0; i < Buhlmann_Compartments; i++) {
        double Compartment_Gradient = (dive->Helium_Pressure[i] + dive->Nitrogen_Pressure[i] + dive->Constant_Pressure_Other_Gases) - (Deco_Stop_Depth + dive->Barometric_Pressure);
        if( Compartment_Gradient <= 0.0) {
            Compartment_Gradient = 0.0;
        }

        dive->Max_Actual_Gradient[i] = max(dive->Max_Actual_Gradient[i], Compartment_Gradient);
    }
}

void vpmb_projected_ascent(dive_state *dive, double Starting_Depth, double Rate, double Step_Size)
{
    int i, j;

    double New_Ambient_Pressure = dive->Deco_Stop_Depth + dive->Barometric_Pressure;
    double Starting_Ambient_Pressure = Starting_Depth + dive->Barometric_Pressure;

    double Initial_Inspired_He_Pressure = (Starting_Ambient_Pressure - dive->Water_Vapor_Pressure - (dive->PpO2 *dive->Units_Factor)) * dive->Fraction_Helium[dive->Mix_Number - 1];

    double Initial_Inspired_N2_Pressure = (Starting_Ambient_Pressure - dive->Water_Vapor_Pressure - (dive->PpO2 *dive->Units_Factor)) * dive->Fraction_Nitrogen[dive->Mix_Number - 1];

    double Helium_Rate = Rate * dive->Fraction_Helium[dive->Mix_Number - 1];
    double Nitrogen_Rate = Rate * dive->Fraction_Nitrogen[dive->Mix_Number - 1];

    double Temp_Gas_Loading[Buhlmann_Compartments];
    double Allowable_Gas_Loading[Buhlmann_Compartments];
    double Initial_Helium_Pressure[Buhlmann_Compartments];
    double Initial_Nitrogen_Pressure[Buhlmann_Compartments];

    for(i = 0; i < Buhlmann_Compartments; i++) {
        Initial_Helium_Pressure[i] = dive->Helium_Pressure[i];
        Initial_Nitrogen_Pressure[i] = dive->Nitrogen_Pressure[i];
    }

    while(TRUE) {
        double Ending_Ambient_Pressure = New_Ambient_Pressure;

        double Segment_Time = (Ending_Ambient_Pressure - Starting_Ambient_Pressure) / Rate;

        for(i = 0; i < Buhlmann_Compartments; i++) {
            double Temp_Helium_Pressure = vpmb_schreiner_equation(Initial_Inspired_He_Pressure, Helium_Rate, Segment_Time, dive->Helium_Time_Constant[i], Initial_Helium_Pressure[i]);

            double Temp_Nitrogen_Pressure = vpmb_schreiner_equation(Initial_Inspired_N2_Pressure, Nitrogen_Rate, Segment_Time, dive->Nitrogen_Time_Constant[i], Initial_Nitrogen_Pressure[i]);
            double Weighted_Allowable_Gradient;

            Temp_Gas_Loading[i] = Temp_Helium_Pressure + Temp_Nitrogen_Pressure;
            if (Temp_Gas_Loading[i] > 0.0) {
                Weighted_Allowable_Gradient = (dive->Allowable_Gradient_He[i] * Temp_Helium_Pressure + dive->Allowable_Gradient_N2[i] * Temp_Nitrogen_Pressure) / Temp_Gas_Loading[i];
            } else {
                Weighted_Allowable_Gradient = min(dive->Allowable_Gradient_He[i], dive->Allowable_Gradient_N2[i]);
            }

            Allowable_Gas_Loading[i] = Ending_Ambient_Pressure + Weighted_Allowable_Gradient - dive->Constant_Pressure_Other_Gases;
        }

        bool end_sub = TRUE;
        for (j = 0; j < Buhlmann_Compartments; j++) {
            if(Temp_Gas_Loading[j] > Allowable_Gas_Loading[j]) {
                New_Ambient_Pressure = Ending_Ambient_Pressure + Step_Size;
                dive->Deco_Stop_Depth = dive->Deco_Stop_Depth + Step_Size;
                end_sub = FALSE;
                break;
            }
        }
		YIELD();
        if(end_sub != TRUE) {
            continue;
        } else {
            break;
        }
    }
}

void vpmb_calc_ascent_ceiling(dive_state *dive)
{
    int i;
    double Gas_Loading = 0.0;
    double Compartment_Ascent_Ceiling[Buhlmann_Compartments];// = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
	for(i = 0; i < Buhlmann_Compartments; i++) Compartment_Ascent_Ceiling[i] = 0;
    for(i = 0; i < Buhlmann_Compartments; i++) {
        double Weighted_Allowable_Gradient, Tolerated_Ambient_Pressure;

        Gas_Loading = dive->Helium_Pressure[i] + dive->Nitrogen_Pressure[i];

        if(Gas_Loading > 0.0) {
            Weighted_Allowable_Gradient = (dive->Allowable_Gradient_He[i] * dive->Helium_Pressure[i] +  dive->Allowable_Gradient_N2[i] * dive->Nitrogen_Pressure[i]) / (dive->Helium_Pressure[i] + dive->Nitrogen_Pressure[i]);
            Tolerated_Ambient_Pressure = (Gas_Loading + dive->Constant_Pressure_Other_Gases) - Weighted_Allowable_Gradient;
        } else {
            Weighted_Allowable_Gradient = min(dive->Allowable_Gradient_He[i], dive->Allowable_Gradient_N2[i]);
            Tolerated_Ambient_Pressure = dive->Constant_Pressure_Other_Gases - Weighted_Allowable_Gradient;
        }
        if (Tolerated_Ambient_Pressure < 0.0) {
            Tolerated_Ambient_Pressure = 0.0;
        }

        Compartment_Ascent_Ceiling[i] = Tolerated_Ambient_Pressure - dive->Barometric_Pressure;
    }


    dive->Ascent_Ceiling_Depth = Compartment_Ascent_Ceiling[0];

    for(i = 1; i < Buhlmann_Compartments; i++) {
        dive->Ascent_Ceiling_Depth = max(dive->Ascent_Ceiling_Depth, Compartment_Ascent_Ceiling[i]);
    }
}


void vpmb_calc_surface_phase_volume_time(dive_state *dive)
{
    int i;
    double Surface_Inspired_N2_Pressure = (dive->Barometric_Pressure - dive->Water_Vapor_Pressure) * fraction_inert_gas;
    for(i = 0; i < Buhlmann_Compartments; i++) {
        if(dive->Nitrogen_Pressure[i] > Surface_Inspired_N2_Pressure) {
            dive->Surface_Phase_Volume_Time[i] = (dive->Helium_Pressure[i] / dive->Helium_Time_Constant[i] + (dive->Nitrogen_Pressure[i] - Surface_Inspired_N2_Pressure) / dive->Nitrogen_Time_Constant[i]) / (dive->Helium_Pressure[i] + dive->Nitrogen_Pressure[i] - Surface_Inspired_N2_Pressure);
        }

        else if((dive->Nitrogen_Pressure[i] <= Surface_Inspired_N2_Pressure) && (dive->Helium_Pressure[i] + dive->Nitrogen_Pressure[i] >= Surface_Inspired_N2_Pressure)) {
            double Decay_Time_to_Zero_Gradient = 1.0 / (dive->Nitrogen_Time_Constant[i] - dive->Helium_Time_Constant[i]) * log((Surface_Inspired_N2_Pressure - dive->Nitrogen_Pressure[i]) / dive->Helium_Pressure[i]);

            double Integral_Gradient_x_Time = dive->Helium_Pressure[i] / dive->Helium_Time_Constant[i] * (1.0 - exp(-dive->Helium_Time_Constant[i] * Decay_Time_to_Zero_Gradient)) + (dive->Nitrogen_Pressure[i] - Surface_Inspired_N2_Pressure) / dive->Nitrogen_Time_Constant[i] * (1.0 - exp(-dive->Nitrogen_Time_Constant[i] * Decay_Time_to_Zero_Gradient));

            dive->Surface_Phase_Volume_Time[i] = Integral_Gradient_x_Time / (dive->Helium_Pressure[i] + dive->Nitrogen_Pressure[i] - Surface_Inspired_N2_Pressure);
        }

        else {
            dive->Surface_Phase_Volume_Time[i] = 0.0;
        }
    }
}

void vpmb_critical_volume(dive_state *dive, double Deco_Phase_Volume_Time)
{

    int i;

    double Parameter_Lambda_Pascals = (dive->Crit_Volume_Parameter_Lambda / 33.0) * ATM;
    double B, C;

    for(i = 0; i < Buhlmann_Compartments; i++) {
        double Phase_Volume_Time = Deco_Phase_Volume_Time +  dive->Surface_Phase_Volume_Time[i];
        double Adj_Crush_Pressure_He_Pascals = (dive->Adjusted_Crushing_Pressure_He[i] / dive->Units_Factor) * ATM;

        double Initial_Allowable_Grad_He_Pa = (dive->Initial_Allowable_Gradient_He[i] / dive->Units_Factor) * ATM;

        B = Initial_Allowable_Grad_He_Pa + (Parameter_Lambda_Pascals * dive->Surface_Tension_Gamma) / (dive->Skin_Compression_GammaC * Phase_Volume_Time);

        C = (dive->Surface_Tension_Gamma * (dive->Surface_Tension_Gamma * (Parameter_Lambda_Pascals * Adj_Crush_Pressure_He_Pascals))) / (dive->Skin_Compression_GammaC * (dive->Skin_Compression_GammaC * Phase_Volume_Time));

        double New_Allowable_Grad_He_Pascals = (B + sqrt(pow(B, 2) - 4.0 * C)) / 2.0;

        dive->Allowable_Gradient_He[i] = (New_Allowable_Grad_He_Pascals / ATM) * dive->Units_Factor;


        double Adj_Crush_Pressure_N2_Pascals = (dive->Adjusted_Crushing_Pressure_N2[i] / dive->Units_Factor) * ATM;

        double Initial_Allowable_Grad_N2_Pa = (dive->Initial_Allowable_Gradient_N2[i] / dive->Units_Factor) * ATM;

        B = Initial_Allowable_Grad_N2_Pa + (Parameter_Lambda_Pascals * dive->Surface_Tension_Gamma) / (dive->Skin_Compression_GammaC * Phase_Volume_Time);

        C = (dive->Surface_Tension_Gamma * (dive->Surface_Tension_Gamma * (Parameter_Lambda_Pascals * Adj_Crush_Pressure_N2_Pascals))) / (dive->Skin_Compression_GammaC * (dive->Skin_Compression_GammaC * Phase_Volume_Time));

        double New_Allowable_Grad_N2_Pascals = (B + sqrt(pow(B, 2) - 4.0 * C)) / 2.0;

        dive->Allowable_Gradient_N2[i] = (New_Allowable_Grad_N2_Pascals / ATM) * dive->Units_Factor;
    }
}

int vpmb_calc_start_of_deco_zone(dive_state *dive, double Starting_Depth, double Rate)
{
    int i, j;

    dive->Depth_Start_of_Deco_Zone = 0.0;
    double Starting_Ambient_Pressure = Starting_Depth + dive->Barometric_Pressure;

    double Initial_Inspired_He_Pressure = (Starting_Ambient_Pressure - dive->Water_Vapor_Pressure - (dive->PpO2*dive->Units_Factor)) * dive->Fraction_Helium[dive->Mix_Number - 1];

    double Initial_Inspired_N2_Pressure = (Starting_Ambient_Pressure - dive->Water_Vapor_Pressure - (dive->PpO2*dive->Units_Factor)) * dive->Fraction_Nitrogen[dive->Mix_Number - 1];

    double Helium_Rate = Rate * dive->Fraction_Helium[dive->Mix_Number - 1];
    double Nitrogen_Rate = Rate * dive->Fraction_Nitrogen[dive->Mix_Number - 1];

    double Low_Bound = 0.0;
    double High_Bound = -1.0 * (Starting_Ambient_Pressure / Rate);

    for(i = 0; i < Buhlmann_Compartments; i++) {
        double Initial_Helium_Pressure = dive->Helium_Pressure[i];
        double Initial_Nitrogen_Pressure = dive->Nitrogen_Pressure[i];

        double Function_at_Low_Bound = Initial_Helium_Pressure + Initial_Nitrogen_Pressure + dive->Constant_Pressure_Other_Gases - Starting_Ambient_Pressure;

        double High_Bound_Helium_Pressure = vpmb_schreiner_equation(Initial_Inspired_He_Pressure, Helium_Rate, High_Bound, dive->Helium_Time_Constant[i], Initial_Helium_Pressure);

        double High_Bound_Nitrogen_Pressure = vpmb_schreiner_equation(Initial_Inspired_N2_Pressure, Nitrogen_Rate, High_Bound, dive->Nitrogen_Time_Constant[i], Initial_Nitrogen_Pressure);

        double Function_at_High_Bound = High_Bound_Helium_Pressure + High_Bound_Nitrogen_Pressure + dive->Constant_Pressure_Other_Gases;

        if((Function_at_High_Bound * Function_at_Low_Bound) >= 0.0) {
            return ROOTERROR;
        }

        double Time_to_Start_of_Deco_Zone, Differential_Change;
        if (Function_at_Low_Bound < 0.0) {
            Time_to_Start_of_Deco_Zone = Low_Bound;
            Differential_Change = High_Bound - Low_Bound;
        } else {
            Time_to_Start_of_Deco_Zone = High_Bound;
            Differential_Change = Low_Bound - High_Bound;
        }
        double Last_Diff_Change;
        for(j = 0; j < 100; j++) {
            Last_Diff_Change = Differential_Change;
            Differential_Change = Last_Diff_Change * 0.5;

            double Mid_Range_Time = Time_to_Start_of_Deco_Zone + Differential_Change;

            double Mid_Range_Helium_Pressure = vpmb_schreiner_equation(Initial_Inspired_He_Pressure, Helium_Rate, Mid_Range_Time, dive->Helium_Time_Constant[i], Initial_Helium_Pressure);

            double Mid_Range_Nitrogen_Pressure = vpmb_schreiner_equation(Initial_Inspired_N2_Pressure, Nitrogen_Rate, Mid_Range_Time, dive->Nitrogen_Time_Constant[i], Initial_Nitrogen_Pressure);

            double Function_at_Mid_Range = Mid_Range_Helium_Pressure + Mid_Range_Nitrogen_Pressure + dive->Constant_Pressure_Other_Gases - (Starting_Ambient_Pressure + Rate * Mid_Range_Time);

            if (Function_at_Mid_Range <= 0.0) {
                Time_to_Start_of_Deco_Zone = Mid_Range_Time;
            }

            if ((fabs(Differential_Change) < 1.0E-3) || (Function_at_Mid_Range == 0.0)) {
                break;
            }

            if(j == 100) {
                return ROOTERROR;
            }
        }

        double Cpt_Depth_Start_of_Deco_Zone = (Starting_Ambient_Pressure + Rate * Time_to_Start_of_Deco_Zone) - dive->Barometric_Pressure;

        dive->Depth_Start_of_Deco_Zone = max(dive->Depth_Start_of_Deco_Zone, Cpt_Depth_Start_of_Deco_Zone);
    }
    return ALLGOOD;
}

void vpmb_calc_initial_allowable_gradient(dive_state *dive)
{
    int i;
    for(i = 0; i < Buhlmann_Compartments; i++) {
        double Initial_Allowable_Grad_N2_Pa = ((2.0 * dive->Surface_Tension_Gamma * (dive->Skin_Compression_GammaC - dive->Surface_Tension_Gamma)) / (dive->Regenerated_Radius_N2[i] * dive->Skin_Compression_GammaC));

        double Initial_Allowable_Grad_He_Pa = ((2.0 * dive->Surface_Tension_Gamma * (dive->Skin_Compression_GammaC - dive->Surface_Tension_Gamma)) / (dive->Regenerated_Radius_He[i] * dive->Skin_Compression_GammaC));

        dive->Initial_Allowable_Gradient_N2[i] = (Initial_Allowable_Grad_N2_Pa / ATM) * dive->Units_Factor;
        dive->Initial_Allowable_Gradient_He[i] = (Initial_Allowable_Grad_He_Pa / ATM) * dive->Units_Factor;

        dive->Allowable_Gradient_He[i] = dive->Initial_Allowable_Gradient_He[i];
        dive->Allowable_Gradient_N2[i] = dive->Initial_Allowable_Gradient_N2[i];
    }
}


void vpmb_gas_loadings_surface_interval(dive_state *dive, double Surface_Interval_Time)
{
    int i;
    double Inspired_Helium_Pressure = 0.0;
    double Inspired_Nitrogen_Pressure = (dive->Barometric_Pressure - dive->Water_Vapor_Pressure) * fraction_inert_gas;

    for (i = 0; i < Buhlmann_Compartments; i++) {
        double Temp_Helium_Pressure = dive->Helium_Pressure[i];
        double Temp_Nitrogen_Pressure = dive->Nitrogen_Pressure[i];

        dive->Helium_Pressure[i] = vpmb_haldane_equation(Temp_Helium_Pressure, Inspired_Helium_Pressure, dive->Helium_Time_Constant[i], Surface_Interval_Time);

        dive->Nitrogen_Pressure[i] = vpmb_haldane_equation(Temp_Nitrogen_Pressure, Inspired_Nitrogen_Pressure, dive->Nitrogen_Time_Constant[i], Surface_Interval_Time);
    }
}

double vpmb_new_critical_radius(dive_state *dive, double Max_Actual_Gradient_Pascals, double Adj_Crush_Pressure_Pascals)
{
    return ((2.0 * dive->Surface_Tension_Gamma * (dive->Skin_Compression_GammaC - dive->Surface_Tension_Gamma))) / (Max_Actual_Gradient_Pascals * dive->Skin_Compression_GammaC - dive->Surface_Tension_Gamma * Adj_Crush_Pressure_Pascals);
}

void vpmb_vpm_repetitive_algorithm(dive_state *dive, double Surface_Interval_Time)
{
    int i;
    for (i = 0; i < Buhlmann_Compartments; i++) {
        double Max_Actual_Gradient_Pascals = (dive->Max_Actual_Gradient[i] / dive->Units_Factor) * ATM;

        double Adj_Crush_Pressure_He_Pascals = (dive->Adjusted_Crushing_Pressure_He[i] / dive->Units_Factor) * ATM;
        double Adj_Crush_Pressure_N2_Pascals = (dive->Adjusted_Crushing_Pressure_N2[i] / dive->Units_Factor) * ATM;

        if (dive->Max_Actual_Gradient[i] > dive->Initial_Allowable_Gradient_N2[i]) {
            double New_Critical_Radius_N2 = vpmb_new_critical_radius(dive, Max_Actual_Gradient_Pascals, Adj_Crush_Pressure_N2_Pascals);

            dive->Adjusted_Critical_Radius_N2[i] = dive->Initial_Critical_Radius_N2[i] + (dive->Initial_Critical_Radius_N2[i] - New_Critical_Radius_N2) * exp(-Surface_Interval_Time / dive->Regeneration_Time_Constant);
        }

        else {
            dive->Adjusted_Critical_Radius_N2[i] = dive->Initial_Critical_Radius_N2[i];
        }

        if( dive->Max_Actual_Gradient[i] > dive->Initial_Allowable_Gradient_He[i]) {
            double New_Critical_Radius_He = vpmb_new_critical_radius(dive, Max_Actual_Gradient_Pascals, Adj_Crush_Pressure_He_Pascals);

            dive->Adjusted_Critical_Radius_He[i] = dive->Initial_Critical_Radius_He[i] + (dive->Initial_Critical_Radius_He[i] - New_Critical_Radius_He) * exp(-Surface_Interval_Time / dive->Regeneration_Time_Constant);
        } else {
            dive->Adjusted_Critical_Radius_He[i] = dive->Initial_Critical_Radius_He[i];
        }

    }
}

//DIRK this can fail hard
bool vpmb_deco_stop_loop_block_within_critical_volume_loop(dive_state *dive)
{
    int i;
    while(TRUE) {
        vpmb_gas_loadings_ascent_descent(dive, dive->Starting_Depth, dive->Deco_Stop_Depth, dive->Rate);
				YIELD();
        if(dive->Deco_Stop_Depth <= 0.0) {
            break;
        }

        if(dive->Number_of_Changes > 1) {
            for (i = 1; i < dive->Number_of_Changes; i++) {
                if (dive->Depth_Change[i] >= dive->Deco_Stop_Depth) {
                    dive->Mix_Number = dive->Mix_Change[i];
                    dive->Rate = dive->Rate_Change[i];
                    dive->Step_Size = dive->Step_Size_Change[i];
                }
            }
        }
        bool res = vpmb_boyles_law_compensation(dive, dive->First_Stop_Depth, dive->Deco_Stop_Depth, dive->Step_Size);
        if (!res) return FALSE;
				vpmb_decompression_stop(dive, dive->Deco_Stop_Depth, dive->Step_Size);
				YIELD();

        dive->Starting_Depth = dive->Deco_Stop_Depth;
        dive->Next_Stop = dive->Deco_Stop_Depth - dive->Step_Size;
        dive->Deco_Stop_Depth = dive->Next_Stop;
        dive->Last_Run_Time = dive->Run_Time;
  }
	return TRUE;
}

void add_decomp_stop(dive_state *dive, double time, double depth, direction dir)
{
    if(dive->Real_Time_Decompression == TRUE) {
			if(dive->decomp_stop_index < dive->ndecomp_stops) {
				/* printf("WOOOOOT %d", dive->decomp_stop_index); */
				dive->decomp_stops[dive->decomp_stop_index].time = time;
				dive->decomp_stops[dive->decomp_stop_index].depth = depth;
				dive->decomp_stops[dive->decomp_stop_index].ascent_or_const = dir;
				dive->decomp_stop_index++;
			} else {
				ERR_PRINT("YOWZA- no space for stop %d avail %d\n", dive->decomp_stop_index, dive->ndecomp_stops);
			}

    }
}
//DIRK this can fail hard

bool vpmb_critical_volume_decision_tree(dive_state *dive, double stop_depth, bool addstops)
{
    int i;

    for(i = 0; i < Buhlmann_Compartments; i++) {
        dive->Helium_Pressure[i] = dive->He_Pressure_Start_of_Ascent[i];
        dive->Nitrogen_Pressure[i] = dive->N2_Pressure_Start_of_Ascent[i];
    }
    dive->Run_Time = dive->Run_Time_Start_of_Ascent;
    dive->Segment_Number = dive->Segment_Number_Start_of_Ascent;
    dive->Starting_Depth = dive->Depth_Change[0];
    dive->Mix_Number = dive->Mix_Change[0];
    dive->Rate = dive->Rate_Change[0];
    dive->Step_Size = dive->Step_Size_Change[0];
    dive->Deco_Stop_Depth = dive->First_Stop_Depth;
    dive->Last_Run_Time = 0.0;

		if (addstops)    vpmb_init_decomp_stop_table(dive);
    while(TRUE) {
        vpmb_gas_loadings_ascent_descent(dive, dive->Starting_Depth, dive->Deco_Stop_Depth, dive->Rate);
				YIELD();
        vpmb_calc_max_actual_gradient(dive, dive->Deco_Stop_Depth);
				YIELD();
        /* dive->output_object.add_decompression_profile_ascent(dive->Segment_Number, dive->Segment_Time, dive->Run_Time, dive->Mix_Number, dive->Deco_Stop_Depth, dive->Rate) */
        /* dive->decomp_stops[dive->decomp_stop_index].time = dive->Run_Time; */
        /* dive->decomp_stops[dive->decomp_stop_index].depth = dive->dive->Deco_Stop_Depth; */
        /* dive->decomp_stop_index++; */
        if (addstops) add_decomp_stop(dive, dive->Run_Time, dive->Deco_Stop_Depth, ASCENT);
        if (dive->Deco_Stop_Depth <= stop_depth) {
            break;
        }

        if( dive->Number_of_Changes > 1) {
            for(i = 1; i <  dive->Number_of_Changes; i++) {
                if(dive->Depth_Change[i] >= dive->Deco_Stop_Depth) {
                    dive->Mix_Number = dive->Mix_Change[i];
                    dive->Rate = dive->Rate_Change[i];
                    dive->Step_Size = dive->Step_Size_Change[i];
                }
            }
        }

        bool res = vpmb_boyles_law_compensation(dive, dive->First_Stop_Depth, dive->Deco_Stop_Depth, dive->Step_Size);
				if (!res) return FALSE;
        vpmb_decompression_stop(dive, dive->Deco_Stop_Depth, dive->Step_Size);
				YIELD();
        if ( dive->Last_Run_Time == 0.0) {
            dive->Stop_Time = round((dive->Segment_Time / dive->Minimum_Deco_Stop_Time) + 0.5) * dive->Minimum_Deco_Stop_Time;
        } else {
            dive->Stop_Time = dive->Run_Time - dive->Last_Run_Time;
        }

        if (trunc(dive->Minimum_Deco_Stop_Time) == dive->Minimum_Deco_Stop_Time) {
            /* dive->output_object.add_decompression_profile_constant(dive->Segment_Number, dive->Segment_Time, dive->Run_Time, dive->Mix_Number, int(dive->Deco_Stop_Depth), int(dive->Stop_Time)) */
            /* dive->decomp_stops[dive->decomp_stop_index].time = dive->Run_Time; */
            /* dive->decomp_stops[dive->decomp_stop_index].depth = dive->dive->Deco_Stop_Depth; */
            /* dive->decomp_stop_index++; */
            if (addstops) add_decomp_stop(dive, dive->Run_Time, dive->Deco_Stop_Depth, CONSTANT);

        } else {
            /* dive->output_object.add_decompression_profile_constant(dive->Segment_Number, dive->Segment_Time, dive->Run_Time, dive->Mix_Number, dive->Deco_Stop_Depth, dive->Stop_Time) */
            /* dive->decomp_stops[dive->decomp_stop_index].time = dive->Run_Time; */
            /* dive->decomp_stops[dive->decomp_stop_index].depth = dive->dive->Deco_Stop_Depth; */
            /* dive->decomp_stop_index++; */
            if (addstops) add_decomp_stop(dive, dive->Run_Time, dive->Deco_Stop_Depth, CONSTANT);

        }
        dive->Starting_Depth = dive->Deco_Stop_Depth;
        dive->Next_Stop = dive->Deco_Stop_Depth - dive->Step_Size;
        dive->Deco_Stop_Depth = dive->Next_Stop;
        dive->Last_Run_Time = dive->Run_Time;
  }
	return TRUE;
}

void vpmb_init_decomp_stop_table(dive_state * dive)
{
		if(dive->Real_Time_Decompression == TRUE) {
			int size = (ceil(dive->First_Stop_Depth / dive->Step_Size) + 1) * 2;
			if (dive->decomp_stops) {
				DBG_PRINT("Aargh- have stops\n");
				free(dive->decomp_stops);
			}
			dive->decomp_stops = calloc(((size/8)+1)*8, sizeof(decompression_stops));
			if (!dive->decomp_stops) {	
				lprintf("M,%s,no memory\n", __func__);

	  		ERR_PRINT("Aaargh- no memory, need %d have%d\n", 
	  		(((size/8)+1)*8)*sizeof (decompression_stops), getFreeHeap());	
				size = 0;
	    }
			dive->ndecomp_stops = size;
			for (int s = 0; s < size; s++) {
				dive->decomp_stops[s].time = 0.0;
				dive->decomp_stops[s].depth = 0.0;
				dive->decomp_stops[s].ascent_or_const = ERROR;
				
			}
			DBG_PRINT ("callocstops %d\n", size);
			dive->decomp_stop_index = 0;
		}	
}
//DIRK fix fail hard baddecostop response?

int vpmb_critical_volume_loop(dive_state *dive, bool init)
{
    int i;
    while(TRUE) {
		//TRC_PRINT("meeple ");
        vpmb_calc_ascent_ceiling(dive);
		//TRC_PRINT("meeple2 ");

        if(dive->Ascent_Ceiling_Depth <= 0.0) {
            dive->Deco_Stop_Depth = 0.0;
        } else {
            double Rounding_Operation2 = (dive->Ascent_Ceiling_Depth / dive->Step_Size) + 0.5;
            dive->Deco_Stop_Depth = round(Rounding_Operation2) * dive->Step_Size;
        }
        if( dive->Deco_Stop_Depth > dive->Depth_Start_of_Deco_Zone) {
					ERR_PRINT("baddeco1\n");

            return BADDECOSTOP;
        }
		YIELD();
		TRC_PRINT("meeple3 ");

        vpmb_projected_ascent(dive, dive->Depth_Start_of_Deco_Zone, dive->Rate, dive->Step_Size);

        if (dive->Deco_Stop_Depth > dive->Depth_Start_of_Deco_Zone) {
			ERR_PRINT("baddeco2\n");

            return BADDECOSTOP;
        }
		TRC_PRINT("meeple4 ");

        if(dive->Deco_Stop_Depth == 0.0) {
            for(i = 0; i < Buhlmann_Compartments; i++) {
                dive->Helium_Pressure[i] = dive->He_Pressure_Start_of_Ascent[i];
                dive->Nitrogen_Pressure[i] = dive->N2_Pressure_Start_of_Ascent[i];
            }
            dive->Run_Time = dive-> Run_Time_Start_of_Ascent;
            dive->Segment_Number = dive->Segment_Number_Start_of_Ascent;
            dive->Starting_Depth = dive->Depth_Change[0];
            dive->Ending_Depth = 0.0;
            vpmb_gas_loadings_ascent_descent(dive, dive->Starting_Depth, dive->Ending_Depth, dive->Rate);

            /* self->output_object.add_decompression_profile_ascent(dive->Segment_Number, dive->Segment_Time, dive->Run_Time, dive->Mix_Number, dive->Deco_Stop_Depth, dive->Rate); */
						TRC_PRINT("meeple6 ");
						vpmb_init_decomp_stop_table(dive);
			
						add_decomp_stop(dive, dive->Run_Time, dive->Deco_Stop_Depth, ASCENT);
			TRC_PRINT("meeple7 ");

            break;
        }
		TRC_PRINT("meeple8 ");

        dive->Starting_Depth = dive->Depth_Start_of_Deco_Zone;
        dive->First_Stop_Depth = dive->Deco_Stop_Depth;
        bool res = vpmb_deco_stop_loop_block_within_critical_volume_loop(dive);
				if (!res){		
					ERR_PRINT("baddeco3\n");
					return BADDECOSTOP;
				}
				YIELD();

        dive->Deco_Phase_Volume_Time = dive->Run_Time - dive->Run_Time_Start_of_Deco_Zone;

        vpmb_calc_surface_phase_volume_time(dive);

        for(i = 0; i < Buhlmann_Compartments; i++) {
            dive->Phase_Volume_Time[i] = dive->Deco_Phase_Volume_Time + dive->Surface_Phase_Volume_Time[i];
            dive->Critical_Volume_Comparison = fabs(dive->Phase_Volume_Time[i] - dive->Last_Phase_Volume_Time[i]);
            if(dive->Critical_Volume_Comparison <= 1.0) {
                dive->Schedule_Converged = TRUE;
            }
        }
				YIELD();
		
				TRC_PRINT("meeple9 ");

        if ((dive->Schedule_Converged == TRUE) || (dive->Critical_Volume_Algorithm_Off == TRUE)) {
					if (init) return ALLGOOD;
					TRC_PRINT("meeple10 ");
		
		      bool res = vpmb_critical_volume_decision_tree(dive, 0.0, true);
					if (!res) {
						ERR_PRINT("baddeco4\n");
						return BADDECOSTOP;
					}
        }
        else {
					TRC_PRINT("meeple11 ");
	
	        vpmb_critical_volume(dive, dive->Deco_Phase_Volume_Time);
					TRC_PRINT("meeple12 ");
	
	        dive->Deco_Phase_Volume_Time = 0.0;
	        dive->Run_Time = dive->Run_Time_Start_of_Deco_Zone;
	        dive->Starting_Depth = dive->Depth_Start_of_Deco_Zone;
	        dive->Mix_Number = dive->Mix_Change[0];
	        dive->Rate = dive->Rate_Change[0];
	        dive->Step_Size = dive->Step_Size_Change[0];
	
	        for(i = 0; i < Buhlmann_Compartments; i++) {
	            dive->Last_Phase_Volume_Time[i] = dive->Phase_Volume_Time[i];
	            dive->Helium_Pressure[i] = dive->He_Pressure_Start_of_Deco_Zone[i];
	            dive->Nitrogen_Pressure[i] = dive->N2_Pressure_Start_of_Deco_Zone[i];
	        }
					YIELD();
					TRC_PRINT("meeple13 ");
			
	        continue;
        }
        break;
    }
	//TRC_PRINT("meeple99 ");

    return ALLGOOD;
}




direction vpmb_current_direction(dive_state *dive, double increment_time)
{
    double high = dive->Last_Direction_Depth + (meters_per_minute_change * increment_time), low = dive->Last_Direction_Depth - (meters_per_minute_change * increment_time);

    /* double rate = (dive->Depth - dive->Last_Direction_Depth ) / increment_time; */
    if((dive->Depth <= high) && (dive->Depth >= low)) {
        dive->Last_Direction_Depth = dive->Depth;
        return CONSTANT;
    } else if(dive->Depth > dive->Last_Direction_Depth) {
        dive->Last_Direction_Depth = dive->Depth;
        return DESCENT;
    } else if(dive->Depth < dive->Last_Direction_Depth) {
        dive->Last_Direction_Depth = dive->Depth;
        return ASCENT;
    }

    return ERROR;
}

/* int msw_driver(dive_state *dive){ */

/*         if(dive->Run_Time >= 30 && dive->Depth > 0){ */
/*                 return 99; */
/*         } */
/*         else if(dive->Depth < 80.0) */
/*                 return 1; */
/*         else if (dive->Run_Time < 30){ */
/*                 return 2; */
/*         } */
/*         return 100; */
/* } */






void vpmb_free_dive_state(dive_state *dive)
{
    free(dive->Fraction_Helium);
    free(dive->Fraction_Nitrogen);
    free(dive->SetPoints);

    if(dive->decomp_stops != NULL) {
        free(dive->decomp_stops);
    }
    free(dive);
}


