# Filter tests run on a computer (no ESP32, no PlatformIO needed).
CXX ?= g++
CXXFLAGS ?= -std=c++17 -Wall -Wextra -IAquaSense
OUT ?= /tmp/aquasense_filter_test

.PHONY: test
test:
	$(CXX) $(CXXFLAGS) AquaSense/filters.cpp test/test_filters.cpp -o $(OUT)
	$(OUT)
