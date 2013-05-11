#include <wdm.h>

#include "ulkMain.h"
#include "ulkDevices.h"

#pragma alloc_text(PAGE, ulkAddDevice)

NTSTATUS 
ulkAddDevice(
  PDRIVER_OBJECT driverObject,
  PDEVICE_OBJECT pdo)
{
    NTSTATUS status;

    PDEVICE_OBJECT fdo = NULL;
    PULK_DEV_EXT devExt;

    PAGED_CODE();

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
    } __finally {
        if (!NT_SUCCESS(status)) {
            if (fdo) {
                if (devExt && devExt->lowerDevice) {
                    IoDetachDevice(devExt->lowerDevice);
                }

                IoDeleteDevice(fdo);
            }
        }
    }
    return STATUS_SUCCESS;
}

