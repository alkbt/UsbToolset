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
uhfPnpStartDevice(
    PDEVICE_OBJECT deviceObject,
    PIRP irp,
    PUHF_DEVICE_EXT devExt,
    PIO_STACK_LOCATION ioSp);

NTSTATUS 
uhfPnpStopDevice(
    PDEVICE_OBJECT deviceObject,
    PIRP irp,
    PUHF_DEVICE_EXT devExt,
    PIO_STACK_LOCATION ioSp);

NTSTATUS 
uhfPnpQueryStopDevice(
    PDEVICE_OBJECT deviceObject,
    PIRP irp,
    PUHF_DEVICE_EXT devExt,
    PIO_STACK_LOCATION ioSp);

NTSTATUS 
uhfPnpCancelStopDevice(
    PDEVICE_OBJECT deviceObject,
    PIRP irp,
    PUHF_DEVICE_EXT devExt,
    PIO_STACK_LOCATION ioSp);

NTSTATUS 
uhfPnpQueryRemoveDevice(
    PDEVICE_OBJECT deviceObject,
    PIRP irp,
    PUHF_DEVICE_EXT devExt,
    PIO_STACK_LOCATION ioSp);

NTSTATUS 
uhfPnpCancelRemoveDevice(
    PDEVICE_OBJECT deviceObject,
    PIRP irp,
    PUHF_DEVICE_EXT devExt,
    PIO_STACK_LOCATION ioSp);

NTSTATUS 
uhfPnpRemoveDevice(
    PDEVICE_OBJECT DeviceObject,
    PIRP Irp,
    PUHF_DEVICE_EXT devExt,
    PIO_STACK_LOCATION ioSp);

NTSTATUS 
uhfPnpSurpriseRemoval(
    PDEVICE_OBJECT DeviceObject,
    PIRP Irp,
    PUHF_DEVICE_EXT devExt,
    PIO_STACK_LOCATION ioSp);


NTSTATUS
uhfPnpQueryDeviceRelations(
    PDEVICE_OBJECT DeviceObject,
    PIRP Irp,
    PUHF_DEVICE_EXT devExt,
    PIO_STACK_LOCATION ioSp);

NTSTATUS 
uhfPnpQueryDeviceState(
    PDEVICE_OBJECT deviceObject,
    PIRP irp,
    PUHF_DEVICE_EXT devExt,
    PIO_STACK_LOCATION ioSp);



#pragma alloc_text(PAGE, uhfDispatchPnp)
#pragma alloc_text(PAGE, uhfPnpStartDevice)
#pragma alloc_text(PAGE, uhfPnpQueryStopDevice)
#pragma alloc_text(PAGE, uhfPnpCancelStopDevice)
#pragma alloc_text(PAGE, uhfPnpStopDevice)
#pragma alloc_text(PAGE, uhfPnpQueryRemoveDevice)
#pragma alloc_text(PAGE, uhfPnpCancelRemoveDevice)
#pragma alloc_text(PAGE, uhfPnpRemoveDevice)
#pragma alloc_text(PAGE, uhfPnpSurpriseRemoval)

#pragma alloc_text(PAGE, uhfPnpQueryDeviceState)

#pragma alloc_text(PAGE, uhfPnpQueryDeviceRelations)

NTSTATUS 
uhfPnpFilterCompletion(
    PDEVICE_OBJECT DeviceObject,
    PIRP Irp,
    PVOID Context)
{
    PKEVENT Event = (PKEVENT)Context;

    KeSetEvent(Event, IO_NO_INCREMENT, FALSE);
    return STATUS_MORE_PROCESSING_REQUIRED;
}

NTSTATUS 
uhfDispatchPnp(
    PDEVICE_OBJECT deviceObject,
    PIRP irp)
{
    NTSTATUS status;

    PUHF_DEVICE_EXT devExt;
    PIO_STACK_LOCATION ioSp;

    USHORT MinorFn;

    PPNP_BUS_INFORMATION busInfo;

    KEVENT Event;

    PAGED_CODE();

    devExt = (PUHF_DEVICE_EXT)deviceObject->DeviceExtension;
    ASSERT(devExt->NextDevice);

    status = IoAcquireRemoveLock(&devExt->RemoveLock, irp);
    if (!NT_SUCCESS(status)) {
        irp->IoStatus.Status = status;
        irp->IoStatus.Information = 0;
        IoCompleteRequest(irp, IO_NO_INCREMENT);
        return status;
    }

    ioSp = IoGetCurrentIrpStackLocation(irp);
    MinorFn = ioSp->MinorFunction;

    switch (ioSp->MinorFunction) {
    case IRP_MN_START_DEVICE:
        status = uhfPnpStartDevice(deviceObject, irp, devExt, ioSp);
        break;
    case IRP_MN_STOP_DEVICE:
        status = uhfPnpStopDevice(deviceObject, irp, devExt, ioSp);
        break;
    case IRP_MN_QUERY_STOP_DEVICE:
        status = uhfPnpQueryStopDevice(deviceObject, irp, devExt, ioSp);
        break;
    case IRP_MN_CANCEL_STOP_DEVICE:
        status = uhfPnpCancelStopDevice(deviceObject, irp, devExt, ioSp);
        break;
    case IRP_MN_QUERY_REMOVE_DEVICE:
        status = uhfPnpQueryRemoveDevice(deviceObject, irp, devExt, ioSp);
        break;
    case IRP_MN_CANCEL_REMOVE_DEVICE:
        status = uhfPnpCancelRemoveDevice(deviceObject, irp, devExt, ioSp);
        break;
    case IRP_MN_SURPRISE_REMOVAL:
        status = uhfPnpSurpriseRemoval(deviceObject, irp, devExt, ioSp);
        break;
    case IRP_MN_REMOVE_DEVICE:
        status = uhfPnpRemoveDevice(deviceObject, irp, devExt, ioSp);
        break;


    case IRP_MN_QUERY_PNP_DEVICE_STATE:
        status = uhfPnpQueryDeviceState(deviceObject, irp, devExt, ioSp);
        break;

    case IRP_MN_QUERY_DEVICE_RELATIONS:
        status = uhfPnpQueryDeviceRelations(deviceObject, irp, devExt, ioSp);
        break;
    default:
        //DbgPrint("[uhf] uhfDispatchPnp(0x%p) [0x%x; 0x%x]\n", devExt->pdo, ioSp->MajorFunction, ioSp->MinorFunction);
        IoSkipCurrentIrpStackLocation(irp);
        status = IoCallDriver(devExt->NextDevice, irp);
        break;
    }

    if (MinorFn != IRP_MN_REMOVE_DEVICE) {
        IoReleaseRemoveLock(&devExt->RemoveLock, irp);
    }

    return status;
}

NTSTATUS 
uhfPnpStartDevice(
    PDEVICE_OBJECT deviceObject,
    PIRP irp,
    PUHF_DEVICE_EXT devExt,
    PIO_STACK_LOCATION ioSp)
{
    NTSTATUS status;
    KEVENT event;

    PAGED_CODE();

    irp->IoStatus.Status = STATUS_SUCCESS;
    IoCopyCurrentIrpStackLocationToNext(irp);
    KeInitializeEvent(&event, NotificationEvent, FALSE);
    IoSetCompletionRoutine(irp, uhfPnpFilterCompletion, &event, TRUE, TRUE, TRUE);
    status = IoCallDriver(devExt->NextDevice, irp);
    if (status == STATUS_PENDING) {
        KeWaitForSingleObject(&event, Executive, KernelMode, FALSE, NULL);
    }
    status = irp->IoStatus.Status;
    if (NT_SUCCESS(status)) {
        devExt->pdoState = Working;
        uhfDumpDevicesTree();
    } else {
        DbgPrint("[uhf] START_DEVICE(0x%p) ERROR %s\n", 
                    devExt->pdo, 
                    OsrNTStatusToString(status));
    }

    IoCompleteRequest(irp, IO_NO_INCREMENT);

    return status;
}

NTSTATUS 
uhfPnpRemoveDevice(
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
uhfPnpStopDevice(
    PDEVICE_OBJECT deviceObject,
    PIRP irp,
    PUHF_DEVICE_EXT devExt,
    PIO_STACK_LOCATION ioSp)
{
    NTSTATUS status;
    KEVENT event;

    PAGED_CODE();

    //irp->IoStatus.Status = STATUS_SUCCESS;
    IoCopyCurrentIrpStackLocationToNext(irp);
    KeInitializeEvent(&event, NotificationEvent, FALSE);
    IoSetCompletionRoutine(irp, uhfPnpFilterCompletion, &event, TRUE, TRUE, TRUE);
    status = IoCallDriver(devExt->NextDevice, irp);
    if (status == STATUS_PENDING) {
        KeWaitForSingleObject(&event, Executive, KernelMode, FALSE, NULL);
    }
    status = irp->IoStatus.Status;
    ASSERT(NT_SUCCESS(status));
    devExt->pdoState = Stopped;
    uhfDumpDevicesTree();

    IoCompleteRequest(irp, IO_NO_INCREMENT);

    return status;
}

NTSTATUS 
uhfPnpQueryStopDevice(
    PDEVICE_OBJECT deviceObject,
    PIRP irp,
    PUHF_DEVICE_EXT devExt,
    PIO_STACK_LOCATION ioSp)
{
    NTSTATUS status;
    KEVENT event;

    PAGED_CODE();

    //irp->IoStatus.Status = STATUS_SUCCESS;
    IoCopyCurrentIrpStackLocationToNext(irp);
    KeInitializeEvent(&event, NotificationEvent, FALSE);
    IoSetCompletionRoutine(irp, uhfPnpFilterCompletion, &event, TRUE, TRUE, TRUE);
    status = IoCallDriver(devExt->NextDevice, irp);
    if (status == STATUS_PENDING) {
        KeWaitForSingleObject(&event, Executive, KernelMode, FALSE, NULL);
    }
    status = irp->IoStatus.Status;
    if (NT_SUCCESS(status)) {
        devExt->pdoState = PendingStop;
        uhfDumpDevicesTree();
    }

    IoCompleteRequest(irp, IO_NO_INCREMENT);

    return status;
}

NTSTATUS 
uhfPnpCancelStopDevice(
    PDEVICE_OBJECT deviceObject,
    PIRP irp,
    PUHF_DEVICE_EXT devExt,
    PIO_STACK_LOCATION ioSp)
{
    NTSTATUS status;
    KEVENT event;

    PAGED_CODE();

    //irp->IoStatus.Status = STATUS_SUCCESS;
    IoCopyCurrentIrpStackLocationToNext(irp);
    KeInitializeEvent(&event, NotificationEvent, FALSE);
    IoSetCompletionRoutine(irp, uhfPnpFilterCompletion, &event, TRUE, TRUE, TRUE);
    status = IoCallDriver(devExt->NextDevice, irp);
    if (status == STATUS_PENDING) {
        KeWaitForSingleObject(&event, Executive, KernelMode, FALSE, NULL);
    }
    status = irp->IoStatus.Status;
    if (NT_SUCCESS(status)) {
        devExt->pdoState = Working;
        uhfDumpDevicesTree();
    }

    IoCompleteRequest(irp, IO_NO_INCREMENT);

    return status;
}

NTSTATUS 
uhfPnpQueryRemoveDevice(
    PDEVICE_OBJECT deviceObject,
    PIRP irp,
    PUHF_DEVICE_EXT devExt,
    PIO_STACK_LOCATION ioSp)
{
    NTSTATUS status;
    KEVENT event;

    PAGED_CODE();

    //irp->IoStatus.Status = STATUS_SUCCESS;
    IoCopyCurrentIrpStackLocationToNext(irp);
    KeInitializeEvent(&event, NotificationEvent, FALSE);
    IoSetCompletionRoutine(irp, uhfPnpFilterCompletion, &event, TRUE, TRUE, TRUE);
    status = IoCallDriver(devExt->NextDevice, irp);
    if (status == STATUS_PENDING) {
        KeWaitForSingleObject(&event, Executive, KernelMode, FALSE, NULL);
    }
    status = irp->IoStatus.Status;
    if (NT_SUCCESS(status)) {
        devExt->pdoState = PendingRemove;
        uhfDumpDevicesTree();
    }

    IoCompleteRequest(irp, IO_NO_INCREMENT);

    return status;
}

NTSTATUS 
uhfPnpCancelRemoveDevice(
    PDEVICE_OBJECT deviceObject,
    PIRP irp,
    PUHF_DEVICE_EXT devExt,
    PIO_STACK_LOCATION ioSp)
{
    NTSTATUS status;
    KEVENT event;

    PAGED_CODE();

    //irp->IoStatus.Status = STATUS_SUCCESS;
    IoCopyCurrentIrpStackLocationToNext(irp);
    KeInitializeEvent(&event, NotificationEvent, FALSE);
    IoSetCompletionRoutine(irp, uhfPnpFilterCompletion, &event, TRUE, TRUE, TRUE);
    status = IoCallDriver(devExt->NextDevice, irp);
    if (status == STATUS_PENDING) {
        KeWaitForSingleObject(&event, Executive, KernelMode, FALSE, NULL);
    }
    status = irp->IoStatus.Status;
    if (NT_SUCCESS(status)) {
        devExt->pdoState = Working;
        uhfDumpDevicesTree();
    }

    IoCompleteRequest(irp, IO_NO_INCREMENT);

    return status;
}

NTSTATUS 
uhfPnpSurpriseRemoval(
    PDEVICE_OBJECT deviceObject,
    PIRP irp,
    PUHF_DEVICE_EXT devExt,
    PIO_STACK_LOCATION ioSp)
{
    NTSTATUS status;
    KEVENT event;

    PAGED_CODE();

    irp->IoStatus.Status = STATUS_SUCCESS;
    IoCopyCurrentIrpStackLocationToNext(irp);
    KeInitializeEvent(&event, NotificationEvent, FALSE);
    IoSetCompletionRoutine(irp, uhfPnpFilterCompletion, &event, TRUE, TRUE, TRUE);
    status = IoCallDriver(devExt->NextDevice, irp);
    if (status == STATUS_PENDING) {
        KeWaitForSingleObject(&event, Executive, KernelMode, FALSE, NULL);
    }
    status = irp->IoStatus.Status;
    ASSERT(NT_SUCCESS(status));
    devExt->pdoState = SupriseRemoved;
    uhfDumpDevicesTree();

    IoCompleteRequest(irp, IO_NO_INCREMENT);

    return status;
}

NTSTATUS 
uhfPnpQueryDeviceState(
    PDEVICE_OBJECT deviceObject,
    PIRP irp,
    PUHF_DEVICE_EXT devExt,
    PIO_STACK_LOCATION ioSp)
{
    NTSTATUS status;
    KEVENT event;

    PPNP_DEVICE_STATE pnpState;

    PAGED_CODE();

    IoCopyCurrentIrpStackLocationToNext(irp);
    KeInitializeEvent(&event, NotificationEvent, FALSE);
    IoSetCompletionRoutine(irp, uhfPnpFilterCompletion, &event, TRUE, TRUE, TRUE);
    status = IoCallDriver(devExt->NextDevice, irp);
    if (status == STATUS_PENDING) {
        KeWaitForSingleObject(&event, Executive, KernelMode, FALSE, NULL);
    }
    status = irp->IoStatus.Status;
#ifdef DBG    
    pnpState = (PPNP_DEVICE_STATE )&irp->IoStatus.Information;
    if (NT_SUCCESS(status) && (*pnpState)) {
        DbgPrint("[uhf] IRP_MN_QUERY_PNP_DEVICE_STATE(0x%p) 0x%x:\n", devExt->pdo, *pnpState);
        
        if (*pnpState & PNP_DEVICE_DISABLED) {
            DbgPrint("\tPNP_DEVICE_DISABLED\n");
        }

        if (*pnpState & PNP_DEVICE_DONT_DISPLAY_IN_UI) {
            DbgPrint("\tPNP_DEVICE_DONT_DISPLAY_IN_UI\n");
        }

        if (*pnpState & PNP_DEVICE_FAILED) {
            DbgPrint("\tPNP_DEVICE_FAILED\n");
        }

        if (*pnpState & PNP_DEVICE_REMOVED) {
            DbgPrint("\tPNP_DEVICE_REMOVED\n");
        }

        if (*pnpState & PNP_DEVICE_RESOURCE_REQUIREMENTS_CHANGED) {
            DbgPrint("\tPNP_DEVICE_RESOURCE_REQUIREMENTS_CHANGED\n");
        }

        if (*pnpState & PNP_DEVICE_NOT_DISABLEABLE) {
            DbgPrint("\tPNP_DEVICE_NOT_DISABLEABLE\n");
        }
    }
#endif

    IoCompleteRequest(irp, IO_NO_INCREMENT);



    return status;

}

NTSTATUS
uhfPnpQueryDeviceRelations(
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
                        uhfPnpFilterCompletion, 
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

