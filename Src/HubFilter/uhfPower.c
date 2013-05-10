#include <wdm.h>

#include "uhfMain.h"
#include "uhfDevices.h"
#include "uhfPower.h"

#pragma alloc_text(PAGE, uhfDispatchPower)

NTSTATUS 
uhfDispatchPower(
    PDEVICE_OBJECT device,
    PIRP irp)
{
    NTSTATUS status;

    PUHF_DEVICE_EXT devExt;
    PIO_STACK_LOCATION ioSp;

    PAGED_CODE();

    devExt = (PUHF_DEVICE_EXT)device->DeviceExtension;
    ASSERT(devExt->NextDevice);

    status = IoAcquireRemoveLock(&devExt->RemoveLock, irp);
    if (!NT_SUCCESS(status)) {
        irp->IoStatus.Status = status;
        irp->IoStatus.Information = 0;
        IoCompleteRequest(irp, IO_NO_INCREMENT);
        return status;
    }

    ioSp = IoGetCurrentIrpStackLocation(irp);

    switch (ioSp->MinorFunction) {
    case IRP_MN_POWER_SEQUENCE:
        DbgPrint("[uhf] IRP_MN_POWER_SEQUENCE(0x%p)\n", devExt->pdo);
        IoSkipCurrentIrpStackLocation(irp);
        status = IoCallDriver(devExt->NextDevice, irp);
        break;
    case IRP_MN_QUERY_POWER:
        DbgPrint("[uhf] IRP_MN_QUERY_POWER(0x%p)\n", devExt->pdo);
        IoSkipCurrentIrpStackLocation(irp);
        status = IoCallDriver(devExt->NextDevice, irp);
        break;
    case IRP_MN_SET_POWER:
        DbgPrint("[uhf] IRP_MN_SET_POWER(0x%p)\n", devExt->pdo);
        IoSkipCurrentIrpStackLocation(irp);
        status = IoCallDriver(devExt->NextDevice, irp);
        break;
    case IRP_MN_WAIT_WAKE:
        DbgPrint("[uhf] IRP_MN_WAIT_WAKE(0x%p)\n", devExt->pdo);
        IoSkipCurrentIrpStackLocation(irp);
        status = IoCallDriver(devExt->NextDevice, irp);
        break;
    }

    IoReleaseRemoveLock(&devExt->RemoveLock, irp);
    return status;
}