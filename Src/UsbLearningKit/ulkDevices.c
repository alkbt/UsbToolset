#include <wdm.h>

#include "ulkMain.h"
#include "ulkDevices.h"

#pragma alloc_text(PAGE, ulkAddDevice)
#pragma alloc_text(PAGE, ulkDeleteDevice)

NTSTATUS 
ulkAddDevice(
  PDRIVER_OBJECT driverObject,
  PDEVICE_OBJECT pdo)
{
    NTSTATUS status;

    PDEVICE_OBJECT fdo = NULL;
    PULK_DEV_EXT devExt;

    PAGED_CODE();
#ifdef DBG
    DbgPrint("[ulk] AddDevice(0x%x)\n", pdo);
#endif
    __try {
        status = IoCreateDevice(driverObject, 
                                sizeof(ULK_DEV_EXT), 
                                NULL, 
                                pdo->DeviceType, 
                                0, 
                                FALSE, 
                                &fdo);
        if(!NT_SUCCESS(status)) {
            DbgPrint("[ulk] ERROR ulkAddDevice:IoCreateDevice(%s)\n", 
                    OsrNTStatusToString(status));
            __leave;
        }
        
        devExt = (PULK_DEV_EXT)fdo->DeviceExtension;
        RtlZeroMemory(devExt, sizeof(ULK_DEV_EXT));

        IoInitializeRemoveLock(&devExt->removeLock, DEV_EXT_TAG, 0, 0);
        
        devExt->pdo = pdo;
        devExt->lowerDevice = IoAttachDeviceToDeviceStack(fdo, pdo);
        if (!devExt->lowerDevice) {
            DbgPrint("[ulk] ERROR ulkAddDevice:IoAttachDeviceToDeviceStack\n");
            status = STATUS_INTERNAL_ERROR;
            __leave;
        }

        fdo->Flags = pdo->Flags & (DO_BUFFERED_IO | DO_DIRECT_IO | DO_POWER_INRUSH | DO_POWER_PAGABLE);
        fdo->Flags &= ~DO_DEVICE_INITIALIZING;
    } __finally {
        if (!NT_SUCCESS(status)) {
            if (fdo) {
                ulkDeleteDevice(fdo);
            }
        }
    }
    return STATUS_SUCCESS;
}

VOID 
ulkDeleteDevice(
    PDEVICE_OBJECT deviceObject)
{
    PULK_DEV_EXT devExt;

    devExt = (PULK_DEV_EXT)deviceObject->DeviceExtension;

#ifdef DBG
    DbgPrint("[ulk] AddDevice(0x%x)\n", devExt->pdo);
#endif

    PAGED_CODE();

    if (devExt->lowerDevice) {
        IoDetachDevice(devExt->lowerDevice);
    }
    IoDeleteDevice(deviceObject);
}
