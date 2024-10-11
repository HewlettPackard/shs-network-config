%define _build_id %{version}-%{release}
%define _prefix /opt/slingshot/%{name}/%{_build_id}
%define _pkgdatadir %{_datadir}

Name: slingshot-network-config
Version: 1.2.0
Release: %(echo $BUILD_METADATA)
Summary: Slingshot Network Configuration Files
Group: System Environment/Libraries
License: BSD
Url: http://www.hpe.com
Source: %{name}-%{version}.tar.gz
Vendor: Hewlett Packard Enterprise Company
BuildRoot: %{_tmppath}/%{name}-%{version}-%{release}-root-%(%{__id_u} -n)
Packager: Hewlett Package Enterprise Company
Obsoletes: slingshot-sysctl-network
Obsoletes: slingshot-limits-network
Obsoletes: slingshot-modprobe-network
Obsoletes: slingshot-common-network-config
Obsoletes: slingshot-network-config-utils
Distribution: Slingshot

%description
Slingshot Network Configuration base package

%package -n slingshot-network-config-full
Summary: meta-package for full integration of the component
Requires: slingshot-network-config
Requires: dracut
Requires: /usr/sbin/lldpad
Requires: /usr/sbin/lldptool
Requires: util-linux

%description -n slingshot-network-config-full
meta-package for full integration of the component


%prep
%setup -q -n %{name}-%{version}

%build
./autogen.sh
%configure
make %{?_smp_mflags}
make check

%install
rm -rf %{buildroot}
make install DESTDIR=%{buildroot}

# workaround for a wierd autoconf-ism
for entry in $(ls %{buildroot}%{_pkgdatadir}/%{name}/) ; do
    mv %{buildroot}%{_pkgdatadir}/%{name}/${entry} %{buildroot}%{_pkgdatadir}
done

rm -rf ${buildroot}%{_pkgdatadir}/%{name}

%clean
rm -rf %{buildroot}

################################################################################
%post
SH_PREFIX=%{_prefix}
# replace version number with default
SH_PREFIX_DEFAULT="${SH_PREFIX%/%{_build_id}}/default"
rm -f ${SH_PREFIX_DEFAULT}
ln -s %{_prefix} ${SH_PREFIX_DEFAULT}

SH_BINDIR=%{_bindir}
SH_BINDIR_DEFAULT=${SH_BINDIR/%{_build_id}/default}
ln -sf ${SH_BINDIR_DEFAULT}/slingshot-ifname.sh /usr/bin/slingshot-ifname
ln -sf ${SH_BINDIR_DEFAULT}/slingshot-ifroute.sh /usr/bin/slingshot-ifroute
ln -sf ${SH_BINDIR_DEFAULT}/slingshot-network-cfg-lldp /sbin/slingshot-network-cfg-lldp

################################################################################
%postun
echo postun %{_build_id}
rm -rf %{_prefix}
SH_PREFIX=%{_prefix}
SH_PREFIX_BASE=${SH_PREFIX%/%{_build_id}}
# replace version number with default
SH_PREFIX_DEFAULT=${SH_PREFIX_BASE}/default
if [ "$(ls -l ${SH_PREFIX_DEFAULT} 2>/dev/null | sed 's#.*/##')" == "%{_build_id}" ]; then
    # if this is the active instance of the package then we delete the default symlinks
    rm -f ${SH_PREFIX_DEFAULT}
    # if previous versions exist then find the latest
    SH_LAST=$(ls -r ${SH_PREFIX_BASE} | grep -E '[0-9]+\.[0-9]+\.[0-9]+' | head -n1)
    if [ ${#SH_LAST} -eq 0 ]; then
        # This is the last instance of the package so we delete all the symlinks
        rm -f /usr/bin/slingshot-ifroute
        rm -f /usr/bin/slingshot-ifname
        rm -f /sbin/slingshot-network-cfg-lldp
        # delete the directories (if empty)
        rmdir ${SH_PREFIX_BASE} 2>/dev/null || true
        rmdir ${SH_PREFIX_BASE%/%{name}} 2>/dev/null || true
    else
        # a previous verion exists, make previous actve with idefault symlink
        ln -s ${SH_PREFIX_BASE}/${SH_LAST} ${SH_PREFIX_DEFAULT}
    fi
fi

################################################################################
# TODO:2021-03-17:james.swaro@hpe.com:NETETH-1295
# Add udev rules for shasta integration
%post -n slingshot-network-config-full
# create symlinks to the default directory
SH_PKGDATADIR=%{_pkgdatadir}
SH_PKGDATADIR_DEFAULT=${SH_PKGDATADIR/%{_build_id}/default}
ln -sf ${SH_PKGDATADIR_DEFAULT}/dracut/96-slingshot-network-lldp /usr/lib/dracut/modules.d/96slingshot-network-lldp
for entry in modprobe ; do
    ln -sf ${SH_PKGDATADIR_DEFAULT}/dracut/99-slingshot-network-${entry} /usr/lib/dracut/modules.d/99slingshot-network-${entry}
done
ln -sf ${SH_PKGDATADIR_DEFAULT}/sysctl/99-slingshot-network.conf /usr/lib/sysctl.d/99-slingshot-network.conf
ln -sf ${SH_PKGDATADIR_DEFAULT}/limits/99-slingshot-network.conf /etc/security/limits.d/99-slingshot-network.conf
ln -sf ${SH_PKGDATADIR_DEFAULT}/modprobe/99-slingshot-network.conf /etc/modprobe.d/99-slingshot-network.conf
ln -sf /usr/bin/slingshot-ifroute /etc/sysconfig/network/if-up.d/slingshot-ifroute

################################################################################
%postun -n slingshot-network-config-full
if [ ${1} -eq 0 ]; then
    # if this is the last instance of the package then we delete the symlinks
    rm -f /usr/lib/dracut/modules.d/96slingshot-network-lldp
    for entry in modprobe ; do
        rm -f /usr/lib/dracut/modules.d/99slingshot-network-${entry}
    done
    rm -f /usr/lib/sysctl.d/99-slingshot-network.conf
    rm -f /etc/modprobe.d/99-slingshot-network.conf
    rm -f /etc/security/limits.d/99-slingshot-network.conf
    rm -f /usr/lib/dracut/modules.d/99slingshot-network-modprobe
    rm -f /etc/sysconfig/network/if-up.d/slingshot-ifroute
fi


%files
%defattr(-,root,root,-)
%{_pkgdatadir}/sysctl/99-slingshot-network.conf
%{_pkgdatadir}/dracut/99-slingshot-network-modprobe/module-setup.sh
%{_pkgdatadir}/dracut/99-slingshot-network-udev/module-setup.sh
%{_pkgdatadir}/dracut/96-slingshot-network-lldp/module-setup.sh
%{_pkgdatadir}/limits/99-slingshot-network.conf
%{_pkgdatadir}/modprobe/99-slingshot-network.conf
%{_pkgdatadir}/udev/99-slingshot-network.rules
%{_bindir}/slingshot-ifroute.sh
%{_bindir}/slingshot-ifname.sh
%{_bindir}/slingshot-network-cfg-lldp
%{_bindir}/stop_lldpad.sh
%{_bindir}/start_lldpad.sh
%{_bindir}/run_slingshot_network_cfg_lldp.sh
%{_bindir}/copy_logfile_to_rootfs.sh
%doc COPYING

%files -n slingshot-network-config-full
%defattr(-,root,root,-)

%changelog
# Mon Aug 17 12:54:52 PDT 2020 S Lester Forked for Slingshot
