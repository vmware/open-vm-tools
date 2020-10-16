#!/bin/sh

# Copyright (C) 2020 VMware, Inc.  All rights reserved.

# check if necesary commands exist
command -v ss >/dev/null 2>&1 || { echo >&2 "ss doesn't exist"; exit 1; }
command -v grep >/dev/null 2>&1 || { echo >&2 "grep doesn't exist"; exit 1; }
command -v sort >/dev/null 2>&1 || { echo >&2 "sort doesn't exist"; exit 1; }
command -v ps >/dev/null 2>&1 || { echo >&2 "ps doesn't exist"; exit 1; }

space_separated_pids=$(ss -lntup | grep -Eo "pid=[0-9]+" | grep -Eo "[0-9]*" | sort -u)

get_command_line() {
  ps --pid $1 -o command
}

get_version() {
  PATTERN=$1
  VERSION_OPTION=$2
  for p in $space_separated_pids
  do
    COMMAND=$(get_command_line $p | grep -Eo "$PATTERN")
    [ ! -z "$COMMAND" ] && echo VERSIONSTART "$p" "$("${COMMAND%%[[:space:]]*}" $VERSION_OPTION 2>&1)" VERSIONEND
  done
}

get_vcops_version() {
  cat /usr/lib/vmware-vcops/user/conf/lastbuildversion.txt 2>/dev/null
}

get_srm_mgt_server_version() {
  grep -oE "<version>.*</version>" /opt/vmware/etc/appliance-manifest.xml 2>/dev/null
}

get_vcenter_appliance_version() {
  $(which vpxd 2>/dev/null) -v 2>/dev/null
}

get_vcloud_director_version() {
  PATTERN="\-DVCLOUD_HOME=\S+"
  for p in $space_separated_pids
  do
    VCLOUD_HOME=$(get_command_line $p | grep -Eo "$PATTERN" | cut -d'=' -f2)
    [ ! -z "$VCLOUD_HOME" ] && echo VERSIONSTART "$p" "$(grep product.version "${VCLOUD_HOME}/etc/global.properties" 2>/dev/null | cut -d'=' -f2 2>/dev/null)" VERSIONEND
  done
}

get_weblogic_version() {
  PATTERN="java\.security\.policy=.+/server/lib/weblogic\.policy"
  for p in $space_separated_pids
  do
    WEBLOGIC_HOME=$(get_command_line $p | grep -Eo "$PATTERN" | cut -d'=' -f2)
    WEBLOGIC_HOME="${WEBLOGIC_HOME%%/server/lib/weblogic.policy*}"
    [ ! -z "$WEBLOGIC_HOME" ] && echo VERSIONSTART "$p" "$(java -cp "${WEBLOGIC_HOME}/server/lib/weblogic.jar" weblogic.version 2>/dev/null)" VERSIONEND
  done
}

get_apache_tomcat_version() {
  PATTERN="/\S*tomcat\S*/bin/bootstrap\.jar.*\s+org\.apache\.catalina\.startup\.Bootstrap"
  for p in $space_separated_pids
  do
    TOMCAT_HOME=$(get_command_line $p | grep -Eo "$PATTERN")
    TOMCAT_HOME="${TOMCAT_HOME%%/bin/bootstrap.jar*}"
    [ ! -z "$TOMCAT_HOME" ] && echo VERSIONSTART "$p" "$(java -cp "${TOMCAT_HOME}/lib/catalina.jar" org.apache.catalina.util.ServerInfo 2>/dev/null)" VERSIONEND
  done
}

get_jboss_version() {
  PATTERN="jboss.home.dir=\S+"
  for p in $space_separated_pids
  do
    JBOSS_HOME=$(get_command_line $p | grep -Eo "$PATTERN" | cut -d'=' -f2)
    [ ! -z "$JBOSS_HOME" ] && echo VERSIONSTART "$p" "$("${JBOSS_HOME}/bin/standalone.sh" --version 2>/dev/null)" VERSIONEND
  done
}

get_db2_version() {
  db2level 2>/dev/null | grep "DB2 v"
}

get_tcserver_version() {
  command -v tcserver >/dev/null 2>&1 && { tcserver version 2>/dev/null; }
}

echo VERSIONSTART "vcops_version" "$(get_vcops_version)" VERSIONEND
echo VERSIONSTART "srm_mgt_server_version" "$(get_srm_mgt_server_version)" VERSIONEND
echo VERSIONSTART "vcenter_appliance_version" "$(get_vcenter_appliance_version)" VERSIONEND
echo VERSIONSTART "db2_version" "$(get_db2_version)" VERSIONEND
echo VERSIONSTART "tcserver_version" "$(get_tcserver_version)" VERSIONEND

get_version "/\S+/(httpd-prefork|httpd|httpd2-prefork)($|\s)" -v
get_version "/usr/(bin|sbin)/apache\S*" -v
get_version "/\S+/mysqld($|\s)" -V
get_version "\.?/\S*nginx($|\s)" -v
get_version "/\S+/srm/bin/vmware-dr($|\s)" --version
get_version "/\S+/dataserver($|\s)" -v

get_vcloud_director_version
get_weblogic_version
get_apache_tomcat_version
get_jboss_version