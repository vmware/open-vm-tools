#!/usr/bin/env bash

# Copyright 2021-2023 VMware, Inc.
# SPDX-License-Identifier: Apache-2

## Salt VMware Tools Integration script
##  integration with Component Manager and GuestStore Helper

# latest shellcheck 0.9.0-1 is showing false negatives
# which 0.8.0-2 does not, disabling since using 0.9.0-1
# shellcheck disable=SC2317,SC2004,SC2320,SC2086

## set -u
## set -xT
set -o functrace
set -o pipefail
## set -o errexit

# using bash for now
# run this script as root, as needed to run Salt

## readonly SCRIPT_VERSION='SCRIPT_VERSION_REPLACE'
readonly SCRIPT_VERSION="1.6"

# definitions

CURL_DOWNLOAD_RETRY_COUNT=5

## Repository locations and naming
readonly default_salt_url_version="latest"
readonly salt_name="salt"
readonly repo_json_file="repo.json"
salt_url_version="${default_salt_url_version}"
pre_3006_base_url="https://repo.saltproject.io/salt/vmware-tools-onedir"
# Release
post_3005_base_url="https://repo.saltproject.io/salt/py3/onedir"
base_url=""

# Salt file and directory locations
readonly base_salt_location="/opt/saltstack"
readonly salt_dir="${base_salt_location}/${salt_name}"
readonly salt_conf_dir="/etc/salt"
readonly salt_minion_conf_name="minion"
readonly salt_minion_conf_file="${salt_conf_dir}/${salt_minion_conf_name}"
readonly salt_master_sign_dir="${salt_conf_dir}/pki/${salt_minion_conf_name}"

readonly log_dir="/var/log"

readonly list_files_systemd_to_remove="/lib/systemd/system/salt-minion.service
/usr/lib/systemd/system/salt-minion.service
/usr/local/lib/systemd/system/salt-minion.service
/etc/systemd/system/salt-minion.service
"

readonly list_file_dirs_to_remove="${base_salt_location}
/etc/salt
/var/run/salt
/var/cache/salt
/var/log/salt
/usr/bin/salt-*
${list_files_systemd_to_remove}
"
## /var/log/vmware-${SCRIPTNAME}-*

readonly salt_dep_file_list="systemctl
curl
sha512sum
vmtoolsd
grep
awk
sed
cut
wget
"

readonly allowed_log_file_action_names="status
depend
install
clear
remove
default
"

readonly salt_wrapper_file_list="minion
call
"

readonly salt_minion_service_wrapper=\
"# Copyright 2021-2023 VMware, Inc.
# SPDX-License-Identifier: Apache-2

[Unit]
Description=The Salt Minion
Documentation=man:salt-minion(1) file:///usr/share/doc/salt/html/contents.html https://docs.saltproject.io/en/latest/contents.html
After=network.target
# After=ConnMgr.service ProcMgr.service sockets.target

[Service]
KillMode=process
Type=notify
NotifyAccess=all
LimitNOFILE=8192
MemoryLimit=250M
Nice=19
ExecStart=/usr/bin/salt-minion

[Install]
WantedBy=multi-user.target
"

# Onedir detection locations
readonly onedir_post_3005_location="${salt_dir}/salt-minion"
readonly onedir_pre_3006_location="${salt_dir}/run/run"

declare -a list_of_onedir_locations_check
list_of_onedir_locations_check[0]="${onedir_pre_3006_location}"
list_of_onedir_locations_check[1]="${onedir_post_3005_location}"

## VMware file and directory locations
readonly vmtools_base_dir_etc="/etc/vmware-tools"
readonly vmtools_conf_file="tools.conf"
readonly vmtools_salt_minion_section_name="salt_minion"

## VMware guestVars file and directory locations
readonly guestvars_base_dir="guestinfo./vmware.components"
readonly \
guestvars_salt_dir="${guestvars_base_dir}.${vmtools_salt_minion_section_name}"
readonly guestvars_salt_args="${guestvars_salt_dir}.args"
readonly guestvars_salt_desiredstate="${guestvars_salt_dir}.desiredstate"


# Array for minion configuration keys and values
# allows for updates from number of configuration sources before final
# write to /etc/salt/minion
declare -a m_cfg_keys
declare -a m_cfg_values


## Component Manager Installer/Script return/exit status codes
# return/exit Status codes
#  100 + 0 => installed
#  100 + 1 => installing
#  100 + 2 => notInstalled
#  100 + 3 => installFailed
#  100 + 4 => removing
#  100 + 5 => removeFailed
#  100 + 6 => externalInstall
#  126 => scriptFailed
#  130 => scriptTerminated
declare -A STATUS_CODES_ARY
STATUS_CODES_ARY[installed]=100
STATUS_CODES_ARY[installing]=101
STATUS_CODES_ARY[notInstalled]=102
STATUS_CODES_ARY[installFailed]=103
STATUS_CODES_ARY[removing]=104
STATUS_CODES_ARY[removeFailed]=105
STATUS_CODES_ARY[externalInstall]=106
STATUS_CODES_ARY[scriptFailed]=126
STATUS_CODES_ARY[scriptTerminated]=130

# log levels available for logging, order sensitive
readonly LOG_MODES_AVAILABLE=(silent error warning info debug)
declare -A LOG_LEVELS_ARY
LOG_LEVELS_ARY[silent]=0
LOG_LEVELS_ARY[error]=1
LOG_LEVELS_ARY[warning]=2
LOG_LEVELS_ARY[info]=3
LOG_LEVELS_ARY[debug]=4


STATUS_CHK=0
DEPS_CHK=0
USAGE_HELP=0
UNINSTALL_FLAG=0
VERBOSE_FLAG=0
VERSION_FLAG=0

CLEAR_ID_KEYS_FLAG=0
CLEAR_ID_KEYS_PARAMS=""

INSTALL_FLAG=0
INSTALL_PARAMS=""

MINION_VERSION_FLAG=0
MINION_VERSION_PARAMS=""

LOG_LEVEL_FLAG=0
LOG_LEVEL_PARAMS=""

#default logging level to errors, similar to Windows script
LOG_LEVEL=${LOG_LEVELS_ARY[warning]}

SOURCE_FLAG=0
SOURCE_PARAMS=""

# Flag for pre_3006 and post_3005, 0 => pre_3006, 1 => post_3005
POST_3005_FLAG=0
POST_3005_MAJOR_VER_FLAG=0


# helper functions

_timestamp() {
    date -u "+%Y-%m-%d %H:%M:%S"
}

_log() {
    echo "$(_timestamp) $*" >> \
        "${log_dir}/vmware-${SCRIPTNAME}-${LOG_ACTION}-${logdate}.log"
}

_display() {
    if [[ ${VERBOSE_FLAG} -eq 1 ]]; then echo "$1"; fi
    _log "$*"
}

_error_log() {
    if [[ ${LOG_LEVELS_ARY[error]} -le ${LOG_LEVEL} ]]; then
        local log_file=""
        log_file="${log_dir}/vmware-${SCRIPTNAME}-${LOG_ACTION}-${logdate}.log"
        msg="ERROR: $*"
        echo "$msg" 1>&2
        echo "$(_timestamp) $msg" >> "${log_file}"
        echo "One or more errors found. See ${log_file} for details." 1>&2
        CURRENT_STATUS=${STATUS_CODES_ARY[scriptFailed]}
        exit ${STATUS_CODES_ARY[scriptFailed]}
    fi
}

_info_log() {
    if [[ ${LOG_LEVELS_ARY[info]} -le ${LOG_LEVEL} ]]; then
        msg="INFO: $*"
        _log "${msg}"
    fi
}

_warning_log() {
    if [[ ${LOG_LEVELS_ARY[error]} -le ${LOG_LEVEL} ]]; then
        msg="WARNING: $*"
        _log "${msg}"
    fi
}

_debug_log() {
    if [[ ${LOG_LEVELS_ARY[debug]} -le ${LOG_LEVEL} ]]; then
        msg="DEBUG: $*"
        _log "${msg}"
    fi
}

_yesno() {
read -r -p "Continue (y/n)?" choice
case "$choice" in
  y|Y ) echo "yes";;
  n|N ) echo "no";;
  * ) echo "invalid";;
esac
}


#
# _usage
#
#   Prints out help text
#

 _usage() {
     echo ""
     echo "usage: ${0}"
     echo "             [-c|--clear] [-d|--depend] [-h|--help] [-i|--install]"
     echo "             [-j|--source] [-l|--loglevel] [-m|--minionversion]"
     echo "             [-r|--remove] [-s|--status] [-v|--version]"
     echo ""
     echo "  -c, --clear     clear previous minion identifier and keys,"
     echo "                     and set specified identifier if present"
     echo "  -d, --depend    check dependencies required to run script exist"
     echo "  -h, --help      this message"
     echo "  -i, --install   install and activate salt-minion configuration"
     echo "                     parameters key=value can also be passed on CLI"
     echo "  -j, --source   specify location to install Salt Minion from"
     echo "                     default is repo.saltproject.io location"
     echo "                 for example: url location"
     echo "                     http://my_web_server.com/my_salt_onedir"
     echo "                     https://my_web_server.com/my_salt_onedir"
     echo "                     file://my_path/my_salt_onedir"
     echo "                     //my_path/my_salt_onedir"
     echo "                 if specific version of Salt Minion specified, -m"
     echo "                 then its appended to source, default[latest]"
     echo "  -l, --loglevel  set log level for logging,"
     echo "                     silent error warning debug info"
     echo "                     default loglevel is warning"
     echo "  -m, --minionversion install salt-minion version, default[latest]"
     echo "  -r, --remove    deactivate and remove the salt-minion"
     echo "  -s, --status    return status for this script"
     echo "  -v, --version   version of this script"
     echo ""
     echo "  salt-minion VMTools integration script"
     echo "      example: $0 --status"
}


# work functions

#
# _cleanup_int
#
#   Cleanups any running process and areas on control-C
#
#
# Results:
#   Exits with hard-coded value 130
#

_cleanup_int() {
    rm -rf "$WORK_DIR"
    _debug_log "$0:${FUNCNAME[0]} Deleted temp working directory $WORK_DIR"

    exit ${STATUS_CODES_ARY[scriptTerminated]}
}

#
# _cleanup_exit
#
#   Cleanups any running process and areas on exit
#
_cleanup_exit() {
    rm -rf "$WORK_DIR"
    _debug_log "$0:${FUNCNAME[0]} Deleted temp working directory $WORK_DIR"
    ## exit ${CURRENT_STATUS}
}

trap _cleanup_int INT
trap _cleanup_exit EXIT


# cheap trim relying on echo to convert tabs to spaces and
# all multiple spaces to a single space
_trim() {
    echo "$1"
}


#
# _set_log_level
#
#   Set log_level for logging,
#       log_level 'silent','error','warning','info','debug'
#       default 'warning'
#
# Results:
#   Returns with exit code
#

_set_log_level() {

    _info_log "$0:${FUNCNAME[0]} processing setting set log_level for logging"

    local ip_level=""
    local valid_level=0
    local old_log_level=${LOG_LEVEL}

    ip_level=$( echo "$1" | cut -d ' ' -f 1)
    scam=${#LOG_MODES_AVAILABLE[@]}
    for ((i=0; i<scam; i++)); do
        name=${LOG_MODES_AVAILABLE[i]}
        if [[ "${ip_level}" = "${name}" ]]; then
            valid_level=1
            break
        fi
    done
    if [[ ${valid_level} -ne 1 ]]; then
        _warning_log "$0:${FUNCNAME[0]} attempted to set log_level with "\
            "invalid input, log_level unchanged, currently "\
            "'${LOG_MODES_AVAILABLE[${LOG_LEVEL}]}'"
    else
        LOG_LEVEL=${LOG_LEVELS_ARY[${ip_level}]}
        _info_log "$0:${FUNCNAME[0]} changed log_level from "\
            "'${LOG_MODES_AVAILABLE[${old_log_level}]}' to "\
            "'${LOG_MODES_AVAILABLE[${LOG_LEVEL}]}'"
    fi
    return 0
}


#
# _set_install_minion_version_fn
#
#   Set the version of Salt Minion wanted to install
#       default 'latest'
#
#   Note: typically Salt version includes the release number in addition to
#         version number or 'latest' for the most recent release
#
#           for example: 3003.3-1
#
# Results:
#   Returns with exit code
#

_set_install_minion_version_fn() {

    if [[ "$#" -ne 1 ]]; then
        _error_log "$0:${FUNCNAME[0]} error expected one parameter "\
            "specifying the version of the salt-minion to install or 'latest'"
    fi

    _info_log "$0:${FUNCNAME[0]} processing setting Salt version for "\
        "salt-minion to install"
    local salt_version=""

    salt_version=$(echo "$1" | cut -d ' ' -f 1)
    if [[ "latest" = "${salt_version}" ]]; then
        _debug_log "$0:${FUNCNAME[0]} input Salt version for salt-minion to "\
            "install is 'latest', leaving as default "\
            "'${default_salt_url_version}' for now"

    else
        _debug_log "$0:${FUNCNAME[0]} input Salt version for salt-minion to "\
            "install is '${salt_version}'"

        salt_url_version="${salt_version}"
        _debug_log "$0:${FUNCNAME[0]} set Salt version for salt-minion to "\
            "install to '${salt_url_version}'"
    fi

    return 0
}

#
# _set_post_3005_flags_from_version
#
#   Sets the POST_3005_FLAG and POST_3005_MAJOR_VER_FLAG
#       from the version currently present in salt_url_version
#
#   Will also set base_url if not already defined by --source option
#
# Results:
#   Returns with exit code
#
_set_post_3005_flags_from_version() {
    _info_log "$0:${FUNCNAME[0]} setting POST_3005_FLAG and "\
        "POST_3005_MAJOR_VER_FLAG from Salt version '${salt_url_version}'"

    if [[ "latest" = "${salt_url_version}" ]]; then
        POST_3005_FLAG=1
        POST_3005_MAJOR_VER_FLAG=1
        base_url="${post_3005_base_url}"
        # done, already have url for latest & major versions
        _debug_log "$0:${FUNCNAME[0]} post-3005 install, using latest "\
            "base_url '${base_url}'"
    else
        ver_chk=$(echo "${salt_url_version}" | cut -d '.' -f 1)
        if [[ ${ver_chk} -ge 3006 ]]; then
            POST_3005_FLAG=1
            ver_chk_major=$(echo "${salt_url_version}" | cut -d '.' -f 1)
            ver_chk_minor=$(echo "${salt_url_version}" | cut -d '.' -f 2)
            _debug_log "$0:${FUNCNAME[0]} post-3005 install, checking "\
                "for major version only '${ver_chk_major}', minor "\
                "'${ver_chk_minor}'"
            if [[ "${ver_chk_major}" = "${ver_chk_minor}" ]]; then
                POST_3005_MAJOR_VER_FLAG=1
                base_url="${post_3005_base_url}"
            else
                base_url="${post_3005_base_url}/minor"
            fi
            _debug_log "$0:${FUNCNAME[0]} post-3005 install, for "\
                "'${salt_url_version}' using base_url '${base_url}'"
        else
            # install pre-3006, use older url
            base_url="${pre_3006_base_url}"
            _debug_log "$0:${FUNCNAME[0]} pre-3006 install, for "\
                "'${salt_url_version}' using base_url '${base_url}'"
        fi
    fi
}


#
# _update_minion_conf_ary
#
#   Updates the running minion_conf array with input key and value
#   updating with the new value if the key is already found
#
# Results:
#   Updated array
#

_update_minion_conf_ary() {
    local cfg_key="$1"
    local cfg_value="$2"
    local _retn=0

    if [[ "$#" -ne 2 ]]; then
        _error_log "$0:${FUNCNAME[0]} error expect two parameters, "\
            "a key and a value"
    fi

    # now search m_cfg_keys array to see if new key
    key_ary_sz=${#m_cfg_keys[@]}
    if [[ ${key_ary_sz} -ne 0 ]]; then
        # need to check if array has same key
        local chk_found=0
        for ((chk_idx=0; chk_idx<key_ary_sz; chk_idx++))
        do
            if [[ "${m_cfg_keys[${chk_idx}]}" = "${cfg_key}" ]]; then
                m_cfg_values[${chk_idx}]="${cfg_value}"
                _debug_log "$0:${FUNCNAME[0]} updating minion configuration "\
                    "array key '${m_cfg_keys[${chk_idx}]}' with "\
                    "value '${cfg_value}'"
                chk_found=1
                break;
            fi
        done
        if [[ ${chk_found} -eq 0 ]]; then
            # new key for array
            m_cfg_keys[${key_ary_sz}]="${cfg_key}"
            m_cfg_values[${key_ary_sz}]="${cfg_value}"
            _debug_log "$0:${FUNCNAME[0]} adding to minion configuration "\
                "array new key '${cfg_key}' and value '${cfg_value}'"
        fi
    else
        # initial entry
        m_cfg_keys[0]="${cfg_key}"
        m_cfg_values[0]="${cfg_value}"
        _debug_log "$0:${FUNCNAME[0]} adding initial minion configuration "\
            "array, key '${cfg_key}' and value '${cfg_value}'"
    fi
    return ${_retn}
}


#
# _fetch_vmtools_salt_minion_conf_tools_conf
#
#   Retrieve the configuration for salt-minion from VMTools
#                                           configuration file tools.conf
#
# Results:
#   Exits with new VMTools configuration file if none found or salt-minion
#   configuration file updated with configuration read from VMTools
#   configuration file section for salt_minion
#

_fetch_vmtools_salt_minion_conf_tools_conf() {
    # fetch the current configuration for section salt_minion
    # from vmtoolsd configuration file
    local _retn=0
    if [[ ! -f "${vmtools_base_dir_etc}/${vmtools_conf_file}" ]]; then
        # conf file doesn't exist, create it
        mkdir -p "${vmtools_base_dir_etc}"
        echo "[${vmtools_salt_minion_section_name}]" \
            > "${vmtools_base_dir_etc}/${vmtools_conf_file}"
        _warning_log "$0:${FUNCNAME[0]} creating empty configuration "\
            "file ${vmtools_base_dir_etc}/${vmtools_conf_file}"
    else
        # need to extract configuration for salt-minion
        # find section name ${vmtools_salt_minion_section_name}
        # read configuration till next section, output salt-minion conf file

        local salt_config_flag=0
        while IFS= read -r line
        do
            line_value=$(_trim "${line}")
            if [[ -n "${line_value}" ]]; then
                _debug_log "$0:${FUNCNAME[0]} processing tools.conf "\
                    "line '${line}'"
                if echo "${line_value}" | grep -q '^\[' ; then
                    if [[ ${salt_config_flag} -eq 1 ]]; then
                        # if new section after doing Salt config, we are done
                        break;
                    fi
                    if [[ ${line_value} = \
                        "[${vmtools_salt_minion_section_name}]" ]]; then
                        # have section, get configuration values, set flag and
                        #  start fresh salt-minion configuration file
                        salt_config_flag=1
                    fi
                elif [[ ${salt_config_flag} -eq 1 ]]; then
                    # read config ahead of section check, better logic flow
                    cfg_key=$(echo "${line}" | cut -d '=' -f 1)
                    cfg_value=$(echo "${line}" | cut -d '=' -f 2)
                    _update_minion_conf_ary "${cfg_key}" "${cfg_value}" || {
                        _error_log "$0:${FUNCNAME[0]} error updating minion "\
                            "configuration array with key '${cfg_key}' and "\
                            "value '${cfg_value}', retcode '$?'";
                    }
                else
                    _debug_log "$0:${FUNCNAME[0]} skipping tools.conf "\
                        "line '${line}'"
                fi
            fi
        done < "${vmtools_base_dir_etc}/${vmtools_conf_file}"
    fi
    return ${_retn}
}


#
# _fetch_vmtools_salt_minion_conf_guestvars
#
#   Retrieve the configuration for salt-minion from VMTools guest variables
#
# Results:
#   salt-minion configuration file updated with configuration read
#                                           from VMTools guest variables
#   configuration file section for salt_minion
#

_fetch_vmtools_salt_minion_conf_guestvars() {
    # fetch the current configuration for section salt_minion
    # from guest variables args

    local _retn=0
    local gvar_args=""

    gvar_args=$(vmtoolsd --cmd "info-get ${guestvars_salt_args}" 2>/dev/null)\
        || { _warning_log "$0:${FUNCNAME[0]} unable to retrieve arguments "\
            "from guest variables location ${guestvars_salt_args}, "\
            "retcode '$?'";
    }

    if [[ -z "${gvar_args}" ]]; then return ${_retn}; fi

    _debug_log "$0:${FUNCNAME[0]} processing arguments from guest variables "\
        "location ${guestvars_salt_args}"

    for idx in ${gvar_args}
    do
        cfg_key=$(echo "${idx}" | cut -d '=' -f 1)
        cfg_value=$(echo "${idx}" | cut -d '=' -f 2)
        _update_minion_conf_ary "${cfg_key}" "${cfg_value}" || {
            _error_log "$0:${FUNCNAME[0]} error updating minion configuration "\
                "array with key '${cfg_key}' and value '${cfg_value}', "\
                "retcode '$?'";
        }
    done

    return ${_retn}
}


#
# _fetch_vmtools_salt_minion_conf_cli_args
#
#   Retrieve the configuration for salt-minion from any args '$@' passed
#                                               on the command line
#
# Results:
#   Exits with new VMTools configuration file if none found
#   or salt-minion configuration file updated with configuration read
#   from VMTools configuration file section for salt_minion
#

_fetch_vmtools_salt_minion_conf_cli_args() {
    local _retn=0
    local cli_args=""
    local cli_no_args=0

    cli_args="$*"
    cli_no_args=$#
    if [[ ${cli_no_args} -ne 0 ]]; then
        _debug_log "$0:${FUNCNAME[0]} processing command line "\
            "arguments '${cli_args}'"
        for idx in ${cli_args}
        do
            # check for start of next option, idx starts with '-' (covers '--')
            if [[ "${idx}" = --* ]]; then
                break
            fi
            cfg_key=$(echo "${idx}" | cut -d '=' -f 1)
            cfg_value=$(echo "${idx}" | cut -d '=' -f 2)
            _update_minion_conf_ary "${cfg_key}" "${cfg_value}" || {
                _error_log "$0:${FUNCNAME[0]} error updating minion "\
                "configuration array with key '${cfg_key}' and "\
                "value '${cfg_value}', retcode '$?'";
            }
        done
    fi
    return ${_retn}
}


#
# _randomize_minion_id
#
#   Added 5 digit random number to input minion identifier
#
# Input:
#       String to add random number to
#       if no input, default string 'minion_' used
#
# Results:
#   exit, return value etc
#

_randomize_minion_id() {

    local ran_minion=""
    local ip_string="$1"

    if [[ -z "${ip_string}" ]]; then
        ran_minion="minion_${RANDOM:0:5}"
    else
        #provided input
        ran_minion="${ip_string}_${RANDOM:0:5}"
    fi
    _debug_log "$0:${FUNCNAME[0]} generated randomized minion "\
            "identifier '${ran_minion}'"
    echo "${ran_minion}"
}


#
# _fetch_vmtools_salt_minion_conf
#
#   Retrieve the configuration for salt-minion
#       precedence order: L -> H
#           from VMware Tools guest Variables
#           from VMware Tools configuration file tools.conf
#           from any command line parameters
#
# Results:
#   Exits with new salt-minion configuration file written
#

_fetch_vmtools_salt_minion_conf() {
    # fetch the current configuration for section salt_minion
    # from vmtoolsd configuration file

    _debug_log "$0:${FUNCNAME[0]} retrieving minion configuration parameters"
    _fetch_vmtools_salt_minion_conf_guestvars || {
        _error_log "$0:${FUNCNAME[0]} failed to process guest variable "\
            "arguments, retcode '$?'";
    }
    _fetch_vmtools_salt_minion_conf_tools_conf || {
        _error_log "$0:${FUNCNAME[0]} failed to process tools.conf file, "\
            "retcode '$?'";
    }
    _fetch_vmtools_salt_minion_conf_cli_args "$*" || {
        _error_log "$0:${FUNCNAME[0]} failed to process command line "\
            "arguments, retcode '$?'";
    }

    # now write minion conf array to salt-minion configuration file
    local mykey_ary_sz=${#m_cfg_keys[@]}
    local myvalue_ary_sz=${#m_cfg_values[@]}
    if [[ "${mykey_ary_sz}" -ne "${myvalue_ary_sz}" ]]; then
        _error_log "$0:${FUNCNAME[0]} key '${mykey_ary_sz}' and "\
            "value '${myvalue_ary_sz}' array sizes for minion_conf "\
            "don't match"
    else
        mkdir -p "${salt_conf_dir}"
        echo "# Minion configuration file - created by VMTools Salt script" \
            > "${salt_minion_conf_file}"
        echo "enable_fqdns_grains: False" >> "${salt_minion_conf_file}"
        for ((chk_idx=0; chk_idx<mykey_ary_sz; chk_idx++))
        do
            # appending to salt-minion configuration file since it
            # should be new and no configuration set

            # check for special case of signed master's public key
            # verify_master_pubkey_sign=master_sign.pub
            if [[ "${m_cfg_keys[${chk_idx}]}" \
                    = "verify_master_pubkey_sign" ]]; then
                _debug_log "$0:${FUNCNAME[0]} processing minion "\
                    "configuration parameters for master public signed key"
                echo "${m_cfg_keys[${chk_idx}]}: True" \
                    >> "${salt_minion_conf_file}"
                mkdir -p "/etc/salt/pki/minion"
                cp -f "${m_cfg_values[${chk_idx}]}" \
                    "${salt_master_sign_dir}/"
            else
                echo "${m_cfg_keys[${chk_idx}]}: ${m_cfg_values[${chk_idx}]}" \
                    >> "${salt_minion_conf_file}"
            fi
        done
    fi

    _info_log "$0:${FUNCNAME[0]} successfully retrieved the salt-minion "\
        "configuration from configuration sources"
    return 0
}


#
# _curl_download
#
#   Retrieve file from specified url to specific file
#
# Results:
#   Exits with 0 or error code
#

_curl_download() {
    local file_name="$1"
    local file_url="$2"
    local download_retry_failed=1       # assume issues
    local _retn=0

    _info_log "$0:${FUNCNAME[0]} attempting download of file '${file_name}'"

    for ((i=0; i<CURL_DOWNLOAD_RETRY_COUNT; i++))
    do
        # ensure minimum version of TLS used is v1.2
        curl -o "${file_name}" --tlsv1.2 -fsSL "${file_url}"
        _retn=$?
        if [[ ${_retn} -ne 0 ]]; then
            _warning_log "$0:${FUNCNAME[0]} failed to download file "\
                "'${file_name}' from '${file_url}' on '${i}' attempt, "\
                "retcode '${_retn}'"
        else
            download_retry_failed=0
            _debug_log "$0:${FUNCNAME[0]} successfully downloaded file "\
                "'${file_name}' from '${file_url}' after '${i}' attempts"
            break
        fi
    done
    if [[ ${download_retry_failed} -ne 0 ]]; then
        _error_log "$0:${FUNCNAME[0]} failed to download file '${file_name}' "\
            "from '${file_url}' after '${CURL_DOWNLOAD_RETRY_COUNT}' attempts"
    fi

    _info_log "$0:${FUNCNAME[0]} successfully downloaded file "\
        "'${file_name}' from '${file_url}'"
    return 0
}


#
# _parse_json_specd_ver
#
#   Retrieve the salt-minion from Salt repository
#
# Results:
#   Echos string containing colon separated version, name and sha512
#   from parsed input repo json file
#   Echos empty '' if 'salt_url_version' is not found in repo json file
#
#   Note: salt_url_version defaults to 'latest' unless set to a specific
#       Salt minion version, for example: 3004.1-1
#

 _parse_json_specd_ver() {
    local file_name="$1"
    local file_value=""
    local blk_count=0
    local specd_ver_blk_count=0
    local specd_ver_flag=0
    local found_specd_ver_linux=0

    local var1=""
    local var2=""
    local machine_arch_chk="${MACHINE_ARCH}"
    declare -A rdict

    _info_log "$0:${FUNCNAME[0]} parsing of repo json file '${file_name}'"

    if [[ ${POST_3005_FLAG} -eq 0 ]]; then
        machine_arch_chk="amd64"    # pre_3006 used amd64
    fi

    file_value=$(<"${file_name}")

    # limit 80 cols
    var1=$(echo "${file_value}" | sed 's/,/,\n/g' | sed 's/{/\n{\n/g')
    var2=$(echo "${var1}" | sed 's/}/\n}\n/g' | sed 's/,//g' | sed 's/"//g')

    while IFS= read -r line
    do
        _debug_log "$0:${FUNCNAME[0]} parsing line '${line}'"
        if [[ -z "${line}" ]]; then
            continue
        fi
        if [[ "${line}" = "{" ]]; then
            (( blk_count++ ))
        elif [[ "${line}" = "}" ]]; then
            # examine directory just read in
            if [[  ${specd_ver_flag} -eq 1 ]]; then
                if [[ "${rdict['os']}" = "linux" \
                    && "${rdict['arch']}" = "${machine_arch_chk}" ]]; then
                    # have linux values for specd_ver
                    _debug_log "$0:${FUNCNAME[0]} parsed following linux for "\
                    "specified version '${salt_url_version}' from repo json "\
                    "file '${file_name}', os ${rdict['os']}, version "\
                    "${rdict['version']}, name ${rdict['name']}, sha512 "\
                    "${rdict['SHA512']}"
                    found_specd_ver_linux=1
                    break
                fi
            fi

            if [[ ${blk_count} -eq ${specd_ver_blk_count} ]]; then
                specd_ver_flag=0
                ## break
            fi
            (( blk_count-- ))
        else
            if [[ ${POST_3005_FLAG} -eq 1 \
                && ${POST_3005_MAJOR_VER_FLAG} -eq 1 ]]; then
                # doing major version check
                line_major_key=$(echo "${line}" | cut -d ':' -f 1 | cut -d '-' -f 2 | cut -d '.' -f 1 |xargs)
                line_key=$(echo "${line}" | cut -d ':' -f 1 | xargs)
                line_value=$(echo "${line}" | cut -d ':' -f 2 | xargs)
                _debug_log "$0:${FUNCNAME[0]} check line_major_key "\
                    "'${line_major_key}' again salt_url_version "\
                    "'${salt_url_version}', line_key '${line_key}', "\
                    "line_value '${line_value}'"
                if [[ "${line_major_key}" = "${salt_url_version}" ]]; then
                    # blk_count encountered 'specd_ver', closing brace check
                    specd_ver_flag=1
                    specd_ver_blk_count=${blk_count}
                    (( specd_ver_blk_count++ ))
                    _debug_log "$0:${FUNCNAME[0]} found specd version, "\
                        "version '${salt_url_version}' and line_major_key "\
                        "'${line_major_key}', line_key '${line_key}' "\
                        "specd_ver_blk_count '${specd_ver_blk_count}'"
                else
                    rdict["${line_key}"]="${line_value}"
                    _debug_log "$0:${FUNCNAME[0]} updated dictionary for "\
                        "major version with line_key '${line_key}' and "\
                        "line_value '${line_value}'"
                fi
            else
                line_key=$(echo "${line}" | cut -d ':' -f 1 | xargs)
                line_value=$(echo "${line}" | cut -d ':' -f 2 | xargs)
                _debug_log "$0:${FUNCNAME[0]} check line_key '${line_key}' "\
                    "again salt_url_version '${salt_url_version}', "\
                    "line_value '${line_value}'"
                if [[ "${line_key}" = "${salt_url_version}" ]]; then
                    # blk_count encountered 'specd_ver', closing brace check
                    specd_ver_flag=1
                    specd_ver_blk_count=${blk_count}
                    (( specd_ver_blk_count++ ))
                    _debug_log "$0:${FUNCNAME[0]} found specd version, "\
                        "version '${salt_url_version}' and line_key "\
                        "'${line_key}' and specd_ver_blk_count "\
                        "'${specd_ver_blk_count}'"
                else
                    rdict["${line_key}"]="${line_value}"
                    _debug_log "$0:${FUNCNAME[0]} updated dictionary with "\
                    "line_key '${line_key}' and line_value '${line_value}'"
                fi
            fi
        fi
    done <<< "${var2}"

    if [[ ${found_specd_ver_linux} -eq 1 ]]; then
        echo "${rdict['version']}:${rdict['name']}:${rdict['SHA512']}"
    else
        _error_log "$0:${FUNCNAME[0]} unable to parse version, name and "\
        "sha512 from repo json file '${file_name}'"
        # echo ""
    fi
    return 0
}


#
# _fetch_salt_minion
#
#   Retrieve the salt-minion from Salt repository
#
# Note:
#   pre_3006    The repo.json file only existed in one place (salt/onedir)
#               and contained everything in the directory and sub-directories
#               where the repo.json file resides.
#   post_3005   There are two repo.json files:
#                   top level (salt/py3/onedir):
#                       repo.json contains 'latest' and 'major versions' only.
#                   minor level (salt/py3/onedir/minor):
#                       repo.json contains 'latest' and 'minor versions' only.
#
# With the 3006 release a breaking change in directory structure was introduced
# to bring conformity with directory structure used for packages.
#
# Side Effects:
#   CURRENT_STATUS updated
#
# Results:
#   Exits with 0 or error code
#

_fetch_salt_minion() {
    # fetch the current salt-minion into specified location
    # could check if already there but by always getting it
    # ensure we are not using stale versions
    local _retn=0
    local calc_sha512sum=1
    local download_retry_failed=1       # assume issues

    local salt_pkg_name=""
    local salt_url=""
    local json_version_name_sha=""

    local local_base_url=""
    local local_file_flag=0
    local local_count_repo_json=0

    local salt_tarball=""
    local salt_tarball_SHA512=""

    local salt_json_version=""
    local salt_json_name=""
    local salt_json_sha512=""
    local salt_pkg512=""

    local install_onedir_chk=0
    local sys_arch=""

    local ver_chk=""
    local ver_chk_major=""
    local ver_chk_minor=""

    _debug_log "$0:${FUNCNAME[0]} retrieve the salt-minion and check "\
        "its validity"

    CURRENT_STATUS=${STATUS_CODES_ARY[installFailed]}
    mkdir -p ${base_salt_location}
    ## cd ${base_salt_location} || return $?
    cd "${WORK_DIR}" || return $?

    # check for pre-3006 or post-3005 and adjust base_url
    # unless already defined by --source option
    if [[ -z "${base_url}" ]]; then
        _debug_log "$0:${FUNCNAME[0]} no source option used, determine "\
            "version attempting to install, version '${salt_url_version}"
        _set_post_3005_flags_from_version
    else
        _debug_log "$0:${FUNCNAME[0]} source url provided, need to scan for "\
            "pre 3006 / post 3005, and local file using base_url '${base_url}'"

        # curl on Linux doesn't support file:// support
        if echo "${base_url}" | grep -q '^/' ; then
            local_base_url="${base_url}"
            local_file_flag=1
            _debug_log "$0:${FUNCNAME[0]} using source '${local_base_url}'"\
            "from '${base_url}'"
        elif echo "${base_url}" | grep -q '^file://' ; then
            local_base_url="${base_url//file:/}"
            local_file_flag=1
            _debug_log "$0:${FUNCNAME[0]} using source '${local_base_url}'"\
            "from '${base_url}'"
        else
            _debug_log "$0:${FUNCNAME[0]} using non-local source '${base_url}'"

            ver_chk=$(echo "${base_url}" | grep  'salt/py3/onedir')
            if [[ -n "${ver_chk}" ]]; then
                POST_3005_FLAG=1
                _set_post_3005_flags_from_version
            fi
        fi
    fi

    if [[ ${local_file_flag} -eq 1 ]]; then
        # local absolute path
        # and allow for Linux handling multiple slashes

        # need to determine if pre 3005 or post 3006
        local_count_repo_json=$(find "${local_base_url}" -name repo.json|wc -l)
        if [[ ${local_count_repo_json} -eq 2 ]]; then
            POST_3005_FLAG=1
            _set_post_3005_flags_from_version
        else
            _debug_log "$0:${FUNCNAME[0]} pre-3006 local install, for "\
                "'${salt_url_version}' using specified source "\
                "'${local_base_url}'"
        fi

        if [[ ${POST_3005_FLAG} -eq 1 ]]; then
            if [[ ${POST_3005_MAJOR_VER_FLAG} -eq 1 ]]; then
                salt_url="${local_base_url}"
            else
                salt_url="${local_base_url}/minor"
            fi
        else
            salt_url="${local_base_url}/${salt_url_version}"
        fi

        if [[ -f "${salt_url}/${repo_json_file}"  ]]; then
            _debug_log "$0:${FUNCNAME[0]} successfully found file "\
            "'${repo_json_file}' in '${salt_url}/${repo_json_file}'"

            cp -a "${salt_url}/${repo_json_file}" .
            _retn=$?
            if [[ ${_retn} -ne 0 ]]; then
                CURRENT_STATUS=${STATUS_CODES_ARY[installFailed]}
                _error_log "$0:${FUNCNAME[0]} failed to find file "\
                "'${repo_json_file}' in specified location ${base_url}, "\
                "error '${_retn}'"
            fi

            # use latest from repo.json file, (version:name:sha512)
            json_version_name_sha=$(_parse_json_specd_ver "${repo_json_file}")
            salt_json_version=$(\
                echo "${json_version_name_sha}" | awk -F":" '{print $1}')
            salt_json_name=$(\
                echo "${json_version_name_sha}" | awk -F":" '{print $2}')
            salt_json_sha512=$(\
                echo "${json_version_name_sha}" | awk -F":" '{print $3}')
            _debug_log "$0:${FUNCNAME[0]} using repo.json values version "\
                "'${salt_json_version}', name '${salt_json_name}, sha512 "\
                "'${salt_json_sha512}'"

            salt_pkg_name="${salt_json_name}"
            if [[ ${POST_3005_FLAG} -eq 1 \
                && ${POST_3005_MAJOR_VER_FLAG} -eq 1 ]]; then
                cp -a "${salt_url}/minor/${salt_json_version}/${salt_pkg_name}" .
            else
                cp -a "${salt_url}/${salt_json_version}/${salt_pkg_name}" .
            fi
            _retn=$?
            if [[ ${_retn} -ne 0 ]]; then
                CURRENT_STATUS=${STATUS_CODES_ARY[installFailed]}
                _error_log "$0:${FUNCNAME[0]} failed to find file "\
                "'${salt_pkg_name}' in specified location "\
                "${salt_url}/${salt_json_version}, error '${_retn}'"
            fi
            _debug_log "$0:${FUNCNAME[0]} successfully copied from "\
                "'${salt_url}/${salt_json_version}' to file '${salt_pkg_name}'"

            salt_pkg512=$(sha512sum "${salt_pkg_name}" |awk -F" " '{print $1}')
            if [[ "${salt_pkg512}" != "${salt_json_sha512}" ]]; then
                CURRENT_STATUS=${STATUS_CODES_ARY[installFailed]}
                _error_log "$0:${FUNCNAME[0]} copied file "\
                "'${salt_url}/${salt_json_version}' failed to match "\
                "checksum in file '${repo_json_file}'"
            fi
        else
            # use defaults
            # repo.json file is missing, look for 'latest'
            # directory with onedir files and retrieve files from it
            salt_url="${local_base_url}/${salt_url_version}"

            _debug_log "$0:${FUNCNAME[0]} current directory $(pwd)"

            cp -a "${salt_url}/${salt_name}"*-linux-*.tar.?z .
            _retn=$?
            if [[ ${_retn} -ne 0 ]]; then
                CURRENT_STATUS=${STATUS_CODES_ARY[installFailed]}
                _error_log "$0:${FUNCNAME[0]} failed to find file "\
                "for Linux in specified location ${salt_url}, "\
                "error '${_retn}'"
            fi

            cp -a "${salt_url}/${salt_name}"*-linux-*.tar.?z.sha512 . 2>/dev/null
            cp -a "${salt_url}/${salt_name}"*_SHA512 .
            _retn=$?
            if [[ ${_retn} -ne 0 ]]; then
                CURRENT_STATUS=${STATUS_CODES_ARY[installFailed]}
                _error_log "$0:${FUNCNAME[0]} failed to find file "\
                "sha512 in specified location ${salt_url}, "\
                "error '${_retn}'"
            fi

            ## shellcheck
            salt_chksum_file=$(ls "${salt_name}"*_SHA512)
            salt_pkg_name=$(ls "${salt_name}"*-linux-amd64.tar.gz 2>/dev/null)
            if  [[ -z "${salt_pkg_name}" ]]; then
                # failed to find pre-3006 linux tarball,
                # attempt to find post-3005 with appro. arch
                sys_arch="${MACHINE_ARCH}"
                salt_chksum_file=$(ls "${salt_name}"*-linux-"${sys_arch}".tar.xz.sha512)
                salt_pkg_name=$(ls "${salt_name}"*-linux-"${sys_arch}".tar.xz)
            fi
            _debug_log "$0:${FUNCNAME[0]} successfully copied tarball from "\
                "'${salt_url}' file '${salt_pkg_name}'"
            _debug_log "$0:${FUNCNAME[0]} successfully coped checksum from "\
                "'${salt_url}' file '${salt_chksum_file}'"
            calc_sha512sum=$(grep "${salt_pkg_name}" \
                "${salt_chksum_file}" | sha512sum --check --status)
            if [[ ${calc_sha512sum} -ne 0 ]]; then
                CURRENT_STATUS=${STATUS_CODES_ARY[installFailed]}
                _error_log "$0:${FUNCNAME[0]} downloaded file "\
                "'${salt_pkg_name}' failed to match checksum in file "\
                "'${salt_chksum_file}'"
            fi
        fi
    else
        # assume use curl for local or remote URI
        _curl_download "${repo_json_file}" "${base_url}/${repo_json_file}"
        _debug_log "$0:${FUNCNAME[0]} successfully downloaded from "\
            "'${base_url}/${repo_json_file}' into file '${repo_json_file}'"

        if [[ -f "${repo_json_file}" ]]; then
            # use latest from repo.json file, (version:name:sha512)
            json_version_name_sha=$(_parse_json_specd_ver "${repo_json_file}")
            salt_json_version=$(\
                echo "${json_version_name_sha}" | awk -F":" '{print $1}')
            salt_json_name=$(\
                echo "${json_version_name_sha}" | awk -F":" '{print $2}')
            salt_json_sha512=$(\
                echo "${json_version_name_sha}" | awk -F":" '{print $3}')
            _debug_log "$0:${FUNCNAME[0]} using repo.json values version "\
                "'${salt_json_version}', name '${salt_json_name}, sha512 "\
                "'${salt_json_sha512}'"/

            salt_pkg_name="${salt_json_name}"
            if [[ ${POST_3005_FLAG} -eq 1 \
                && ${POST_3005_MAJOR_VER_FLAG} -eq 1 ]]; then
                salt_url="${base_url}/minor/${salt_json_version}/${salt_pkg_name}"
            else
                salt_url="${base_url}/${salt_json_version}/${salt_pkg_name}"
            fi
            _curl_download "${salt_pkg_name}" "${salt_url}"
            _debug_log "$0:${FUNCNAME[0]} successfully downloaded from "\
                "'${salt_url}' into file '${salt_pkg_name}'"

            salt_pkg512=$(sha512sum "${salt_pkg_name}" |awk -F" " '{print $1}')
            if [[ "${salt_pkg512}" != "${salt_json_sha512}" ]]; then
                CURRENT_STATUS=${STATUS_CODES_ARY[installFailed]}
                _error_log "$0:${FUNCNAME[0]} downloaded file '${salt_url}' "\
                    "failed to match checksum in file '${repo_json_file}'"
            fi
        else
            # use defaults
            # repo.json file is missing, look for 'latest'
            # directory with onedir files and retrieve files from it
            salt_url="${base_url}/${salt_url_version}"
            salt_tarball="${salt_name}*-linux-*.tar.?z"
            salt_tarball_SHA512="${salt_name}*_SHA512"

            # assume http://, https:// or similar
            wget -q -r -l1 -nd -np -A "${salt_tarball}" "${salt_url}"
            _retn=$?
            if [[ ${_retn} -ne 0 ]]; then
                CURRENT_STATUS=${STATUS_CODES_ARY[installFailed]}
                _error_log "$0:${FUNCNAME[0]} downloaded file "\
                "'${salt_tarball}' failed to download, error '${_retn}'"
            fi
            wget -q -r -l1 -nd -np -A "${salt_name}*_SHA512" "${salt_url}"
            _retn=$?
            if [[ ${_retn} -ne 0 ]]; then
                CURRENT_STATUS=${STATUS_CODES_ARY[installFailed]}
                _error_log "$0:${FUNCNAME[0]} downloaded file "\
                "'${salt_tarball_SHA512}' failed to download, error '${_retn}'"
            fi

            ## shellcheck
            salt_chksum_file=$(ls "${salt_name}"*_SHA512)
            salt_pkg_name=$(ls "${salt_name}"*-linux-amd64.tar.gz)
            if  [[ -z "${salt_pkg_name}" ]]; then
                # failed to find pre-3006 linux tarball,
                # attempt to find post-3005 with appro. arch
                sys_arch="${MACHINE_ARCH}"
                salt_chksum_file=$(ls "${salt_name}"*-linux-"${sys_arch}".tar.xz.sha512)
                salt_pkg_name=$(ls "${salt_name}"*-linux-"${sys_arch}".tar.xz)
            fi
            _debug_log "$0:${FUNCNAME[0]} successfully downloaded tarball "\
                "from '${salt_url}' into file '${salt_pkg_name}'"
            _debug_log "$0:${FUNCNAME[0]} successfully downloaded checksum "\
                "from '${salt_url}' into file '${salt_chksum_file}'"

            calc_sha512sum=$(grep "${salt_pkg_name}" \
                "${salt_chksum_file}" | sha512sum --check --status)
            if [[ ${calc_sha512sum} -ne 0 ]]; then
                CURRENT_STATUS=${STATUS_CODES_ARY[installFailed]}
                _error_log "$0:${FUNCNAME[0]} downloaded file "\
                    "'${salt_pkg_name}' failed to match checksum in file "\
                    "'${salt_chksum_file}'"
            fi
        fi
    fi

    _debug_log "$0:${FUNCNAME[0]} sha512sum match was successful"
    if [[ ${POST_3005_FLAG} -eq 1 ]]; then
        # need to setup salt user and group if not already existing
        _debug_log "$0:${FUNCNAME[0]} setup salt user and group if not "\
            "already existing"
        _SALT_GROUP=salt
        _SALT_USER=salt
        _SALT_NAME=Salt
        # 1. create group if not existing
        if getent group "${_SALT_GROUP}" 1>/dev/null; then
            _debug_log "$0:${FUNCNAME[0]} already group salt, assume user "\
                "and group setup for Salt"
        else
            _debug_log "$0:${FUNCNAME[0]} setup group and user salt"
            # create user to avoid running server as root
            # 1. create group if not existing
            groupadd --system "${_SALT_GROUP}" 2>/dev/null
            # 2. create homedir if not existing
            if [[ ! -d "${salt_dir}" ]]; then
                mkdir -p "${salt_dir}"
            fi
            # 3. create user if not existing
            if ! getent passwd | grep -q "^${_SALT_USER}:"; then
              useradd --system --no-create-home -s /sbin/nologin -g \
                "${_SALT_GROUP}" "${_SALT_USER}" 2>/dev/null
            fi
            # 4. adjust passwd entry
            usermod -c "${_SALT_NAME}" -d "${salt_dir}" -g "${_SALT_GROUP}" \
                "${_SALT_USER}" 2>/dev/null
        fi
        tar xf "${salt_pkg_name}" -C "${base_salt_location}" 1>/dev/null
        # 5. adjust file and directory permissions
        chown -R "${_SALT_USER}":"${_SALT_GROUP}" "${salt_dir}"
    else
        tar xf "${salt_pkg_name}" -C "${base_salt_location}" 1>/dev/null
    fi
    _retn=$?
    if [[ ${_retn} -ne 0 ]]; then
        CURRENT_STATUS=${STATUS_CODES_ARY[installFailed]}
        _error_log "$0:${FUNCNAME[0]} tar xf expansion of downloaded "\
            "file '${salt_pkg_name}' failed, return code '${_retn}'"
    fi
    install_onedir_chk=$(_check_onedir_minion_install)
    if [[ ${install_onedir_chk} -eq 0 ]]; then
        CURRENT_STATUS=${STATUS_CODES_ARY[installFailed]}
        _error_log "$0:${FUNCNAME[0]} expansion of downloaded file "\
            "'${salt_url}' failed to provide any onedir installed "\
            "critical files for salt-minion"
    fi
    CURRENT_STATUS=${STATUS_CODES_ARY[installed]}
    cd "${CURRDIR}" || return $?

    _info_log "$0:${FUNCNAME[0]} successfully retrieved salt-minion"
    return 0
}


#
# _check_multiple_script_running
#
#   check if more than one version of the script is running
#
# Results:
#   Checks the number of scripts running, allowing for forks etc
#   from bash etc, as root a single instance of the script returns 3
#   as sudo root a single instance of the script returns 4
#

_check_multiple_script_running() {
    local count=0
    local procs_found=""

    _info_log "$0:${FUNCNAME[0]} checking how many versions of the "\
        "script are running"

    procs_found=$(pgrep -f "${SCRIPTNAME}")
    count=$(echo "${procs_found}" | wc -l)

    _debug_log "$0:${FUNCNAME[0]} checking versions of script are running, "\
        "bashpid '${BASHPID}', processes found '${procs_found}', "\
        "and count '${count}'"

    if [[ ${count} -gt 4 ]]; then
        _error_log "$0:${FUNCNAME[0]} failed to check status, "\
            "multiple versions of the script are running"
    fi

    return 0
}


#
# _check_classic_minion_install
#
# Check if classic salt-minion is installed for the OS
#   for example: install salt-minion from rpm or deb package
#
# Results:
#   0 - No standard install found and empty string output
#   !0 - Standard install found and Salt version found output
#

_check_classic_minion_install() {

    # checks for /usr/bin, then /usr/local/bin
    # this catches 80% to 90%  of the regular cases
    # if salt-call is there, then so is a salt-minion
    # as they are installed together

    local _retn=0
    local max_file_sz=200
    local list_of_files_check="
/usr/bin/salt-call
/usr/local/bin/salt-call
"
    _info_log "$0:${FUNCNAME[0]} check if standard salt-minion installed"

    for idx in ${list_of_files_check}
    do
        if [[ -h "${idx}" ]]; then
            _debug_log "$0:${FUNCNAME[0]} found file '${idx}' "\
                "symbolic link, post-3005 installation"
            break
        elif [[ -f "${idx}" ]]; then
            #check size of file, if larger than 200, not script wrapper file
            local file_sz=0
            file_sz=$(( $(wc -c < "${idx}") ))
            _debug_log "$0:${FUNCNAME[0]} found file '${idx}', "\
                "size '${file_sz}'"
            if [[ ${file_sz} -gt ${max_file_sz} ]]; then
                # get salt-version
                local s_ver=""
                s_ver=$("${idx}" --local test.version |grep -v 'local:' |xargs)
                _debug_log "$0:${FUNCNAME[0]} found standard salt-minion, "\
                    "Salt version: '${s_ver}'"
                echo "${s_ver}"
                _retn=1
                break
            fi
        fi
    done
    echo ""
    return ${_retn}
}


#
# _check_onedir_minion_install
#
# Check if onedir pre_3006 or post_3005 salt-minion is installed on the OS
#   for example: install salt-minion from rpm or deb package
#
# Results:
#   Echos the following values:
#   0 - No onedir install found and empty string output
#   1 - pre_3006 onedir install found
#   2 - post_3005 onedir install found
#

_check_onedir_minion_install() {

    # checks for following executables:
    # post_3005 - /opt/saltstack/salt/salt-minion
    # pre_3006  - /opt/saltstack/salt/run/run

    local _retn=0
    local pre_3006=1
    local post_3005=2

    _info_log "$0:${FUNCNAME[0]} check if standard onedir-minion installed"

    if [[ -f "${list_of_onedir_locations_check[0]}" ]]; then
        _debug_log "$0:${FUNCNAME[0]} found pre 3006 version of Salt, "\
                    "at location ${list_of_onedir_locations_check[0]}"
        _retn=${pre_3006}
    elif [[ -f "${list_of_onedir_locations_check[1]}" ]]; then
        _debug_log "$0:${FUNCNAME[0]} found post 3005 version of Salt, "\
                    "at location ${list_of_onedir_locations_check[1]}"
        _retn=${post_3005}
    else
        _debug_log "$0:${FUNCNAME[0]} failed to find a onedir installation"
    fi
    echo ${_retn}
}


#
# _find_salt_pid
#
#   finds the pid for the Salt process
#
# Results:
#   Echos ${salt_pid} which could be empty '' if Salt process not found
#

_find_salt_pid() {
    # find the pid for salt-minion if active
    local salt_pid=0
    if [[ ${POST_3005_FLAG} -eq 1 ]]; then
        salt_pid=$(pgrep -f "\/usr\/bin\/salt-minion" | head -n 1)
    else
        salt_pid=$(pgrep -f "${salt_name}\/run\/run minion" | head -n 1 |
            awk -F " " '{print $1}')
    fi
    _debug_log "$0:${FUNCNAME[0]} checking for salt-minion process id, "\
        "found '${salt_pid}'"
    echo "${salt_pid}"
}

#
# _ensure_id_or_fqdn
#
#   Ensures that a valid minion identifier has been specified, and if not a
#   valid Fully Qualified Domain Name exists (not default Unknown.example.org)
#   else generates a minion id to use.
#
# Note: this function should only be run before starting the salt-minion
#       via systemd after it has been installed
#
# Side Effect:
#   Updates salt-minion configuration file with generated identifier
#       if no valid FQDN
#
# Results:
#   salt-minion configuration contains a valid identifier or FQDN to use.
#   Exits with 0
#

_ensure_id_or_fqdn () {
    # ensure minion id or fqdn for salt-minion

    local minion_fqdn=""

    # quick check if id specified
    if grep -q '^id:' < "${salt_minion_conf_file}"; then
        _debug_log "$0:${FUNCNAME[0]} salt-minion identifier found, no "\
            "need to check further"
        return 0
    fi

    _debug_log "$0:${FUNCNAME[0]} ensuring salt-minion identifier or "\
        "FQDN is specified for salt-minion configuration"
    minion_fqdn=$(/usr/bin/salt-call --local grains.get fqdn |
        grep -v 'local:' | xargs)
    if [[ -n "${minion_fqdn}" &&
        "${minion_fqdn}" != "Unknown.example.org" ]]; then
        _debug_log "$0:${FUNCNAME[0]} non-default salt-minion FQDN "\
            "'${minion_fqdn}' is specified for salt-minion configuration"
        return 0
    fi

    # default FQDN, no id is specified, generate one and update conf file
    local minion_genid=""
    minion_genid=$(_generate_minion_id)
    echo "id: ${minion_genid}" >> "${salt_minion_conf_file}"
    _debug_log "$0:${FUNCNAME[0]} no salt-minion identifier found, "\
        "generated identifier '${minion_genid}'"

    return 0
}


#
# _create_pre_3006_helper_scripts
#
#   Create helper scripts for salt-call and salt-minion
#
#       Example: _create_pre_3006_helper_scripts
#
# Results:
#   Exits with 0 or error code
#

_create_pre_3006_helper_scripts() {

    for idx in ${salt_wrapper_file_list}
    do
        local abs_filepath=""
        abs_filepath="/usr/bin/salt-${idx}"

        _debug_log "$0:${FUNCNAME[0]} creating helper file 'salt-${idx}' "\
            "in directory /usr/bin"

        echo "#!/usr/bin/env bash

# Copyright (c) 2021 VMware, Inc. All rights reserved.
" > "${abs_filepath}" || {
            _error_log "$0:${FUNCNAME[0]} failed to create helper file "\
                "'salt-${idx}' in directory /usr/bin, retcode '$?'";
        }
        {
            echo -n "exec /opt/saltstack/salt/run/run ${idx}";
            echo -n "\"$";
            echo -n "{";
            echo -n "@";
            echo -n ":";
            echo -n "1}";
            echo -n "\"";
        } >> "${abs_filepath}" || {
            _error_log "$0:${FUNCNAME[0]} failed to finish creating helper "\
                "file 'salt-${idx}' in directory /usr/bin, retcode '$?'";
        }
        echo  "" >> "${abs_filepath}"

        # ensure executable
        chmod 755 "${abs_filepath}" || {
            _error_log "$0:${FUNCNAME[0]} failed to make helper file "\
                "'salt-${idx}' executable in directory /usr/bin, retcode '$?'";
        }
    done

}


#
# _status_fn
#
#   discover and return the current status
#
#       0 => installed
#       1 => installing
#       2 => notInstalled
#       3 => installFailed
#       4 => removing
#       5 => removeFailed
#       6 => externalInstall
#       126 => scriptFailed
#
# Side Effects:
#   CURRENT_STATUS updated
#
# Results:
#   Exits numerical status
#

_status_fn() {
    # return status
    local _retn_status=${STATUS_CODES_ARY[notInstalled]}
    local install_onedir_chk=0
    local found_salt_ver=""

    _info_log "$0:${FUNCNAME[0]} checking status for script"

    _check_multiple_script_running

    found_salt_ver=$(_check_classic_minion_install)
    if [[ -n "${found_salt_ver}" ]]; then
        _debug_log "$0:${FUNCNAME[0]}" \
            "existing Standard Salt Installation detected, "\
            "Salt version: '${found_salt_ver}'"
            CURRENT_STATUS=${STATUS_CODES_ARY[externalInstall]}
            _retn_status=${STATUS_CODES_ARY[externalInstall]}
    else
        _debug_log "$0:${FUNCNAME[0]} no standardized install found"

        install_onedir_chk=$(_check_onedir_minion_install)
        if [[ ${install_onedir_chk} -eq 2 ]]; then
            POST_3005_FLAG=1    # ensure note 3006 and above
        fi

        svpid=$(_find_salt_pid)
        if [[ ${install_onedir_chk} -eq 0 && -z ${svpid} ]]; then
            # check not installed and no process id
            CURRENT_STATUS=${STATUS_CODES_ARY[notInstalled]}
            _retn_status=${STATUS_CODES_ARY[notInstalled]}
        elif [[ ${install_onedir_chk} -ne 0 ]]; then
            # check installed
            CURRENT_STATUS=${STATUS_CODES_ARY[installed]}
            _retn_status=${STATUS_CODES_ARY[installed]}
            # normal case but double-check
            svpid=$(_find_salt_pid)
            if [[ -z ${svpid} ]]; then
                # Note: someone could have stopped the salt-minion,
                # so installed but not running,
                # status codes don't allow for that case
                CURRENT_STATUS=${STATUS_CODES_ARY[installFailed]}
                _retn_status=${STATUS_CODES_ARY[installFailed]}
            fi
        elif [[ -z ${svpid} ]]; then
            # check no process id and main directory still left, =>removeFailed
            if [[ ${install_onedir_chk} -ne 0 ]]; then
                CURRENT_STATUS=${STATUS_CODES_ARY[removeFailed]}
                _retn_status=${STATUS_CODES_ARY[removeFailed]}
            fi
        fi
    fi

    return ${_retn_status}
}


#
# _deps_chk_fn
#
#   Check dependencies for using salt-minion
#
# Side Effects:
# Results:
#   Exits with 0 or error code
#
_deps_chk_fn() {
    # return dependency check
    local error_missing_deps=""

    _info_log "$0:${FUNCNAME[0]} checking script dependencies"
    for idx in ${salt_dep_file_list}
    do
        command -v "${idx}" 1>/dev/null || {
            if [[ -z "${error_missing_deps}" ]]; then
                error_missing_deps="${idx}"
            else
                error_missing_deps="${error_missing_deps} ${idx}"
            fi
        }
    done
    if [[ -n "${error_missing_deps}" ]]; then
        _error_log "$0:${FUNCNAME[0]} failed to find required "\
            "dependencies '${error_missing_deps}'";
    fi
    return 0
}

#
# _find_system_lib_path
#
# find with systemd library path to use
#
# Result:
#   echos the systemd library path
#   will error if no systemd library path can be determined
#
# Note:
#   /lib/systemd/system
#       System units installed by the distribution package manage
#   /usr/lib/systemd/system
#       System units installed by the Administrator
#   /usr/local/lib/systemd/system
#       System units installed by the Administrator (possible on some OS)
#
# Will use /usr/lib/systemd/system available, since this is generally
# the default used on modern Linux OS by salt-minion, some earlier OS's
# (Debian 9, Ubuntu 18.04) use /lib/systemd/system
#
_find_system_lib_path () {

    local path_found=""
    _info_log "$0:${FUNCNAME[0]} finding systemd library path to use"
    if [[ -d "/usr/lib/systemd/system" ]]; then
        path_found="/usr/lib/systemd/system"
    elif [[ -d "/lib/systemd/system" ]]; then
        path_found="/lib/systemd/system"
    elif [[ -d "/usr/local/lib/systemd/system" ]]; then
        path_found="/usr/local/lib/systemd/system"
    else
        _error_log "$0:${FUNCNAME[0]} unable to determine systemd "\
        "library path to use"
    fi
    _debug_log "$0:${FUNCNAME[0]} found library path to use ${path_found}"
    echo "${path_found}"
}


#
#  _install_fn
#
#   Executes scripts to install Salt from Salt repository
#       and start the salt-minion using systemd
#
# Results:
#   Exits with 0 or error code
#

_install_fn () {
    # execute install of Salt minion
    local _retn=0
    local existing_chk=""
    local found_salt_ver=""
    local install_onedir_chk=0

    _info_log "$0:${FUNCNAME[0]} processing script install"

    _check_multiple_script_running

    found_salt_ver=$(_check_classic_minion_install)
    if [[ -n "${found_salt_ver}" ]]; then
        _warning_log "$0:${FUNCNAME[0]} failed to install, "\
            "existing Standard Salt Installation detected, "\
            "Salt version: '${found_salt_ver}'"
        CURRENT_STATUS=${STATUS_CODES_ARY[externalInstall]}
        exit ${STATUS_CODES_ARY[externalInstall]}
    else
        _debug_log "$0:${FUNCNAME[0]} no standardized install found"
    fi

    # check if salt-minion or salt-master (salt-cloud etc req master)
    # and log warning that they will be overwritten
    existing_chk=$(pgrep -l "salt-minion|salt-master" | cut -d ' ' -f 2 | uniq)
    if [[ -n  "${existing_chk}" ]]; then
        for idx in ${existing_chk}
        do
            local salt_fn=""
            salt_fn="$(basename "${idx}")"
            _warning_log "$0:${FUNCNAME[0]} existing Salt functionality "\
                "${salt_fn} shall be stopped and replaced when new "\
                "salt-minion is installed"
        done
    fi

    # fetch salt-minion from repository
    _fetch_salt_minion || {
        _error_log "$0:${FUNCNAME[0]} failed to fetch salt-minion "\
            "from repository , retcode '$?'";
    }

    # get configuration for salt-minion
    _fetch_vmtools_salt_minion_conf "$@" || {
        _error_log "$0:${FUNCNAME[0]} failed, read configuration for "\
            "salt-minion, retcode '$?'";
    }

    if [[ ${_retn} -eq 0 && -f "${onedir_pre_3006_location}" ]]; then
        # create helper scripts for /usr/bin to ensure they are present
        # before attempting to use them in _ensure_id_or_fqdn
        # this is for earlier than 3006 versions of Salt onedir
        _debug_log "$0:${FUNCNAME[0]} creating helper files salt-call "\
            "and salt-minion in directory /usr/bin"
        _create_pre_3006_helper_scripts || {
            _error_log "$0:${FUNCNAME[0]} failed to create helper files "\
                "salt-call or salt-minion in directory /usr/bin, retcode '$?'";
        }
    elif [[ ${_retn} -eq 0 && -f "${onedir_post_3005_location}" ]]; then
        # create symbolic links for /usr/bin to ensure they are present
        _debug_log "$0:${FUNCNAME[0]} creating symbolic links for salt-call "\
            "and salt-minion in directory /usr/bin"
        ln -s -f "${salt_dir}/salt-minion" "/usr/bin/salt-minion" || {
            _error_log "$0:${FUNCNAME[0]} failed to create symbolic link "\
                "for salt-minion in directory /usr/bin, retcode '$?'";
        }
        ln -s -f "${salt_dir}/salt-call" "/usr/bin/salt-call" || {
            _error_log "$0:${FUNCNAME[0]} failed to create symbolic link "\
                "for salt-call in directory /usr/bin, retcode '$?'";
        }
    else
        _error_log "$0:${FUNCNAME[0]} problems creating helper files "\
            "or symbolic links for salt-call or salt-minion in "\
            "directory /usr/bin, should not have reached this code point";
    fi

    # ensure minion id or fqdn for salt-minion
    _ensure_id_or_fqdn
    install_onedir_chk=$(_check_onedir_minion_install)

    if [[ ${_retn} -eq 0 && ${install_onedir_chk} -ne 0 ]]; then
        if [[ -n  "${existing_chk}" ]]; then
            # be nice and stop any current Salt functionality found
            for idx in ${existing_chk}
            do
                local salt_fn=""
                salt_fn="$(basename "${idx}")"
                _warning_log "$0:${FUNCNAME[0]} stopping Salt functionality "\
                    "${salt_fn} it's replaced with new installed salt-minion"
                systemctl stop "${salt_fn}" || {
                    _warning_log "$0:${FUNCNAME[0]} stopping existing Salt "\
                        "functionality ${salt_fn} encountered difficulties "\
                        "using systemctl, it will be over-written with the "\
                        "new installed salt-minion regardless, retcode '$?'";
                }
            done
        fi

        # install salt-minion systemd service script
        # first find with systemd library path to use
        local systemd_lib_path=""
        systemd_lib_path=$(_find_system_lib_path)
        local name_service="salt-minion.service"
        _debug_log "$0:${FUNCNAME[0]} copying systemd service script "\
            "${name_service} to directory ${systemd_lib_path}"
        echo "${salt_minion_service_wrapper}" \
            > "${systemd_lib_path}/${name_service}" || {
            _error_log "$0:${FUNCNAME[0]} failed to copy systemd service "\
                "file ${name_service} to directory "\
                "${systemd_lib_path}, retcode '$?'";
        }

        cd "${CURRDIR}" || return $?

        # start the salt-minion using systemd
        systemctl daemon-reload || {
            _error_log "$0:${FUNCNAME[0]} reloading the systemd daemon "\
                "failed , retcode '$?'";
        }
        _debug_log "$0:${FUNCNAME[0]} successfully executed systemctl "\
            "daemon-reload"
        systemctl restart "${name_service}" || {
            _error_log "$0:${FUNCNAME[0]} starting the salt-minion using "\
                "systemctl failed , retcode '$?'";
        }
        _debug_log "$0:${FUNCNAME[0]} successfully executed systemctl "\
            "restart '${name_service}'"
        systemctl enable "${name_service}" || {
            _error_log "$0:${FUNCNAME[0]} enabling the salt-minion using "\
                "systemctl failed , retcode '$?'";
        }
        _debug_log "$0:${FUNCNAME[0]} successfully executed systemctl "\
            "enable '${name_service}'"
    fi
    return ${_retn}
}


#
#  _source_fn
#
#   Set the location to retrieve the Salt Minion from
#       default is to use the Salt Project repository
#
#   Set the version of Salt Minion wanted to install
#       default 'latest'
#
#   Note: handle all protocols (http, https, ftp, file, unc, etc)
#           for example:
#               http://my_web_server.com/my_salt_onedir
#               https://my_web_server.com/my_salt_onedir
#               ftp://my_ftp_server.com/my_salt_onedir
#               file://mytopdir/mymiddledir/my_salt_onedir
#               ///mytopdir/mymiddledir/my_salt_onedir
#
#           If a specific version of the Salt Minion is specified
#           then it will be appended to the specified source location
#           otherwise a default of 'latest' is applied.
#
# Results:
#   Exits with 0 or error code
#

_source_fn () {
    local _retn=0
    local salt_source=""

    if [[ $# -ne 1 ]]; then
        _error_log "$0:${FUNCNAME[0]} error expected one parameter "\
            "specifying the source for location of onedir files"
    fi

    _info_log "$0:${FUNCNAME[0]} processing script source for location "\
        "of onedir files"

    salt_source=$(echo "$1" | cut -d ' ' -f 1)
    _debug_log "$0:${FUNCNAME[0]} input Salt source is '${salt_source}'"

    if [[ -n "${salt_source}" ]]; then
        base_url=${salt_source}
    fi
    _debug_log "$0:${FUNCNAME[0]} input Salt source for salt-minion to "\
            "install from is '${base_url}'"

    return ${_retn}
}


#
# _generate_minion_id
#
#   Searches salt-minion configuration file for current id, and disables it
#   and generates a new id based on the existing id found,
#   or an older commented out id, and provides it with a randomized 5 digit
#   post-pended to it, for example: myminion_12345
#
#   If no previous id found, a generated minion_<random number> is output
#
# Side Effects:
#   Disables any id found in minion configuration file
#
# Result:
#   Outputs randomized minion id for use in a minion configuration file
#   Exits with 0 or error code
#

_generate_minion_id () {

    local salt_id_flag=0
    local minion_id=""
    local cfg_value=""
    local ifield=""
    local tfields=""

    _debug_log "$0:${FUNCNAME[0]} generating a salt-minion identifier"

    # always comment out what was there
    sed -i 's/^id:/# id:/g' "${salt_minion_conf_file}"

    while IFS= read -r line
    do
        line_value=$(_trim "${line}")
        if [[ -n "${line_value}" ]]; then
            if echo "${line_value}" | grep -q '^# id:' ; then
                # get value and write out value_<random>
                cfg_value=$(echo "${line_value}" | cut -d ' ' -f 3)
                if [[ -n "${cfg_value}" ]]; then
                    salt_id_flag=1
                    minion_id=$(_randomize_minion_id "${cfg_value}")
                    _debug_log "$0:${FUNCNAME[0]} found previously used id "\
                        "field, randomizing it"
                fi
            elif echo "${line_value}" | grep -q -w 'id:' ; then
                # might have commented out id, get value and
                # write out value_<random>
                tfields=$(echo "${line_value}"|awk -F ':' '{print $2}'|xargs)
                ifield=$(echo "${tfields}" | cut -d ' ' -f 1)
                if [[ -n ${ifield} ]]; then
                    minion_id=$(_randomize_minion_id "${ifield}")
                    salt_id_flag=1
                    _debug_log "$0:${FUNCNAME[0]} found previously used "\
                        "id field, randomizing it"
                fi
            else
                _debug_log "$0:${FUNCNAME[0]} skipping line '${line}'"
            fi
        fi
    done < "${salt_minion_conf_file}"

    if [[ ${salt_id_flag} -eq 0 ]]; then
        # no id field found, write minion_<random?
        _debug_log "$0:${FUNCNAME[0]} no previous id field found, "\
            "generating new identifier"
        minion_id=$(_randomize_minion_id)
    fi
    _debug_log "$0:${FUNCNAME[0]} generated a salt-minion "\
        "identifier '${minion_id}'"
    echo "${minion_id}"
    return 0
}


#
# _clear_id_key_fn
#
#   Executes scripts to clear the minion identifier and keys and
#   re-generates new identifier, allows for a VM containing a salt-minion,
#   to be cloned and not have conflicting id and keys
#   salt-minion is stopped, id and keys cleared, and restarted
#   if it was previously running
#
# Input:
#   Optional specified input ID to be used, default generate randomized value
#
# Note:
#   Normally a salt-minion if no id is specified will rely on
#   it's Fully Qualified Domain Name but with VM Cloning, there is no surety
#   that the FQDN will have been altered, and duplicates can occur.
#   Also if there is no FQDN, then default 'Unknown.example.org' is used,
#   again with the issue of duplicates for multiple salt-minions
#   with no FQDN specified
#
# Side Effects:
#   New minion identifier in configuration file and keys for the salt-minion
#
# Results:
#   Exits with 0 or error code
#

_clear_id_key_fn () {
    # execute clearing of Salt minion id and keys
    local _retn=0
    local salt_minion_pre_active_flag=0
    local salt_id_flag=0
    local minion_id=""
    local minion_ip_id=""
    local install_onedir_chk=0

    _info_log "$0:${FUNCNAME[0]} processing clearing of salt-minion "\
        "identifier and its keys"

    _check_multiple_script_running

    install_onedir_chk=$(_check_onedir_minion_install)
    if [[ ${install_onedir_chk} -eq 0 ]]; then
        _debug_log "$0:${FUNCNAME[0]} salt-minion is not installed, "\
            "nothing to do"
        return ${_retn}
    fi

    # get any minion identifier in case specified
    minion_ip_id=$(echo "$1" | cut -d ' ' -f 1)
    svpid=$(_find_salt_pid)
    if [[ -n ${svpid} ]]; then
        # stop the active salt-minion using systemd
        # and give it a little time to stop
        systemctl stop salt-minion || {
            _error_log "$0:${FUNCNAME[0]} failed to stop salt-minion "\
                "using systemctl, retcode '$?'";
        }
        _debug_log "$0:${FUNCNAME[0]} successfully executed systemctl "\
            "stop salt-minion"
        salt_minion_pre_active_flag=1
    fi

    rm -fR "${salt_conf_dir}/minion_id"
    rm -fR "${salt_conf_dir}/pki/${salt_minion_conf_name}"
    # always comment out what was there
    sed -i 's/^id/# id/g' "${salt_minion_conf_file}"
    _debug_log "$0:${FUNCNAME[0]} removed '${salt_conf_dir}/minion_id' "\
        "and '${salt_conf_dir}/pki/${salt_minion_conf_name}', and "\
        "commented out id in '${salt_minion_conf_file}'"

    if [[ -z "${minion_ip_id}" ]] ;then
        minion_id=$(_generate_minion_id)
    else
        minion_id="${minion_ip_id}"
    fi

    # add new minion id to bottom of minion configuration file
    echo "id: ${minion_id}" >> "${salt_minion_conf_file}"
    _debug_log "$0:${FUNCNAME[0]} updated salt-minion identifier "\
        "'${minion_id}' in configuration file '${salt_minion_conf_file}'"

    if [[ ${salt_minion_pre_active_flag} -eq 1 ]]; then
        # restart the stopped salt-minion using systemd
        systemctl restart salt-minion || {
            _error_log "$0:${FUNCNAME[0]} failed to restart salt-minion "\
                "using systemctl, retcode '$?'";
        }

        _debug_log "$0:${FUNCNAME[0]} successfully executed systemctl "\
            "restart salt-minion"
    fi

    return ${_retn}
}


#
# _remove_installed_files_dirs
#
#   Removes all Salt files and directories that may be used
#
# Results:
#   Exits with 0 or error code
#

_remove_installed_files_dirs() {
    _debug_log "$0:${FUNCNAME[0]} removing directories and files "\
        "in '${list_file_dirs_to_remove}'"
    for idx in ${list_file_dirs_to_remove}
    do
        rm -fR "${idx}" || {
            _error_log "$0:${FUNCNAME[0]} failed to remove file or "\
                "directory '${idx}' , retcode '$?'";
        }
    done
    return 0
}


#
#  _uninstall_fn
#
#   Executes scripts to uninstall Salt from system
#       stopping the salt-minion using systemd
#
# Side Effects:
#   CURRENT_STATUS updated
#
# Results:
#   Exits with 0 or error code
#

_uninstall_fn () {
    # remove Salt minion
    local _retn=0
    local found_salt_ver=""
    local install_onedir_chk=0

    _info_log "$0:${FUNCNAME[0]} processing script remove"

    _check_multiple_script_running

    found_salt_ver=$(_check_classic_minion_install)
    if [[ -n "${found_salt_ver}" ]]; then
        _warning_log "$0:${FUNCNAME[0]} failed to install, "\
            "existing Standard Salt Installation detected, "\
            "Salt version: '${found_salt_ver}'"
        CURRENT_STATUS=${STATUS_CODES_ARY[externalInstall]}
        exit ${STATUS_CODES_ARY[externalInstall]}
    else
        _debug_log "$0:${FUNCNAME[0]} no standardized install found"
    fi

    install_onedir_chk=$(_check_onedir_minion_install)
    if [[ ${install_onedir_chk} -eq 0 ]]; then
        CURRENT_STATUS=${STATUS_CODES_ARY[notInstalled]}

        # assume rest is gone
        # TBD enhancement, could loop thru and check all of files to remove
        # and if salt_pid empty but we error out if issues when uninstalling,
        # so safe for now.
        _retn=0
    else
        CURRENT_STATUS=${STATUS_CODES_ARY[removing]}
        # remove salt-minion from systemd
        # and give it a little time to stop
        systemctl stop salt-minion || {
            _error_log "$0:${FUNCNAME[0]} failed to stop salt-minion "\
                "using systemctl, retcode '$?'";
        }
        _debug_log "$0:${FUNCNAME[0]} successfully executed systemctl "\
            "stop salt-minion"
        systemctl disable salt-minion || {
            _error_log "$0:${FUNCNAME[0]} disabling the salt-minion "\
                "using systemctl failed , retcode '$?'";
        }
        _debug_log "$0:${FUNCNAME[0]} successfully executed systemctl "\
            "disable salt-minion"

        _debug_log "$0:${FUNCNAME[0]} removing systemd directories and files "\
            "in '${list_files_systemd_to_remove}'"
        for idx in ${list_files_systemd_to_remove}
        do
            rm -fR "${idx}" || {
                _error_log "$0:${FUNCNAME[0]} failed to remove file or "\
                    "directory '${idx}' , retcode '$?'";
            }
        done

        systemctl daemon-reload || {
            _error_log "$0:${FUNCNAME[0]} reloading the systemd daemon "\
                "failed , retcode '$?'";
        }
        _debug_log "$0:${FUNCNAME[0]} successfully executed systemctl "\
            "daemon-reload"
        systemctl reset-failed || {
            _error_log "$0:${FUNCNAME[0]} reloading the systemd daemon "\
                "failed , retcode '$?'";
        }
        _debug_log "$0:${FUNCNAME[0]} successfully executed systemctl "\
            "reset-failed"

        if [[ ${_retn} -eq 0 ]]; then
            svpid=$(_find_salt_pid)
            if [[ -n ${svpid} ]]; then
                _debug_log "$0:${FUNCNAME[0]} found salt-minion process "\
                    "id '${salt_pid}', systemctl stop should have "\
                    "eliminated it, killing it now"
                kill "${svpid}"
                ## given it a little time
                sleep 5
            fi
            svpid=$(_find_salt_pid)
            if [[ -n ${svpid} ]]; then
                CURRENT_STATUS=${STATUS_CODES_ARY[removeFailed]}
                _error_log "$0:${FUNCNAME[0]} failed to kill the "\
                    "salt-minion, pid '${svpid}' during uninstall"
            else
                _remove_installed_files_dirs || {
                    _error_log "$0:${FUNCNAME[0]} failed to remove all "\
                        "installed salt-minion files and directories, "\
                        "retcode '$?'";
                }
                CURRENT_STATUS=${STATUS_CODES_ARY[notInstalled]}
            fi
        fi
    fi

    _info_log "$0:${FUNCNAME[0]} successfully removed salt-minion and "\
        "associated files and directories"
    return ${_retn}
}


#
#  _clean_up_log_files
#
#   Limits number of log files by removing oldest log files which exceed
#   limit LOG_FILE_NUMBER
#
# Results:
#   Exits with 0 or error code
#
_clean_up_log_files() {

    _info_log "$0:${FUNCNAME[0]} removing and limiting log files"
    for idx in ${allowed_log_file_action_names}
    do
        local count_f=0
        local found_f=""
        local -a found_f_ary
        found_f=$(ls -t "${log_dir}/vmware-${SCRIPTNAME}-${idx}"* 2>/dev/null)
        count_f=$(echo "${found_f}" | wc | awk -F" " '{print $2}')
        mapfile -t found_f_ary <<< "${found_f}"

        if [[ ${count_f} -gt ${LOG_FILE_NUMBER} ]]; then
            # allow for org-0
            for ((i=count_f-1; i>=LOG_FILE_NUMBER; i--)); do
                _debug_log "$0:${FUNCNAME[0]} removing log file "\
                    "'${found_f_ary[i]}', for count '${i}', "\
                    "limit '${LOG_FILE_NUMBER}'"
                rm -f "${found_f_ary[i]}" || {
                    _error_log "$0:${FUNCNAME[0]} failed to remove file "\
                    "'${found_f_ary[i]}', for count '${i}', "\
                    "limit '${LOG_FILE_NUMBER}'"
                }
            done
        else
            _debug_log "$0:${FUNCNAME[0]} found '${count_f}' "\
                "log files starting with "\
                "${log_dir}/vmware-${SCRIPTNAME}-${idx}-, "\
                "limit '${LOG_FILE_NUMBER}'"
        fi
    done
    return 0
}

################################### MAIN ####################################

# static definitions

CURRDIR=$(pwd)

# get machine architecture once
MACHINE_ARCH=$(uname -m)

# setup work-area
DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
# the temp working directory used, within $DIR
WORK_DIR=$(mktemp -d -p "$DIR")

# check if temp working dir was created
if [[ ! "${WORK_DIR}" || ! -d "${WORK_DIR}" ]]; then
  echo "Could not create temp dir"
  exit 1
fi

# default status is notInstalled
CURRENT_STATUS=${STATUS_CODES_ARY[notInstalled]}
export CURRENT_STATUS

## build date-time tag used for logging UTC YYYYMMDDhhmmss
## YearMontDayHourMinuteSecondMicrosecond aka jid
logdate=$(date -u +%Y%m%d%H%M%S)

# set logging information
LOG_FILE_NUMBER=5
SCRIPTNAME=$(basename "$0")
mkdir -p "${log_dir}"

# set to action e.g. 'remove', 'install'
# default is for any logging not associated with a specific action
# for example: debug logging and --version
LOG_ACTION="default"

CLI_ACTION=0

while true; do
    if [[ -z "$1" ]]; then break; fi
    case "$1" in
        -c | --clear )
            CLEAR_ID_KEYS_FLAG=1;
            shift;
            CLEAR_ID_KEYS_PARAMS=$*;
            ;;
        -d | --depend )
            DEPS_CHK=1;
            shift;
            ;;
        -h | --help )
            USAGE_HELP=1;
            shift;
            ;;
        -i | --install )
            INSTALL_FLAG=1;
            shift;
            INSTALL_PARAMS="$*";
            ;;
        -j | --source )
            SOURCE_FLAG=1;
            shift;
            SOURCE_PARAMS="$*";
            ;;
        -l | --loglevel )
            LOG_LEVEL_FLAG=1;
            shift;
            LOG_LEVEL_PARAMS="$*";
            ;;
        -m | --minionversion )
            MINION_VERSION_FLAG=1;
            shift;
            MINION_VERSION_PARAMS="$*";
            ;;
        -r | --remove )
            UNINSTALL_FLAG=1;
            shift;
            ;;
        -s | --status )
            STATUS_CHK=1;
            shift;
            ;;
        -v | --version )
            VERSION_FLAG=1;
            shift;
            ;;
        -- )
            shift;
            break;
            ;;
        * )
            shift;
            ;;
    esac
done

## check if want help, display usage and exit
if [[ ${USAGE_HELP} -eq 1 ]]; then
  _usage
  exit 0
fi


##  MAIN BODY OF SCRIPT

retn=0

if [[ ${LOG_LEVEL_FLAG} -eq 1 ]]; then
    # ensure logging level changes are processed before any actions
    CLI_ACTION=1
    _set_log_level "${LOG_LEVEL_PARAMS}"
    retn=$?
fi
if [[ ${STATUS_CHK} -eq 1 ]]; then
    CLI_ACTION=1
    LOG_ACTION="status"
    _status_fn
    retn=$?
fi
if [[ ${DEPS_CHK} -eq 1 ]]; then
    CLI_ACTION=1
    LOG_ACTION="depend"
    _deps_chk_fn
    retn=$?
fi
if [[ ${SOURCE_FLAG} -eq 1 ]]; then
    CLI_ACTION=1
    LOG_ACTION="install"
    # ensure this is processed before install
    _source_fn "${SOURCE_PARAMS}"
    retn=$?
fi
if [[ ${MINION_VERSION_FLAG} -eq 1 ]]; then
    CLI_ACTION=1
    # ensure this is processed before install
    _set_install_minion_version_fn "${MINION_VERSION_PARAMS}"
    retn=$?
fi
if [[ ${INSTALL_FLAG} -eq 1 ]]; then
    CLI_ACTION=1
    LOG_ACTION="install"
    _install_fn "${INSTALL_PARAMS}"
    retn=$?
fi
if [[ ${CLEAR_ID_KEYS_FLAG} -eq 1 ]]; then
    CLI_ACTION=1
    LOG_ACTION="clear"
    _clear_id_key_fn "${CLEAR_ID_KEYS_PARAMS}"
    retn=$?
fi
if [[ ${UNINSTALL_FLAG} -eq 1 ]]; then
    CLI_ACTION=1
    LOG_ACTION="remove"
    _uninstall_fn
    retn=$?
fi
if [[ ${VERSION_FLAG} -eq 1 ]]; then
    CLI_ACTION=1
    echo "${SCRIPT_VERSION}"
    retn=0
fi

if [[ ${CLI_ACTION} -eq 0 ]]; then
    # check if guest variables have an action since none from CLI
    # since none presented on the command line
    gvar_action=$(vmtoolsd --cmd "info-get ${guestvars_salt_desiredstate}" \
        2>/dev/null) || {
            _warning_log "$0 unable to retrieve any action arguments from "\
                "guest variables ${guestvars_salt_desiredstate}, retcode '$?'";
    }

    if [[ -n "${gvar_action}" ]]; then
        case "${gvar_action}" in
            depend)
                LOG_ACTION="depend"
                _deps_chk_fn
                retn=$?
                ;;
            present)
                LOG_ACTION="install"
                _install_fn
                retn=$?
                ;;
            absent)
                LOG_ACTION="remove"
                _uninstall_fn
                retn=$?
                ;;
            status)
                LOG_ACTION="status"
                _status_fn
                retn=$?
                ;;
            *)
                ;;
        esac
    fi
fi

_clean_up_log_files

exit ${retn}
