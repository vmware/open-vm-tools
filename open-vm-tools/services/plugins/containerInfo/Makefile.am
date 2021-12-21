################################################################################
### Copyright (c) 2021 VMware, Inc.  All rights reserved.
###
### This program is free software; you can redistribute it and/or modify
### it under the terms of version 2 of the GNU General Public License as
### published by the Free Software Foundation.
###
### This program is distributed in the hope that it will be useful,
### but WITHOUT ANY WARRANTY; without even the implied warranty of
### MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
### GNU General Public License for more details.
###
### You should have received a copy of the GNU General Public License
### along with this program; if not, write to the Free Software
### Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
################################################################################

SUBDIRS =

plugindir = @VMSVC_PLUGIN_INSTALLDIR@
plugin_LTLIBRARIES = libcontainerInfo.la

PWD = $(shell pwd)
CONTAINERS_PROTO = $(PWD)/containers.proto

libcontainerInfo_la_CPPFLAGS =
libcontainerInfo_la_CPPFLAGS += @PLUGIN_CPPFLAGS@

libcontainerInfo_la_LDFLAGS =
libcontainerInfo_la_LDFLAGS += @PLUGIN_LDFLAGS@

libcontainerInfo_la_LIBADD =
libcontainerInfo_la_LIBADD += @VMTOOLS_LIBS@
libcontainerInfo_la_LIBADD += @GOBJECT_LIBS@

libcontainerInfo_la_SOURCES =
libcontainerInfo_la_SOURCES += containerInfo.h
libcontainerInfo_la_SOURCES += containerInfoInt.h
libcontainerInfo_la_SOURCES += containerInfo.c

libcontainerInfo_la_SOURCES += containerInfo_docker.c
libcontainerInfo_la_LDFLAGS += -lcurl
libcontainerInfo_la_CPPFLAGS += @CURL_CPPFLAGS@
libcontainerInfo_la_LIBADD += ../../../lib/jsmn/libJsmn.la

libcontainerInfo_la_SOURCES += gogoproto/gogo.pb.h
libcontainerInfo_la_SOURCES += gogoproto/gogo.pb.cc
libcontainerInfo_la_SOURCES += containers.pb.cc
libcontainerInfo_la_SOURCES += containers.pb.h
libcontainerInfo_la_SOURCES += containers.grpc.pb.cc
libcontainerInfo_la_SOURCES += containers.grpc.pb.h
libcontainerInfo_la_SOURCES += containerInfo_grpc.cc

libcontainerInfo_la_CPPFLAGS += @GRPC_CPPFLAGS@
libcontainerInfo_la_LDFLAGS += -lprotobuf
libcontainerInfo_la_LDFLAGS += -lgrpc++

containers.grpc.pb.cc \
containers.grpc.pb.h: $(CONTAINERS_PROTO)
	$(PROTOC) -I$(PWD) -I$(GOGO_PROTOPATH) \
             --grpc_out=. --plugin=protoc-gen-grpc=`which $(GRPC_CPP)` $^

gogoproto/gogo.pb.cc \
gogoproto/gogo.pb.h \
containers.pb.cc \
containers.pb.h: $(CONTAINERD_PROTOPATH)/containers.proto \
                 $(GOGO_PROTOPATH)/gogoproto/gogo.proto
	sed 's/import weak /import /' \
	    $(CONTAINERD_PROTOPATH)/containers.proto > $(CONTAINERS_PROTO)
	$(MKDIR_P) gogoproto/
	$(PROTOC) --cpp_out=. -I$(PWD) -I$(GOGO_PROTOPATH) \
                        $(CONTAINERS_PROTO) $(GOGO_PROTOPATH)/gogoproto/gogo.proto
