#include <wdm.h>

#include "uhfMain.h"
#include "uhfDevices.h"

NTSTATUS
uhfGetPdoStringProperty (
    PDEVICE_OBJECT PhysicalDeviceObject,
    DEVICE_REGISTRY_PROPERTY DeviceProperty,
    PWCHAR *string,
    PULONG retLength);

NTSTATUS
uhfBuildPdoIdentifiers(
    PDEVICE_OBJECT PhysicalDeviceObject,
    PDEVICE_OBJECT fido,
    PUHF_DEVICE_EXT fidoExt);

PUHF_DEVICE_EXT 
isPdoInRootList(
    PDEVICE_OBJECT pdo);

#pragma alloc(PAGE, uhfAddDevice)
#pragma alloc(PAGE, uhfDeleteDevice)
#pragma alloc(PAGE, uhfGetPdoStringProperty)
#pragma alloc(PAGE, uhfQueryPdoIds)
#pragma alloc(PAGE, uhfBuildPdoIdentifiers)
#pragma alloc(PAGE, isPdoInRootList)

LIST_ENTRY g_rootDevices;
FAST_MUTEX g_rootDevicesMutex;

NTSTATUS 
uhfAddDevice(
    PDRIVER_OBJECT DriverObject,
    PDEVICE_OBJECT pdo)
{
    NTSTATUS status;

    PDEVICE_OBJECT fido = NULL;
    PUHF_DEVICE_EXT fidoExt = NULL;

    ULONG retLength;

    PAGED_CODE();

#ifdef DBG
    DbgPrint("[uhf] uhfAddDevice(0x%p)\n", pdo);
#endif
    __try {
        fidoExt = isPdoInRootList(pdo);
        if (fidoExt) {
            DbgPrint("\t This is hub\n", pdo);
            fidoExt->DeviceRole = UHF_DEVICE_ROLE_HUB_FiDO;
            return STATUS_SUCCESS;
        }

        status = IoCreateDevice(DriverObject, 
                                sizeof(UHF_DEVICE_EXT ), 
                                NULL, 
                                pdo->DeviceType, 
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
        fidoExt->pdo = pdo;
        ExInitializeFastMutex(&fidoExt->fastMutex);
        InitializeListHead(&fidoExt->childs);
        ExAcquireFastMutex(&g_rootDevicesMutex);
        InsertHeadList(&g_rootDevices, &fidoExt->gLink);
        ExReleaseFastMutex(&g_rootDevicesMutex);


        IoInitializeRemoveLock(&fidoExt->RemoveLock, UHF_DEV_EXT_TAG, 0, 0);

        fidoExt->DeviceClassName = NULL;
        fidoExt->DeviceDeviceDescription = NULL;
        fidoExt->deviceHardwareId = NULL;
        fidoExt->deviceCompatibleIds = NULL;
        fidoExt->DeviceManufacturer = NULL;

        status = uhfBuildPdoIdentifiers(pdo, fido, fidoExt);
        if (!NT_SUCCESS(status)) {
            __leave;
        }

        fidoExt->NextDevice = IoAttachDeviceToDeviceStack(fido, pdo);

        fido->Flags = pdo->Flags & (DO_DIRECT_IO | DO_BUFFERED_IO | DO_POWER_PAGABLE);        
        fido->Flags &= ~DO_DEVICE_INITIALIZING;

        status = STATUS_SUCCESS;
    } __finally {
        if (!NT_SUCCESS(status)) {
            if (fido) {
                if (fidoExt) {
                    if (isPdoInRootList(fidoExt->pdo)) {
                        ExAcquireFastMutex(&g_rootDevicesMutex);
                        RemoveEntryList(&fidoExt->gLink);
                        ExReleaseFastMutex(&g_rootDevicesMutex);
                    }

                    if (fidoExt->NextDevice) {
                        IoDetachDevice(fidoExt->NextDevice);
                    }
                }

                uhfDeleteDevice(fido);
            }
        }
    }

    return status;
}

NTSTATUS 
uhfAddChildDevice(
    PDRIVER_OBJECT DriverObject,
    PUHF_DEVICE_EXT parentDevExt,
    PDEVICE_OBJECT pdo)
{
    NTSTATUS status;
    
    PDEVICE_OBJECT fido;
    
    PUHF_DEVICE_EXT fidoExt;

    PAGED_CODE();

    ASSERT(DriverObject);
    ASSERT(parentDevExt);
    ASSERT(pdo);

    if (isPdoInRootList(pdo)) {
        return STATUS_SUCCESS;
    }

#ifdef DBG
    DbgPrint("[uhf] uhfAddChildDevice(0x%p, 0x%p)\n",
            parentDevExt->pdo,
            pdo);
#endif
    __try {
        status = IoCreateDevice(DriverObject, 
                                sizeof(UHF_DEVICE_EXT ), 
                                NULL, 
                                pdo->DeviceType, 
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
        fidoExt->DeviceRole = UHF_DEVICE_ROLE_DEVICE_FiDO;
        fidoExt->pdo = pdo;

        ExInitializeFastMutex(&fidoExt->fastMutex);
        InitializeListHead(&fidoExt->childs);

        ExAcquireFastMutex(&g_rootDevicesMutex);
        InsertHeadList(&g_rootDevices, &fidoExt->gLink);
        ExReleaseFastMutex(&g_rootDevicesMutex);

        IoInitializeRemoveLock(&fidoExt->RemoveLock, UHF_DEV_EXT_TAG, 0, 0);

        fidoExt->DeviceClassName = NULL;
        fidoExt->DeviceDeviceDescription = NULL;
        fidoExt->deviceHardwareId = NULL;
        fidoExt->deviceCompatibleIds = NULL;
        fidoExt->DeviceManufacturer = NULL;

        status = uhfBuildPdoIdentifiers(pdo, fido, fidoExt);
        if (!NT_SUCCESS(status)) {
            __leave;
        }

        fidoExt->NextDevice = IoAttachDeviceToDeviceStack(fido, pdo);

        fido->Flags = pdo->Flags & (DO_DIRECT_IO | DO_BUFFERED_IO | DO_POWER_PAGABLE);        
        fido->Flags &= ~DO_DEVICE_INITIALIZING;

        status = STATUS_SUCCESS;
    } __finally {
        if (!NT_SUCCESS(status)) {
            if (fido) {
                if (fidoExt) {
                    if (isPdoInRootList(fidoExt->pdo)) {
                        ExAcquireFastMutex(&g_rootDevicesMutex);
                        RemoveEntryList(&fidoExt->gLink);
                        ExReleaseFastMutex(&g_rootDevicesMutex);
                    }

                    if (fidoExt->NextDevice) {
                        IoDetachDevice(fidoExt->NextDevice);
                    }
                }

                uhfDeleteDevice(fido);
            }
        }
    }

    fidoExt->parent = parentDevExt;
    ExAcquireFastMutex(&parentDevExt->fastMutex);
    InsertTailList(&parentDevExt->childs, &fidoExt->siblings);
    ExReleaseFastMutex(&parentDevExt->fastMutex);

    return STATUS_SUCCESS;
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

    if (devExt->deviceHardwareId) {
        ExFreePoolWithTag(devExt->deviceHardwareId, UHF_DEV_EXT_TAG);
        devExt->deviceHardwareId = NULL;
    }

    if (devExt->DeviceManufacturer) {
        ExFreePoolWithTag(devExt->DeviceManufacturer, UHF_DEV_EXT_TAG);
        devExt->DeviceManufacturer = NULL;
    }

    if (devExt->deviceCompatibleIds) {
        ExFreePoolWithTag(devExt->deviceCompatibleIds, UHF_DEV_EXT_TAG);
        devExt->deviceCompatibleIds = NULL;
    }

    ExAcquireFastMutex(&g_rootDevicesMutex);
    RemoveEntryList(&devExt->gLink);
    ExReleaseFastMutex(&g_rootDevicesMutex);

    if (devExt->parent) {
        ExAcquireFastMutex(&devExt->parent->fastMutex);
        RemoveEntryList(&devExt->childs);
        ExReleaseFastMutex(&devExt->parent->fastMutex);
    }

    IoDeleteDevice(device);
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
uhfQueryPdoIds(
    PDEVICE_OBJECT pdo,
    BUS_QUERY_ID_TYPE idType,
    PWCHAR *ids, 
    PULONG retLength)
{
    NTSTATUS status;

    PIRP irp;
    PIO_STACK_LOCATION ioSp;

    PWCHAR strIterator;

    KEVENT Event;
    IO_STATUS_BLOCK ioSb;

    PAGED_CODE();

    ASSERT(pdo);
    ASSERT(ids);
    ASSERT(retLength);

    KeInitializeEvent(&Event, NotificationEvent, FALSE);
    irp = IoBuildSynchronousFsdRequest(IRP_MJ_PNP, pdo, NULL, 0, NULL, &Event, &ioSb);
    if (!irp) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    ioSp = IoGetNextIrpStackLocation(irp);
    ioSp->MajorFunction = IRP_MJ_PNP;
    ioSp->MinorFunction = IRP_MN_QUERY_ID;

    ioSp->Parameters.QueryId.IdType = idType;

    ObReferenceObject(pdo);
    status = IoCallDriver(pdo, irp);
    ObDereferenceObject(pdo);
    if (status == STATUS_PENDING) {
        KeWaitForSingleObject(&Event, Executive, KernelMode, FALSE, NULL);
        status = ioSb.Status;
    }
    if (!NT_SUCCESS(status)) {
        return status;
    }
    
    if ((idType == BusQueryHardwareIDs) || (idType == BusQueryCompatibleIDs)) {
        *retLength = 0;
        strIterator = (PWCHAR)ioSb.Information;
        while (*strIterator != 0) {
            *retLength += (wcslen(strIterator) + 1) * sizeof(WCHAR);
            strIterator += wcslen(strIterator) + 1;
        }
        *retLength += sizeof(WCHAR);
    } else {
        *retLength  = (wcslen((PWCHAR)ioSb.Information) + 1) * sizeof(WCHAR);
    }

    *ids = (PWCHAR)ExAllocatePoolWithTag(PagedPool, *retLength, UHF_DEV_EXT_TAG);
    if (!*ids) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    RtlCopyMemory(*ids , (PVOID)ioSb.Information, *retLength);
    ExFreePool((PVOID)ioSb.Information);

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

    PWCHAR str;

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
                                DevicePropertyManufacturer, 
                                &fidoExt->DeviceManufacturer, 
                                &retLength);

        uhfGetPdoStringProperty(PhysicalDeviceObject, 
                                DevicePropertyHardwareID, 
                                &fidoExt->deviceHardwareId, 
                                &retLength);
        uhfGetPdoStringProperty(PhysicalDeviceObject, 
                                DevicePropertyCompatibleIDs, 
                                &fidoExt->deviceCompatibleIds, 
                                &retLength);
/*
#ifdef DBG
        if (fidoExt->DeviceDeviceDescription) {
            DbgPrint("\tDescription \"%ws\"\n", fidoExt->DeviceDeviceDescription);
        }

        DbgPrint("\tdriver \"%wZ\"\n", &PhysicalDeviceObject->DriverObject->DriverName);
        

        if (fidoExt->DeviceClassName) {
            DbgPrint("\tClass \"%ws\"\n", fidoExt->DeviceClassName);
        }

        if (fidoExt->DeviceManufacturer) {
            DbgPrint("\tManufacturer \"%ws\"\n", fidoExt->DeviceManufacturer);
        }

        if (fidoExt->deviceHardwareId) {
            DbgPrint("\tHardware id \"%ws\"\n", fidoExt->deviceHardwareId);
        }

        if (fidoExt->deviceCompatibleIds) {
            DbgPrint("\tCompatible ids:\n");
            str = fidoExt->deviceCompatibleIds;
            while (*str) {
                DbgPrint("\t\t\"%ws\"\n", str);
                str += (wcslen(str) + 1);
            }
            
        }

#endif
*/
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

            if (fidoExt->deviceHardwareId) {
                ExFreePoolWithTag(fidoExt->deviceHardwareId, UHF_DEV_EXT_TAG);
                fidoExt->deviceHardwareId = NULL;
            }

            if (fidoExt->DeviceManufacturer) {
                ExFreePoolWithTag(fidoExt->DeviceManufacturer, UHF_DEV_EXT_TAG);
                fidoExt->DeviceManufacturer = NULL;
            }

            if (fidoExt->deviceCompatibleIds) {
                ExFreePoolWithTag(fidoExt->deviceCompatibleIds, UHF_DEV_EXT_TAG);
                fidoExt->deviceCompatibleIds = NULL;
            }
        }
    }

    return status;
}

PUHF_DEVICE_EXT
isPdoInRootList(
    PDEVICE_OBJECT pdo)
{
    PLIST_ENTRY link;
    PUHF_DEVICE_EXT devExt;

    PAGED_CODE();

    ASSERT(pdo);

    ExAcquireFastMutex(&g_rootDevicesMutex);
    link = g_rootDevices.Flink;
    while (link != &g_rootDevices) {
        devExt = CONTAINING_RECORD(link, UHF_DEVICE_EXT, gLink);
        if (devExt->pdo == pdo) {
            ExReleaseFastMutex(&g_rootDevicesMutex);
            return devExt;
        }

        link = link->Flink;
    }

    ExReleaseFastMutex(&g_rootDevicesMutex);

    return NULL;
}

