#ifndef cr_hpp
#define cr_hpp

#include <sys/ioctl.h>

#define VERIFS2_IOC_CODE    '1'
#define VERIFS2_IOC_NO(x)   (VERIFS2_IOC_CODE + (x))
#define VERIFS2_IOC(n)      _IO(VERIFS2_IOC_CODE, VERIFS2_IOC_NO(n))
#define VERIFS2_GET_IOC(n, type)  _IOR(VERIFS2_IOC_CODE, VERIFS2_IOC_NO(n), type)
#define VERIFS2_SET_IOC(n, type)  _IOW(VERIFS2_IOC_CODE, VERIFS2_IOC_NO(n), type)

#define VERIFS2_CHECKPOINT  VERIFS2_IOC(1)
#define VERIFS2_RESTORE     VERIFS2_IOC(2)

int insert_state(uint64_t key, std::vector <Inode *> inode_vec);
std::vector <Inode *> find_state(uint64_t key);
int remove_state(uint64_t key);

#endif