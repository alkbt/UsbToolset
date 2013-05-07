#include <wdm.h>

#include "uhfMain.h"
#include "uhfDevices.h"
#include "uhfPnP.h"


NTSTATUS 
uhfDispatchPassThrough(
    PDEVICE_OBJECT DeviceObject,
    PIRP Irp);

#pragma alloc(INIT, DriverEntry)

PDRIVER_OBJECT g_DriverObject = NULL;


NTSTATUS
DriverEntry(
    PDRIVER_OBJECT DriverObject,
    PUNICODE_STRING RegistryPath)
{
    ULONG i;
    PAGED_CODE();

#ifdef DBG
    DbgPrint("[uhf] DriverEntry..\n");
#endif
    g_DriverObject = DriverObject;
    InitializeListHead(&g_rootDevices);
    ExInitializeFastMutex(&g_rootDevicesMutex);

    DriverObject->DriverExtension->AddDevice = uhfAddDevice;
    for (i = 0; i <= IRP_MJ_MAXIMUM_FUNCTION; i++) {
        DriverObject->MajorFunction[i] = uhfDispatchPassThrough;
    }

    DriverObject->MajorFunction[IRP_MJ_PNP] = uhfDispatchPnP;

    return STATUS_SUCCESS;
}

NTSTATUS 
uhfDispatchPassThrough(
    PDEVICE_OBJECT DeviceObject,
    PIRP Irp)
{
    NTSTATUS status;

    PUHF_DEVICE_EXT devExt;

    devExt = (PUHF_DEVICE_EXT)DeviceObject->DeviceExtension;
    ASSERT(devExt->NextDevice);

    IoSkipCurrentIrpStackLocation(Irp);

    status = IoAcquireRemoveLock(&devExt->RemoveLock, Irp);
    if (!NT_SUCCESS(status)) {
        Irp->IoStatus.Status = status;
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
        return status;
    }

    status = IoCallDriver(devExt->NextDevice, Irp);
    IoReleaseRemoveLock(&devExt->RemoveLock, Irp);

    return status;
}

