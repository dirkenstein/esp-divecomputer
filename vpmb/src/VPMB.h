/* Copyright 2010, Bryan Waite, Erik C. Baker. All rights reserved.
 * Arduino adaptation Copyright 2018 Dirk Niggemann
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
 * THIS SOFTWARE IS PROVIDED BY Bryan Waite, Erik C. Baker, Dirk Niggemann ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED 
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. 
 * IN NO EVENT SHALL Bryan Waite, or Erik C. Baker, or Dirk Niggemann OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * The views and conclusions contained in the software and documentation are those of the
 * authors and should not be interpreted as representing official policies, either expressed
 * or implied, of Bryan Waite, or Erik C. Baker, or Dirk Niggemann.
 *
 */

#ifndef VPM_B_hh
#define VPM_B_hh
#include <Arduino.h>
//typedef BOOL bool;

#include "FS.h"
#include "ArduinoJson.h"
#include "clib/vpmb.h"
#include "clib/vpmb-data.h"
#include "clib/vpmb-data-repetitive.h"
#include "clib/vpmb-data-realtime.h"
#include "clib/vpmb-json.h"


class VPMB
{
public:
	VPMB(void);
	VPMB(JsonObject &json);
	VPMB(JsonObject &json,double(*depthf)(void));
	VPMB(double(*depthf)(void));
	VPMB(double(*depthf)(void), double(*po2f)(void));
	VPMB(JsonObject &json,double(*depthf)(void), double(*po2f)(void));

	~VPMB();
	//
	bool loadconfig (JsonObject &json);
	bool saveconfig (JsonObject &json);

	bool loaddives(JsonObject &json);

	bool validate();
	void real_time_dive(void);
	void repetitive_dive_loop(void);

	void output_dive_state(void);
		void output_dive_state_short(void);

	bool real_time_dive_loop(void);
	void setPlannedDepth(double);
	double getPlannedDepth(void);
	bool getDiveStarted(void);
	double realtime_find_deepest_stop(void);

	int getDecoStops(void);
	int getActualDecoStops(void); //No ascents 

	double getNextStopDepth (float depth);
	double getNextStopTime (float depth);
	int getNextStopCount (float depth);
	int   getNextStopNum (float depth); 

	double getDiveTime(void);
	
	double getNDTime();
		double getOTU();
		double getCNS();

	gases * getGasList();
	int getGasListLength();
	void gasChange();
	int getNumberOfTestDiveProfiles();
	dive_profile * getTestDiveProfiles();
private:
	 void calc_nextstop(float depth);
	 void decostopcalc(void);
	 bool updating = false;
    dive_state *dive;
    vpmb_input input;
	double recent_max_depth = 0.0;
	unsigned long last_depth_window = 0;
	double(*depthfunc)(void) = NULL;
	double(*po2func)(void) = NULL;

	unsigned long dive_start_millis = 0;
	unsigned long dive_last_millis =0;

	unsigned long dive_intvl_millis=0;
	void output_array_double(char *name, double arr[], int len);
	void output_array_double_short(char *name, double arr[], int len);

	void output_array_int(char *name, int arr[], int len);
	void output_deco_stops(void);
	const unsigned char depth_window = 300000; // 5 mins
	const double trigger_depth = 1.5;
	bool dive_started = false;
	double last_depth = 0.0;
	double planned_depth = 0.0;
	void initDiveState(void);
	double last_nextstopdepth = 0.0;
	double last_nextstoptime = 0.0;
	int last_nextstopidx =0;
  int last_nextstopnum =0;

	int last_nextstopcount = 0;
	int nlast_decostops = 0;
	int nlast_decocount = 0; //Actual stops w/o ascents
	int loop_call_count = 0;
	double nd_time = 0.0;
	double next_stop = 0.0;
	double deepest_stop = 0.0;
};

//extern SyslogClass Syslog;

#endif