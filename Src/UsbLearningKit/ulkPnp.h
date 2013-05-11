#ifndef __ULK_PNP_H__
#define __ULK_PNP_H__

NTSTATUS 
ulkDispatchPnp(
    PDEVICE_OBJECT deviceObject,
    PIRP irp);

#endif //__ULK_PNP_H__