# Mineways macOS ARM64 build
# Usage: make -C Mac

CXX      := clang++
CXXFLAGS := -std=c++17 -arch arm64 -O2 -Wall -Wno-unused-function \
            -Wno-macro-redefined -Wno-writable-strings \
            -DMINEWAYS_X64 \
            -I. -I../Win \
            -include compat.h \
            $(shell pkg-config --cflags zlib 2>/dev/null)

LDFLAGS  := -arch arm64
LIBS     := -lz

SRC_DIR  := ../Win
SRCS     := $(SRC_DIR)/blockInfo.cpp \
            $(SRC_DIR)/biomes.cpp \
            $(SRC_DIR)/lodepng.cpp \
            $(SRC_DIR)/rwpng.cpp \
            $(SRC_DIR)/nbt.cpp \
            $(SRC_DIR)/region.cpp \
            $(SRC_DIR)/cache.cpp \
            $(SRC_DIR)/ObjFileManip.cpp \
            $(SRC_DIR)/terrainExtData.cpp \
            $(SRC_DIR)/stdafx.cpp

OBJS     := $(patsubst $(SRC_DIR)/%.cpp, %.o, $(SRCS))

.PHONY: all clean

all: $(OBJS)
	@echo "Core library objects built. GUI layer (Mineways.cpp / MinewaysMap.cpp) not compiled — needs macOS UI framework."

%.o: $(SRC_DIR)/%.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f *.o
