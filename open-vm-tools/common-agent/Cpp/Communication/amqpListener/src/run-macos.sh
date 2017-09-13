#!/bin/sh
LIB_BASE=$HOME/src/vcm/common-agent/main/Cpp
LIB_DIR=lib/mac64
TOOLCHAIN=$HOME/toolchain/mac64
LOG4CPP_LIB=$TOOLCHAIN/log4cpp-1.0/lib
export DYLD_LIBRARY_PATH=$LIB_BASE/Base/$LIB_DIR:$LIB_BASE/Common/$LIB_DIR:$LOG4CPP_LIB:$LIB_BASE/Subsystems/Integration/$LIB_DIR:$LIB_BASE/Subsystems/Communication/$LIB_DIR:$LIB_BASE/Subsystems/Communication/vmcfsdk/lib/mac64
export CAF_APPCONFIG=$HOME/src/vcm/common-agent/main/Cpp/Subsystems/Communication/src/services/amqpListener/commAmqpListener.config
./Release_mac64/CommAmqpListener -n
