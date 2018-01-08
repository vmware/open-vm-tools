#!/bin/sh

#Standard env
SCRIPT=`basename "$0"`

installDir=$(dirname $(readlink -f $0))
scriptsDir=$installDir/../scripts
configDir=$installDir/../config

set_caf_pme_paths()
{
	PATH=$PATH:$installDir:$scriptsDir
	PERSISTENCE_DIR=${CAF_INPUT_DIR}/persistence
        CERTS_DIR=${CERTS_DIR:-'/etc/vmware-tools/GuestProxyData/server'}
}

configure_caf_common()
{
    mkdir -p ${PERSISTENCE_DIR}/local
    mkdir -p ${PERSISTENCE_DIR}/remote/remote_default/cmsCertCollection
    mkdir -p ${PERSISTENCE_DIR}/protocol/amqpBroker_default/tlsCertCollection
    mkdir -p ${PERSISTENCE_DIR}/protocol/amqpBroker_default/tlsCipherCollection/

    echo "amqpBroker_default" > ${PERSISTENCE_DIR}/remote/remote_default/protocolName.txt
    echo "remote_default" > ${PERSISTENCE_DIR}/remote/remote_default/remoteId.txt
    echo "des-ede3-cbc" > ${PERSISTENCE_DIR}/remote/remote_default/cmsCipherName.txt

    echo "SRP-RSA-AES-128-CBC-SHA" > ${PERSISTENCE_DIR}/protocol/amqpBroker_default/tlsCipherCollection/tlsCipher0.txt
    echo "amqpBroker_default" >  ${PERSISTENCE_DIR}/protocol/amqpBroker_default/protocolName.txt
    echo "TLSv1" >  ${PERSISTENCE_DIR}/protocol/amqpBroker_default/tlsProtocol.txt

    cp -rf ${CERTS_DIR}/cert.pem ${PERSISTENCE_DIR}/local/cert.pem
    cp -rf ${CERTS_DIR}/key.pem ${PERSISTENCE_DIR}/local/privateKey.pem

    cp -rf ${CERTS_DIR}/cert.pem ${PERSISTENCE_DIR}/protocol/amqpBroker_default/tlsCert.pem
    cp -rf ${CERTS_DIR}/cert.pem ${PERSISTENCE_DIR}/protocol/amqpBroker_default/tlsCertCollection/tlsCert0.pem

    cp -rf ${CERTS_DIR}/cert.pem ${PERSISTENCE_DIR}/remote/remote_default/cmsCertCollection/cmsCert0.pem
    cp -rf ${CERTS_DIR}/cert.pem ${PERSISTENCE_DIR}/remote/remote_default/cmsCert.pem

    /usr/bin/vmware-guestproxycerttool -a ${PERSISTENCE_DIR}/local/cert.pem
    /usr/bin/vmware-guestproxycerttool -a ${PERSISTENCE_DIR}/protocol/amqpBroker_default/tlsCert.pem
    /usr/bin/vmware-guestproxycerttool -a ${PERSISTENCE_DIR}/protocol/amqpBroker_default/tlsCertCollection/tlsCert0.pem

}

##=============================================================================
## Main
##=============================================================================
. $scriptsDir/caf-common
sourceCafenv "$configDir"

set_caf_pme_paths
configure_caf_common

#echo QUIT | openssl s_client -connect localhost:6672 -cert ${CERTS_DIR}/cert.pem -key ${CERTS_DIR}/key.pem -CAfile ${CERTS_DIR}/cert.pem  -tls1_2
#echo QUIT | openssl s_client -connect localhost:6672 -cert ${CERTS_DIR}/cert.pem -key ${CERTS_DIR}/key.pem -CAfile ${CERTS_DIR}/cert.pem  -tls1_2

echo -n true

