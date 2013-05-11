#include <wdm.h>

#include "ulkDevices.h"
#include "ulkPnp.h"

NTSTATUS
DriverEntry(
    PDRIVER_OBJECT DriverObject,
    PUNICODE_STRING RegistryPath);


NTSTATUS 
ulkDispatchPassThrough(
    PDEVICE_OBJECT deviceObject,
    PIRP irp);

#pragma alloc_text(INIT, DriverEntry)

PDRIVER_OBJECT g_driverObject = NULL;

NTSTATUS
DriverEntry(
    PDRIVER_OBJECT driverObject,
    PUNICODE_STRING registryPath)
{
    ULONG i;
    PAGED_CODE();

#ifdef DBG
    DbgPrint("[ulk] DriverEntry..\n");
#endif
    g_driverObject = driverObject;


    driverObject->DriverExtension->AddDevice = ulkAddDevice;
    for (i = 0; i <= IRP_MJ_MAXIMUM_FUNCTION; i++) {
        driverObject->MajorFunction[i] = ulkDispatchPassThrough;
    }

    driverObject->MajorFunction[IRP_MJ_PNP] = ulkDispatchPnp;

    return STATUS_SUCCESS;
}

NTSTATUS 
ulkDispatchPassThrough(
    PDEVICE_OBJECT deviceObject,
    PIRP irp)
{
    NTSTATUS status;

    PULK_DEV_EXT devExt;
    PIO_STACK_LOCATION ioSp;

    devExt = (PULK_DEV_EXT)deviceObject->DeviceExtension;
    ASSERT(devExt->lowerDevice);

    status = IoAcquireRemoveLock(&devExt->removeLock, irp);
    if (!NT_SUCCESS(status)) {
        irp->IoStatus.Status = status;
        irp->IoStatus.Information = 0;
        IoCompleteRequest(irp, IO_NO_INCREMENT);
        return status;
    }

    ioSp = IoGetCurrentIrpStackLocation(irp);
    DbgPrint("[ulk] ulkDispatchPassThrough(0x%p, [0x%x, 0x%x])\n", 
                deviceObject, 
                ioSp->MajorFunction,
                ioSp->MinorFunction);
    IoSkipCurrentIrpStackLocation(irp);
    status = IoCallDriver(devExt->lowerDevice, irp);
    IoReleaseRemoveLock(&devExt->removeLock, irp);

    return status;
}

