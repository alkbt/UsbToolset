#ifndef __UHF_DEVICES_H__
#define __UHF_DEVICES_H__

#define UHF_DEVICE_ROLE_HUB_FiDO 0x1

typedef struct _UHF_DEVICE_EXT {
    ULONG DeviceRole;
    PDEVICE_OBJECT NextDevice;

    PWCHAR DeviceClassName;
    PWCHAR DeviceDeviceDescription;
    PWCHAR DeviceFriendlyName;
    PWCHAR DeviceManufacturer;
    PWCHAR DeviceDriverKeyName;
} UHF_DEVICE_EXT, *PUHF_DEVICE_EXT;

#define UHF_DEV_EXT_TAG ' fhu'

#endif //__UHF_DEVICES_H__