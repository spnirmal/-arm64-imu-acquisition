CXX = g++

TARGET = spi-userspace-acquisition
SRC = main.cpp

CXXFLAGS += -Wall -Wextra -O2

all: $(TARGET)

$(TARGET): $(SRC)
	$(CXX) $(CXXFLAGS) $(SRC) -o $(TARGET)

clean:
	rm -f $(TARGET)
