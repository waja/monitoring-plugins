#!/bin/bash

set -x
set -e

export DEBIAN_FRONTEND=noninteractive

. .github/os_detect.sh
#sed "s/main/non-free contrib/g" /etc/apt/sources.list.d/debian.sources > /etc/apt/sources.list.d/debian-nonfree.sources
#apt-get update
#apt-get -y install software-properties-common
#if [ $(lsb_release -is) = "Debian" ]; then
#  apt-add-repository non-free
#  apt-get update
#fi
#apt-get -y install perl \
#	autotools-dev \
#	libdbi-dev \
#	libldap2-dev \
#	libpq-dev \
#	libradcli-dev \
#	libnet-snmp-perl \
#	procps \
#	libdbi0-dev \
#	libdbd-sqlite3 \
#	libssl-dev \
#	dnsutils \
#	snmp-mibs-downloader \
#	libsnmp-perl \
#	snmpd \
#	fping \
#	snmp \
#	netcat-openbsd \
#	smbclient \
#	vsftpd \
#	apache2 \
#	ssl-cert \
#	postfix \
#	libhttp-daemon-ssl-perl \
#	libdbd-sybase-perl \
#	libnet-dns-perl \
#	slapd \
#	ldap-utils \
#	gcc \
#	make \
#	autoconf \
#	automake \
#	gettext \
#	faketime \
#	libmonitoring-plugin-perl \
#	libcurl4-openssl-dev \
#	liburiparser-dev \
#	squid \
#	openssh-server \
#	mariadb-server \
#	mariadb-client \
#	libmariadb-dev \
#	iputils-ping \
#	iproute2

# remove ipv6 interface from hosts
if [ $(ip addr show | grep "inet6 ::1" | wc -l) -eq "0" ]; then
    sed '/^::1/d' /etc/hosts > /tmp/hosts
    cp -f /tmp/hosts /etc/hosts
fi

ip addr show

cat /etc/hosts

echo $distro_id

# replace cron with sshd in the check_nagios test, so we don't need to install (and start) cron
# which is not available on rhel based distros
sed -i 's/procname = "cron"/procname = "sshd"/' ./plugins/t/check_nagios.t

# apache
[ -x /usr/sbin/a2enmod ] && a2enmod ssl
[ -x /usr/sbin/a2ensite ] && a2ensite default-ssl
[ -x /usr/sbin/a2enflag ] && a2enflag SSL
# create empty index.html
[ -d touch /srv/www/htdocs ] && touch touch /srv/www/htdocs/index.html
[ -d touch /var/www/html ] && touch touch /var/www/html/index.html
# replace snakeoil certs with openssl generated ones as the make-ssl-cert ones
# seems to cause problems with our plugins
if [ -d /etc/apache2/ssl.key/ ]; then
	KEY="/etc/apache2/ssl.key/server.key"
	CERT="/etc/apache2/ssl.crt/server.crt"
	sed -i "s/#SSLCertificateFile \/etc\/apache2\/ssl.crt\/server.crt/SSLCertificateFile \/etc\/apache2\/ssl.crt\/server.crt/" /etc/apache2/ssl-global.conf
	sed -i "s/#SSLCertificateKeyFile \/etc\/apache2\/ssl.key\/server.key/SSLCertificateKeyFile \/etc\/apache2\/ssl.key\/server.key/" /etc/apache2/ssl-global.conf
	ln -s $KEY /etc/apache2/ssl.key/vhost-example.key && ln -s $CERT /etc/apache2/ssl.crt/vhost-example.crt
elif [ -d /etc/ssl/private/ ]; then
	KEY="/etc/ssl/private/ssl-cert-snakeoil.key"
	CERT="/etc/ssl/certs/ssl-cert-snakeoil.pem"
elif [ -d /etc/pki/tls/private/ ]; then
	KEY="/etc/pki/tls/private/localhost.key"
	CERT="/etc/pki/tls/certs/localhost.crt"
fi
rm -f $KEY $CERT
openssl req -nodes -newkey rsa:2048 -x509 -sha256 -days 365 -nodes -keyout $KEY -out $CERT -subj "/C=GB/ST=London/L=London/O=Global Security/OU=IT Department/CN=$(hostname)"
if [ -x "$(command -v apache2)" ]; then
	APACHE_BIN="$(basename $(command -v apache2))"
elif [ -x "$(command -v httpd)" ]; then
	APACHE_BIN="$(basename $(command -v httpd))"
else
	echo "No apache binary found"
	exit 1
fi

[ -x /usr/sbin/service ] && service $APACHE_BIN restart || $APACHE_BIN -D SSL
ps aux | grep -E "(apache|http)"
curl -Ik https://localhost --verbose

# squid
cp tools/squid.conf /etc/squid/squid.conf
[ -x /usr/sbin/service -a -n "$(command -v squid)" ] && service squid start || squid
ps aux | grep squid

# mariadb
case "$distro_id" in
    'debian'|'ubuntu')
	[ -x /usr/sbin/service -a -n "$(command -v mariadb)" ] && service mariadb start
	;;
    *)
	HOME=$(pwd)
	mkdir -p /var/lib/mysql/ /var/log/mariadb /var/log/mysql  && mysql_install_db > /dev/null && chown -R mysql /var/lib/mysql/ /var/log/mariadb /var/log/mysql && cd '/usr' ; /usr/bin/mysqld_safe --datadir='/var/lib/mysql' --nowatch
	cd $HOME
	sleep 3
	;;
esac
ps aux | grep -E "(mysql|mariadb)"
mysql -e "create database IF NOT EXISTS test;" -uroot

# ldap
[ -x /etc/openldap/slapd.conf ] && cat /etc/openldap/slapd.conf && sed -i "s/^#modulepath/modulepath/g" /etc/openldap/slapd.conf && sed -i "s/^#moduleload back_mdb/moduleload back_mdb/g" /etc/openldap/slapd.conf
sed -e 's/cn=admin,dc=nodomain/'$(/usr/sbin/slapcat|grep ^dn:|head -1|awk '{print $2}')'/' -i .github/NPTest.cache
[ -x /usr/libexec/openldap/check-config.sh ] && sh /usr/libexec/openldap/check-config.sh || [ -d /var/lib/ldap/ ] && chown -R "$(grep ldap /etc/passwd | cut -d: -f1)":"$(grep ldap /etc/passwd | cut -d: -f1)" /var/lib/ldap/
[ -x /usr/sbin/service -a -n "$(command -v slapd)" ] && service slapd start || /usr/sbin/slapd -u ldap -h "ldap:/// ldaps:/// ldapi:///" || /usr/sbin/slapd -h "ldap:///" -g ldap -u ldap -F /etc/openldap/slapd.d || /usr/lib/openldap/start
ps aux| grep slapd

# sshd
SSH_HOME="/root/.ssh"
mkdir -p $SSH_HOME
ssh-keygen -t rsa -N "" -f $SSH_HOME/id_rsa
cat $SSH_HOME/id_rsa.pub >> $SSH_HOME/authorized_keys
[ -x /usr/libexec/openssh/sshd-keygen ] && /usr/libexec/openssh/sshd-keygen rsa && /usr/libexec/openssh/sshd-keygen ecdsa && /usr/libexec/openssh/sshd-keygen ed25519
[ -x /usr/sbin/sshd-gen-keys-start ] && /usr/sbin/sshd-gen-keys-start
[ -x /usr/sbin/service -a -n "$(command -v ssh)" ] && service ssh start || /usr/sbin/sshd
sleep 1
ssh-keyscan localhost >> $SSH_HOME/known_hosts
touch $SSH_HOME/config

# start one login session, required for check_users
ssh -tt localhost </dev/null >/dev/null 2>/dev/null &
disown %1

# snmpd
for DIR in /usr/share/snmp/mibs /usr/share/mibs; do
    rm -f $DIR/ietf/SNMPv2-PDU \
          $DIR/ietf/IPSEC-SPD-MIB \
          $DIR/ietf/IPATM-IPMC-MIB \
          $DIR/iana/IANA-IPPM-METRICS-REGISTRY-MIB
done
mkdir -p /var/lib/snmp/mib_indexes
sed -e 's/^agentaddress.*/agentaddress 127.0.0.1/' -i /etc/snmp/snmpd.conf
[ -x /usr/sbin/service -a -n "$(command -v snmpd)" ] && service snmpd start || snmpd

# start postfix
[ -x /usr/sbin/service -a -n "$(command -v postfix)" ] && service postfix start || postfix start

# start ftpd
[ -x /usr/sbin/service -a -n "$(command -v vsftpd)" ] && service vsftpd start || /usr/sbin/vsftpd /etc/vsftpd.conf &

# hostname
sed "/NP_HOST_TLS_CERT/s/.*/'NP_HOST_TLS_CERT' => '$(hostname)',/" -i /src/.github/NPTest.cache

# create some test files to lower inodes
for i in $(seq 10); do
    touch /media/ramdisk2/test.$1
done
