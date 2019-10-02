%define llvmcpy_version 0.1.5

Name:           diffkemp
Version:        0.1.0
Release:        1%{?dist}
Summary:        A tool for analyzing differences in kernel functions

License:        ASL 2.0
URL:            https://github.com/viktormalik/diffkemp
Source0:        https://github.com/viktormalik/diffkemp/archive/%{version}.tar.gz#/%{name}-%{version}-%{release}.tar.gz
Source1:        %pypi_source llvmcpy %{llvmcpy_version}

BuildRequires:  gcc gcc-c++ cmake ninja-build
BuildRequires:  python3-devel
Requires:       cscope
Requires:       clang llvm-devel
Requires:       make

%{?python_enable_dependency_generator}

%description
DiffKemp is a tool for finding changes in semantics of various parts of the
Linux kernel between different kernel versions. It allows to compare semantics
of functions and of sysctl kernel parameters. The comparison is based on static
analysis of the source code that is translated into the LLVM intermediate
representation.


%prep
%setup -q -n %{name}-rpm
%setup -q -a 1 -T -D -n %{name}-rpm
cd llvmcpy-%{llvmcpy_version}


%build
# Python part
%py3_build
# SimpLL (C++ part)
%cmake . -GNinja
%ninja_build
# llvmcpy Python package
cd llvmcpy-%{llvmcpy_version}
%py3_build
cd ..


%install
# Python part
%py3_install
# SimpLL (C++ part)
%ninja_install
mkdir -p %{buildroot}/%{_bindir}
install -m 0755 bin/%{name} %{buildroot}/%{_bindir}/%{name}
# llvmcpy Python package
cd llvmcpy-%{llvmcpy_version}
%py3_install
cd ..


%files
%license LICENSE
%doc README.md
# Python part
%{python3_sitelib}/%{name}-*.egg-info/
%{python3_sitelib}/%{name}
# SimpLL (C++ part)
%{_bindir}/%{name}
%{_bindir}/%{name}-simpll
# llvmcpy Python package
%{python3_sitelib}/llvmcpy-%{llvmcpy_version}-*.egg-info/
%{python3_sitelib}/llvmcpy


%changelog
* Thu Aug 22 2019 Viktor Malik <vmalik@redhat.com> 0.1-1
- Initial RPM release
