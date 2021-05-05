CXX := g++
override CXXFLAGS += -g -D_FILE_OFFSET_BITS=64 -D PRINTF

all: ckpt restore

ckpt: ckpt.cpp
	$(CXX) $< -o $@

restore: restore.cpp
	$(CXX) $< -o $@