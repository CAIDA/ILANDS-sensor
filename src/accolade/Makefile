ACCOLADE_ROOT = /opt/accolade
GRAPHBLAS_ROOT = /usr/local
DEBUG = n

CC = gcc
CFLAGS  = -Wall -Wstrict-prototypes -Wno-unused -Wno-incompatible-pointer-types
CFLAGS += -I$(ACCOLADE_ROOT)/include -I$(GRAPHBLAS_ROOT)/include 

OTHER_LIBS = -lssl -lcrypto -lrt 
ANIC_LIBS = -L$(ACCOLADE_ROOT)/lib -lanic -Wl,-rpath,$(ACCOLADE_ROOT)/lib
GRAPHBLAS_LIBS = -L$(GRAPHBLAS_ROOT)/lib -lgraphblas -lgomp -pthread -Wl,-rpath,$(GRAPHBLAS_ROOT)/lib

ifeq ($(DEBUG),y)
CFLAGS += -O0 -g
else
CFLAGS += -O3 -DNDEBUG
endif

ALL_FILES  = gbdeserialize makecache pcap2grb anic2grb
WITHYAML = $(if $(wildcard /usr/include/yaml.h),y)

all: $(ALL_FILES)

install: all
	mkdir -p /usr/local/bin
	install -p $(ALL_FILES) /usr/local/bin

.PHONY : all clean distclean install

anic2grb: anic2grb.c anic_libpcap.c cryptopANT.c
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(ANIC_LIBS) $(GRAPHBLAS_LIBS) $(OTHER_LIBS) 

clean distclean mostlyclean maintainer-clean:
	rm -f $(ALL_FILES) *.o
