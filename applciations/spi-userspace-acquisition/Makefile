CROSS_COMPILE ?=
CXX ?= $(CROSS_COMPILE)g++

ifeq ($(origin CXX),default)
  ifeq ($(shell which $(CXX) 2>/dev/null),)
    ifneq ($(wildcard ../../buildroot-output-dir/host/bin/aarch64-linux-g++),)
      CXX = ../../buildroot-output-dir/host/bin/aarch64-linux-g++
    endif
  endif
endif

TARGET = spi-userspace-acquisition
SRCS = src/main.cpp src/MPU9250.cpp src/spi-layer.cpp src/gpio-layer.cpp
OBJS = $(SRCS:.cpp=.o)

CXXFLAGS += -Wall -Wextra -O2 -pthread -Iinc -I.
LDLIBS += -lgpiod -pthread

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $(OBJS) -o $(TARGET) $(LDLIBS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: all clean
