CXX = g++
CXXFLAGS = -std=c++17 -Wall -g

SRCS = $(wildcard *.cc)
TARGETS = $(SRCS:.cc=)

all: $(TARGETS)

%: %.cc
	$(CXX) $(CXXFLAGS) -g -o $@ $< -lpthread
clean:
	rm -f $(TARGETS)

.PHONY: all clean
