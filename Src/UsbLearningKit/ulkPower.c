#include <wdm.h>

#include "ulkDevices.h"

NTSTATUS 
ulkDispatchPower(
    PDEVICE_OBJECT deviceObject,
    PIRP irp)
{
    NTSTATUS status;

    PIO_STACK_LOCATION ioSp;
    PULK_DEV_EXT devExt;
    BOOLEAN passThrough = TRUE;

    ioSp = IoGetCurrentIrpStackLocation(irp);
    devExt = (PULK_DEV_EXT)deviceObject->DeviceExtension;
    
    ASSERT(devExt->lowerDevice);

    status = IoAcquireRemoveLock(&devExt->removeLock, irp);
    if (!NT_SUCCESS(status)) {
        irp->IoStatus.Status = status;
        irp->IoStatus.Information = 0;
        IoCompleteRequest(irp, IO_NO_INCREMENT);
        return status;
    }

    switch (ioSp->MinorFunction) {
    case IRP_MN_WAIT_WAKE:
        DbgPrint("[ulk] IRP_MN_WAIT_WAKE(0x%p)\n", devExt->pdo);
        break;
    case IRP_MN_POWER_SEQUENCE:
        DbgPrint("[ulk] IRP_MN_POWER_SEQUENCE(0x%p)\n", devExt->pdo);
        break;
    case IRP_MN_SET_POWER:
        DbgPrint("[ulk] IRP_MN_SET_POWER(0x%p)\n", devExt->pdo);
        break;
    case IRP_MN_QUERY_POWER:
        DbgPrint("[ulk] IRP_MN_QUERY_POWER(0x%p)\n", devExt->pdo);
        break;
    }

    if (passThrough) {
        IoSkipCurrentIrpStackLocation(irp);
        status = IoCallDriver(devExt->pdo, irp);
    }

    IoReleaseRemoveLock(&devExt->removeLock, irp);
    
    return status;
}