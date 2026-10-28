/* N-API interface for Measurement Computing Universal Library

   N-API callable functions that directly correspond to UL functions, 
   and additional wrapper functions

   Copyright (c) 2026 Control Solutions LLC. All rights reserved.
*/
   

#include <assert.h>      // provide assert
#include <node_api.h>    // provide N-API types, constants, and functions
#include <stdio.h>       // provide printf
#include <stdint.h>      // provide uintX_t
#include <stdlib.h>      // provide calloc, free
#include <string.h>      // provide strnlen

/* #include <windows.h>     // provide Win32 types, constants, and functions */

#include <uldaq.h>

#include "cs_mccul.h"
#include "cs_mccul_helper.h"
#include "napi_helper.h"      // provide DECLARE_NAPI_METHOD

// ----------------------------------- // -----------------------------------
// Definitions

// From UL examples
#define MAX_DEV_COUNT  100
#define MAX_STR_LENGTH 64

// ----------------------------------- // -----------------------------------
// Global variables

// ----------------------------------- // -----------------------------------
// Local variables

// ----------------------------------- // -----------------------------------
// Local functions

// Throw a UL error as a JS Error. The error's code property is the numeric
// UlError value (as a string), and its message includes the UL's own
// description of the error.
static void mcculThrowError(napi_env env, UlError err, const char *fnName)
{
    char errCode[16];
    char errMsg[ERR_MSG_LEN] = { 0 };
    char message[ERR_MSG_LEN + 64];

#ifdef MCCUL_DEBUG
    mcculPrintError(err);
#endif
    ulGetErrMsg(err, errMsg);

    snprintf(errCode, sizeof(errCode), "%d", err);
    snprintf(message, sizeof(message), "%s: %s", fnName, errMsg);
    napi_throw_error(env, errCode, message);
}


// ----------------------------------- // -----------------------------------
// Scan buffer registry
//
// A background scan (ulAInScan() etc.) keeps writing into the caller's buffer
// after the call returns. To stop the JS garbage collector from freeing that
// buffer mid-scan, we hold a reference to it until the scan is stopped, a new
// scan of the same type is started on the device, or the device is released.

typedef enum
{
    MCCUL_SCAN_NONE = -1,   // for mcculCallHandleFn(): release nothing
    MCCUL_SCAN_AI = 0,
    MCCUL_SCAN_DI,
    MCCUL_SCAN_CI,
    MCCUL_SCAN_DAQI,
    MCCUL_SCAN_ALL          // for mcculCallHandleFn(): release all for device
} mcculScanType;

#define MAX_SCAN_BUFFERS 64

typedef struct
{
    napi_env env;               // env that owns ref (0 = slot free)
    DaqDeviceHandle handle;
    mcculScanType type;
    napi_ref ref;               // reference to the JS typed array
    void *extra;                // other memory to free with the scan, or 0
} mcculScanBuffer;

static mcculScanBuffer scanBuffers[MAX_SCAN_BUFFERS];


// Release one registry slot
static void mcculFreeScanBufferSlot(mcculScanBuffer *slot)
{
    napi_delete_reference(slot->env, slot->ref);
    free(slot->extra);
    memset(slot, 0, sizeof(*slot));
}


// Release held scan buffers for a device: one scan type, or MCCUL_SCAN_ALL
static void mcculReleaseScanBuffers(napi_env env, DaqDeviceHandle handle, mcculScanType type)
{
    int i;
    for (i = 0; i < MAX_SCAN_BUFFERS; i++)
    {
        if ((scanBuffers[i].env == env) && (scanBuffers[i].handle == handle) &&
            ((type == MCCUL_SCAN_ALL) || (scanBuffers[i].type == type)))
        {
            mcculFreeScanBufferSlot(&scanBuffers[i]);
        }
    }
}


// Take a reference to a scan buffer before starting a scan. Returns the slot,
// or 0 (with an exception pending) if the registry is full. Call
// mcculCommitScanBuffer() if the scan starts, mcculFreeScanBufferSlot() if not.
static mcculScanBuffer *mcculHoldScanBuffer(napi_env env, DaqDeviceHandle handle,
                                            mcculScanType type, napi_value buffer,
                                            void *extra)
{
    int i;
    for (i = 0; i < MAX_SCAN_BUFFERS; i++)
    {
        if (scanBuffers[i].env == 0)
        {
            napi_status status = napi_create_reference(env, buffer, 1, &scanBuffers[i].ref);
            assert(status == napi_ok);

            scanBuffers[i].env = env;
            scanBuffers[i].handle = handle;
            scanBuffers[i].type = MCCUL_SCAN_NONE; // not active until committed
            scanBuffers[i].extra = extra;
            return &scanBuffers[i];
        }
    }

    free(extra);
    napi_throw_error(env, 0, "Too many scan buffers in use; stop or release some scans first.");
    return 0;
}


// The scan started: drop the buffer from any previous scan of the same type
// on this device, and mark this one active
static void mcculCommitScanBuffer(napi_env env, mcculScanBuffer *slot, mcculScanType type)
{
    mcculReleaseScanBuffers(env, slot->handle, type);
    slot->type = type;
}


// Convert a JS value (Number or BigInt) to a DaqDeviceHandle. Throws a
// TypeError and returns false if the value is not a usable handle.
static bool mcculGetHandleArg(napi_env env, napi_value value, DaqDeviceHandle *handle)
{
    napi_status status = napi_generic_failure;
    napi_valuetype vt;
    int64_t handleValue = 0;
    bool lossless = true;

    status = napi_typeof(env, value, &vt);
    assert(status == napi_ok);

    if (vt == napi_number)
    {
        status = napi_get_value_int64(env, value, &handleValue);
    }
    else if (vt == napi_bigint)
    {
        status = napi_get_value_bigint_int64(env, value, &handleValue, &lossless);
    }
    else
    {
        napi_throw_type_error(env, 0, "DAQ device handle must be a Number or BigInt.");
        return false;
    }

    if ((status != napi_ok) || !lossless || (handleValue <= 0))
    {
        napi_throw_type_error(env, 0, "Invalid DAQ device handle.");
        return false;
    }

    *handle = (DaqDeviceHandle)handleValue;
    return true;
}


// Read a JS Number argument as an int32. Throws a TypeError and returns false
// if the value is not a number.
static bool mcculGetInt32Arg(napi_env env, napi_value value, const char *name, int32_t *result)
{
    if (napi_get_value_int32(env, value, result) != napi_ok)
    {
        char message[96];
        snprintf(message, sizeof(message), "Argument '%s' must be a number.", name);
        napi_throw_type_error(env, 0, message);
        return false;
    }

    return true;
}


// Read a JS Number argument as a uint32. Throws a TypeError and returns false
// if the value is not a number.
static bool mcculGetUint32Arg(napi_env env, napi_value value, const char *name, uint32_t *result)
{
    if (napi_get_value_uint32(env, value, result) != napi_ok)
    {
        char message[96];
        snprintf(message, sizeof(message), "Argument '%s' must be a number.", name);
        napi_throw_type_error(env, 0, message);
        return false;
    }

    return true;
}


// Common implementation for UL functions of the form
// UlError fn(DaqDeviceHandle): takes one handle argument, throws on error,
// and returns undefined. On success, releases the device's held scan buffers
// of type releaseScan (MCCUL_SCAN_NONE for none, MCCUL_SCAN_ALL for all).
static napi_value mcculCallHandleFn(napi_env env, napi_callback_info info,
                                    UlError (*fn)(DaqDeviceHandle), const char *fnName,
                                    mcculScanType releaseScan)
{
    napi_status status = napi_generic_failure;

    // Retrieve callback info and argument list
    size_t argc = 1;
    napi_value argv[1];

    status = napi_get_cb_info(env, info, &argc, argv, 0, 0);
    assert(status == napi_ok);

    if (argc != 1)
    {
        napi_throw_type_error(env, 0, "Incorrect number of arguments.");
        return 0;
    }

    DaqDeviceHandle ul_handle;
    if (!mcculGetHandleArg(env, argv[0], &ul_handle))
    {
        return 0; // exception already pending
    }

    UlError err = fn(ul_handle);

    if (err != ERR_NO_ERROR)
    {
        mcculThrowError(env, err, fnName);
        return 0;
    }

    if (releaseScan != MCCUL_SCAN_NONE)
    {
        mcculReleaseScanBuffers(env, ul_handle, releaseScan);
    }

    napi_value result;
    status = napi_get_undefined(env, &result);
    assert(status == napi_ok);

    return result;
}


// Read a JS Number argument as a double. Throws a TypeError and returns false
// if the value is not a number.
static bool mcculGetDoubleArg(napi_env env, napi_value value, const char *name, double *result)
{
    if (napi_get_value_double(env, value, result) != napi_ok)
    {
        char message[96];
        snprintf(message, sizeof(message), "Argument '%s' must be a number.", name);
        napi_throw_type_error(env, 0, message);
        return false;
    }

    return true;
}


// Return an unsigned 64-bit value as a Number, or as a BigInt if it exceeds
// Number.MAX_SAFE_INTEGER (2^53 - 1)
static napi_value mcculCreateUint64(napi_env env, unsigned long long value)
{
    napi_status status;
    napi_value result;

    if (value <= 9007199254740991ULL)
    {
        status = napi_create_int64(env, (int64_t)value, &result);
    }
    else
    {
        status = napi_create_bigint_uint64(env, (uint64_t)value, &result);
    }
    assert(status == napi_ok);

    return result;
}


// Check that a scan buffer argument is a typed array of the expected type
// with room for at least minLength elements, and return its data pointer.
// Throws a TypeError/RangeError and returns false otherwise.
static bool mcculGetScanBufferArg(napi_env env, napi_value value,
                                  napi_typedarray_type expectedType, const char *typeName,
                                  size_t minLength, void **data)
{
    bool isTypedArray = false;
    napi_typedarray_type type;
    size_t length = 0;
    char message[128];

    if ((napi_is_typedarray(env, value, &isTypedArray) != napi_ok) || !isTypedArray ||
        (napi_get_typedarray_info(env, value, &type, &length, data, 0, 0) != napi_ok) ||
        (type != expectedType))
    {
        snprintf(message, sizeof(message), "Scan buffer must be a %s.", typeName);
        napi_throw_type_error(env, 0, message);
        return false;
    }

    if (length < minLength)
    {
        snprintf(message, sizeof(message),
                 "Scan buffer is too small: %zu elements needed, %zu provided.",
                 minLength, length);
        napi_throw_range_error(env, 0, message);
        return false;
    }

    return true;
}


// Common implementation for the UL *ScanStatus functions. Takes one handle
// argument and returns { status, currentScanCount, currentTotalCount,
// currentIndex }; throws on error.
static napi_value mcculScanStatus(napi_env env, napi_callback_info info,
                                  UlError (*fn)(DaqDeviceHandle, ScanStatus *, TransferStatus *),
                                  const char *fnName)
{
    napi_status status = napi_generic_failure;

    size_t argc = 1;
    napi_value argv[1];

    status = napi_get_cb_info(env, info, &argc, argv, 0, 0);
    assert(status == napi_ok);

    if (argc != 1)
    {
        napi_throw_type_error(env, 0, "Incorrect number of arguments.");
        return 0;
    }

    DaqDeviceHandle ul_handle;
    if (!mcculGetHandleArg(env, argv[0], &ul_handle))
    {
        return 0; // exception already pending
    }

    ScanStatus ul_status = SS_IDLE;
    TransferStatus ul_xferStatus;
    memset(&ul_xferStatus, 0, sizeof(ul_xferStatus));

    UlError err = fn(ul_handle, &ul_status, &ul_xferStatus);

    if (err != ERR_NO_ERROR)
    {
        mcculThrowError(env, err, fnName);
        return 0;
    }

    // Build result object. Counts would take centuries to exceed 2^53 at any
    // supported rate, so plain Numbers are used.
    napi_value result, value;
    status = napi_create_object(env, &result);
    assert(status == napi_ok);

    status = napi_create_int32(env, (int32_t)ul_status, &value);
    assert(status == napi_ok);
    status = napi_set_named_property(env, result, "status", value);
    assert(status == napi_ok);

    status = napi_create_int64(env, (int64_t)ul_xferStatus.currentScanCount, &value);
    assert(status == napi_ok);
    status = napi_set_named_property(env, result, "currentScanCount", value);
    assert(status == napi_ok);

    status = napi_create_int64(env, (int64_t)ul_xferStatus.currentTotalCount, &value);
    assert(status == napi_ok);
    status = napi_set_named_property(env, result, "currentTotalCount", value);
    assert(status == napi_ok);

    status = napi_create_int64(env, (int64_t)ul_xferStatus.currentIndex, &value);
    assert(status == napi_ok);
    status = napi_set_named_property(env, result, "currentIndex", value);
    assert(status == napi_ok);

    return result;
}


// Parse the (handle, infoItem, index) arguments shared by the UL *GetInfo
// functions. Throws and returns false on bad arguments.
static bool mcculGetInfoArgs(napi_env env, napi_callback_info info,
                             DaqDeviceHandle *handle, int32_t *infoItem, uint32_t *index)
{
    napi_status status = napi_generic_failure;

    size_t argc = GETINFO_ARGC;
    napi_value argv[GETINFO_ARGC];

    status = napi_get_cb_info(env, info, &argc, argv, 0, 0);
    assert(status == napi_ok);

    if (argc != GETINFO_ARGC)
    {
        napi_throw_type_error(env, 0, "Incorrect number of arguments.");
        return false;
    }

    return mcculGetHandleArg(env, argv[0], handle) &&
           mcculGetInt32Arg(env, argv[1], "infoItem", infoItem) &&
           mcculGetUint32Arg(env, argv[2], "index", index);
}


// Return the result of a UL *GetInfo call as a Number, or throw if it failed.
// Info values are counts, enum values, and bitmasks; all fit in a Number.
static napi_value mcculGetInfoResult(napi_env env, UlError err, long long infoValue,
                                     const char *fnName)
{
    if (err != ERR_NO_ERROR)
    {
        mcculThrowError(env, err, fnName);
        return 0;
    }

    napi_value result;
    napi_status status = napi_create_int64(env, (int64_t)infoValue, &result);
    assert(status == napi_ok);

    return result;
}


// Copy a string property of a JS object into a fixed-size, NUL-terminated
// char buffer (e.g. the char[64] fields of DaqDeviceDescriptor). Throws a
// TypeError and returns false if the property is missing or not a string.
static bool mcculGetStringProperty(napi_env env, napi_value object, const char *name,
                                   char *buffer, size_t bufferSize)
{
    napi_status status = napi_generic_failure;
    napi_value property;
    size_t copied = 0;

    status = napi_get_named_property(env, object, name, &property);
    if (status == napi_ok)
    {
        // Truncates to bufferSize - 1 characters and always NUL-terminates
        status = napi_get_value_string_utf8(env, property, buffer, bufferSize, &copied);
    }

    if (status != napi_ok)
    {
        char message[96];
        snprintf(message, sizeof(message), "Device descriptor property '%s' must be a string.", name);
        napi_throw_type_error(env, 0, message);
        return false;
    }

    return true;
}


// Read a numeric property of a JS object as a uint32. Throws a TypeError and
// returns false if the property is missing or not a number.
static bool mcculGetUint32Property(napi_env env, napi_value object, const char *name,
                                   uint32_t *value)
{
    napi_status status = napi_generic_failure;
    napi_value property;

    status = napi_get_named_property(env, object, name, &property);
    if (status == napi_ok)
    {
        status = napi_get_value_uint32(env, property, value);
    }

    if (status != napi_ok)
    {
        char message[96];
        snprintf(message, sizeof(message), "Device descriptor property '%s' must be a number.", name);
        napi_throw_type_error(env, 0, message);
        return false;
    }

    return true;
}

// Register N-API module
NAPI_MODULE(NODE_GYP_MODULE_NAME, Init);

napi_value Init(napi_env env, napi_value exports)
{
    napi_status status = napi_generic_failure;

    // Turn off buffering on stdout
    setvbuf(stdout, NULL, _IONBF, 0);

    // Build array of property descriptors
    napi_property_descriptor descriptors[] = {
        DECLARE_NAPI_METHOD("GetDaqDeviceInventory", mccul_getDaqDeviceInventory),
        DECLARE_NAPI_METHOD("CreateDaqDevice", mccul_createDaqDevice),
        DECLARE_NAPI_METHOD("GetDevInfoHasAi", mccul_getDevInfoHasAi),
        DECLARE_NAPI_METHOD("ConnectDaqDevice", mccul_connectDaqDevice),
        DECLARE_NAPI_METHOD("DisconnectDaqDevice", mccul_disconnectDaqDevice),
        DECLARE_NAPI_METHOD("ReleaseDaqDevice", mccul_releaseDaqDevice),
        DECLARE_NAPI_METHOD("AIGetInfo", mccul_aiGetInfo),
        DECLARE_NAPI_METHOD("AIn", mccul_aIn),
        DECLARE_NAPI_METHOD("DevGetInfo", mccul_devGetInfo),
        DECLARE_NAPI_METHOD("DIOGetInfo", mccul_dioGetInfo),
        DECLARE_NAPI_METHOD("DConfigPort", mccul_dConfigPort),
        DECLARE_NAPI_METHOD("DIn", mccul_dIn),
        DECLARE_NAPI_METHOD("AInScan", mccul_aInScan),
        DECLARE_NAPI_METHOD("AInScanStatus", mccul_aInScanStatus),
        DECLARE_NAPI_METHOD("AInScanStop", mccul_aInScanStop),
        DECLARE_NAPI_METHOD("DInScan", mccul_dInScan),
        DECLARE_NAPI_METHOD("DInScanStatus", mccul_dInScanStatus),
        DECLARE_NAPI_METHOD("DInScanStop", mccul_dInScanStop),
        DECLARE_NAPI_METHOD("CtrGetInfo", mccul_ctrGetInfo),
        DECLARE_NAPI_METHOD("CIn", mccul_cIn),
        DECLARE_NAPI_METHOD("CClear", mccul_cClear),
        DECLARE_NAPI_METHOD("CInScan", mccul_cInScan),
        DECLARE_NAPI_METHOD("CInScanStatus", mccul_cInScanStatus),
        DECLARE_NAPI_METHOD("CInScanStop", mccul_cInScanStop),
        DECLARE_NAPI_METHOD("DaqIGetInfo", mccul_daqIGetInfo),
        DECLARE_NAPI_METHOD("DaqInScan", mccul_daqInScan),
        DECLARE_NAPI_METHOD("DaqInScanStatus", mccul_daqInScanStatus),
        DECLARE_NAPI_METHOD("DaqInScanStop", mccul_daqInScanStop),
    };

    status = napi_define_properties(env, exports,
                                    sizeof(descriptors) / sizeof(descriptors[0]),
                                    descriptors);
    assert(status == napi_ok);

    return exports;
}


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
        mcculThrowError(env, err, "ulGetDaqDeviceInventory");
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


// CreateDaqDevice(devDescriptor) -> handle
//
// devDescriptor is one of the objects returned by GetDaqDeviceInventory().
// Returns the DAQ device handle as a Number.
napi_value mccul_createDaqDevice(napi_env env, napi_callback_info info)
{
    napi_status status = napi_generic_failure;

    // Retrieve callback info and argument list
    size_t argc = CREATEDAQDEVICE_ARGC;
    napi_value argv[CREATEDAQDEVICE_ARGC];

    status = napi_get_cb_info(env, info, &argc, argv, 0, 0);
    assert(status == napi_ok);

    if (argc != CREATEDAQDEVICE_ARGC)
    {
        napi_throw_type_error(env, 0, "Incorrect number of arguments.");
        return 0;
    }

    napi_valuetype vt;
    status = napi_typeof(env, argv[0], &vt);
    assert(status == napi_ok);

    if (vt != napi_object)
    {
        napi_throw_type_error(env, 0, "Device descriptor must be an object.");
        return 0;
    }

    // Rebuild the UL descriptor from the JS object's properties
    DaqDeviceDescriptor ul_devDescriptor;
    memset(&ul_devDescriptor, 0, sizeof(ul_devDescriptor));

    uint32_t devInterface = 0;

    if (!mcculGetStringProperty(env, argv[0], "productName",
                                ul_devDescriptor.productName, sizeof(ul_devDescriptor.productName)) ||
        !mcculGetUint32Property(env, argv[0], "productId", &ul_devDescriptor.productId) ||
        !mcculGetUint32Property(env, argv[0], "devInterface", &devInterface) ||
        !mcculGetStringProperty(env, argv[0], "devString",
                                ul_devDescriptor.devString, sizeof(ul_devDescriptor.devString)) ||
        !mcculGetStringProperty(env, argv[0], "uniqueId",
                                ul_devDescriptor.uniqueId, sizeof(ul_devDescriptor.uniqueId)))
    {
        return 0; // exception already pending
    }

    ul_devDescriptor.devInterface = (DaqDeviceInterface)devInterface;

    // ulCreateDaqDevice() reports failure by returning a zero handle, not an
    // error code
    DaqDeviceHandle ul_handle = ulCreateDaqDevice(ul_devDescriptor);

    if (ul_handle == 0)
    {
        napi_throw_error(env, 0, "ulCreateDaqDevice: unable to create a DAQ device for this descriptor");
        return 0;
    }

    // Handles are small device numbers, so a Number holds them exactly
    napi_value handle;
    status = napi_create_int64(env, (int64_t)ul_handle, &handle);
    assert(status == napi_ok);

    return handle;
}


// GetDevInfoHasAi(handle) -> boolean
//
// Returns true if the DAQ device has an analog input subsystem. Equivalent to
// the getDevInfoHasAi() helper in the UL examples' utility.h.
napi_value mccul_getDevInfoHasAi(napi_env env, napi_callback_info info)
{
    napi_status status = napi_generic_failure;

    // Retrieve callback info and argument list
    size_t argc = GETDEVINFOHASAI_ARGC;
    napi_value argv[GETDEVINFOHASAI_ARGC];

    status = napi_get_cb_info(env, info, &argc, argv, 0, 0);
    assert(status == napi_ok);

    if (argc != GETDEVINFOHASAI_ARGC)
    {
        napi_throw_type_error(env, 0, "Incorrect number of arguments.");
        return 0;
    }

    DaqDeviceHandle ul_handle;
    if (!mcculGetHandleArg(env, argv[0], &ul_handle))
    {
        return 0; // exception already pending
    }

    long long ul_hasAi = 0;
    UlError err = ulDevGetInfo(ul_handle, DEV_INFO_HAS_AI_DEV, 0, &ul_hasAi);

    if (err != ERR_NO_ERROR)
    {
        mcculThrowError(env, err, "ulDevGetInfo");
        return 0;
    }

    napi_value hasAi;
    status = napi_get_boolean(env, ul_hasAi != 0, &hasAi);
    assert(status == napi_ok);

    return hasAi;
}


// ConnectDaqDevice(handle) -> undefined
//
// Establishes a connection to the DAQ device. Throws on failure.
napi_value mccul_connectDaqDevice(napi_env env, napi_callback_info info)
{
    return mcculCallHandleFn(env, info, ulConnectDaqDevice, "ulConnectDaqDevice", MCCUL_SCAN_NONE);
}


// DisconnectDaqDevice(handle) -> undefined
//
// Disconnects from the DAQ device. The handle remains valid and the device
// can be reconnected with ConnectDaqDevice(). Throws on failure.
napi_value mccul_disconnectDaqDevice(napi_env env, napi_callback_info info)
{
    return mcculCallHandleFn(env, info, ulDisconnectDaqDevice, "ulDisconnectDaqDevice", MCCUL_SCAN_NONE);
}


// ReleaseDaqDevice(handle) -> undefined
//
// Releases the DAQ device and its resources (including any scan buffers held
// for it). The handle is invalid afterward. Throws on failure.
napi_value mccul_releaseDaqDevice(napi_env env, napi_callback_info info)
{
    return mcculCallHandleFn(env, info, ulReleaseDaqDevice, "ulReleaseDaqDevice", MCCUL_SCAN_ALL);
}


// AIGetInfo(handle, infoItem, index) -> Number
//
// Returns analog input information for the device. infoItem is an AiInfoItem
// value (see lib/mcculConst.js); index is item-specific (e.g. the input mode
// for AI_INFO_NUM_CHANS_BY_MODE, or the range index for AI_INFO_SE_RANGE).
napi_value mccul_aiGetInfo(napi_env env, napi_callback_info info)
{
    DaqDeviceHandle ul_handle;
    int32_t infoItem = 0;
    uint32_t index = 0;

    if (!mcculGetInfoArgs(env, info, &ul_handle, &infoItem, &index))
    {
        return 0; // exception already pending
    }

    long long ul_infoValue = 0;
    UlError err = ulAIGetInfo(ul_handle, (AiInfoItem)infoItem, index, &ul_infoValue);

    return mcculGetInfoResult(env, err, ul_infoValue, "ulAIGetInfo");
}


// AIn(handle, channel, inputMode, range, flags) -> Number
//
// Reads a single value from an analog input channel. inputMode, range, and
// flags are AiInputMode, Range, and AInFlag values (see lib/mcculConst.js).
// Returns the value in engineering units unless AIN_FF_NOSCALEDATA is set.
napi_value mccul_aIn(napi_env env, napi_callback_info info)
{
    napi_status status = napi_generic_failure;

    // Retrieve callback info and argument list
    size_t argc = AIN_ARGC;
    napi_value argv[AIN_ARGC];

    status = napi_get_cb_info(env, info, &argc, argv, 0, 0);
    assert(status == napi_ok);

    if (argc != AIN_ARGC)
    {
        napi_throw_type_error(env, 0, "Incorrect number of arguments.");
        return 0;
    }

    DaqDeviceHandle ul_handle;
    int32_t channel = 0;
    int32_t inputMode = 0;
    int32_t range = 0;
    int32_t flags = 0;

    if (!mcculGetHandleArg(env, argv[0], &ul_handle) ||
        !mcculGetInt32Arg(env, argv[1], "channel", &channel) ||
        !mcculGetInt32Arg(env, argv[2], "inputMode", &inputMode) ||
        !mcculGetInt32Arg(env, argv[3], "range", &range) ||
        !mcculGetInt32Arg(env, argv[4], "flags", &flags))
    {
        return 0; // exception already pending
    }

    double ul_data = 0;
    UlError err = ulAIn(ul_handle, channel, (AiInputMode)inputMode, (Range)range,
                        (AInFlag)flags, &ul_data);

    if (err != ERR_NO_ERROR)
    {
        mcculThrowError(env, err, "ulAIn");
        return 0;
    }

    napi_value data;
    status = napi_create_double(env, ul_data, &data);
    assert(status == napi_ok);

    return data;
}


// DevGetInfo(handle, infoItem, index) -> Number
//
// Returns device information. infoItem is a DevInfoItem value (see
// lib/mcculConst.js), e.g. DEV_INFO_HAS_DIO_DEV.
napi_value mccul_devGetInfo(napi_env env, napi_callback_info info)
{
    DaqDeviceHandle ul_handle;
    int32_t infoItem = 0;
    uint32_t index = 0;

    if (!mcculGetInfoArgs(env, info, &ul_handle, &infoItem, &index))
    {
        return 0; // exception already pending
    }

    long long ul_infoValue = 0;
    UlError err = ulDevGetInfo(ul_handle, (DevInfoItem)infoItem, index, &ul_infoValue);

    return mcculGetInfoResult(env, err, ul_infoValue, "ulDevGetInfo");
}


// DIOGetInfo(handle, infoItem, index) -> Number
//
// Returns digital I/O information. infoItem is a DioInfoItem value (see
// lib/mcculConst.js); for per-port items, index is the port index (0 = first).
napi_value mccul_dioGetInfo(napi_env env, napi_callback_info info)
{
    DaqDeviceHandle ul_handle;
    int32_t infoItem = 0;
    uint32_t index = 0;

    if (!mcculGetInfoArgs(env, info, &ul_handle, &infoItem, &index))
    {
        return 0; // exception already pending
    }

    long long ul_infoValue = 0;
    UlError err = ulDIOGetInfo(ul_handle, (DioInfoItem)infoItem, index, &ul_infoValue);

    return mcculGetInfoResult(env, err, ul_infoValue, "ulDIOGetInfo");
}


// DConfigPort(handle, portType, direction) -> undefined
//
// Configures a digital port as input or output. portType is a
// DigitalPortType value and direction a DigitalDirection value (see
// lib/mcculConst.js). Throws on failure.
napi_value mccul_dConfigPort(napi_env env, napi_callback_info info)
{
    napi_status status = napi_generic_failure;

    // Retrieve callback info and argument list
    size_t argc = DCONFIGPORT_ARGC;
    napi_value argv[DCONFIGPORT_ARGC];

    status = napi_get_cb_info(env, info, &argc, argv, 0, 0);
    assert(status == napi_ok);

    if (argc != DCONFIGPORT_ARGC)
    {
        napi_throw_type_error(env, 0, "Incorrect number of arguments.");
        return 0;
    }

    DaqDeviceHandle ul_handle;
    int32_t portType = 0;
    int32_t direction = 0;

    if (!mcculGetHandleArg(env, argv[0], &ul_handle) ||
        !mcculGetInt32Arg(env, argv[1], "portType", &portType) ||
        !mcculGetInt32Arg(env, argv[2], "direction", &direction))
    {
        return 0; // exception already pending
    }

    UlError err = ulDConfigPort(ul_handle, (DigitalPortType)portType, (DigitalDirection)direction);

    if (err != ERR_NO_ERROR)
    {
        mcculThrowError(env, err, "ulDConfigPort");
        return 0;
    }

    napi_value result;
    status = napi_get_undefined(env, &result);
    assert(status == napi_ok);

    return result;
}


// DIn(handle, portType) -> Number
//
// Reads the value of a digital port. portType is a DigitalPortType value (see
// lib/mcculConst.js). Returns a Number; in the unlikely case the value
// exceeds Number.MAX_SAFE_INTEGER (2^53 - 1), returns a BigInt instead.
napi_value mccul_dIn(napi_env env, napi_callback_info info)
{
    napi_status status = napi_generic_failure;

    // Retrieve callback info and argument list
    size_t argc = DIN_ARGC;
    napi_value argv[DIN_ARGC];

    status = napi_get_cb_info(env, info, &argc, argv, 0, 0);
    assert(status == napi_ok);

    if (argc != DIN_ARGC)
    {
        napi_throw_type_error(env, 0, "Incorrect number of arguments.");
        return 0;
    }

    DaqDeviceHandle ul_handle;
    int32_t portType = 0;

    if (!mcculGetHandleArg(env, argv[0], &ul_handle) ||
        !mcculGetInt32Arg(env, argv[1], "portType", &portType))
    {
        return 0; // exception already pending
    }

    unsigned long long ul_data = 0;
    UlError err = ulDIn(ul_handle, (DigitalPortType)portType, &ul_data);

    if (err != ERR_NO_ERROR)
    {
        mcculThrowError(env, err, "ulDIn");
        return 0;
    }

    return mcculCreateUint64(env, ul_data);
}



// ----------------------------------- // -----------------------------------
// Analog input scan


// AInScan(handle, lowChan, highChan, inputMode, range, samplesPerChan, rate,
//         options, flags, buffer) -> Number (actual rate)
//
// Starts an analog input scan. buffer is a Float64Array of at least
// (highChan - lowChan + 1) * samplesPerChan elements; for SO_CONTINUOUS scans
// it is used as a circular buffer. The scan runs in the background (unless
// SO_BLOCKIO-style blocking options are used); poll it with AInScanStatus().
// Returns the actual scan rate.
napi_value mccul_aInScan(napi_env env, napi_callback_info info)
{
    napi_status status = napi_generic_failure;

    size_t argc = AINSCAN_ARGC;
    napi_value argv[AINSCAN_ARGC];

    status = napi_get_cb_info(env, info, &argc, argv, 0, 0);
    assert(status == napi_ok);

    if (argc != AINSCAN_ARGC)
    {
        napi_throw_type_error(env, 0, "Incorrect number of arguments.");
        return 0;
    }

    DaqDeviceHandle ul_handle;
    int32_t lowChan = 0, highChan = 0, inputMode = 0, range = 0, samplesPerChan = 0;
    int32_t options = 0, flags = 0;
    double rate = 0;
    void *data = 0;

    if (!mcculGetHandleArg(env, argv[0], &ul_handle) ||
        !mcculGetInt32Arg(env, argv[1], "lowChan", &lowChan) ||
        !mcculGetInt32Arg(env, argv[2], "highChan", &highChan) ||
        !mcculGetInt32Arg(env, argv[3], "inputMode", &inputMode) ||
        !mcculGetInt32Arg(env, argv[4], "range", &range) ||
        !mcculGetInt32Arg(env, argv[5], "samplesPerChan", &samplesPerChan) ||
        !mcculGetDoubleArg(env, argv[6], "rate", &rate) ||
        !mcculGetInt32Arg(env, argv[7], "options", &options) ||
        !mcculGetInt32Arg(env, argv[8], "flags", &flags))
    {
        return 0; // exception already pending
    }

    if ((highChan < lowChan) || (samplesPerChan <= 0))
    {
        napi_throw_range_error(env, 0, "Invalid channel range or samplesPerChan.");
        return 0;
    }

    size_t needed = (size_t)(highChan - lowChan + 1) * (size_t)samplesPerChan;
    if (!mcculGetScanBufferArg(env, argv[9], napi_float64_array, "Float64Array", needed, &data))
    {
        return 0; // exception already pending
    }

    mcculScanBuffer *slot = mcculHoldScanBuffer(env, ul_handle, MCCUL_SCAN_AI, argv[9], 0);
    if (!slot)
    {
        return 0; // exception already pending
    }

    UlError err = ulAInScan(ul_handle, lowChan, highChan, (AiInputMode)inputMode, (Range)range,
                            samplesPerChan, &rate, (ScanOption)options, (AInScanFlag)flags,
                            (double *)data);

    if (err != ERR_NO_ERROR)
    {
        mcculFreeScanBufferSlot(slot);
        mcculThrowError(env, err, "ulAInScan");
        return 0;
    }

    mcculCommitScanBuffer(env, slot, MCCUL_SCAN_AI);

    napi_value actualRate;
    status = napi_create_double(env, rate, &actualRate);
    assert(status == napi_ok);

    return actualRate;
}


// AInScanStatus(handle) -> { status, currentScanCount, currentTotalCount, currentIndex }
napi_value mccul_aInScanStatus(napi_env env, napi_callback_info info)
{
    return mcculScanStatus(env, info, ulAInScanStatus, "ulAInScanStatus");
}


// AInScanStop(handle) -> undefined
napi_value mccul_aInScanStop(napi_env env, napi_callback_info info)
{
    return mcculCallHandleFn(env, info, ulAInScanStop, "ulAInScanStop", MCCUL_SCAN_AI);
}


// ----------------------------------- // -----------------------------------
// Digital input scan


// DInScan(handle, lowPort, highPort, samplesPerPort, rate, options, flags,
//         buffer) -> Number (actual rate)
//
// Starts a digital input scan. buffer is a BigUint64Array of at least
// (highPort - lowPort + 1) * samplesPerPort elements. Returns the actual scan
// rate.
napi_value mccul_dInScan(napi_env env, napi_callback_info info)
{
    napi_status status = napi_generic_failure;

    size_t argc = DINSCAN_ARGC;
    napi_value argv[DINSCAN_ARGC];

    status = napi_get_cb_info(env, info, &argc, argv, 0, 0);
    assert(status == napi_ok);

    if (argc != DINSCAN_ARGC)
    {
        napi_throw_type_error(env, 0, "Incorrect number of arguments.");
        return 0;
    }

    DaqDeviceHandle ul_handle;
    int32_t lowPort = 0, highPort = 0, samplesPerPort = 0, options = 0, flags = 0;
    double rate = 0;
    void *data = 0;

    if (!mcculGetHandleArg(env, argv[0], &ul_handle) ||
        !mcculGetInt32Arg(env, argv[1], "lowPort", &lowPort) ||
        !mcculGetInt32Arg(env, argv[2], "highPort", &highPort) ||
        !mcculGetInt32Arg(env, argv[3], "samplesPerPort", &samplesPerPort) ||
        !mcculGetDoubleArg(env, argv[4], "rate", &rate) ||
        !mcculGetInt32Arg(env, argv[5], "options", &options) ||
        !mcculGetInt32Arg(env, argv[6], "flags", &flags))
    {
        return 0; // exception already pending
    }

    if ((highPort < lowPort) || (samplesPerPort <= 0))
    {
        napi_throw_range_error(env, 0, "Invalid port range or samplesPerPort.");
        return 0;
    }

    // DigitalPortType values for consecutive ports are consecutive, so this
    // counts the ports in the scan (or overestimates, which is safe)
    size_t needed = (size_t)(highPort - lowPort + 1) * (size_t)samplesPerPort;
    if (!mcculGetScanBufferArg(env, argv[7], napi_biguint64_array, "BigUint64Array", needed, &data))
    {
        return 0; // exception already pending
    }

    mcculScanBuffer *slot = mcculHoldScanBuffer(env, ul_handle, MCCUL_SCAN_DI, argv[7], 0);
    if (!slot)
    {
        return 0; // exception already pending
    }

    UlError err = ulDInScan(ul_handle, (DigitalPortType)lowPort, (DigitalPortType)highPort,
                            samplesPerPort, &rate, (ScanOption)options, (DInScanFlag)flags,
                            (unsigned long long *)data);

    if (err != ERR_NO_ERROR)
    {
        mcculFreeScanBufferSlot(slot);
        mcculThrowError(env, err, "ulDInScan");
        return 0;
    }

    mcculCommitScanBuffer(env, slot, MCCUL_SCAN_DI);

    napi_value actualRate;
    status = napi_create_double(env, rate, &actualRate);
    assert(status == napi_ok);

    return actualRate;
}


// DInScanStatus(handle) -> { status, currentScanCount, currentTotalCount, currentIndex }
napi_value mccul_dInScanStatus(napi_env env, napi_callback_info info)
{
    return mcculScanStatus(env, info, ulDInScanStatus, "ulDInScanStatus");
}


// DInScanStop(handle) -> undefined
napi_value mccul_dInScanStop(napi_env env, napi_callback_info info)
{
    return mcculCallHandleFn(env, info, ulDInScanStop, "ulDInScanStop", MCCUL_SCAN_DI);
}


// ----------------------------------- // -----------------------------------
// Counter input


// CtrGetInfo(handle, infoItem, index) -> Number
//
// Returns counter information. infoItem is a CtrInfoItem value (see
// lib/mcculConst.js).
napi_value mccul_ctrGetInfo(napi_env env, napi_callback_info info)
{
    DaqDeviceHandle ul_handle;
    int32_t infoItem = 0;
    uint32_t index = 0;

    if (!mcculGetInfoArgs(env, info, &ul_handle, &infoItem, &index))
    {
        return 0; // exception already pending
    }

    long long ul_infoValue = 0;
    UlError err = ulCtrGetInfo(ul_handle, (CtrInfoItem)infoItem, index, &ul_infoValue);

    return mcculGetInfoResult(env, err, ul_infoValue, "ulCtrGetInfo");
}


// Parse the (handle, counterNum) arguments shared by CIn() and CClear()
static bool mcculGetCounterArgs(napi_env env, napi_callback_info info,
                                DaqDeviceHandle *handle, int32_t *counterNum)
{
    napi_status status = napi_generic_failure;

    size_t argc = COUNTER_ARGC;
    napi_value argv[COUNTER_ARGC];

    status = napi_get_cb_info(env, info, &argc, argv, 0, 0);
    assert(status == napi_ok);

    if (argc != COUNTER_ARGC)
    {
        napi_throw_type_error(env, 0, "Incorrect number of arguments.");
        return false;
    }

    return mcculGetHandleArg(env, argv[0], handle) &&
           mcculGetInt32Arg(env, argv[1], "counterNum", counterNum);
}


// CIn(handle, counterNum) -> Number
//
// Reads the value of a counter. Returns a Number, or a BigInt if the count
// exceeds Number.MAX_SAFE_INTEGER (possible with 64-bit counters).
napi_value mccul_cIn(napi_env env, napi_callback_info info)
{
    DaqDeviceHandle ul_handle;
    int32_t counterNum = 0;

    if (!mcculGetCounterArgs(env, info, &ul_handle, &counterNum))
    {
        return 0; // exception already pending
    }

    unsigned long long ul_data = 0;
    UlError err = ulCIn(ul_handle, counterNum, &ul_data);

    if (err != ERR_NO_ERROR)
    {
        mcculThrowError(env, err, "ulCIn");
        return 0;
    }

    return mcculCreateUint64(env, ul_data);
}


// CClear(handle, counterNum) -> undefined
//
// Clears a counter to zero. Throws on failure.
napi_value mccul_cClear(napi_env env, napi_callback_info info)
{
    DaqDeviceHandle ul_handle;
    int32_t counterNum = 0;

    if (!mcculGetCounterArgs(env, info, &ul_handle, &counterNum))
    {
        return 0; // exception already pending
    }

    UlError err = ulCClear(ul_handle, counterNum);

    if (err != ERR_NO_ERROR)
    {
        mcculThrowError(env, err, "ulCClear");
        return 0;
    }

    napi_value result;
    napi_status status = napi_get_undefined(env, &result);
    assert(status == napi_ok);

    return result;
}


// CInScan(handle, lowCounterNum, highCounterNum, samplesPerCounter, rate,
//         options, flags, buffer) -> Number (actual rate)
//
// Starts a counter input scan. buffer is a BigUint64Array of at least
// (highCounterNum - lowCounterNum + 1) * samplesPerCounter elements. Returns
// the actual scan rate.
napi_value mccul_cInScan(napi_env env, napi_callback_info info)
{
    napi_status status = napi_generic_failure;

    size_t argc = CINSCAN_ARGC;
    napi_value argv[CINSCAN_ARGC];

    status = napi_get_cb_info(env, info, &argc, argv, 0, 0);
    assert(status == napi_ok);

    if (argc != CINSCAN_ARGC)
    {
        napi_throw_type_error(env, 0, "Incorrect number of arguments.");
        return 0;
    }

    DaqDeviceHandle ul_handle;
    int32_t lowCtr = 0, highCtr = 0, samplesPerCounter = 0, options = 0, flags = 0;
    double rate = 0;
    void *data = 0;

    if (!mcculGetHandleArg(env, argv[0], &ul_handle) ||
        !mcculGetInt32Arg(env, argv[1], "lowCounterNum", &lowCtr) ||
        !mcculGetInt32Arg(env, argv[2], "highCounterNum", &highCtr) ||
        !mcculGetInt32Arg(env, argv[3], "samplesPerCounter", &samplesPerCounter) ||
        !mcculGetDoubleArg(env, argv[4], "rate", &rate) ||
        !mcculGetInt32Arg(env, argv[5], "options", &options) ||
        !mcculGetInt32Arg(env, argv[6], "flags", &flags))
    {
        return 0; // exception already pending
    }

    if ((highCtr < lowCtr) || (samplesPerCounter <= 0))
    {
        napi_throw_range_error(env, 0, "Invalid counter range or samplesPerCounter.");
        return 0;
    }

    size_t needed = (size_t)(highCtr - lowCtr + 1) * (size_t)samplesPerCounter;
    if (!mcculGetScanBufferArg(env, argv[7], napi_biguint64_array, "BigUint64Array", needed, &data))
    {
        return 0; // exception already pending
    }

    mcculScanBuffer *slot = mcculHoldScanBuffer(env, ul_handle, MCCUL_SCAN_CI, argv[7], 0);
    if (!slot)
    {
        return 0; // exception already pending
    }

    UlError err = ulCInScan(ul_handle, lowCtr, highCtr, samplesPerCounter, &rate,
                            (ScanOption)options, (CInScanFlag)flags,
                            (unsigned long long *)data);

    if (err != ERR_NO_ERROR)
    {
        mcculFreeScanBufferSlot(slot);
        mcculThrowError(env, err, "ulCInScan");
        return 0;
    }

    mcculCommitScanBuffer(env, slot, MCCUL_SCAN_CI);

    napi_value actualRate;
    status = napi_create_double(env, rate, &actualRate);
    assert(status == napi_ok);

    return actualRate;
}


// CInScanStatus(handle) -> { status, currentScanCount, currentTotalCount, currentIndex }
napi_value mccul_cInScanStatus(napi_env env, napi_callback_info info)
{
    return mcculScanStatus(env, info, ulCInScanStatus, "ulCInScanStatus");
}


// CInScanStop(handle) -> undefined
napi_value mccul_cInScanStop(napi_env env, napi_callback_info info)
{
    return mcculCallHandleFn(env, info, ulCInScanStop, "ulCInScanStop", MCCUL_SCAN_CI);
}


// ----------------------------------- // -----------------------------------
// DAQ input scan (mixed analog/digital/counter)


// DaqIGetInfo(handle, infoItem, index) -> Number
//
// Returns DAQ input subsystem information. infoItem is a DaqIInfoItem value
// (see lib/mcculConst.js), e.g. DAQI_INFO_CHAN_TYPES.
napi_value mccul_daqIGetInfo(napi_env env, napi_callback_info info)
{
    DaqDeviceHandle ul_handle;
    int32_t infoItem = 0;
    uint32_t index = 0;

    if (!mcculGetInfoArgs(env, info, &ul_handle, &infoItem, &index))
    {
        return 0; // exception already pending
    }

    long long ul_infoValue = 0;
    UlError err = ulDaqIGetInfo(ul_handle, (DaqIInfoItem)infoItem, index, &ul_infoValue);

    return mcculGetInfoResult(env, err, ul_infoValue, "ulDaqIGetInfo");
}


// Build a DaqInChanDescriptor array from a JS array of { channel, type,
// range } objects (range is only needed for analog channels). Returns a
// calloc'd array (caller frees), or 0 with an exception pending.
static DaqInChanDescriptor *mcculGetDaqInChanDescriptors(napi_env env, napi_value array,
                                                         uint32_t *numChans)
{
    napi_status status;
    bool isArray = false;

    status = napi_is_array(env, array, &isArray);
    if ((status != napi_ok) || !isArray)
    {
        napi_throw_type_error(env, 0, "Channel descriptors must be an array.");
        return 0;
    }

    status = napi_get_array_length(env, array, numChans);
    assert(status == napi_ok);

    if (*numChans == 0)
    {
        napi_throw_range_error(env, 0, "At least one channel descriptor is required.");
        return 0;
    }

    DaqInChanDescriptor *descriptors = calloc(*numChans, sizeof(DaqInChanDescriptor));
    if (!descriptors)
    {
        napi_throw_error(env, 0, "Out of memory.");
        return 0;
    }

    uint32_t i;
    for (i = 0; i < *numChans; i++)
    {
        napi_value element, property;
        napi_valuetype vt;
        int32_t value = 0;

        status = napi_get_element(env, array, i, &element);
        assert(status == napi_ok);

        status = napi_typeof(env, element, &vt);
        assert(status == napi_ok);

        if (vt != napi_object)
        {
            free(descriptors);
            napi_throw_type_error(env, 0, "Each channel descriptor must be an object.");
            return 0;
        }

        // channel and type are required
        if ((napi_get_named_property(env, element, "channel", &property) != napi_ok) ||
            !mcculGetInt32Arg(env, property, "channel", &value))
        {
            free(descriptors);
            return 0;
        }
        descriptors[i].channel = value;

        if ((napi_get_named_property(env, element, "type", &property) != napi_ok) ||
            !mcculGetInt32Arg(env, property, "type", &value))
        {
            free(descriptors);
            return 0;
        }
        descriptors[i].type = (DaqInChanType)value;

        // range is optional (unused for digital and counter channels)
        status = napi_get_named_property(env, element, "range", &property);
        assert(status == napi_ok);
        status = napi_typeof(env, property, &vt);
        assert(status == napi_ok);

        if (vt != napi_undefined)
        {
            if (!mcculGetInt32Arg(env, property, "range", &value))
            {
                free(descriptors);
                return 0;
            }
            descriptors[i].range = (Range)value;
        }
    }

    return descriptors;
}


// DaqInScan(handle, chanDescriptors, samplesPerChan, rate, options, flags,
//           buffer) -> Number (actual rate)
//
// Starts a scan of mixed analog, digital, and counter channels.
// chanDescriptors is an array of { channel, type, range } objects (type is a
// DaqInChanType value; range is only needed for analog channels). buffer is a
// Float64Array of at least chanDescriptors.length * samplesPerChan elements.
// Returns the actual scan rate.
//
// Note: the C function's numChans argument is taken from
// chanDescriptors.length.
napi_value mccul_daqInScan(napi_env env, napi_callback_info info)
{
    napi_status status = napi_generic_failure;

    size_t argc = DAQINSCAN_ARGC;
    napi_value argv[DAQINSCAN_ARGC];

    status = napi_get_cb_info(env, info, &argc, argv, 0, 0);
    assert(status == napi_ok);

    if (argc != DAQINSCAN_ARGC)
    {
        napi_throw_type_error(env, 0, "Incorrect number of arguments.");
        return 0;
    }

    DaqDeviceHandle ul_handle;
    int32_t samplesPerChan = 0, options = 0, flags = 0;
    double rate = 0;
    void *data = 0;

    if (!mcculGetHandleArg(env, argv[0], &ul_handle) ||
        !mcculGetInt32Arg(env, argv[2], "samplesPerChan", &samplesPerChan) ||
        !mcculGetDoubleArg(env, argv[3], "rate", &rate) ||
        !mcculGetInt32Arg(env, argv[4], "options", &options) ||
        !mcculGetInt32Arg(env, argv[5], "flags", &flags))
    {
        return 0; // exception already pending
    }

    if (samplesPerChan <= 0)
    {
        napi_throw_range_error(env, 0, "Invalid samplesPerChan.");
        return 0;
    }

    uint32_t numChans = 0;
    DaqInChanDescriptor *descriptors = mcculGetDaqInChanDescriptors(env, argv[1], &numChans);
    if (!descriptors)
    {
        return 0; // exception already pending
    }

    size_t needed = (size_t)numChans * (size_t)samplesPerChan;
    if (!mcculGetScanBufferArg(env, argv[6], napi_float64_array, "Float64Array", needed, &data))
    {
        free(descriptors);
        return 0; // exception already pending
    }

    // The descriptor array is kept (and freed) with the scan buffer, in case
    // the library refers to it while the scan runs
    mcculScanBuffer *slot = mcculHoldScanBuffer(env, ul_handle, MCCUL_SCAN_DAQI, argv[6], descriptors);
    if (!slot)
    {
        return 0; // exception already pending; descriptors freed
    }

    UlError err = ulDaqInScan(ul_handle, descriptors, (int)numChans, samplesPerChan, &rate,
                              (ScanOption)options, (DaqInScanFlag)flags, (double *)data);

    if (err != ERR_NO_ERROR)
    {
        mcculFreeScanBufferSlot(slot);
        mcculThrowError(env, err, "ulDaqInScan");
        return 0;
    }

    mcculCommitScanBuffer(env, slot, MCCUL_SCAN_DAQI);

    napi_value actualRate;
    status = napi_create_double(env, rate, &actualRate);
    assert(status == napi_ok);

    return actualRate;
}


// DaqInScanStatus(handle) -> { status, currentScanCount, currentTotalCount, currentIndex }
napi_value mccul_daqInScanStatus(napi_env env, napi_callback_info info)
{
    return mcculScanStatus(env, info, ulDaqInScanStatus, "ulDaqInScanStatus");
}


// DaqInScanStop(handle) -> undefined
napi_value mccul_daqInScanStop(napi_env env, napi_callback_info info)
{
    return mcculCallHandleFn(env, info, ulDaqInScanStop, "ulDaqInScanStop", MCCUL_SCAN_DAQI);
}
