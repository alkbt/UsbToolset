/**
    1. Единственные устройства, которые попадают к нам сразу в
        AddDevice - контроллеры шины, об остальных мы узнаем
        через BusRelations.

    2. 
*/

#include <wdm.h>

#include "uhfMain.h"
#include "uhfDevices.h"
#include "uhfPnP.h"

NTSTATUS 
uhfPnPRemoveDevice(
    PDEVICE_OBJECT DeviceObject,
    PIRP Irp,
    PUHF_DEVICE_EXT devExt,
    PIO_STACK_LOCATION ioSp);

NTSTATUS 
uhfPnPSurpriseRemoval(
    PDEVICE_OBJECT DeviceObject,
    PIRP Irp,
    PUHF_DEVICE_EXT devExt,
    PIO_STACK_LOCATION ioSp);


NTSTATUS
uhfPnPQueryDeviceRelations(
    PDEVICE_OBJECT DeviceObject,
    PIRP Irp,
    PUHF_DEVICE_EXT devExt,
    PIO_STACK_LOCATION ioSp);

#pragma alloc_text(PAGE, uhfDispatchPnP)
#pragma alloc_text(PAGE, uhfPnPRemoveDevice)
#pragma alloc_text(PAGE, uhfPnPSurpriseRemoval)
#pragma alloc_text(PAGE, uhfPnPQueryDeviceRelations)

NTSTATUS 
uhfPnPFilterCompletion(
    PDEVICE_OBJECT DeviceObject,
    PIRP Irp,
    PVOID Context)
{
    PKEVENT Event = (PKEVENT)Context;

    KeSetEvent(Event, IO_NO_INCREMENT, FALSE);
    return STATUS_MORE_PROCESSING_REQUIRED;
}

NTSTATUS 
uhfDispatchPnP(
    PDEVICE_OBJECT DeviceObject,
    PIRP Irp)
{
    NTSTATUS status;

    PUHF_DEVICE_EXT devExt;
    PIO_STACK_LOCATION ioSp;

    USHORT MinorFn;

    PPNP_BUS_INFORMATION busInfo;

    KEVENT Event;

    PAGED_CODE();

    devExt = (PUHF_DEVICE_EXT)DeviceObject->DeviceExtension;
    ASSERT(devExt->NextDevice);

    status = IoAcquireRemoveLock(&devExt->RemoveLock, Irp);
    if (!NT_SUCCESS(status)) {
        Irp->IoStatus.Status = status;
        Irp->IoStatus.Information = 0;
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
        return status;
    }

    ioSp = IoGetCurrentIrpStackLocation(Irp);
    MinorFn = ioSp->MinorFunction;

    switch (ioSp->MinorFunction) {
    case IRP_MN_REMOVE_DEVICE:
        status = uhfPnPRemoveDevice(DeviceObject, Irp, devExt, ioSp);
        break;
    case IRP_MN_SURPRISE_REMOVAL:
        status = uhfPnPSurpriseRemoval(DeviceObject, Irp, devExt, ioSp);
        break;
    case IRP_MN_QUERY_DEVICE_RELATIONS:
        status = uhfPnPQueryDeviceRelations(DeviceObject, Irp, devExt, ioSp);
        break;
    case IRP_MN_START_DEVICE:
        DbgPrint("[uhf] IRP_MN_START_DEVICE(0x%p)\n", devExt->pdo);
        IoSkipCurrentIrpStackLocation(Irp);
        status = IoCallDriver(devExt->NextDevice, Irp);
        break;
    default:        
        IoSkipCurrentIrpStackLocation(Irp);
        status = IoCallDriver(devExt->NextDevice, Irp);
        break;
    }

    if (MinorFn != IRP_MN_REMOVE_DEVICE) {
        IoReleaseRemoveLock(&devExt->RemoveLock, Irp);
    }

    return status;
}

NTSTATUS 
uhfPnPRemoveDevice(
    PDEVICE_OBJECT DeviceObject,
    PIRP Irp,
    PUHF_DEVICE_EXT devExt,
    PIO_STACK_LOCATION ioSp)
{
    NTSTATUS status;

    PAGED_CODE();

    IoSkipCurrentIrpStackLocation(Irp);

    status = IoCallDriver(devExt->NextDevice, Irp);
    IoReleaseRemoveLockAndWait(&devExt->RemoveLock, Irp);
    
    IoDetachDevice(devExt->NextDevice);
    uhfDeleteDevice(DeviceObject);

    return status;
}

NTSTATUS 
uhfPnPSurpriseRemoval(
    PDEVICE_OBJECT DeviceObject,
    PIRP Irp,
    PUHF_DEVICE_EXT devExt,
    PIO_STACK_LOCATION ioSp)
{
    NTSTATUS status;
    KEVENT event;

    IoCopyCurrentIrpStackLocationToNext(Irp);
    KeInitializeEvent(&event, NotificationEvent, FALSE);
    IoSetCompletionRoutine(Irp, uhfPnPFilterCompletion, &event, TRUE, TRUE, TRUE);
    status = IoCallDriver(devExt->NextDevice, Irp);
    if (status == STATUS_PENDING) {
        KeWaitForSingleObject(&event, Executive, KernelMode, FALSE, NULL);
    }
    status = Irp->IoStatus.Status;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);

    return status;
}

NTSTATUS
uhfPnPQueryDeviceRelations(
    PDEVICE_OBJECT DeviceObject,
    PIRP Irp,
    PUHF_DEVICE_EXT devExt,
    PIO_STACK_LOCATION ioSp)    
{
    NTSTATUS status;

    ULONG i;

    KEVENT Event;
    PDEVICE_RELATIONS deviceRelations;

    PLIST_ENTRY link;
    PUHF_DEVICE_EXT childDevExt;
    BOOLEAN flg;

    PAGED_CODE();

    if (devExt->DeviceRole != UHF_DEVICE_ROLE_HUB_FiDO) {
        IoSkipCurrentIrpStackLocation(Irp);
        status = IoCallDriver(devExt->NextDevice, Irp);
        return status;
    }

    if (ioSp->Parameters.QueryDeviceRelations.Type != BusRelations) {
        IoSkipCurrentIrpStackLocation(Irp);
        status = IoCallDriver(devExt->NextDevice, Irp);
        return status;
    }

    KeInitializeEvent(&Event, NotificationEvent, FALSE);

    IoCopyCurrentIrpStackLocationToNext(Irp);
    IoSetCompletionRoutine(Irp, 
                        uhfPnPFilterCompletion, 
                        &Event, 
                        TRUE, TRUE, TRUE);

    status = IoCallDriver(devExt->NextDevice, Irp);
    if (status == STATUS_PENDING) {
        KeWaitForSingleObject(&Event, 
                            Executive, 
                            KernelMode,
                            FALSE,
                            NULL);
    }

    if (NT_SUCCESS(Irp->IoStatus.Status) && (Irp->IoStatus.Information != 0)) {
        deviceRelations = (PDEVICE_RELATIONS)Irp->IoStatus.Information;
        for (i = 0; i < deviceRelations->Count; i++) {
            if (!uhfIsPdoInGlobalList(deviceRelations->Objects[i])) {
                uhfAddChildDevice(g_DriverObject, devExt, deviceRelations->Objects[i]);
            }
        }

        for (link = devExt->childs.Flink; link != &devExt->childs; link = link->Flink) {
            childDevExt = CONTAINING_RECORD(link, UHF_DEVICE_EXT, siblings);
            flg = FALSE;
            for (i = 0; i < deviceRelations->Count; i++) {
                if (childDevExt->pdo == deviceRelations->Objects[i]) {
                    flg = TRUE;
                    break;
                }
            }

            if (!flg) {
                DbgPrint("[uhf] MISSGING %p\n", childDevExt->pdo);
            }
        }
    }
    status = Irp->IoStatus.Status;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);

    return status;
}

