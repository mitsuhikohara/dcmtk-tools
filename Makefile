#
#	Makefile for dcmnet/apps
#

SHELL = /bin/sh
srcdir = .
configdir = .

include $(configdir)/Makefile.def

LOCALINCLUDES = -I/usr/local/include -I/usr/include
LIBDIRS = -L/usr/local/lib64 -L/usr/lib64
LOCALLIBS = -ldcmnet -ldcmdata -loflog -lofstd $(ZLIBLIBS) $(TCPWRAPPERLIBS)
DCMTLSLIBS = -ldcmtls

mppssrcs = mppsrecv.cc dmppsscp.cc
mppsobjs = $(mppssrcs:.cc=.o)
storcmtsrcs = storcmtrecv.cc dstorcmtscp.cc dstorcmtscu.cc
storcmtobjs = $(storcmtsrcs:.cc=.o)

progs = mppsrecv storcmtrecv

all: $(progs)

mppsrecv: $(mppsobjs)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $(LIBDIRS) -o $@ $(mppsobjs) $(LOCALLIBS) $(DCMTLSLIBS) $(OPENSSLLIBS) $(MATHLIBS) $(LIBS)

storcmtrecv: $(storcmtobjs)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $(LIBDIRS) -o $@ $(storcmtobjs) $(LOCALLIBS) $(DCMTLSLIBS) $(OPENSSLLIBS) $(MATHLIBS) $(LIBS)

clean:
	rm -f $(progs) $(mppsobjs) $(storcmtobjs)

