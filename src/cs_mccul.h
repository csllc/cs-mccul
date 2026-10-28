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
#define CREATEDAQDEVICE_ARGC         (1)
#define GETDEVINFOHASAI_ARGC         (1)
#define CONNECTDAQDEVICE_ARGC        (1)
#define GETINFO_ARGC                 (3) // all *GetInfo functions
#define AIN_ARGC                     (5)
#define DCONFIGPORT_ARGC             (3)
#define DIN_ARGC                     (2)
#define AINSCAN_ARGC                 (10)
#define DINSCAN_ARGC                 (8)
#define COUNTER_ARGC                 (2) // CIn, CClear
#define CINSCAN_ARGC                 (8)
#define DAQINSCAN_ARGC               (7)

napi_value Init(napi_env env, napi_value exports);
napi_value mccul_getDaqDeviceInventory(napi_env env, napi_callback_info info);
napi_value mccul_createDaqDevice(napi_env env, napi_callback_info info);
napi_value mccul_getDevInfoHasAi(napi_env env, napi_callback_info info);
napi_value mccul_connectDaqDevice(napi_env env, napi_callback_info info);
napi_value mccul_disconnectDaqDevice(napi_env env, napi_callback_info info);
napi_value mccul_releaseDaqDevice(napi_env env, napi_callback_info info);
napi_value mccul_aiGetInfo(napi_env env, napi_callback_info info);
napi_value mccul_aIn(napi_env env, napi_callback_info info);
napi_value mccul_devGetInfo(napi_env env, napi_callback_info info);
napi_value mccul_dioGetInfo(napi_env env, napi_callback_info info);
napi_value mccul_dConfigPort(napi_env env, napi_callback_info info);
napi_value mccul_dIn(napi_env env, napi_callback_info info);
napi_value mccul_aInScan(napi_env env, napi_callback_info info);
napi_value mccul_aInScanStatus(napi_env env, napi_callback_info info);
napi_value mccul_aInScanStop(napi_env env, napi_callback_info info);
napi_value mccul_dInScan(napi_env env, napi_callback_info info);
napi_value mccul_dInScanStatus(napi_env env, napi_callback_info info);
napi_value mccul_dInScanStop(napi_env env, napi_callback_info info);
napi_value mccul_ctrGetInfo(napi_env env, napi_callback_info info);
napi_value mccul_cIn(napi_env env, napi_callback_info info);
napi_value mccul_cClear(napi_env env, napi_callback_info info);
napi_value mccul_cInScan(napi_env env, napi_callback_info info);
napi_value mccul_cInScanStatus(napi_env env, napi_callback_info info);
napi_value mccul_cInScanStop(napi_env env, napi_callback_info info);
napi_value mccul_daqIGetInfo(napi_env env, napi_callback_info info);
napi_value mccul_daqInScan(napi_env env, napi_callback_info info);
napi_value mccul_daqInScanStatus(napi_env env, napi_callback_info info);
napi_value mccul_daqInScanStop(napi_env env, napi_callback_info info);

#endif // _CS_MCCUL_H_
