# This file builds Debug or Release versions of the ElevatorVerificationTest
# project executable (evt in macOS and Cygwin).
#
# This software is copyrighted © 2007 - 2021 by Codecraft, Inc.
#
# The following terms apply to all files associated with the software
# unless explicitly disclaimed in individual files.
#
# The authors hereby grant permission to use, copy, modify, distribute,
# and license this software and its documentation for any purpose, provided
# that existing copyright notices are retained in all copies and that this
# notice is included verbatim in any distributions. No written agreement,
# license, or royalty fee is required for any of the authorized uses.
# Modifications to this software may be copyrighted by their authors
# and need not follow the licensing terms described here, provided that
# the new terms are clearly indicated on the first page of each file where
# they apply.
#
# IN NO EVENT SHALL THE AUTHORS OR DISTRIBUTORS BE LIABLE TO ANY PARTY
# FOR DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR CONSEQUENTIAL DAMAGES
# ARISING OUT OF THE USE OF THIS SOFTWARE, ITS DOCUMENTATION, OR ANY
# DERIVATIVES THEREOF, EVEN IF THE AUTHORS HAVE BEEN ADVISED OF THE
# POSSIBILITY OF SUCH DAMAGE.
#
# THE AUTHORS AND DISTRIBUTORS SPECIFICALLY DISCLAIM ANY WARRANTIES,
# INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE, AND NON-INFRINGEMENT.  THIS SOFTWARE
# IS PROVIDED ON AN "AS IS" BASIS, AND THE AUTHORS AND DISTRIBUTORS HAVE
# NO OBLIGATION TO PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR
# MODIFICATIONS.
#
# GOVERNMENT USE: If you are acquiring this software on behalf of the
# U.S. government, the Government shall have only "Restricted Rights"
# in the software and related documentation as defined in the Federal
# Acquisition Regulations (FARs) in Clause 52.227.19 (c) (2).  If you
# are acquiring the software on behalf of the Department of Defense, the
# software shall be classified as "Commercial Computer Software" and the
# Government shall have only "Restricted Rights" as defined in Clause
# 252.227-7014 (b) (3) of DFARs.  Notwithstanding the foregoing, the
# authors grant the U.S. Government and others acting in its behalf
# permission to use and distribute the software in accordance with the
# terms specified in this license.

MAKE = make
RM = rm
MKDIR = mkdir
GCC = clang++

all : outfile

BASE = ..
INCLUDE = include
CODEDIR	= ./code
INCDIR = ./$(INCLUDE)
SCCOR = sccor
RRTGEN = RRTGen

REQUIRED_DIRS = \
	$(CODEDIR)\
	$(NULL)

_MKDIRS := $(shell for d in $(REQUIRED_DIRS) ;	\
	     do					\
	       [ -d $$d ] || mkdir -p $$d ;	\
	     done)

VERSION = 1.0

vpath %.cpp  $(CODEDIR)

PROGNAME = evt

# If no configuration is specified, "Debug" will be used.
ifndef CFG
CFG = Debug
endif

# Set architecture and PIC options.
ARCH_OPT =
PIC_OPT =
DEFS =
OS := $(shell uname)
ifeq ($(OS),Darwin)
# It's macOS.
ARCH_OPT += -arch x86_64
PIC_OPT += -Wl,-no_pie
else ifeq ($(OS),$(filter CYGWIN_NT%, $(OS)))
# It's Windows.
DEFS = -D"CYGWIN"
else
$(error The sccor library requires either macOS Big Sur (11.0.1) or later or Cygwin.)
endif

OUTDIR = ./$(CFG)
OUTFILE = $(OUTDIR)/$(PROGNAME)
SCCORDIR = $(BASE)/$(SCCOR)/$(CFG)
#RRTGENOBJ = $(BASE)/$(RRTGEN)/$(CFG)/*.o
RRTGENINCDIR = $(BASE)/$(RRTGEN)/$(INCLUDE)
SCCORINCDIR = $(BASE)/$(SCCOR)/$(INCLUDE)
MYOBJ = $(OUTDIR)/$(PROGNAME).o 
OTHEROBJS = $(OUTDIR)/EVTModel_Insert.o $(OUTDIR)/EVTModelActions.o
#OBJ = $(MYOBJ) $(RRTGENOBJ) $(OTHEROBJS)
OBJ = $(MYOBJ) $(OTHEROBJS)
#OBJ = $(MYOBJ)

#
# Configuration: Debug
#
ifeq "$(CFG)" "Debug"
COMPILE = $(GCC) -c $(ARCH_OPT) $(DEFS) -fno-stack-protector -std=c++17 -O0 -g -o "$(OUTDIR)/$(*F).o" -I$(INCDIR) -I$(RRTGENINCDIR) -I$(SCCORINCDIR) "$<"
LINK = $(GCC) $(ARCH_OPT) $(DEFS) -g -o "$(OUTFILE)" $(PIC_OPT) $(OBJ) $(SCCORDIR)/sccorlib.a -lncurses
endif

#
# Configuration: Release
#
ifeq "$(CFG)" "Release"
COMPILE = $(GCC) -c $(ARCH_OPT) $(DEFS) -fno-stack-protector -std=c++17 -O0 -o "$(OUTDIR)/$(*F).o" -I$(INCDIR) -I$(RRTGENINCDIR) -I$(SCCORINCDIR) "$<"
LINK = $(GCC) $(ARCH_OPT) $(DEFS) -o "$(OUTFILE)" $(PIC_OPT) $(OBJ) $(SCCORDIR)/sccorlib.a -lncurses
endif

# Pattern rules
$(OUTDIR)/%.o : $(CODEDIR)/%.cpp
	$(COMPILE)

#$(OUTFILE): $(OUTDIR) deps $(OBJ)
#	$(LINK)
#$(OUTFILE): $(OUTDIR) deps $(MYOBJ) $(OTHEROBJS) 
#	$(LINK)
$(OUTFILE): $(OUTDIR) $(MYOBJ) $(OTHEROBJS) 
	$(LINK)

$(OUTDIR):
	$(MKDIR) -p "$(OUTDIR)"

CODEFILES =\
	$(PROGNAME).cpp\
	$(NULL)

CLEANFILES =\
	$(OUTFILE)\
	$(OBJ)\
	$(NULL)

.PHONY : outfile clean

## Build dependencies
#deps:
#	@(cd ../RRTGen;$(MAKE) -f Makefile CFG=Debug)

outfile : $(OUTFILE)

clean :
	$(RM) -f $(CLEANFILES)


