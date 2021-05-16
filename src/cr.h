#ifndef cr_h
#define cr_h

#ifdef __cplusplus
extern "C" {
#endif
#include <sys/ioctl.h>

#define VERIFS2_IOC_CODE    '1'
#define VERIFS2_IOC_NO(x)   (VERIFS2_IOC_CODE + (x))
#define VERIFS2_IOC(n)      _IO(VERIFS2_IOC_CODE, VERIFS2_IOC_NO(n))
#define VERIFS2_GET_IOC(n, type)  _IOR(VERIFS2_IOC_CODE, VERIFS2_IOC_NO(n), type)
#define VERIFS2_SET_IOC(n, type)  _IOW(VERIFS2_IOC_CODE, VERIFS2_IOC_NO(n), type)

#define VERIFS_CHECKPOINT  VERIFS2_IOC(1)
#define VERIFS_RESTORE     VERIFS2_IOC(2)

// the PICKLE and LOAD should receive a parameter of `struct verifs_str` which
// contains the path to the output / input file.
#define VERIFS_PICKLE      VERIFS2_IOC(3)
#define VERIFS_LOAD        VERIFS2_IOC(4)
#define VERIFS_PICKLE_CFG  "/tmp/pickle.cfg"

#ifdef __cplusplus
}
#endif

#endif
