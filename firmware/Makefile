# Conversion helpers (no Arduino, no PlatformIO required)
CXX ?= g++
CXXFLAGS ?= -std=c++17 -Wall -Wextra -Iinclude

.PHONY: test
test:
	$(CXX) $(CXXFLAGS) src/convert.cpp test/test_convert/test_native.cpp -o /tmp/aquasense_convert_test
	/tmp/aquasense_convert_test
