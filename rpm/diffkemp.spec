Name:           diffkemp
Version:        0.3.0
Release:        1%{?dist}
Summary:        A tool for analyzing differences in kernel functions

License:        ASL 2.0
URL:            https://github.com/viktormalik/diffkemp
Source0:        https://github.com/viktormalik/diffkemp/archive/v%{version}.tar.gz#/%{name}-%{version}.tar.gz

BuildRequires:  gcc gcc-c++ cmake ninja-build
BuildRequires:  llvm-devel
BuildRequires:  python3-devel python3-pip
BuildRequires:  git
Requires:       cscope
Requires:       clang llvm-devel
Requires:       make
Requires:       diffutils
Requires:       python3-setuptools

%{?python_enable_dependency_generator}

%description
DiffKemp is a tool for finding changes in semantics of various parts of the
Linux kernel between different kernel versions. It allows to compare semantics
of functions and of sysctl kernel parameters. The comparison is based on static
analysis of the source code that is translated into the LLVM intermediate
representation.


%prep
%setup -q


%build
mkdir build
# SimpLL (C++ part)
%cmake -S . -B build -GNinja
%ninja_build -C build
# Python part
%py3_build


%install
# SimpLL (C++ part)
%ninja_install -C build
mkdir -p %{buildroot}/%{_bindir}
install -m 0755 bin/%{name} %{buildroot}/%{_bindir}/%{name}
# Python part
%py3_install


%check
# Run SimpLL unit tests
tests/unit_tests/simpll/runTests


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


%changelog
* Mon Feb 28 2022 Viktor Malik <vmalik@redhat.com> - 0.3.0-1
- Improved CLI interface
- Support analysis of projects built into a single LLVM IR file
- Support for LLVM 11, 12, and 13
- Handling analysis of "swapped if branches" refactoring
- Building SimpLL as a shared library
- Analysis result caching
- Improved verbosity output
- Improved field access comparison
- Drop llvmcpy dependency

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
