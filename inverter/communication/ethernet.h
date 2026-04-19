#ifndef ETHERNET_H
#define ETHERNET_H

#include <stdint.h>

#define ETHERNET_CONTROL_TCP_PORT 3030U

void Ethernet_Init(void);
void Ethernet_Process(void);

#endif
