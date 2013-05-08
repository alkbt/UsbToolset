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
uhfPnPQueryDeviceRelations(
    PDEVICE_OBJECT DeviceObject,
    PIRP Irp,
    PUHF_DEVICE_EXT devExt,
    PIO_STACK_LOCATION ioSp);

#pragma alloc(PAGE, uhfDispatchPnP)
#pragma alloc(PAGE, uhfPnPRemoveDevice)
#pragma alloc(PAGE, uhfPnPQueryDeviceRelations)

NTSTATUS 
uhfPnPPassThroughCompletion(
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
    case IRP_MN_QUERY_DEVICE_RELATIONS:
        status = uhfPnPQueryDeviceRelations(DeviceObject, Irp, devExt, ioSp);
        break;
    case IRP_MN_QUERY_BUS_INFORMATION:
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

#ifdef DBG
    DbgPrint("[uhf]\tIRP_MN_REMOVE_DEVICE(0x%p)\n", 
            devExt->pdo);
#endif
    IoSkipCurrentIrpStackLocation(Irp);

    status = IoCallDriver(devExt->NextDevice, Irp);
    IoReleaseRemoveLockAndWait(&devExt->RemoveLock, Irp);
    
    IoDetachDevice(devExt->NextDevice);
    uhfDeleteDevice(DeviceObject);

#ifdef DBG
    DbgPrint("[uhf]\tIRP_MN_REMOVE_DEVICE(0x%p) DONE! (%s)\n", 
            devExt->pdo, OsrNTStatusToString(status));
#endif
    return status;
}

NTSTATUS 
uhfQueryDeviceRelationIoCompletion(
    PDEVICE_OBJECT DeviceObject,
    PIRP Irp,
    PVOID Context)
{
    PKEVENT Event = (PKEVENT)Context;

    KeSetEvent(Event, IO_NO_INCREMENT, FALSE);
    return STATUS_MORE_PROCESSING_REQUIRED;
}

NTSTATUS
uhfPnPQueryDeviceRelations(
    PDEVICE_OBJECT DeviceObject,
    PIRP Irp,
    PUHF_DEVICE_EXT devExt,
    PIO_STACK_LOCATION ioSp)    
{
    NTSTATUS status;

    KEVENT Event;

    ULONG i;
    PDEVICE_RELATIONS deviceRelations;
    PWCHAR deviceName;
    ULONG retLength;

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

#ifdef DBG
    DbgPrint("[uhf]\tIRP_MN_QUERY_DEVICE_RELATIONS(0x%p):BusRelations\n", 
            devExt->pdo);
#endif

    KeInitializeEvent(&Event, NotificationEvent, FALSE);

    IoCopyCurrentIrpStackLocationToNext(Irp);
    IoSetCompletionRoutine(Irp, 
                        uhfQueryDeviceRelationIoCompletion, 
                        &Event, 
                        TRUE, TRUE, TRUE);

    status = IoAcquireRemoveLock(&devExt->RemoveLock, Irp);
    if (!NT_SUCCESS(status)) {
        Irp->IoStatus.Status = status;
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
        return status;
    }
    status = IoCallDriver(devExt->NextDevice, Irp);
    if (status == STATUS_PENDING) {
        KeWaitForSingleObject(&Event, 
                            Executive, 
                            KernelMode,
                            FALSE,
                            NULL);
    }
    IoReleaseRemoveLock(&devExt->RemoveLock, Irp);

    if (NT_SUCCESS(Irp->IoStatus.Status) && (Irp->IoStatus.Information != 0)) {
        deviceRelations = (PDEVICE_RELATIONS)Irp->IoStatus.Information;
#ifdef DBG
        DbgPrint("\tEnumerated %d objects:\n", deviceRelations->Count);
#endif
        for (i = 0; i < deviceRelations->Count; i++) {
#ifdef DBG
            status = uhfQueryPdoIds(deviceRelations->Objects[i], BusQueryDeviceID, &deviceName, &retLength);
            if (NT_SUCCESS(status)) {
                DbgPrint("\t\t0x%p %ws\n", deviceRelations->Objects[i], deviceName);
                ExFreePool(deviceName);
            } else {
                DbgPrint("\t\t0x%p %s\n", deviceRelations->Objects[i], OsrNTStatusToString(status));
            }
#endif

            uhfAddChildDevice(g_DriverObject, devExt, deviceRelations->Objects[i]);
        }
    } 
#ifdef DBG 
    else {
        DbgPrint("\tNo child devices\n");
    }
#endif
    status = Irp->IoStatus.Status;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);

    return status;
}

