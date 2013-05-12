#include <wdm.h>

#include "uhfMain.h"
#include "uhfDevices.h"
#include "uhfPnP.h"
#include "uhfPower.h"

NTSTATUS
DriverEntry(
    PDRIVER_OBJECT DriverObject,
    PUNICODE_STRING RegistryPath);


NTSTATUS 
uhfDispatchPassThrough(
    PDEVICE_OBJECT DeviceObject,
    PIRP Irp);

#pragma alloc_text(INIT, DriverEntry)

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

    InitializeListHead(&g_pdoList);
    ExInitializeFastMutex(&g_pdoListMutex);

    InitializeListHead(&g_rootDevices);
    ExInitializeFastMutex(&g_rootDevicesMutex);

    DriverObject->DriverExtension->AddDevice = uhfAddDevice;
    for (i = 0; i <= IRP_MJ_MAXIMUM_FUNCTION; i++) {
        DriverObject->MajorFunction[i] = uhfDispatchPassThrough;
    }

    DriverObject->MajorFunction[IRP_MJ_PNP] = uhfDispatchPnp;
    DriverObject->MajorFunction[IRP_MJ_POWER] = uhfDispatchPower;

    return STATUS_SUCCESS;
}

NTSTATUS 
uhfDispatchPassThrough(
    PDEVICE_OBJECT DeviceObject,
    PIRP irp)
{
    NTSTATUS status;

    PUHF_DEVICE_EXT devExt;

    devExt = (PUHF_DEVICE_EXT)DeviceObject->DeviceExtension;
    ASSERT(devExt->NextDevice);

    status = IoAcquireRemoveLock(&devExt->RemoveLock, irp);
    if (!NT_SUCCESS(status)) {
        irp->IoStatus.Status = status;
        irp->IoStatus.Information = 0;
        IoCompleteRequest(irp, IO_NO_INCREMENT);
        return status;
    }

    IoSkipCurrentIrpStackLocation(irp);
    status = IoCallDriver(devExt->NextDevice, irp);

    IoReleaseRemoveLock(&devExt->RemoveLock, irp);

    return status;
}

