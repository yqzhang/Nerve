TARGETS=syst_smpl

PERF_EVENT_HDR = /usr/local/include/perfmon/
PFMLIB         = -lpfm
LPC_UTILS      = perf_util.o
LPC_UTILS_HDR  = perf_util.h

INCLUDES       = -I . -I $(PERF_EVENT_HDR)
LIBS           = -pthread $(PFMLIB)

CFLAGS         += -Wall -D_GNU_SOURCE $(INCLUDES)

.PHONY: all

all: $(TARGETS)

$(TARGETS): %:%.o $(LPC_UTILS)
	$(CC) $(CFLAGS) -o $@ $(LDFLAGS) $< $(LPC_UTILS) $(LIBS)

$(LPC_UTILS): $(LPC_UTILS_HDR)

.PHONY: clean

clean:
	$(RM) -f *.o $(TARGETS) *~
