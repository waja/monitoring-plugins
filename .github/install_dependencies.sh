#!/bin/sh -e
# See https://raw.githubusercontent.com/Exiv2/exiv2/main/ci/install_dependencies.sh

# workaround for really bare-bones Archlinux containers:
if [ -x "$(command -v pacman)" ]; then
    pacman --noconfirm -Sy
    pacman --noconfirm -S grep gawk sed
fi

distro_id=$(grep '^ID=' /etc/os-release|awk -F = '{print $2}'|sed 's/\"//g')

case "$distro_id" in
    'fedora')
        dnf -y --refresh install gcc-c++ make
        ;;

    'debian'|'ubuntu')
        export DEBIAN_FRONTEND=noninteractive
        apt-get update
        apt-get -y install software-properties-common
        if [ "$distro_id" = "debian" ]; then
          apt-add-repository non-free
          apt-get update
        fi
        apt-get -y install perl autotools-dev libdbi-dev libldap2-dev libpq-dev libradcli-dev libnet-snmp-perl procps
        apt-get -y install libdbi0-dev libdbd-sqlite3 libssl-dev dnsutils snmp-mibs-downloader libsnmp-perl snmpd
        apt-get -y install fping snmp netcat-openbsd smbclient vsftpd apache2 ssl-cert postfix libhttp-daemon-ssl-perl
        apt-get -y install libdbd-sybase-perl libnet-dns-perl
        apt-get -y install slapd ldap-utils
        apt-get -y install gcc make autoconf automake gettext
        apt-get -y install faketime
        apt-get -y install libmonitoring-plugin-perl
        apt-get -y install libcurl4-openssl-dev
        apt-get -y install liburiparser-dev
        apt-get -y install squid
        apt-get -y install openssh-server
        apt-get -y install mariadb-server mariadb-client libmariadb-dev
        apt-get -y install cron iputils-ping
        apt-get -y install iproute2
        ;;

    'arch')
        pacman --noconfirm -Syu
        pacman --noconfirm -S gcc make
        ;;

    'alpine')
        apk update
        apk add gcc g++ make
        ;;

    'centos'|'rhel')
        yum -y update libarchive # workaround for https://bugs.centos.org/view.php?id=18212
        yum -y install epel-release
        yum clean all
        yum -y install gcc-c++ make
        ;;

    'opensuse-tumbleweed')
        zypper --non-interactive refresh
        zypper --non-interactive install gcc-c++ make
        ;;
    *)
        echo "Sorry, no predefined dependencies for your distribution $distro_id exist yet"
        exit 1
        ;;
esac
