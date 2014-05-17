objs =	\
	main.o		\
	tun_dev.o	\
	unix_sck.o

program = tun_dev

cxxflags = -Wall -Wextra -Wpedantic -std=c++11
linkflags =

libs = -lboost_system

ifeq ($(debug), 1)
	cxxflags += -Werror -Og -g
else
	cxxflags += -O2 -flto
	linkflags += -flto
endif

all: $(program)

clean:
	rm -f $(program)
	rm -f *.o
	rm -f *.d

$(program): $(objs)
	$(CXX) $(linkflags) $(objs) $(libs) -o $(program)

.cc.o:
	$(CXX) $(cxxflags) -MD -c $*.cc -o $*.o -MF $*.d

-include *.d

.PHONY: all clean

