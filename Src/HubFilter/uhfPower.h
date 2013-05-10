#ifndef __UHF_POWER_H__
#define __UHF_POWER_H__

NTSTATUS 
uhfDispatchPower(
    PDEVICE_OBJECT device,
    PIRP irp);

#endif //__UHF_POWER_H__