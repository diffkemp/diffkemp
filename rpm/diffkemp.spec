%define llvmcpy_version 0.1.5

Name:           diffkemp
Version:        0.2.2
Release:        1%{?dist}
Summary:        A tool for analyzing differences in kernel functions

License:        ASL 2.0
URL:            https://github.com/viktormalik/diffkemp
Source0:        https://github.com/viktormalik/diffkemp/archive/v%{version}.tar.gz#/%{name}-%{version}.tar.gz
Source1:        %pypi_source llvmcpy %{llvmcpy_version}

BuildRequires:  gcc gcc-c++ cmake ninja-build
BuildRequires:  llvm-devel
BuildRequires:  python3-devel
Requires:       cscope
Requires:       clang llvm-devel
Requires:       make
Requires:       diffutils

%{?python_enable_dependency_generator}

%description
DiffKemp is a tool for finding changes in semantics of various parts of the
Linux kernel between different kernel versions. It allows to compare semantics
of functions and of sysctl kernel parameters. The comparison is based on static
analysis of the source code that is translated into the LLVM intermediate
representation.


%prep
%setup -q -n %{name}-%{version}
%setup -q -a 1 -T -D -n %{name}-%{version}
cd llvmcpy-%{llvmcpy_version}


%build
mkdir build
# SimpLL (C++ part)
%cmake -S . -B build -GNinja
%ninja_build -C build
# Python part
%py3_build
# llvmcpy Python package
cd llvmcpy-%{llvmcpy_version}
%py3_build
cd ..


%install
# SimpLL (C++ part)
%ninja_install -C build
mkdir -p %{buildroot}/%{_bindir}
install -m 0755 bin/%{name} %{buildroot}/%{_bindir}/%{name}
# Python part
%py3_install
# llvmcpy Python package
cd llvmcpy-%{llvmcpy_version}
%py3_install
cd ..


%files
%license LICENSE
%doc README.md
# Python part
%{python3_sitearch}/%{name}-*.egg-info/
%{python3_sitearch}/%{name}
# SimpLL (C++ part)
%{_bindir}/%{name}
%{_bindir}/%{name}-simpll
%{_libdir}/libsimpll-lib.so
# llvmcpy Python package
%{python3_sitelib}/llvmcpy-%{llvmcpy_version}-*.egg-info/
%{python3_sitelib}/llvmcpy


%changelog
* Tue Apr 07 2020 Viktor Malik <vmalik@redhat.com> - 0.2.2-1
- Support kernel 5.x versions
- Support for LLVM 10
- Experimental: build diffkemp-simpll as a shared library
- Bugfixes and optimisations

* Thu Mar 12 2020 Viktor Malik <vmalik@redhat.com> - 0.2.1-1
- Require diffutils
- Bugfix

* Mon Mar 09 2020 Viktor Malik <vmalik@redhat.com> - 0.2.0-1
- Detection of differences in types
- Improved snapshot format
- Caching comparison results among different compared symbols
- Optimisation of macro difference analysis
- Various bugfixes
- Support for LLVM 9

* Tue Oct 08 2019 Viktor Malik <vmalik@redhat.com> 0.1.1-1
- Fix bug in parsing inline assembly.
- Fix function inlining causing empty diffs.
- Fix bugs in and improve handling of unused return values.

* Wed Oct 02 2019 Viktor Malik <vmalik@redhat.com> 0.1.0-1
- Initial RPM release
