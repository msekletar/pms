#  -*- Mode: makefile; indent-tabs-mode: t -*-

#  Implementation of Pipeline Merge Sort using OpenMPI
#  Copyright (C) 2015  Michal Sekletar <xsekle00@stud.fit.vutbr.cz>

#  This program is free software; you can redistribute it and/or
#  modify it under the terms of the GNU General Public License
#  as published by the Free Software Foundation; either version 2
#  of the License, or (at your option) any later version.

#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.

#  You should have received a copy of the GNU General Public License
#  along with this program; if not, write to the Free Software
#  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

BUT_LOGIN = xsekle00

SOURCE = pms.cpp
BIN = pms
SUBMISSION_FILELIST = $(SOURCE) Makefile test.sh pow2.bc

CC = mpiCC

ifndef DEBUG
OPTFLAGS = -O3 -DNDEBUG
else
OPTFLAGS = -O0 -ggdb3 -fsanitize=address -fno-omit-frame-pointer
LDFLAGS = -lasan
endif

LDFLAGS += -lm
CFLAGS += -D_GNU_SOURCE -std=gnu++11 -Wall -march=native -mtune=native $(OPTFLAGS) $(LDFLAGS)

.PHONY: clean submission

all: $(SOURCE)
	$(CC) $(CFLAGS) -o $(BIN) $^

clean:
	rm -f $(BIN) $(BUT_LOGIN).tgz numbers

submission:
	tar -czf $(BUT_LOGIN).tgz $(SUBMISSION_FILELIST)
