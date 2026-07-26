#ifndef PINCTRL_H
#define PINCTRL_H

extern void PINCTRL_init(void);
extern void PINCTRL_setLed(bool on);
extern bool PINCTRL_getLed(void);

#endif
