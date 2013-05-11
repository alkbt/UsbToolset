#ifndef __ULK_DEVICES_H__
#define __ULK_DEVICES_H__

typedef struct _ULK_DEV_EXT {
    PDEVICE_OBJECT pdo;
    PDEVICE_OBJECT lowerDevice;

    IO_REMOVE_LOCK removeLock;
} ULK_DEV_EXT, *PULK_DEV_EXT;

#define DEV_EXT_TAG ' KLU'

NTSTATUS ulkAddDevice(
  PDRIVER_OBJECT driver,
  PDEVICE_OBJECT pdo);


#endif //__ULK_DEVICES_H__