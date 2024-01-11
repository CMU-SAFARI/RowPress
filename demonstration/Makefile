ALGO1SOURCES := main-algo1.cpp Hist.cpp Mapping.cpp
ALGO1OBJS := $(ALGO1SOURCES:.cpp=.o)

ALGO2SOURCES := main-algo2.cpp Hist.cpp Mapping.cpp
ALGO2OBJS := $(ALGO2SOURCES:.cpp=.o)

CFLAGS := -std=c++11 -g -O3 -DTIME_GLOBAL -DUSE_RDTSC

all: demo-algo1 demo-algo2

demo-algo1: $(ALGO1OBJS)
	$(CXX) $(CFLAGS) -o demo-algo1 $(ALGO1OBJS) $(LFLAGS) $(LIBS)

demo-algo2: $(ALGO2OBJS)
	$(CXX) $(CFLAGS) -o demo-algo2 $(ALGO2OBJS) $(LFLAGS) $(LIBS)

.cpp.o:
	$(CXX) $(CFLAGS) $(INCLUDES) -c $<

clean:
	rm -f *.o demo-algo1 demo-algo2
