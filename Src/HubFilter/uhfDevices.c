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

VOID
uhfAddPdoInGlobalList(
    PUHF_DEVICE_EXT devExt);

VOID
uhfRemovePdoFromGlobalList(
    PUHF_DEVICE_EXT devExt);

PUHF_DEVICE_EXT 
uhfIsPdoInChildList(
    PUHF_DEVICE_EXT devExt);

VOID 
uhfAddPdoToChildList(
    PUHF_DEVICE_EXT parentDevExt,
    PUHF_DEVICE_EXT devExt);

VOID 
uhfRemovePdoFromChildList(
    PUHF_DEVICE_EXT devExt);

VOID
uhfDumpDevicesTree(VOID);


#pragma alloc_text(PAGE, uhfAddDevice)
#pragma alloc_text(PAGE, uhfDeleteDevice)
#pragma alloc_text(PAGE, uhfGetPdoStringProperty)
#pragma alloc_text(PAGE, uhfQueryPdoText)
#pragma alloc_text(PAGE, uhfQueryPdoIds)
#pragma alloc_text(PAGE, uhfBuildPdoIdentifiers)

#pragma alloc_text(PAGE, uhfIsPdoInGlobalList)
#pragma alloc_text(PAGE, uhfAddPdoInGlobalList)
#pragma alloc_text(PAGE, uhfRemovePdoFromGlobalList)
#pragma alloc_text(PAGE, uhfIsPdoInChildList)
#pragma alloc_text(PAGE, uhfAddPdoToChildList)
#pragma alloc_text(PAGE, uhfRemovePdoFromChildList)

#pragma alloc(PAGE, uhfDumpDevicesTree)

LIST_ENTRY g_pdoList;
FAST_MUTEX g_pdoListMutex;

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

    __try {
        fidoExt = uhfIsPdoInGlobalList(pdo);
        if (fidoExt) {
            fidoExt->DeviceRole = UHF_DEVICE_ROLE_HUB_FiDO;
            return STATUS_SUCCESS;
        }

#ifdef DBG
    DbgPrint("[uhf] uhfAddDevice(0x%p)\n", pdo);
#endif

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
        RtlZeroMemory(fidoExt, sizeof(UHF_DEVICE_EXT));

        fidoExt->DeviceRole = UHF_DEVICE_ROLE_HUB_FiDO;
        fidoExt->pdo = pdo;
        fidoExt->pdoState = Stopped;
        ExInitializeFastMutex(&fidoExt->fastMutex);
        InitializeListHead(&fidoExt->childs);
        
        IoInitializeRemoveLock(&fidoExt->RemoveLock, UHF_DEV_EXT_TAG, 0, 0);

        status = uhfBuildPdoIdentifiers(pdo, fido, fidoExt);
        if (!NT_SUCCESS(status)) {
            __leave;
        }

        fidoExt->NextDevice = IoAttachDeviceToDeviceStack(fido, pdo);

        fido->Flags = pdo->Flags & (DO_POWER_INRUSH | DO_POWER_PAGABLE | DO_DIRECT_IO | DO_BUFFERED_IO | DO_POWER_PAGABLE);        
        fido->Flags &= ~DO_DEVICE_INITIALIZING;

        uhfAddPdoInGlobalList(fidoExt);
        uhfAddPdoToChildList(NULL, fidoExt);

        status = STATUS_SUCCESS;
    } __finally {
        if (!NT_SUCCESS(status)) {
            if (fido) {
                if (fidoExt) {
                    if (uhfIsPdoInGlobalList(fidoExt->pdo)) {
                        uhfRemovePdoFromGlobalList(fidoExt);
                    }

                    if (uhfIsPdoInChildList(fidoExt)) {
                        uhfRemovePdoFromChildList(fidoExt);
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

    if (uhfIsPdoInGlobalList(pdo)) {
        return STATUS_SUCCESS;
    }

#ifdef DBG
    DbgPrint("[uhf] uhfAddChildDevice(0x%p, 0x%p)\n", parentDevExt->pdo, pdo);
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
        fidoExt->pdoState = Stopped;

        ExInitializeFastMutex(&fidoExt->fastMutex);
        InitializeListHead(&fidoExt->childs);

        IoInitializeRemoveLock(&fidoExt->RemoveLock, UHF_DEV_EXT_TAG, 0, 0);

        status = uhfBuildPdoIdentifiers(pdo, fido, fidoExt);
        if (!NT_SUCCESS(status)) {
            __leave;
        }

        fidoExt->NextDevice = IoAttachDeviceToDeviceStack(fido, pdo);

        fido->Flags = pdo->Flags & (DO_POWER_INRUSH | DO_POWER_PAGABLE | DO_DIRECT_IO | DO_BUFFERED_IO | DO_POWER_PAGABLE);        
        fido->Flags &= ~DO_DEVICE_INITIALIZING;

        uhfAddPdoInGlobalList(fidoExt);
        uhfAddPdoToChildList(parentDevExt, fidoExt);

        status = STATUS_SUCCESS;
    } __finally {
        if (!NT_SUCCESS(status)) {
            if (fido) {
                if (fidoExt) {
                    if (uhfIsPdoInGlobalList(fidoExt->pdo)) {
                        uhfRemovePdoFromGlobalList(fidoExt);
                    }

                    if (fidoExt->NextDevice) {
                        IoDetachDevice(fidoExt->NextDevice);
                    }
                }

                uhfDeleteDevice(fido);
            }
        }
    }

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

#ifdef DBG
    DbgPrint("[uhf]\tuhfDeleteDevice(0x%p):\"%ws\"\n", 
            devExt->pdo, devExt->pdoDescription.deviceId.Buffer);
    if (devExt->pdoDescription.description.Buffer) {
        DbgPrint("\t\"%ws\"\n", devExt->pdoDescription.description.Buffer);
    }
#endif
    
    if (devExt->pdoDescription.deviceId.Buffer) {
        ExFreePoolWithTag(devExt->pdoDescription.deviceId.Buffer, UHF_DEV_EXT_TAG);
    }

    if (devExt->pdoDescription.hardwareIds.Buffer) {
        ExFreePoolWithTag(devExt->pdoDescription.hardwareIds.Buffer, UHF_DEV_EXT_TAG);
    }

    if (devExt->pdoDescription.compatibleIds.Buffer) {
        ExFreePoolWithTag(devExt->pdoDescription.compatibleIds.Buffer, UHF_DEV_EXT_TAG);
    }

    if (devExt->pdoDescription.instanceId.Buffer) {
        ExFreePoolWithTag(devExt->pdoDescription.instanceId.Buffer, UHF_DEV_EXT_TAG);
    }

    if (devExt->pdoDescription.serialNumber.Buffer) {
        ExFreePoolWithTag(devExt->pdoDescription.serialNumber.Buffer, UHF_DEV_EXT_TAG);
    }

    if (devExt->pdoDescription.containerId.Buffer) {
        ExFreePoolWithTag(devExt->pdoDescription.containerId.Buffer, UHF_DEV_EXT_TAG);
    }

    if (devExt->pdoDescription.description.Buffer) {
        ExFreePoolWithTag(devExt->pdoDescription.description.Buffer, UHF_DEV_EXT_TAG);
    }

    if (devExt->pdoDescription.location.Buffer) {
        ExFreePoolWithTag(devExt->pdoDescription.location.Buffer, UHF_DEV_EXT_TAG);
    }

    RtlZeroMemory(&devExt->pdoDescription, sizeof(devExt->pdoDescription));

    uhfRemovePdoFromGlobalList(devExt);
    uhfRemovePdoFromChildList(devExt);

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

    *ids = NULL;
    *retLength = 0;

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
    if (!NT_SUCCESS(status) || (ioSb.Information == 0)) {
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
uhfQueryPdoText(
    PDEVICE_OBJECT pdo,
    DEVICE_TEXT_TYPE textType,
    PWCHAR *text, 
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
    ASSERT(text);
    ASSERT(retLength);

    *text = NULL;
    *retLength = 0;

    KeInitializeEvent(&Event, NotificationEvent, FALSE);
    irp = IoBuildSynchronousFsdRequest(IRP_MJ_PNP, pdo, NULL, 0, NULL, &Event, &ioSb);
    if (!irp) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    ioSp = IoGetNextIrpStackLocation(irp);
    ioSp->MajorFunction = IRP_MJ_PNP;
    ioSp->MinorFunction = IRP_MN_QUERY_DEVICE_TEXT;

    ioSp->Parameters.QueryDeviceText.DeviceTextType = textType;
    ioSp->Parameters.QueryDeviceText.LocaleId = MAKELCID(MAKELANGID(LANG_NEUTRAL, SUBLANG_CUSTOM_DEFAULT), SORT_DEFAULT);

    ObReferenceObject(pdo);
    status = IoCallDriver(pdo, irp);
    ObDereferenceObject(pdo);
    if (status == STATUS_PENDING) {
        KeWaitForSingleObject(&Event, Executive, KernelMode, FALSE, NULL);
        status = ioSb.Status;
    }
    if (!NT_SUCCESS(status) || (ioSb.Information == 0)) {
        return status;
    }
    
    *retLength  = (wcslen((PWCHAR)ioSb.Information) + 1) * sizeof(WCHAR);

    *text = (PWCHAR)ExAllocatePoolWithTag(PagedPool, *retLength, UHF_DEV_EXT_TAG);
    if (!*text) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    RtlCopyMemory(*text, (PVOID)ioSb.Information, *retLength);
    ExFreePool((PVOID)ioSb.Information);

    return status;
}


NTSTATUS
uhfBuildPdoIdentifiers(
    PDEVICE_OBJECT pdo,
    PDEVICE_OBJECT fido,
    PUHF_DEVICE_EXT fidoExt)
{
    NTSTATUS status = STATUS_SUCCESS;
    ULONG retLength;

    PWCHAR id;

    PAGED_CODE();

    RtlZeroMemory(&fidoExt->pdoDescription, sizeof(fidoExt->pdoDescription));

    status = uhfQueryPdoIds(pdo, BusQueryDeviceID, &id, &retLength);
    if (NT_SUCCESS(status) && id) {
        fidoExt->pdoDescription.deviceId.Buffer = id;
        fidoExt->pdoDescription.deviceId.Length = (USHORT)retLength;
        fidoExt->pdoDescription.deviceId.MaximumLength = (USHORT)retLength;
    }

    status = uhfQueryPdoIds(pdo, BusQueryHardwareIDs, &id, &retLength);
    if (NT_SUCCESS(status) && id) {
        fidoExt->pdoDescription.hardwareIds.Buffer = id;
        fidoExt->pdoDescription.hardwareIds.Length = (USHORT)retLength;
        fidoExt->pdoDescription.hardwareIds.MaximumLength = (USHORT)retLength;
    }

    status = uhfQueryPdoIds(pdo, BusQueryCompatibleIDs, &id, &retLength);
    if (NT_SUCCESS(status) && id) {
        fidoExt->pdoDescription.compatibleIds.Buffer = id;
        fidoExt->pdoDescription.compatibleIds.Length = (USHORT)retLength;
        fidoExt->pdoDescription.compatibleIds.MaximumLength = (USHORT)retLength;
    }

    status = uhfQueryPdoIds(pdo, BusQueryInstanceID, &id, &retLength);
    if (NT_SUCCESS(status) && id) {
        fidoExt->pdoDescription.instanceId.Buffer = id;
        fidoExt->pdoDescription.instanceId.Length = (USHORT)retLength;
        fidoExt->pdoDescription.instanceId.MaximumLength = (USHORT)retLength;
    }

    status = uhfQueryPdoIds(pdo, BusQueryDeviceSerialNumber, &id, &retLength);
    if (NT_SUCCESS(status) && id) {
        fidoExt->pdoDescription.serialNumber.Buffer = id;
        fidoExt->pdoDescription.serialNumber.Length = (USHORT)retLength;
        fidoExt->pdoDescription.serialNumber.MaximumLength = (USHORT)retLength;
    }

    status = uhfQueryPdoIds(pdo, BusQueryContainerID, &id, &retLength);
    if (NT_SUCCESS(status) && id) {
        fidoExt->pdoDescription.containerId.Buffer = id;
        fidoExt->pdoDescription.containerId.Length = (USHORT)retLength;
        fidoExt->pdoDescription.containerId.MaximumLength = (USHORT)retLength;
    }

    status = uhfQueryPdoText(pdo, DeviceTextDescription, &id, &retLength);
    if (NT_SUCCESS(status) && id) {
        fidoExt->pdoDescription.description.Buffer = id;
        fidoExt->pdoDescription.description.Length = (USHORT)retLength;
        fidoExt->pdoDescription.description.MaximumLength = (USHORT)retLength;
    }

    status = uhfQueryPdoText(pdo, DeviceTextLocationInformation, &id, &retLength);
    if (NT_SUCCESS(status) && id) {
        fidoExt->pdoDescription.location.Buffer = id;
        fidoExt->pdoDescription.location.Length = (USHORT)retLength;
        fidoExt->pdoDescription.location.MaximumLength = (USHORT)retLength;
    }

    status = STATUS_SUCCESS;

#ifdef DBG
    if (fidoExt->pdoDescription.deviceId.Buffer) {
        DbgPrint("\tdeviceId: \"%ws\"\n", fidoExt->pdoDescription.deviceId.Buffer);
    }

    if (fidoExt->pdoDescription.hardwareIds.Buffer) {
        DbgPrint("\thardwareIds:\n");
        id = fidoExt->pdoDescription.hardwareIds.Buffer;
        while (*id) {
            DbgPrint("\t\t\"%ws\"\n", id);
            id += wcslen(id) + 1;
        }
    }

    if (fidoExt->pdoDescription.compatibleIds.Buffer) {
        DbgPrint("\tcompatibleIds:\n");
        id = fidoExt->pdoDescription.compatibleIds.Buffer;
        while (*id) {
            DbgPrint("\t\t\"%ws\"\n", id);
            id += wcslen(id) + 1;
        }
    }

    if (fidoExt->pdoDescription.instanceId.Buffer) {
        DbgPrint("\tinstanceId: \"%ws\"\n", fidoExt->pdoDescription.instanceId.Buffer);
    }

    if (fidoExt->pdoDescription.serialNumber.Buffer) {
        DbgPrint("\tserialNumber: \"%ws\"\n", fidoExt->pdoDescription.serialNumber.Buffer);
    }

    if (fidoExt->pdoDescription.containerId.Buffer) {
        DbgPrint("\tcontainerId: \"%ws\"\n", fidoExt->pdoDescription.containerId.Buffer);
    }

    if (fidoExt->pdoDescription.description.Buffer) {
        DbgPrint("\tdescription: \"%ws\"\n", fidoExt->pdoDescription.description.Buffer);
    }

    if (fidoExt->pdoDescription.location.Buffer) {
        DbgPrint("\tlocation: \"%ws\"\n", fidoExt->pdoDescription.location.Buffer);
    }

#endif
    return status;
}

PUHF_DEVICE_EXT
uhfIsPdoInGlobalList(
    PDEVICE_OBJECT pdo)
{
    PLIST_ENTRY link;
    PUHF_DEVICE_EXT devExt;

    PAGED_CODE();

    ASSERT(pdo);

    ExAcquireFastMutex(&g_pdoListMutex);
    link = g_pdoList.Flink;
    while (link != &g_pdoList) {
        devExt = CONTAINING_RECORD(link, UHF_DEVICE_EXT, gLink);
        if (devExt->pdo == pdo) {
            ExReleaseFastMutex(&g_pdoListMutex);
            return devExt;
        }

        link = link->Flink;
    }

    ExReleaseFastMutex(&g_pdoListMutex);

    return NULL;
}

VOID
uhfAddPdoInGlobalList(
    PUHF_DEVICE_EXT devExt)
{
    PAGED_CODE();

    ASSERT(devExt);

    ExAcquireFastMutex(&g_pdoListMutex);
    InsertTailList(&g_pdoList, &devExt->gLink);
    ExReleaseFastMutex(&g_pdoListMutex);
}

VOID
uhfRemovePdoFromGlobalList(
    PUHF_DEVICE_EXT devExt)
{
    PAGED_CODE();

    ASSERT(devExt);

    ExAcquireFastMutex(&g_pdoListMutex);
    RemoveEntryList(&devExt->gLink);
    ExReleaseFastMutex(&g_pdoListMutex);
}

PWCHAR
uhfPdoStateToStr(
    UHF_PDO_STATE state)
{
    switch(state) {
    case Stopped:
        return L"Stopped";
    case Working:
        return L"Working";
    case PendingStop:
        return L"PendingStop";
    case PendingRemove:
        return L"PendingRemove";
    case SupriseRemoved:
        return L"SupriseRemoved";
    default:
        return L"ERROR";
    }
}

VOID 
uhfDumpTreeBrunch(
    PUHF_DEVICE_EXT devExt,
    PWCHAR prefix)
{
    PLIST_ENTRY link;
    PUHF_DEVICE_EXT childDevExt;

    WCHAR localPrefix[256];

    PAGED_CODE();
    ASSERT(devExt);
    ASSERT(prefix);

    DbgPrint("%ws [%ws] 0x%p:\"%ws\"; \"%ws\"\n", 
            prefix, 
            uhfPdoStateToStr(devExt->pdoState),
            devExt->pdo, 
            devExt->pdoDescription.deviceId.Buffer, 
            devExt->pdoDescription.description.Buffer);
    RtlCopyMemory(localPrefix, prefix, sizeof(localPrefix));
    localPrefix[wcslen(localPrefix)] = '\t';
    ExAcquireFastMutex(&devExt->fastMutex);
    for (link = devExt->childs.Flink; link != &devExt->childs; link = link->Flink) {
        childDevExt = CONTAINING_RECORD(link, UHF_DEVICE_EXT, siblings);
        
        uhfDumpTreeBrunch(childDevExt, localPrefix);
    }
    ExReleaseFastMutex(&devExt->fastMutex);
}

VOID
uhfDumpDevicesTree(VOID)
{
    PLIST_ENTRY link;
    PUHF_DEVICE_EXT devExt;

    WCHAR prefix[256];

    PAGED_CODE();

    DbgPrint("\n[uhf] Device Tree\n");
    RtlZeroMemory(prefix, sizeof(prefix));

    prefix[0] = '\t';

    ExAcquireFastMutex(&g_rootDevicesMutex);
    for (link = g_rootDevices.Flink; link != &g_rootDevices; link = link->Flink) {
        devExt = CONTAINING_RECORD(link, UHF_DEVICE_EXT, siblings);
        uhfDumpTreeBrunch(devExt, prefix);
    }
    ExReleaseFastMutex(&g_rootDevicesMutex);
}

PUHF_DEVICE_EXT 
uhfIsPdoInChildList(
    PUHF_DEVICE_EXT devExt)
{
    PLIST_ENTRY link;
    PUHF_DEVICE_EXT devExtIterator;

    PLIST_ENTRY listHead;
    PFAST_MUTEX mutex;

    PAGED_CODE();

    ASSERT(devExt);

    if (devExt->parent) {
        listHead = &devExt->parent->childs;
        mutex = &devExt->parent->fastMutex;
    } else {
        listHead = &g_rootDevices;
        mutex = &g_rootDevicesMutex;
    }

    ExAcquireFastMutex(mutex);
    link = listHead->Flink;
    while (link != listHead) {
        devExtIterator = CONTAINING_RECORD(link, UHF_DEVICE_EXT, childs);
        if (devExt == devExtIterator) {
            ExReleaseFastMutex(mutex);
            return devExt;
        }

        link = link->Flink;
    }

    ExReleaseFastMutex(mutex);

    return NULL;
}

VOID 
uhfAddPdoToChildList(
    PUHF_DEVICE_EXT parentDevExt,
    PUHF_DEVICE_EXT devExt)
{
    PAGED_CODE();

    ASSERT(devExt);

    if (parentDevExt) {
        ExAcquireFastMutex(&parentDevExt->fastMutex);
        InsertTailList(&parentDevExt->childs, &devExt->siblings);
        ExReleaseFastMutex(&parentDevExt->fastMutex);
        devExt->parent = parentDevExt;
    } else {
        ExAcquireFastMutex(&g_rootDevicesMutex);
        InsertTailList(&g_rootDevices, &devExt->siblings);
        ExReleaseFastMutex(&g_rootDevicesMutex);
        devExt->parent = NULL;
    }

    uhfDumpDevicesTree();
}

VOID 
uhfRemovePdoFromChildList(
    PUHF_DEVICE_EXT devExt)
{
    PAGED_CODE();

    ASSERT(devExt);

    if (devExt->parent) {
        ExAcquireFastMutex(&devExt->parent->fastMutex);
        RemoveEntryList(&devExt->siblings);
        ExReleaseFastMutex(&devExt->parent->fastMutex);
    } else {
        ExAcquireFastMutex(&g_rootDevicesMutex);
        RemoveEntryList(&devExt->siblings);
        ExReleaseFastMutex(&g_rootDevicesMutex);
    }

    uhfDumpDevicesTree();
}
