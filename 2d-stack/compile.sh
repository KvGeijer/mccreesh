COMMON_FLAGS="-std=gnu11 -O3 -flto -Wall -Wextra -Wno-unused-parameter \
  -DGC=1 -DGCCLAIM=0 -DCORES_PER_SOCKET=1 -DNO_SET_CPU -mcx16"

gcc $COMMON_FLAGS -c 2Dc-stack.c -o 2Dc-stack.o
gcc $COMMON_FLAGS -c ssmem.c     -o ssmem.o
gcc $COMMON_FLAGS -c ssalloc.c   -o ssalloc.o

gcc -r -flto 2Dc-stack.o ssmem.o ssalloc.o -o 2dc_stack_all.o

g++ -O3 -flto -std=c++17 -pthread main.cpp 2dc_stack_all.o -latomic -lm -o test_stack