#ifndef FREQMETER_H
#define FREQMETER_H

extern void FREQMETER_init(void);
extern float FREQMETER_getFrequency(void); /* Hz, last valid reading, 0 until first one */

#endif
