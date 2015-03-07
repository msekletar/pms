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

SOURCE = pms.c
BIN = pms
SUBMISSION_FILELIST = $(SOURCE) test.sh

CC ?= gcc

ifndef DEBUG
OPTFLAGS = -O3
else
OPTFLAGS = -O0 -ggdb3
endif

CFLAGS += -D_GNU_SOURCE -std=gnu99 -Wall $(OPTFLAGS)

.PHONY: clean submission

all: $(SOURCE)
	$(CC) $(CFLAGS) -o $(BIN) $^

clean:
	rm -f $(BIN) $(BUT_LOGIN).tgz

submission:
	tar -czf $(BUT_LOGIN).tgz $(SUBMISSION_FILELIST)
