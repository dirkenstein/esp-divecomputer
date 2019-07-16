#ifndef UTILS_H
#define UTILS_H

#include <Arduino.h>
#include <math.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

/*==========================================*/
/* C++ compatible */

#ifdef __cplusplus
extern "C" {
#endif

#ifndef min
double min(double i, double j);
#endif
#ifndef max
double max(double i, double j);
#endif
void lowercase_string(char *str);

	
	
/*==========================================*/
/* C++ compatible */
	
#ifdef __cplusplus
}
#endif

	
#endif
