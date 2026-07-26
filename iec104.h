#ifndef IEC104_H
#define IEC104_H

extern void IEC104_init(void);
extern void IEC104_process(void);
extern void IEC104_reportLedState(bool on);
extern void IEC104_setVoltageEnabled(bool en);
extern bool IEC104_isVoltageEnabled(void);
extern float IEC104_getVoltage(void);

#endif
