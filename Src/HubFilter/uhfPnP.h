#ifndef __UHF_PNP_H__
#define __UHF_PNP_H__

NTSTATUS 
uhfDispatchPnp(
    PDEVICE_OBJECT DeviceObject,
    PIRP Irp);

#endif //__UHF_PNP_H__