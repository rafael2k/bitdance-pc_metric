#
# Copyright (C) 2019-2021 Rafael Diniz <rafael@riseup.net>
#
# This is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 3, or (at your option)
# any later version.
#
# This software is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this software; see the file COPYING.  If not, write to
# the Free Software Foundation, Inc., 51 Franklin Street,
# Boston, MA 02110-1301, USA.
#

PREFIX=/usr

# Open3D install prefix path
OPEN3D_PREFIX=$(PREFIX)

# if installed manually, typically /usr/local is used
##OPEN3D_PREFIX=/usr/local

# Change to your C++ compiler of preference...
CPP=g++

CXXFLAGS= -g -std=c++17 -fPIC -fopenmp -Wall  -Wno-deprecated-declarations -Wno-unused-result -DUNIX -I$(OPEN3D_PREFIX)/include \
	-I$(OPEN3D_PREFIX)/include/Open3D -I$(OPEN3D_PREFIX)/include/open3d \
	-I$(PREFIX)/include/eigen3 \
	-I. -I./ColorSpace
LDFLAGS= -g -std=c++17 -fPIC -fopenmp -Wl,--no-as-needed -rdynamic -lOpen3D -lGLEW -lGLU -lGL -lglfw

##

all: bitdance_pcqa create_normals optimize_voxel_size


# main metric binary rules
bitdance_pcqa: bitdance_pcqa.o ColorSpace/ColorSpace.o ColorSpace/Conversion.o ColorSpace/Comparison.o
	$(CPP) $(LDFLAGS) -o $@ $^

bitdance_pcqa.o: bitdance_pcqa.cpp bitdance_pcqa.h
	$(CPP) -c $(CXXFLAGS) $< -o $@



# auxiliary commands for creating normals...
create_normals: create_normals.cpp
	$(CPP) $(CXXFLAGS) $(LDFLAGS) -o $@ $^

# and definition of the voxel size
optimize_voxel_size: optimize_voxel_size.cpp
	$(CPP) $(CXXFLAGS) $(LDFLAGS) -o $@ $^



# color space external code
ColorSpace/Comparison.o: ColorSpace/Comparison.cpp
	$(CPP) -c $(CXXFLAGS) $< -o $@

ColorSpace/Conversion.o: ColorSpace/Conversion.cpp
	$(CPP) -c $(CXXFLAGS) $< -o $@

ColorSpace/ColorSpace.o: ColorSpace/ColorSpace.cpp
	$(CPP) -c $(CXXFLAGS) $< -o $@

##

install: bitdance_pcqa
	install -d $(PREFIX)/bin
	install bitdance_pcqa $(PREFIX)/bin
	install create_normals $(PREFIX)/bin
	install optimize_voxel_size $(PREFIX)/bin

.PHONY: clean
clean:
	rm -f bitdance_pcqa create_normals optimize_voxel_size *.o ColorSpace/*.o
