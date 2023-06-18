SOURCES := main.cpp Hist.cpp Mapping.cpp
OBJS := $(SOURCES:.cpp=.o)

CFLAGS := -std=c++11 -g -O3 -DTIME_GLOBAL -DUSE_RDTSC

all: demo

demo: $(OBJS)
	$(CXX) $(CFLAGS) -o demo $(OBJS) $(LFLAGS) $(LIBS)

.cpp.o:
	$(CXX) $(CFLAGS) $(INCLUDES) -c $<

clean:
	rm -f *.o demo
