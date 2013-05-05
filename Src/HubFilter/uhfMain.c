#include <wdm.h>

#include "uhfMain.h"
#include "uhfDevices.h"

NTSTATUS 
uhfAddDevice(
    PDRIVER_OBJECT DriverObject,
    PDEVICE_OBJECT PhysicalDeviceObject);

NTSTATUS 
uhfDispatchPassThrough(
    PDEVICE_OBJECT DeviceObject,
    PIRP Irp);

NTSTATUS 
uhfDispatchPnP(
    PDEVICE_OBJECT DeviceObject,
    PIRP Irp);

#pragma alloc(INIT, DriverEntry)
#pragma alloc(PAGE, uhfAddDevice)
#pragma alloc(PAGE, uhfDispatchPnP)

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

    DriverObject->DriverExtension->AddDevice = uhfAddDevice;
    for (i = 0; i <= IRP_MJ_MAXIMUM_FUNCTION; i++) {
        DriverObject->MajorFunction[i] = uhfDispatchPassThrough;
    }

    DriverObject->MajorFunction[IRP_MJ_PNP] = uhfDispatchPnP;

    return STATUS_SUCCESS;
}

NTSTATUS
uhfGetPdoStringProperty (
    PDEVICE_OBJECT PhysicalDeviceObject,
    DEVICE_REGISTRY_PROPERTY DeviceProperty,
    PWCHAR *string,
    PULONG retLength)
{
    NTSTATUS status;

    ULONG bufferSize;
    
    PAGED_CODE();

    if (!PhysicalDeviceObject || !string || !retLength) {
        return STATUS_INVALID_PARAMETER;
    }

    *string = NULL;
    bufferSize = 512;
    *string = (PWCHAR)ExAllocatePoolWithTag(NonPagedPool, 
                                            bufferSize, 
                                            UHF_DEV_EXT_TAG);

    status = IoGetDeviceProperty(PhysicalDeviceObject, 
                                DeviceProperty, 
                                bufferSize, 
                                *string, 
                                retLength);

    if (status == STATUS_BUFFER_TOO_SMALL) {
        ExFreePoolWithTag(*string, UHF_DEV_EXT_TAG);
        bufferSize += 512;
        *string = (PWCHAR)ExAllocatePoolWithTag(NonPagedPool, 
                                                bufferSize, 
                                                UHF_DEV_EXT_TAG);

        status = IoGetDeviceProperty(PhysicalDeviceObject, 
                                    DeviceProperty, 
                                    bufferSize, 
                                    *string, 
                                    retLength);
    }

    if(!NT_SUCCESS(status) && *string) {
        ExFreePoolWithTag(*string, UHF_DEV_EXT_TAG);
        *string = NULL;
    }

    return status;
}

NTSTATUS
uhfBuildPdoIdentifiers(
    PDEVICE_OBJECT PhysicalDeviceObject,
    PDEVICE_OBJECT fido,
    PUHF_DEVICE_EXT fidoExt)
{
    NTSTATUS status = STATUS_SUCCESS;
    ULONG retLength;

    PAGED_CODE();

    __try {
        uhfGetPdoStringProperty(PhysicalDeviceObject, 
                                        DevicePropertyClassName, 
                                        &fidoExt->DeviceClassName, 
                                        &retLength);
        uhfGetPdoStringProperty(PhysicalDeviceObject, 
                                        DevicePropertyDeviceDescription, 
                                        &fidoExt->DeviceDeviceDescription, 
                                        &retLength);
        uhfGetPdoStringProperty(PhysicalDeviceObject, 
                                        DevicePropertyFriendlyName, 
                                        &fidoExt->DeviceFriendlyName, 
                                        &retLength);
        uhfGetPdoStringProperty(PhysicalDeviceObject, 
                                        DevicePropertyManufacturer, 
                                        &fidoExt->DeviceManufacturer, 
                                        &retLength);
        uhfGetPdoStringProperty(PhysicalDeviceObject, 
                                        DevicePropertyDriverKeyName , 
                                        &fidoExt->DeviceDriverKeyName , 
                                        &retLength);
#ifdef DBG
        if (fidoExt->DeviceFriendlyName) {
            DbgPrint("[uhf]\tFriendly name \"%ws\"\n", fidoExt->DeviceFriendlyName);
        }

        if (fidoExt->DeviceDeviceDescription) {
            DbgPrint("[uhf]\tDescription \"%ws\"\n", fidoExt->DeviceDeviceDescription);
        }

        if (fidoExt->DeviceClassName) {
            DbgPrint("[uhf]\tClass \"%ws\"\n", fidoExt->DeviceClassName);
        }

        if (fidoExt->DeviceManufacturer) {
            DbgPrint("[uhf]\tManufacturer \"%ws\"\n", fidoExt->DeviceManufacturer);
        }

        if (fidoExt->DeviceDriverKeyName) {
            DbgPrint("[uhf]\tDriver key name \"%ws\"\n", fidoExt->DeviceDriverKeyName);
        }
#endif
    } __finally {
        if (!NT_SUCCESS(status)) {
#ifdef DBG
            DbgPrint("[uhf] uhfBuildPdoIdentifiers ERROR(%s)\n", OsrNTStatusToString(status));
#endif
            if (fidoExt->DeviceClassName) {
                ExFreePoolWithTag(fidoExt->DeviceClassName, UHF_DEV_EXT_TAG);
                fidoExt->DeviceClassName = NULL;
            }

            if (fidoExt->DeviceDeviceDescription) {
                ExFreePoolWithTag(fidoExt->DeviceDeviceDescription, UHF_DEV_EXT_TAG);
                fidoExt->DeviceDeviceDescription = NULL;
            }

            if (fidoExt->DeviceFriendlyName) {
                ExFreePoolWithTag(fidoExt->DeviceFriendlyName, UHF_DEV_EXT_TAG);
                fidoExt->DeviceFriendlyName = NULL;
            }

            if (fidoExt->DeviceManufacturer) {
                ExFreePoolWithTag(fidoExt->DeviceManufacturer, UHF_DEV_EXT_TAG);
                fidoExt->DeviceManufacturer = NULL;
            }

            if (fidoExt->DeviceDriverKeyName) {
                ExFreePoolWithTag(fidoExt->DeviceDriverKeyName, UHF_DEV_EXT_TAG);
                fidoExt->DeviceDriverKeyName = NULL;
            }
        }
    }

    return status;
}

VOID 
uhfDeleteDevice(
    PDEVICE_OBJECT device)
{
    PUHF_DEVICE_EXT devExt;

    ASSERT(device);
    ASSERT(device->DeviceExtension);

    PAGED_CODE();

    devExt = (PUHF_DEVICE_EXT)device->DeviceExtension;
    if (devExt->DeviceClassName) {
        ExFreePoolWithTag(devExt->DeviceClassName, UHF_DEV_EXT_TAG);
        devExt->DeviceClassName = NULL;
    }

    if (devExt->DeviceDeviceDescription) {
        ExFreePoolWithTag(devExt->DeviceDeviceDescription, UHF_DEV_EXT_TAG);
        devExt->DeviceDeviceDescription = NULL;
    }

    if (devExt->DeviceFriendlyName) {
        ExFreePoolWithTag(devExt->DeviceFriendlyName, UHF_DEV_EXT_TAG);
        devExt->DeviceFriendlyName = NULL;
    }

    if (devExt->DeviceManufacturer) {
        ExFreePoolWithTag(devExt->DeviceManufacturer, UHF_DEV_EXT_TAG);
        devExt->DeviceManufacturer = NULL;
    }

    if (devExt->DeviceDriverKeyName) {
        ExFreePoolWithTag(devExt->DeviceDriverKeyName, UHF_DEV_EXT_TAG);
        devExt->DeviceDriverKeyName = NULL;
    }
    IoDeleteDevice(device);
}


NTSTATUS 
uhfAddDevice(
    PDRIVER_OBJECT DriverObject,
    PDEVICE_OBJECT PhysicalDeviceObject)
{
    NTSTATUS status;

    PDEVICE_OBJECT fido = NULL;
    PUHF_DEVICE_EXT fidoExt;

    ULONG retLength;

    PAGED_CODE();

#ifdef DBG
    DbgPrint("[uhf] uhfAddDevice(0x%p)\n", PhysicalDeviceObject);
#endif
    __try {
        status = IoCreateDevice(DriverObject, 
                                sizeof(UHF_DEVICE_EXT ), 
                                NULL, 
                                PhysicalDeviceObject->DeviceType, 
                                0, 
                                FALSE, 
                                &fido);
        if (!NT_SUCCESS(status)) {
#ifdef DBG
            DbgPrint("[uhf] uhfAddDevice::IoCreateDevice ERROR(%s)\n", OsrNTStatusToString(status));
#endif
            __leave;
        }

        fidoExt = (PUHF_DEVICE_EXT)fido->DeviceExtension;
        fidoExt->DeviceRole = UHF_DEVICE_ROLE_HUB_FiDO;

        fidoExt->DeviceClassName = NULL;
        fidoExt->DeviceDeviceDescription = NULL;
        fidoExt->DeviceFriendlyName = NULL;
        fidoExt->DeviceManufacturer = NULL;

        status = uhfBuildPdoIdentifiers(PhysicalDeviceObject, fido, fidoExt);
        if (!NT_SUCCESS(status)) {
            __leave;
        }

        fidoExt->NextDevice = IoAttachDeviceToDeviceStack(fido, PhysicalDeviceObject);

        fido->Flags = PhysicalDeviceObject->Flags & (DO_DIRECT_IO | DO_BUFFERED_IO | DO_POWER_PAGABLE);
        fido->Flags &= ~DO_DEVICE_INITIALIZING;

        status = STATUS_SUCCESS;
    } __finally {
        if (!NT_SUCCESS(status)) {
            if (fidoExt->NextDevice) {
                IoDetachDevice(fidoExt->NextDevice);
            }

            if (fido) {
                uhfDeleteDevice(fido);
            }
        }
    }

    return status;
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

    status = IoCallDriver(devExt->NextDevice, Irp);

    return status;
}

NTSTATUS 
uhfDispatchPnP(
    PDEVICE_OBJECT DeviceObject,
    PIRP Irp)
{
    NTSTATUS status;

    PUHF_DEVICE_EXT devExt;
    PIO_STACK_LOCATION ioSp;

    devExt = (PUHF_DEVICE_EXT)DeviceObject->DeviceExtension;
    ASSERT(devExt->NextDevice);

#ifdef DBG
    DbgPrint("[uhf] uhfDispatchPnP(\"%ws\")\n", 
        devExt->DeviceDeviceDescription);
#endif

    ioSp = IoGetCurrentIrpStackLocation(Irp);
    switch (ioSp->MinorFunction) {
    case IRP_MN_REMOVE_DEVICE:
    case IRP_MN_SURPRISE_REMOVAL:
#ifdef DBG
        DbgPrint("[uhf]\t%s\n", 
                ioSp->MinorFunction == IRP_MN_REMOVE_DEVICE?"IRP_MN_REMOVE_DEVICE":"IRP_MN_SURPRISE_REMOVAL");
#endif
        IoSkipCurrentIrpStackLocation(Irp);
        status = IoCallDriver(devExt->NextDevice, Irp);
        IoDetachDevice(devExt->NextDevice);
        uhfDeleteDevice(DeviceObject);
        return status;
#ifdef DBG
    case IRP_MN_START_DEVICE:
        DbgPrint("[uhf]\tIRP_MN_START_DEVICE\n");
        break;
    case IRP_MN_QUERY_REMOVE_DEVICE:
        DbgPrint("[uhf]\tIRP_MN_QUERY_REMOVE_DEVICE\n");
        break;
    case IRP_MN_CANCEL_REMOVE_DEVICE:
        DbgPrint("[uhf]\tIRP_MN_CANCEL_REMOVE_DEVICE\n");
        break;
    case IRP_MN_STOP_DEVICE:
        DbgPrint("[uhf]\tIRP_MN_STOP_DEVICE\n");
        break;
    case IRP_MN_QUERY_STOP_DEVICE:
        DbgPrint("[uhf]\tIRP_MN_QUERY_STOP_DEVICE\n");
        break;
    case IRP_MN_CANCEL_STOP_DEVICE:
        DbgPrint("[uhf]\tIRP_MN_CANCEL_STOP_DEVICE\n");
        break;
    case IRP_MN_QUERY_DEVICE_RELATIONS:
        DbgPrint("[uhf]\tIRP_MN_QUERY_DEVICE_RELATIONS\n");
        break;
    case IRP_MN_QUERY_INTERFACE:
        DbgPrint("[uhf]\tIRP_MN_QUERY_INTERFACE\n");
        break;
    case IRP_MN_QUERY_CAPABILITIES:
        DbgPrint("[uhf]\tIRP_MN_QUERY_CAPABILITIES\n");
        break;
    case IRP_MN_QUERY_RESOURCES:
        DbgPrint("[uhf]\tIRP_MN_QUERY_RESOURCES\n");
        break;
    case IRP_MN_QUERY_RESOURCE_REQUIREMENTS:
        DbgPrint("[uhf]\tIRP_MN_QUERY_RESOURCE_REQUIREMENTS\n");
        break;
    case IRP_MN_QUERY_DEVICE_TEXT:
        DbgPrint("[uhf]\tIRP_MN_QUERY_DEVICE_TEXT\n");
        break;
    case IRP_MN_FILTER_RESOURCE_REQUIREMENTS:
        DbgPrint("[uhf]\tIRP_MN_FILTER_RESOURCE_REQUIREMENTS\n");
        break;
    case IRP_MN_READ_CONFIG:
        DbgPrint("[uhf]\tIRP_MN_READ_CONFIG\n");
        break;
    case IRP_MN_WRITE_CONFIG:
        DbgPrint("[uhf]\tIRP_MN_WRITE_CONFIG\n");
        break;
    case IRP_MN_EJECT:
        DbgPrint("[uhf]\tIRP_MN_EJECT\n");
        break;
    case IRP_MN_SET_LOCK:
        DbgPrint("[uhf]\tIRP_MN_SET_LOCK\n");
        break;
    case IRP_MN_QUERY_ID:
        DbgPrint("[uhf]\tIRP_MN_QUERY_ID\n");
        break;
    case IRP_MN_QUERY_PNP_DEVICE_STATE:
        DbgPrint("[uhf]\tIRP_MN_QUERY_PNP_DEVICE_STATE\n");
        break;
    case IRP_MN_QUERY_BUS_INFORMATION:
        DbgPrint("[uhf]\tIRP_MN_QUERY_BUS_INFORMATION\n");
        break;
    case IRP_MN_DEVICE_USAGE_NOTIFICATION:
        DbgPrint("[uhf]\tIRP_MN_DEVICE_USAGE_NOTIFICATION\n");
        break;
#endif    
    }

    IoSkipCurrentIrpStackLocation(Irp);

    status = IoCallDriver(devExt->NextDevice, Irp);

    return status;
}
