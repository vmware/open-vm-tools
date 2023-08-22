################################################################################
### Copyright (c) 2021-2023 VMware, Inc.  All rights reserved.
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
libcontainerInfo_la_LIBADD += -lcurl
libcontainerInfo_la_CPPFLAGS += @CURL_CPPFLAGS@
libcontainerInfo_la_LIBADD += ../../../lib/jsmn/libJsmn.la

libcontainerInfo_la_SOURCES += gogoproto/gogo.pb.h
libcontainerInfo_la_SOURCES += gogoproto/gogo.pb.cc
libcontainerInfo_la_SOURCES += $(TYPES_DIR)/mount.pb.h
libcontainerInfo_la_SOURCES += $(TYPES_DIR)/mount.pb.cc
libcontainerInfo_la_SOURCES += $(TYPES_DIR)/metrics.pb.h
libcontainerInfo_la_SOURCES += $(TYPES_DIR)/metrics.pb.cc
libcontainerInfo_la_SOURCES += $(TYPES_DIR)/descriptor.pb.h
libcontainerInfo_la_SOURCES += $(TYPES_DIR)/descriptor.pb.cc
libcontainerInfo_la_SOURCES += $(TYPES_DIR)/task/task.pb.h
libcontainerInfo_la_SOURCES += $(TYPES_DIR)/task/task.pb.cc
libcontainerInfo_la_SOURCES += tasks.pb.h
libcontainerInfo_la_SOURCES += tasks.pb.cc
libcontainerInfo_la_SOURCES += containers.pb.h
libcontainerInfo_la_SOURCES += containers.pb.cc
libcontainerInfo_la_SOURCES += tasks.grpc.pb.h
libcontainerInfo_la_SOURCES += tasks.grpc.pb.cc
libcontainerInfo_la_SOURCES += containers.grpc.pb.h
libcontainerInfo_la_SOURCES += containers.grpc.pb.cc
libcontainerInfo_la_SOURCES += containerInfo_grpc.cc

libcontainerInfo_la_CPPFLAGS += ${grpcxx_CFLAGS}
libcontainerInfo_la_LIBADD += -lprotobuf
libcontainerInfo_la_LIBADD += ${grpcxx_LIBS}

tasks.grpc.pb.cc containers.grpc.pb.cc: %.grpc.pb.cc : %.proto %.pb.cc
	$(PROTOC) -I. -I$(GOGO_PROTOPATH) \
             --grpc_out=. --plugin=protoc-gen-grpc=`which $(GRPC_CPP)` $<

containerInfo_grpc.cc : containers.grpc.pb.cc tasks.grpc.pb.cc

$(TYPES_DIR)/mount.proto \
$(TYPES_DIR)/metrics.proto \
$(TYPES_DIR)/descriptor.proto \
$(TYPES_DIR)/task/task.proto: %.proto : $(DEP_PROTOPATH)/%.proto
	$(MKDIR_P) $(@D)
	sed 's/import weak /import /' $< > $@

$(TYPES_DIR)/mount.pb.cc \
$(TYPES_DIR)/metrics.pb.cc \
$(TYPES_DIR)/descriptor.pb.cc \
$(TYPES_DIR)/task/task.pb.cc: %.pb.cc : %.proto
	$(PROTOC) --cpp_out=. -I$(GOGO_PROTOPATH) -I. $<

tasks.proto: $(TASKS_PROTOPATH)/tasks.proto
	sed 's/import weak /import /' $< > $@

containers.proto: $(CONTAINERD_PROTOPATH)/containers.proto
	sed 's/import weak /import /' $< > $@

tasks.pb.cc containers.pb.cc: %.pb.cc : %.proto
	$(PROTOC) --cpp_out=. -I. -I$(GOGO_PROTOPATH) $<

gogoproto/gogo.pb.cc: $(GOGO_PROTOPATH)/gogoproto/gogo.proto
	$(MKDIR_P) $(@D)
	$(PROTOC) --cpp_out=. -I$(GOGO_PROTOPATH) $<

