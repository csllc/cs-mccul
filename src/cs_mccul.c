/* N-API interface for Measurement Computing Universal Library

   N-API callable functions that directly correspond to UL functions, 
   and additional wrapper functions

   Copyright (c) 2026 Control Solutions LLC. All rights reserved.
*/
   

#include <assert.h>      // provide assert
#include <node_api.h>    // provide N-API types, constants, and functions
#include <stdio.h>       // provide printf
#include <stdint.h>      // provide uintX_t
#include <string.h>      // provide strnlen

/* #include <windows.h>     // provide Win32 types, constants, and functions */

#include <uldaq.h>

#include "cs_mccul.h"
#include "cs_mccul_helper.h"
#include "napi_helper.h"      // provide DECLARE_NAPI_METHOD

// ----------------------------------- // -----------------------------------
// Definitions

// Length of N-API module descriptor list, used in Init
enum { descriptors_len = 1 };

// From UL examples
#define MAX_DEV_COUNT  100
#define MAX_STR_LENGTH 64

// ----------------------------------- // -----------------------------------
// Global variables

// ----------------------------------- // -----------------------------------
// Local variables

// ----------------------------------- // -----------------------------------
// Local functions

// Register N-API module
NAPI_MODULE(NODE_GYP_MODULE_NAME, Init);

napi_value Init(napi_env env, napi_value exports)
{
    napi_status status = napi_generic_failure;

    // Turn off buffering on stdout
    setvbuf(stdout, NULL, _IONBF, 0);

    // Build array of property descriptors
    // If modifying this list, remember to update descriptors_len, above 
    napi_property_descriptor descriptors[descriptors_len] = {
        // CANlib functions
        DECLARE_NAPI_METHOD("GetDaqDeviceInventory", mccul_getDaqDeviceInventory),
        // DECLARE_NAPI_METHOD("OpenChannel", canlib_canOpenChannel),
        // DECLARE_NAPI_METHOD("Close", canlib_canClose),
        // DECLARE_NAPI_METHOD("BusOn", canlib_canBusOn),
        // DECLARE_NAPI_METHOD("BusOff", canlib_canBusOff),
        // DECLARE_NAPI_METHOD("TranslateBaud", canlib_canTranslateBaud),
        // DECLARE_NAPI_METHOD("SetBusParams", canlib_canSetBusParams),
        // DECLARE_NAPI_METHOD("GetBusParams", canlib_canGetBusParams),
        // DECLARE_NAPI_METHOD("Write", canlib_canWrite),
        // DECLARE_NAPI_METHOD("WriteSync", canlib_canWriteSync),
        // DECLARE_NAPI_METHOD("Read", canlib_canRead),
        // DECLARE_NAPI_METHOD("ReadWait", canlib_canReadWait),
        // DECLARE_NAPI_METHOD("SetAcceptanceFilter", canlib_canSetAcceptanceFilter),
        // DECLARE_NAPI_METHOD("FlushTransmitQueue", canlib_canFlushTransmitQueue),
        // DECLARE_NAPI_METHOD("FlushReceiveQueue", canlib_canFlushReceiveQueue),
        // DECLARE_NAPI_METHOD("ReadStatus", canlib_canReadStatus),
        // DECLARE_NAPI_METHOD("ResetBus", canlib_canResetBus),
        // DECLARE_NAPI_METHOD("GetErrorText", canlib_canGetErrorText),
        // DECLARE_NAPI_METHOD("ReadErrorCounters", canlib_canReadErrorCounters),
        // DECLARE_NAPI_METHOD("RequestChipStatus", canlib_canRequestChipStatus),
        // Additional functions
        // DECLARE_NAPI_METHOD("ChannelInfo", canlib_canChannelInfo),
        // DECLARE_NAPI_METHOD("EnableEvent", canlib_canEnableEvent),
        // DECLARE_NAPI_METHOD("DisableEvent", canlib_canDisableEvent),
        // DECLARE_NAPI_METHOD("SetLoopback", canlib_canSetLoopback),
        // DECLARE_NAPI_METHOD("ClearErrorCounters", canlib_canClearErrorCounters),
        // DECLARE_NAPI_METHOD("GetFlagText", canlib_canGetFlagText),
    };

    status = napi_define_properties(env, exports, descriptors_len, descriptors);
    assert(status == napi_ok);

    return exports;
}




// void canlib_canEventCallback(int canlibHandle)
// {
//     napi_status status = napi_generic_failure;

// #ifdef PCAN_DEBUG
//     printf("canlib_canEventCallback()\n");
// #endif

//     status = napi_call_threadsafe_function(canlibCallback, 0, true);
//     assert(status == napi_ok);

//     return;
// }




// void canlib_canEventFinalize(napi_env env, void* finalize_data, void* finalize_hint)
// {
// #ifdef PCAN_DEBUG
//     printf("canlib_canEventFinalize\n");
// #endif
// }




// ----------------------------------- // -----------------------------------
// Public functions


napi_value mccul_getDaqDeviceInventory(napi_env env, napi_callback_info info)
{
    napi_status status = napi_generic_failure;

    // Retrieve callback info and argument list
    size_t argc = GETDAQDEVICEINVENTORY_ARGC;
    napi_value argv[GETDAQDEVICEINVENTORY_ARGC];

    status = napi_get_cb_info(env, info, &argc, argv, 0, 0);
    assert(status == napi_ok);

    if (argc != GETDAQDEVICEINVENTORY_ARGC)
    {
        napi_throw_type_error(env, 0, "Incorrect number of arguments.");
        return 0;
    }


	DaqDeviceDescriptor ul_devDescriptors[MAX_DEV_COUNT];
	DaqDeviceInterface ul_interfaceType;
	unsigned int ul_numDevs = MAX_DEV_COUNT;

    // Retrieve or check arguments
    status = napi_get_value_int32(env, argv[0], (int32_t *)&ul_interfaceType);
    assert(status == napi_ok);

    // Get device list from library
	UlError err = ERR_NO_ERROR;
    err = ulGetDaqDeviceInventory(ul_interfaceType, ul_devDescriptors, &ul_numDevs);

    if (err != ERR_NO_ERROR)
    {
        mcculPrintError(err);
        char errCode[16];
        snprintf(errCode, sizeof(errCode), "%d", err);
        napi_throw_error(env, errCode, "Error encountered at ulGetDaqDeviceInventory");
        return 0;
        
    }

    napi_value devDescriptors;
    status = napi_create_array(env, &devDescriptors);
    assert(status == napi_ok);

    napi_value devDescriptorElement;
    napi_value devDescriptorElement_productName; // char[64]
    napi_value devDescriptorElement_productId; // unsigned int
    napi_value devDescriptorElement_devInterface; // enum DaqDevInterface
    napi_value devDescriptorElement_devString; // char[64]
    napi_value devDescriptorElement_uniqueId; // char[64]
    // napi_value devDescriptorElement_reserved;

    // Add one object per device to the N-API array
    unsigned int i;
    for (i = 0; i < ul_numDevs; i++)
    {
#ifdef MCCUL_DEBUG
        mcculDumpDaqDeviceDescriptor();
#endif

        // Create N-API object for array element
        status = napi_create_object(env, &devDescriptorElement);
        assert(status == napi_ok);

        // Assign properties
        // - Product name
        status = napi_create_string_utf8(env, ul_devDescriptors[i].productName,
                                         strnlen(ul_devDescriptors[i].productName, sizeof(ul_devDescriptors[i].productName)),
                                         &devDescriptorElement_productName);
        assert(status == napi_ok);
        status = napi_set_named_property(env, devDescriptorElement, "productName", devDescriptorElement_productName);
        assert(status == napi_ok);

        // - Product ID
        status = napi_create_uint32(env, ul_devDescriptors[i].productId, &devDescriptorElement_productId);
        assert(status == napi_ok);
        status = napi_set_named_property(env, devDescriptorElement, "productId", devDescriptorElement_productId);
        assert(status == napi_ok);

        // - Interface type (DaqDeviceInterface value, see lib/mcculConst.js)
        status = napi_create_int32(env, (int32_t)ul_devDescriptors[i].devInterface, &devDescriptorElement_devInterface);
        assert(status == napi_ok);
        status = napi_set_named_property(env, devDescriptorElement, "devInterface", devDescriptorElement_devInterface);
        assert(status == napi_ok);

        // - Device string
        status = napi_create_string_utf8(env, ul_devDescriptors[i].devString,
                                         strnlen(ul_devDescriptors[i].devString, sizeof(ul_devDescriptors[i].devString)),
                                         &devDescriptorElement_devString);
        assert(status == napi_ok);
        status = napi_set_named_property(env, devDescriptorElement, "devString", devDescriptorElement_devString);
        assert(status == napi_ok);

        // - Unique ID (serial number or MAC address)
        status = napi_create_string_utf8(env, ul_devDescriptors[i].uniqueId,
                                         strnlen(ul_devDescriptors[i].uniqueId, sizeof(ul_devDescriptors[i].uniqueId)),
                                         &devDescriptorElement_uniqueId);
        assert(status == napi_ok);
        status = napi_set_named_property(env, devDescriptorElement, "uniqueId", devDescriptorElement_uniqueId);
        assert(status == napi_ok);

        // Add complete element to array
        status = napi_set_element(env, devDescriptors, i, devDescriptorElement);
        assert(status == napi_ok);
    }

    // Return N-API object
    return devDescriptors;
}
