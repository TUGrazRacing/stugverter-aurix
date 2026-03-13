#include "Ifx_Types.h"

void PI_Init(PI_Controller *pi, float32 kp, float32 ki, float32 min, float32 max);
static inline float32 PI_Run(PI_Controller *pi, float32 reference, float32 measurement);