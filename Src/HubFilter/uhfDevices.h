#ifndef __UHF_DEVICES_H__
#define __UHF_DEVICES_H__

#define UHF_DEVICE_ROLE_HUB_FiDO 0x1
#define UHF_DEVICE_ROLE_DEVICE_FiDO 0x2


extern LIST_ENTRY g_rootDevices;
extern FAST_MUTEX g_rootDevicesMutex;

LIST_ENTRY g_pdoList;
FAST_MUTEX g_pdoListMutex;

typedef struct _UHF_PDO_DESCRIPTION {
    UNICODE_STRING deviceId;
    UNICODE_STRING hardwareIds;
    UNICODE_STRING compatibleIds;
    UNICODE_STRING instanceId;
    UNICODE_STRING serialNumber;
    UNICODE_STRING containerId;
    UNICODE_STRING description;
    UNICODE_STRING location;
} UHF_PDO_DESCRIPTION, *PUHF_PDO_DESCRIPTION;

typedef enum _UHF_PDO_STATE {
    Stopped,
    Working,
    PendingStop,
    PendingRemove,
    SupriseRemoved
} UHF_PDO_STATE;

typedef struct _UHF_DEVICE_EXT {
    LIST_ENTRY gLink;
    LIST_ENTRY childs;
    LIST_ENTRY siblings;

    struct _UHF_DEVICE_EXT *parent;

    FAST_MUTEX fastMutex;

    ULONG DeviceRole;
    PDEVICE_OBJECT NextDevice;
    IO_REMOVE_LOCK RemoveLock;

    PDEVICE_OBJECT pdo;

    UHF_PDO_STATE pdoState;
    UHF_PDO_DESCRIPTION pdoDescription;
} UHF_DEVICE_EXT, *PUHF_DEVICE_EXT;

#define UHF_DEV_EXT_TAG ' fhu'

NTSTATUS 
uhfAddDevice(
    PDRIVER_OBJECT DriverObject,
    PDEVICE_OBJECT pdo);

NTSTATUS 
uhfAddChildDevice(
    PDRIVER_OBJECT DriverObject,
    PUHF_DEVICE_EXT parentDevExt,
    PDEVICE_OBJECT pdo);

VOID 
uhfDeleteDevice(
    PDEVICE_OBJECT device);

NTSTATUS
uhfGetPdoStringProperty(
    PDEVICE_OBJECT PhysicalDeviceObject,
    DEVICE_REGISTRY_PROPERTY DeviceProperty,
    PWCHAR *string,
    PULONG retLength);

NTSTATUS
uhfQueryPdoIds(
    PDEVICE_OBJECT pdo,
    BUS_QUERY_ID_TYPE idType,
    PWCHAR *ids, 
    PULONG retLength);

NTSTATUS
uhfQueryPdoText(
    PDEVICE_OBJECT pdo,
    DEVICE_TEXT_TYPE textType,
    PWCHAR *text, 
    PULONG retLength);

PUHF_DEVICE_EXT
uhfIsPdoInGlobalList(
    PDEVICE_OBJECT pdo);

VOID
uhfDumpDevicesTree(VOID);

#endif //__UHF_DEVICES_H__