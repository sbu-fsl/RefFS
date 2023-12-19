/*
 * This file is part of RefFS.
 * 
 * Copyright (c) 2020-2024 Yifei Liu
 * Copyright (c) 2020-2024 Wei Su
 * Copyright (c) 2020-2024 Erez Zadok
 * Copyright (c) 2020-2024 Stony Brook University
 * Copyright (c) 2020-2024 The Research Foundation of SUNY
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * RefFS is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

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
#define VERIFS_LOAD_CFG    "/tmp/pickle.cfg"

#ifdef __cplusplus
}
#endif

#endif
