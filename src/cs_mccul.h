/* N-API interface for Measurement Computing Universal Library

   N-API callable functions that directly correspond to UL functions, 
   and additional wrapper functions

   Copyright (c) 2026 Control Solutions LLC. All rights reserved.
*/


#ifndef _CS_MCCUL_H_
#define _CS_MCCUL_H_

#include <node_api.h> // provide N-API types

//#define MCCUL_DEBUG
// Uncomment to enable debugging messages, which may affect timing or
// performance


// ----------------------------------- // -----------------------------------
// Definitions

#define GETDAQDEVICEINVENTORY_ARGC   (1)

napi_value Init(napi_env env, napi_value exports);
napi_value mccul_getDaqDeviceInventory(napi_env env, napi_callback_info info);

#endif // _CS_MCCUL_H_
