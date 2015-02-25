TARGET         = neuron

PERF_EVENT_HDR = /usr/local/include/perfmon/
PFMLIB         = -lpfm

INCLUDES       = -I . -I $(PERF_EVENT_HDR)
LIBS           = -pthread $(PFMLIB)

CFLAGS         = -Wall -D_GNU_SOURCE $(INCLUDES)

SRCS = main.c \
       perf_util.c \
       pmu_sample.c \
       proc_sample.c

OBJS = $(SRCS:.c=.o)

.PHONY: all

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $? $(LIBS)

.PHONY: clean

clean:
	$(RM) -f *.o $(TARGET) *~
