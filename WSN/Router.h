﻿
/****************************************************************/
#ifndef ROUTER_H
#define ROUTER_H

#include "common.h"
#include "ZComDef.h"

extern void LEMApp_Init( byte task_id );
extern UINT16 LEMApp_ProcessEvent( byte task_id, UINT16 events );

#endif