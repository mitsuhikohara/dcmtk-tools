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
storcmtscusrcs = storcmtscu.cc
storcmtscuobjs = $(storcmtscusrcs:.cc=.o)

progs = mppsscp storcmtscp storcmtscu

all: $(progs)

mppsscp: $(mppsobjs)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $(LIBDIRS) -o $@ $(mppsobjs) $(LOCALLIBS) $(DCMTLSLIBS) $(OPENSSLLIBS) $(MATHLIBS) $(LIBS)

storcmtscp: $(storcmtscpobjs)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $(LIBDIRS) -o $@ $(storcmtscpobjs) $(LOCALLIBS) $(DCMTLSLIBS) $(OPENSSLLIBS) $(MATHLIBS) $(LIBS)

storcmtscu: $(storcmtscuobjs)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $(LIBDIRS) -o $@ $(storcmtscuobjs) $(LOCALLIBS) $(DCMTLSLIBS) $(OPENSSLLIBS) $(MATHLIBS) $(LIBS)

clean:
	rm -f $(objs) $(progs) $(TRASH)

