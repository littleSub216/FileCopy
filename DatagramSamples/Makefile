# Makefile for COMP 150IDS Datagram Samples
#
#    Copyright 2012 - Noah Mendelsohn
#
#
# Useful targets:
#
#
#    pingclient  - demonstration message ping client
#    pingserver  - demonstration message ping server
#    shatstincr  - sample code for computing SHA1 hash incrementally
#
#    clean       - clean out all compiled object and executable files
#    all         - (default target) make sure everything's compiled
#

# Do all C++ compies with g++
CPP = g++
CPPFLAGS = -g -Wall -Werror -I$(C150LIB)
CPPFLAGS = -g -Wall -Werror -I$(C150LIB)

# Where the COMP 150 shared utilities live, including c150ids.a and userports.csv
# Note that environment variable COMP117 must be set for this to work!

C150LIB = $(COMP117)/files/c150Utils/
C150AR = $(C150LIB)c150ids.a

LDFLAGS = 
INCLUDES = $(C150LIB)c150dgmsocket.h $(C150LIB)c150nastydgmsocket.h $(C150LIB)c150network.h $(C150LIB)c150exceptions.h $(C150LIB)c150debug.h $(C150LIB)c150utility.h

all: pingclient	pingclient2 pingserver	

pingclient: pingclient.o  $(C150AR) $(INCLUDES)
	$(CPP) -o pingclient pingclient.o $(C150AR)

pingclient2: pingclient2.o  $(C150AR) $(INCLUDES)
	$(CPP) -o pingclient2 pingclient2.o $(C150AR)


pingserver: pingserver.o  $(C150AR) $(INCLUDES)
	$(CPP) -o pingserver pingserver.o $(C150AR)


sha1tstincr: sha1tstincr.o
	$(CPP) -o sha1tstincr sha1tstincr.cpp -lssl

%.o:%.cpp  $(INCLUDES)
	$(CPP) -c  $(CPPFLAGS) $< 


# command to be executed.
clean:
	 rm -f pingclient pingclient2 pingserver sha1tstincr *.o 


