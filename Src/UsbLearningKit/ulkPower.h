#ifndef __ULK_POWER_H__
#define __ULK_POWER_H__

NTSTATUS 
ulkDispatchPower(
    PDEVICE_OBJECT deviceObject,
    PIRP irp);

#endif //__ULK_POWER_H__