all : max_clique queue_test

# CXX = g++-4.7
CXX = g++
override CXXFLAGS += -O3 -march=native -std=c++11 -I./ -W -Wall -g -ggdb3
override LDLIBS += -lboost_regex -lboost_thread -lboost_program_options -pthread -lrt

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
HEADERS = $(foreach c,$(FILES),$(c).hh)
SOURCES = $(foreach c,$(FILES),$(c).hh)

$(OBJECTS) : %.o : %.cc $(HEADERS)
	$(CXX) $(CXXFLAGS) -c -o $@ $<

max_clique : $(CLIQUEOBJECTS) max_clique.o
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDLIBS)

queue_test : $(CLIQUEOBJECTS) queue_test.o
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDLIBS)

clean :
	rm -f $(OBJECTS) max_clique queue_test

