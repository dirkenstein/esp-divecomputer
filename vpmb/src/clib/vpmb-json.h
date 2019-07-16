/*
 *  vpm-json.h
 *  
 *
 *  Created by Dirk Niggemann on 03/02/2018.
 *  Copyright 2018 __MyCompanyName__. All rights reserved.
 *
 */

#ifndef __VPMB_JSON__
#define __VPMB_JSON__



#include "vpmb-data.h"
#include <ArduinoJSON.h>
bool vpmb_load_dives_from_json(vpmb_input *in, JsonObject &json);
bool vpmb_load_config_from_json(vpmb_input *in, JsonObject &json);
bool vpmb_save_config_to_json(vpmb_input *in, JsonObject &json);

#endif