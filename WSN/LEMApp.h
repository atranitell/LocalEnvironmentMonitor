
/****************************************************************/
#ifndef LEMAPP_H
#define LEMAPP_H

#include "common.h"
#include "ZComDef.h"

extern void LEMAPP_Init( byte task_id );
extern UINT16 LEMAPP_ProcessEvent( byte task_id, UINT16 events );

#endif