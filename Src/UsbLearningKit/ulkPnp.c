#include <wdm.h>

#include "ulkDevices.h"
#include "ulkPnp.h"

#pragma alloc_text(PAGE, ulkDispatchPnp)

NTSTATUS 
ulkDispatchPnp(
    PDEVICE_OBJECT deviceObject,
    PIRP irp)
{
    NTSTATUS status;

    PULK_DEV_EXT devExt;
    PIO_STACK_LOCATION ioSp;

    USHORT minorFn;
    BOOLEAN passThrough = TRUE;
    PAGED_CODE();

    ioSp = IoGetCurrentIrpStackLocation(irp);
    devExt = (PULK_DEV_EXT)deviceObject->DeviceExtension;
    minorFn = ioSp->MinorFunction;

    ASSERT(devExt);
    ASSERT(devExt->pdo);
    ASSERT(ioSp->MajorFunction == IRP_MJ_PNP);

    status = IoAcquireRemoveLock(&devExt->removeLock, irp);
    if (!NT_SUCCESS(status)) {
        irp->IoStatus.Status = status;
        irp->IoStatus.Information = 0;
        IoCompleteRequest(irp, IO_NO_INCREMENT);
        return status;
    }
    
    switch (minorFn) {
    case IRP_MN_START_DEVICE:
        DbgPrint("[ulk] IRP_MN_START_DEVICE(0x%p)\n", deviceObject);
        break;
    case IRP_MN_QUERY_REMOVE_DEVICE:
        DbgPrint("[ulk] IRP_MN_QUERY_REMOVE_DEVICE(0x%p)\n", deviceObject);
        break;
    case IRP_MN_REMOVE_DEVICE:
        DbgPrint("[ulk] IRP_MN_REMOVE_DEVICE(0x%p)\n", deviceObject);
        break;
    case IRP_MN_CANCEL_REMOVE_DEVICE:
        DbgPrint("[ulk] IRP_MN_CANCEL_REMOVE_DEVICE(0x%p)\n", deviceObject);
        break;
    case IRP_MN_STOP_DEVICE:
        DbgPrint("[ulk] IRP_MN_STOP_DEVICE(0x%p)\n", deviceObject);
        break;
    case IRP_MN_QUERY_STOP_DEVICE:
        DbgPrint("[ulk] IRP_MN_QUERY_STOP_DEVICE(0x%p)\n", deviceObject);
        break;
    case IRP_MN_CANCEL_STOP_DEVICE:
        DbgPrint("[ulk] IRP_MN_CANCEL_STOP_DEVICE(0x%p)\n", deviceObject);
        break;
    case IRP_MN_QUERY_DEVICE_RELATIONS:
        DbgPrint("[ulk] IRP_MN_QUERY_DEVICE_RELATIONS(0x%p)\n", deviceObject);
        break;
    case IRP_MN_QUERY_INTERFACE:
        DbgPrint("[ulk] IRP_MN_QUERY_INTERFACE(0x%p)\n", deviceObject);
        break;
    case IRP_MN_QUERY_CAPABILITIES:
        DbgPrint("[ulk] IRP_MN_QUERY_CAPABILITIES(0x%p)\n", deviceObject);
        break;
    case IRP_MN_QUERY_RESOURCES:
        DbgPrint("[ulk] IRP_MN_QUERY_RESOURCES(0x%p)\n", deviceObject);
        break;
    case IRP_MN_QUERY_RESOURCE_REQUIREMENTS:
        DbgPrint("[ulk] IRP_MN_QUERY_RESOURCE_REQUIREMENTS(0x%p)\n", deviceObject);
        break;
    case IRP_MN_QUERY_DEVICE_TEXT:
        DbgPrint("[ulk] IRP_MN_QUERY_DEVICE_TEXT(0x%p)\n", deviceObject);
        break;
    case IRP_MN_FILTER_RESOURCE_REQUIREMENTS:
        DbgPrint("[ulk] IRP_MN_FILTER_RESOURCE_REQUIREMENTS(0x%p)\n", deviceObject);
        break;
    case IRP_MN_READ_CONFIG:
        DbgPrint("[ulk] IRP_MN_READ_CONFIG(0x%p)\n", deviceObject);
        break;
    case IRP_MN_WRITE_CONFIG:
        DbgPrint("[ulk] IRP_MN_WRITE_CONFIG(0x%p)\n", deviceObject);
        break;
    case IRP_MN_EJECT:
        DbgPrint("[ulk] IRP_MN_EJECT(0x%p)\n", deviceObject);
        break;
    case IRP_MN_SET_LOCK:
        DbgPrint("[ulk] IRP_MN_SET_LOCK(0x%p)\n", deviceObject);
        break;
    case IRP_MN_QUERY_ID:
        DbgPrint("[ulk] IRP_MN_QUERY_ID(0x%p)\n", deviceObject);
        break;
    case IRP_MN_QUERY_PNP_DEVICE_STATE:
        DbgPrint("[ulk] IRP_MN_QUERY_PNP_DEVICE_STATE(0x%p)\n", deviceObject);
        break;
    case IRP_MN_QUERY_BUS_INFORMATION:
        DbgPrint("[ulk] IRP_MN_QUERY_BUS_INFORMATION(0x%p)\n", deviceObject);
        break;
    case IRP_MN_DEVICE_USAGE_NOTIFICATION:
        DbgPrint("[ulk] IRP_MN_DEVICE_USAGE_NOTIFICATION(0x%p)\n", deviceObject);
        break;
    case IRP_MN_SURPRISE_REMOVAL:
        DbgPrint("[ulk] IRP_MN_SURPRISE_REMOVAL(0x%p)\n", deviceObject);
        break;
    }

    if (passThrough) {
        IoSkipCurrentIrpStackLocation(irp);
        status = IoCallDriver(devExt->lowerDevice, irp);
    }

    if (minorFn != IRP_MN_REMOVE_DEVICE) {
        IoReleaseRemoveLock(&devExt->removeLock, irp);
    }

    return status;
}

NTSTATUS 
uhfPnpRemoveDevice(
    PDEVICE_OBJECT DeviceObject,
    PIRP Irp,
    PULK_DEV_EXT devExt,
    PIO_STACK_LOCATION ioSp)
{
    NTSTATUS status;

    PAGED_CODE();

    IoSkipCurrentIrpStackLocation(Irp);

    status = IoCallDriver(devExt->lowerDevice, Irp);
    IoReleaseRemoveLockAndWait(&devExt->removeLock, Irp);
    
    ulkDeleteDevice(DeviceObject);

    return status;
}
