TARGET         = nerve

PERF_EVENT_HDR = /usr/local/include/perfmon/
PFMLIB         = -lpfm

INCLUDES       = -I . -I $(PERF_EVENT_HDR)
LIBS           = -pthread -ljansson $(PFMLIB)

CFLAGS         = -Wall -D_GNU_SOURCE $(INCLUDES)

SRCS = app_sample.c \
			 config_util.c \
       file_util.c \
       log_util.c \
       main.c \
       perf_util.c \
       pmu_sample.c \
       proc_sample.c

OBJS = $(SRCS:.c=.o)

.PHONY: all

all: clean $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $? $(LIBS)

%.o: %.c
	$(CC) -c $(CFLAGS) $< -o $@

.PHONY: clean

clean:
	$(RM) -f *.o $(TARGET) *~
