all : max_clique queue_test

# CXX = g++-4.7
CXX = g++
CC = gcc
override CXXFLAGS += -O3 -march=native -std=c++11 -I./ -W -Wall -g -ggdb3
override TWODSTACK_CFLAGS += -std=gnu11 -O3 -Wall -Wextra -Wno-unused-parameter \
	-DGC=1 -DGCCLAIM=0 -DCORES_PER_SOCKET=1 -DNO_SET_CPU -mcx16
override LDLIBS += -lboost_regex -lboost_thread -lboost_program_options -pthread -lrt -latomic -lm

FILES = clique/graph \
	clique/bit_graph \
	clique/dimacs \
	clique/max_clique_params \
	clique/colourise \
	clique/degree_sort \
	clique/queue \
	clique/naive_max_clique \
	clique/mcsa1_max_clique \
	clique/tmcsa1_max_clique \
	clique/bmcsa1_max_clique \
	clique/tbmcsa1_max_clique

CLIQUEOBJECTS = $(foreach c,$(FILES),$(c).o)
OBJECTS = $(CLIQUEOBJECTS) max_clique.o queue_test.o
TWODSTACKOBJECTS = 2d-stack/2Dc-stack.o 2d-stack/ssmem.o 2d-stack/ssalloc.o
HEADERS = $(foreach c,$(FILES),$(c).hh)
SOURCES = $(foreach c,$(FILES),$(c).hh)

$(OBJECTS) : %.o : %.cc $(HEADERS)
	$(CXX) $(CXXFLAGS) -c -o $@ $<

$(TWODSTACKOBJECTS) : %.o : %.c
	$(CC) $(TWODSTACK_CFLAGS) -c -o $@ $<

max_clique : $(CLIQUEOBJECTS) $(TWODSTACKOBJECTS) max_clique.o
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDLIBS)

queue_test : $(CLIQUEOBJECTS) $(TWODSTACKOBJECTS) queue_test.o
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDLIBS)

clean :
	rm -f $(OBJECTS) $(TWODSTACKOBJECTS) max_clique queue_test
