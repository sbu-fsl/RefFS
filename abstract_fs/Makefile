REPO_DIR := /home/yifei/zram_test/VeriFS2-ioctl-20210212/nfs-validator
IPC_DIR := $(REPO_DIR)/ipc
INCLUDE_DIR := $(REPO_DIR)/include
COMMON_DIR = $(REPO_DIR)/common
COMMON_SRC := $(wildcard $(COMMON_DIR)/*.c) $(wildcard $(COMMON_DIR)/*.cpp)
COMMON_OBJ := $(patsubst $(COMMON_DIR)/%,%.o,$(COMMON_SRC))

override CFLAGS += -g -I$(IPC_DIR) -I$(INCLUDE_DIR)
override LIBS += -lssl -lcrypto -lrt -lstdc++ -lstdc++fs -lpthread 

compute-astates: compute_astates.c common-libs
	gcc -g -o compute_astates compute_astates.c common-libs.a $(CFLAGS) $(LIBS)

common-libs: $(COMMON_OBJ)
	ar rvs $@.a $^

%.c.o: $(COMMON_DIR)/%.c
	gcc -Wall -Werror -o $@ -fPIC -c $< $(CFLAGS) $(LIBS)

%.cpp.o: $(COMMON_DIR)/%.cpp
	g++ -std=c++11 -Wall -Werror -o $@ -fPIC -c $< $(CFLAGS) $(LIBS)

clean:
	rm -f compute_astates *.o *.a
