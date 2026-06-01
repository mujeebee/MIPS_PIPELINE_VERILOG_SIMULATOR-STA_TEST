## Makefile — Gate-level Static Timing Analyzer

CXX     = g++
CXXFLAGS = -std=c++17 -O2 -Wall -Wextra -I include
TARGET  = sta_tool
SRC     = src/main.cpp

.PHONY: all test clean

all: $(TARGET)

$(TARGET): $(SRC) include/*.h
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(SRC)

test: $(TARGET)
	@echo "\n--- Test 1: Ripple carry adder (500ps clock) ---"
	./$(TARGET) tests/adder.net
	@echo "\n--- Test 2: Deep path with timing violation (300ps clock) ---"
	./$(TARGET) tests/violation.net; true

test_dot: $(TARGET)
	./$(TARGET) tests/adder.net 500 adder.dot
	@echo "Run: dot -Tpng adder.dot -o adder.png && open adder.png"

clean:
	rm -f $(TARGET) *.dot *.png

help:
	@echo "make          — build the tool"
	@echo "make test     — run both test netlists"
	@echo "make test_dot — generate Graphviz DOT output"
