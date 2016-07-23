#
#	Makefile for dcmnet/apps
#

SHELL = /bin/sh
srcdir = .
configdir = .

include $(configdir)/Makefile.def

LOCALINCLUDES = -I/usr/include
LIBDIRS = -L/usr/lib64
LOCALLIBS = -ldcmnet -ldcmdata -loflog -lofstd $(ZLIBLIBS) $(TCPWRAPPERLIBS)
DCMTLSLIBS = -ldcmtls

mppssrcs = mppsscp.cc
mppsobjs = $(mppssrcs:.cc=.o)
storcmtscpsrcs = storcmtscp.cc
storcmtscpobjs = $(storcmtscpsrcs:.cc=.o)

progs = mppsscp storcmtscp

all: $(progs)

mppsscp: $(mppsobjs)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $(LIBDIRS) -o $@ $(mppsobjs) $(LOCALLIBS) $(DCMTLSLIBS) $(OPENSSLLIBS) $(MATHLIBS) $(LIBS)

storcmtscp: $(storcmtscpobjs)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $(LIBDIRS) -o $@ $(storcmtscpobjs) $(LOCALLIBS) $(DCMTLSLIBS) $(OPENSSLLIBS) $(MATHLIBS) $(LIBS)

clean:
	rm -f $(objs) $(progs) $(TRASH)

