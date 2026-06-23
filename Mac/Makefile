# Mineways macOS ARM64 build  (wxWidgets GUI)
# Usage: make -C Mac          (from repo root)

CXX      := clang++

WX_CFLAGS := $(shell wx-config --cxxflags)
WX_LIBS   := $(shell wx-config --libs)

CXXFLAGS := -std=c++17 -arch arm64 -O2 -Wall \
            -Wno-unused-function \
            -Wno-macro-redefined \
            -Wno-writable-strings \
            -DMINEWAYS_X64 \
            -DMINEWAYS_VERSION_STRING=\"13.0\" \
            -I. -I../Win \
            -include compat.h \
            $(shell pkg-config --cflags zlib 2>/dev/null)

# GUI files need wxWidgets flags on top of the base flags
GUI_CXXFLAGS := $(CXXFLAGS) $(WX_CFLAGS)

LDFLAGS  := -arch arm64
LIBS     := -lz $(WX_LIBS)

SRC_DIR  := ../Win

# Core library (no GUI, no Win32 UI calls)
CORE_SRCS := $(SRC_DIR)/blockInfo.cpp \
             $(SRC_DIR)/biomes.cpp \
             $(SRC_DIR)/lodepng.cpp \
             $(SRC_DIR)/rwpng.cpp \
             $(SRC_DIR)/nbt.cpp \
             $(SRC_DIR)/region.cpp \
             $(SRC_DIR)/cache.cpp \
             $(SRC_DIR)/ObjFileManip.cpp \
             $(SRC_DIR)/terrainExtData.cpp \
             $(SRC_DIR)/stdafx.cpp \
             $(SRC_DIR)/MinewaysMap.cpp

# wxWidgets GUI sources (live in Mac/)
GUI_SRCS  := MinewaysApp.cpp \
             MinewaysFrame.cpp \
             MapPanel.cpp \
             LocationDialog.cpp \
             CullingStubs.cpp

CORE_OBJS := $(patsubst $(SRC_DIR)/%.cpp, %.o, $(CORE_SRCS))
GUI_OBJS  := $(patsubst %.cpp, %.o, $(GUI_SRCS))
ALL_OBJS  := $(CORE_OBJS) $(GUI_OBJS)

TARGET    := mineways

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(ALL_OBJS)
	$(CXX) $(LDFLAGS) -o $@ $^ $(LIBS)
	@echo "Built $(TARGET)"

# Core files: base flags only
%.o: $(SRC_DIR)/%.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

# GUI files: base + wx flags (explicit pattern rules override the core rule above)
MinewaysApp.o: MinewaysApp.cpp
	$(CXX) $(GUI_CXXFLAGS) -c $< -o $@

MinewaysFrame.o: MinewaysFrame.cpp
	$(CXX) $(GUI_CXXFLAGS) -c $< -o $@

MapPanel.o: MapPanel.cpp
	$(CXX) $(GUI_CXXFLAGS) -c $< -o $@

LocationDialog.o: LocationDialog.cpp
	$(CXX) $(GUI_CXXFLAGS) -c $< -o $@

CullingStubs.o: CullingStubs.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f *.o $(TARGET)
