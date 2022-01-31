#!/bin/sh -e
# See https://raw.githubusercontent.com/Exiv2/exiv2/main/ci/install_dependencies.sh
# and https://github.com/netdata/netdata/blob/c9a3c837f4028edc5a6b9bd8ef6d7cb523f96115/packaging/installer/install-required-packages.sh

## workaround for really bare-bones Archlinux containers:
#if [ -x "$(command -v pacman)" ]; then
#    pacman --noconfirm -Sy
#    pacman --noconfirm -S grep gawk sed
#fi
#
#os_release_file=
#if [ -s "/etc/os-release" ]; then
#  os_release_file="/etc/os-release"
#elif [ -s "/usr/lib/os-release" ]; then
#  os_release_file="/usr/lib/os-release"
#else
#  echo >&2 "Cannot find an os-release file ..."
#  return 1
#fi
#export distro_id=$(grep '^ID=' $os_release_file|awk -F = '{print $2}'|sed 's/\"//g')

. .github/os_detect.sh

#rpm_packages="autoconf automake iputils libdbi-devel libsmbclient-devel libtool mysql-devel net-snmp-devel openldap2-devel openssh openssl-devel net-snmp-perl net-snmp-utils postfix postgresql-devel procps samba-client freeradius-client-devel rpcbind krb5-devel heimdal-devel"
case "$distro_id" in
    'fedora')
        #dnf -y --refresh install $rpm_packages
        dnf -y --refresh install perl which libdbi-devel openldap-devel libpq-devel radcli-compat-devel freeradius-devel procps libdbi-devel libdbi-dbd-sqlite openssl-devel bind-utils net-snmp-perl net-snmp fping net-snmp-utils netcat samba-client vsftpd httpd mod_ssl postfix perl-HTTP-Daemon-SSL perl-Net-DNS openldap-clients openldap-servers gcc make autoconf automake gettext libfaketime perl-Monitoring-Plugin uriparser-devel squid openssh-server mariadb mariadb-server mariadb-devel iputils iproute perl-Net-SNMP openssh-clients libcurl-devel
	dnf -y install curl
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
        apt-get -y install iputils-ping
        apt-get -y install iproute2
        apt-get -y install curl
        ;;

    'arch')
        pacman --noconfirm -Syu
        pacman --noconfirm -S gcc make
        ;;

    'alpine')
        apk update
        apk add gcc g++ make
        ;;

    'centos'|'rhel'|'almalinux'|'rocky')
	dnf install -y 'dnf-command(config-manager)'
	if [ "$distro_id" == "almalinux" ]; then
		curl https://git.rockylinux.org/original/rpms/rocky-release/-/raw/r8/SOURCES/Rocky-Plus.repo -so /etc/yum.repos.d/Rocky-Plus.repo && curl https://dl.rockylinux.org/pub/rocky/RPM-GPG-KEY-rockyofficial -so /etc/pki/rpm-gpg/RPM-GPG-KEY-rockyofficial
	fi
	dnf config-manager --enable plus powertools
	#if [ "$distro_id" == "rocky" -o "$distro_id" == "almalinux" ]; then
	#	dnf config-manager --enable plus
	#fi
	yum repolist all
        yum -y update libarchive # workaround for https://bugs.centos.org/view.php?id=18212
        yum -y install epel-release
	#dnf config-manager --enable powertools
        yum clean all
        #yum -y install $rpm_packages
        #yum -y install autoconf automake iputils libdbi-devel libtool mysql-devel net-snmp-devel openssh openssl-devel net-snmp-perl net-snmp-utils postfix postgresql-devel procps samba-client freeradius-client-devel rpcbind krb5-devel heimdal-devel iproute httpd perl-Net-SNMP
        # libsmbclient-devel openldap2-devel
        yum -y install perl which libdbi-devel openldap-devel libpq-devel radcli-compat-devel freeradius-devel procps libdbi-devel libdbi-dbd-sqlite openssl-devel bind-utils net-snmp-perl net-snmp fping net-snmp-utils netcat samba-client vsftpd httpd mod_ssl postfix perl-HTTP-Daemon-SSL perl-Net-DNS openldap-clients openldap-servers gcc make autoconf automake gettext libfaketime perl-Monitoring-Plugin uriparser-devel squid openssh-server mariadb mariadb-server mariadb-devel iputils iproute perl-Net-SNMP openssh-clients libcurl-devel
	yum -y install curl
        ;;

    'opensuse-tumbleweed'|'opensuse-leap')
        zypper --non-interactive refresh
        #zypper --non-interactive install $rpm_packages
        #zypper --non-interactive install autoconf automake iputils libdbi-devel libsmbclient-devel libtool net-snmp-devel openldap2-devel openssh postfix postgresql-devel procps samba-client freeradius-client-devel rpcbind krb5-devel iproute perl-Net-SNMP perl-Digest-SHA1 httpd
        # net-snmp-perl net-snmp-utils openssl-devel mysql-devel heimdal-devel
        zypper --non-interactive install perl which libdbi-devel radcli-compat-devel procps libdbi-devel bind-utils net-snmp fping samba-client vsftpd postfix perl-Net-DNS gcc make autoconf automake libfaketime perl-Monitoring-Plugin uriparser-devel squid openssh-server mariadb iputils perl-Net-SNMP libmariadb-devel libopenssl-devel perl-HTTPS-Daemon gettext-tools netcat-openbsd apache2 iproute2 openldap2-devel libdbi-drivers-dbd-sqlite3 postgresql-devel openldap2-client openldap2 openssh-clients libcurl-devel
	zypper --non-interactive install curl
        # mariadb-devel net-snmp-perl net-snmp-utils openssl-devel perl-HTTP-Daemon-SSL gettext netcat httpd iproute openldap-devel freeradius-devel libdbi-dbd-sqlite libpq-devel openldap-clients freeradius-client-devel
        ;;
    *)
        echo "Sorry, no predefined dependencies for your distribution $distro_id exist yet"
        exit 1
        ;;
esac
