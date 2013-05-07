#ifndef __UHF_PNP_H__
#define __UHF_PNP_H__

NTSTATUS 
uhfDispatchPnP(
    PDEVICE_OBJECT DeviceObject,
    PIRP Irp);

#endif //__UHF_PNP_H__