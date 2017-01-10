#!/bin/sh

export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:$CAF_LIB_DIR
$CAF_BIN_DIR/InstallProvider $*
