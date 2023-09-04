Name:           diffkemp
Version:        0.5.0
Release:        1%{?dist}
Summary:        A tool for analyzing semantic differences in C projects

License:        ASL 2.0
URL:            https://github.com/viktormalik/diffkemp
Source0:        https://github.com/viktormalik/diffkemp/archive/v%{version}.tar.gz#/%{name}-%{version}.tar.gz
Source1:        https://files.pythonhosted.org/packages/f9/71/4931d0c9b9be01a401f89ced4afb41aa46064f08b2659c31ccdaa5a8b679/rpython-0.2.1.tar.gz
Source2:        https://files.pythonhosted.org/packages/98/ff/fec109ceb715d2a6b4c4a85a61af3b40c723a961e8828319fbcb15b868dc/py-1.11.0.tar.gz

BuildRequires:  gcc gcc-c++ cmake ninja-build
BuildRequires:  llvm-devel
BuildRequires:  python3-devel python3-pip python3-setuptools python2
BuildRequires:  git
BuildRequires:  /usr/bin/pathfix.py
Requires:       cscope
Requires:       clang llvm-devel
Requires:       make
Requires:       diffutils

%{?python_enable_dependency_generator}

%description
DiffKemp is a tool for finding changes in semantics between versions of C
projects, with the main focus on the Linux kernel. It allows to compare
semantics of functions and of sysctl kernel parameters. The comparison is based
on static analysis of the source code that is translated into the LLVM
intermediate representation.


%prep
%setup -q -a 1 -a 2

%build
PYTHONPATH=$PWD/rpython-0.2.1:$PWD/py-1.11.0:$PYTHONPATH
export PYTHONPATH
mkdir build
# SimpLL (C++ part)
%cmake -S . -B build -GNinja -DBUILD_VIEWER=On
%ninja_build -C build
# Python part
%py3_build


%install
# SimpLL (C++ part) + Viewer
%ninja_install -C build
mkdir -p %{buildroot}/%{_bindir}
install -m 0755 bin/%{name} %{buildroot}/%{_bindir}/%{name}
# Python part
%py3_install
pathfix.py -pni "%{__python3} %{py3_shbang_opts}" %{buildroot}%{_bindir}/diffkemp-cc-wrapper.py


%check
# Run SimpLL unit tests
tests/unit_tests/simpll/runTests


%files
%license LICENSE
%doc README.md
# Python part
%{python3_sitearch}/%{name}-*.egg-info/
%{python3_sitearch}/%{name}
%{_bindir}/%{name}-cc-wrapper
%{_bindir}/%{name}-cc-wrapper.py
# SimpLL (C++ part)
%{_bindir}/%{name}
%{_libdir}/libsimpll-lib.so
# Viewer
%{_sharedstatedir}/diffkemp/view/


%changelog
* Mon Sep 04 2023 Viktor Malik <vmalik@redhat.com> - 0.5.0-1
- New web-based viewer of found differences
- CLI options for built-in patterns
- Improved logging and debugging
- Support for LLVM 16
- Move to C++17

* Wed Nov 23 2022 Viktor Malik <vmalik@redhat.com> - 0.4.0-1
- Reworked CLI interface (generate split into multiple commands)
- New command for analysing any Makefile-based projects
- Support for LLVM 14 and 15
- SimpLL used as a library by default
- Support for custom patterns
- Handling code relocations
- Improved handling of inverse branching conditions
- Prevent analysing snapshots with other LLVM version

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
