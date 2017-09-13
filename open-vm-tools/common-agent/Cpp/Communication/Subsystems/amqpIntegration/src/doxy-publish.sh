#!/bin/sh
set -e
if [ "" = "$CAF_WEB_SERVER" ]; then
	CAFWEBSVR=10.25.57.32
	echo 'Using default CAFWEBSVR address'
fi
echo 'CAFWEBSVR='${CAFWEBSVR}
doxygen Doxyfile
ssh autodb@$CAFWEBSVR mkdir -p caf-downloads/doxy/communication/amqpIntegration
ssh autodb@$CAFWEBSVR rm -rf caf-downloads/doxy/communication/amqpIntegration/*
scp -r html/* autodb@$CAFWEBSVR:caf-downloads/doxy/communication/amqpIntegration
