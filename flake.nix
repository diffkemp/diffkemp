{
  description = "Static analyser of semantic differences in large C projects";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/release-25.05";
    # We need nixpkgs 23.11 to get GCC 7 into the test-kernel-buildenv flake
    nixpkgs-2311.url = "github:NixOS/nixpkgs/release-23.11";
  };

  outputs = { self, nixpkgs, nixpkgs-2311, ... }:
    let
      system = "x86_64-linux";
      pkgs = import nixpkgs { inherit system; };

      llvmVersionMin = 9;
      llvmVersionMax = 17;
      llvmVersions = pkgs.lib.lists.range llvmVersionMin llvmVersionMax;

      mkDiffkemp =
        llvmPackages:
          with pkgs;
          python3Packages.buildPythonPackage {
            pname = "diffkemp";
            version = "0.6.1";
            pyproject = true;

            src = self;

            nativeBuildInputs = with llvmPackages; [
              cmake
              gcc
              libllvm
              ninja
              nodejs_20
            ];

            buildInputs = with llvmPackages; [
              clangNoLibcxx
              cscope
              diffutils
              gtest
              gnumake
              libmpc
              mpfr
              z3
            ];

            propagatedBuildInputs = with python3Packages; [
              cffi
              pyyaml
              setuptools
              pypaInstallHook
            ];

            build-system = with python3Packages; [
                setuptools
            ];

            WITHOUT_RPYTHON = true;

            # Including cmake in nativeBuildInputs automatically runs it during
            # configurePhase so we just need to set correct flags.
            cmakeFlags = [ "-DCMAKE_BUILD_TYPE=Release" "-GNinja" ];

            # We're mixing Ninja and Python build here so we need to manually
            # define buildPhase and installPhase to make sure both are built.
            # CMake has switched dir to build/ so let's switch back and define
            # ninjaFlags.
            ninjaFlags = [ "-C" "build" ];

            buildPhase = ''
              cd ..
              ninjaBuildPhase
              pypaBuildPhase
            '';

            installPhase = ''
              ninjaInstallPhase
              pypaInstallPhase
            '';
          };

      mkDiffkempDevShell =
        diffkemp-pkg:
          with pkgs;
          let
            rhel_kernel_get = python3Packages.buildPythonApplication {
              pname = "rhel-kernel-get";
              version = "0.1";
              src = fetchFromGitHub {
                owner = "viktormalik";
                repo = "rhel-kernel-get";
                rev = "v0.1";
                sha256 = "0ci5hdkzc2aq7s8grnkqc9ni7zajyndj7b9r5fqqxvbjqvm7lqi7";
              };
              propagatedBuildInputs = [ python3Packages.progressbar ];
            };
          in
          mkShell {
            inputsFrom = [ diffkemp-pkg ];

            # Dependies for projects on which we run experiments
            # (kernel, security libraries, ...).
            buildInputs = [
              bc
              bison
              bzip2
              cpio
              flex
              gdb
              git
              gmp
              kmod
              openssl
              rhel_kernel_get
              rpm
              wget
              xz
              libtool
              autoconf
              automake
              autogen
            ];

            propagatedBuildInputs = with python3Packages; [
              pytest
              pytest-mock
              flake8
              jinja2
              jsonschema
            ];

            WITHOUT_RPYTHON = true;

            shellHook = ''
              # Adding current (diffkemp) directory to PYTHONPATH,
              # the `diffkemp build` subcommand does not work without it
              # - `cc_wrapper.py` ends with `ModuleNotFoundError`.
              export PYTHONPATH="$(pwd):$PYTHONPATH"
            '';
          };
    in
    {
      formatter.${system} = pkgs.nixpkgs-fmt;

      # Create a package for each supported LLVM.
      # The default is always the latest LLVM.
      packages.${system} = (builtins.listToAttrs (map
        (ver: {
          name = "diffkemp-llvm${ver}";
          value = mkDiffkemp pkgs."llvmPackages_${ver}";
        })
        (map toString llvmVersions))) // {
        default = mkDiffkemp pkgs."llvmPackages_${toString llvmVersionMax}";
      };

      # Create a dev shell for each supported LLVM.
      # The default is always the latest LLVM.
      devShells.${system} = (builtins.listToAttrs (map
        (ver: {
          name = "diffkemp-llvm${ver}";
          value = mkDiffkempDevShell self.packages.${system}."diffkemp-llvm${ver}";
        })
        (map toString llvmVersions))) // rec {
        default = mkDiffkempDevShell self.packages.${system}."diffkemp-llvm${toString llvmVersionMax}";

        # Environment for downloading and preparing test kernels (RHEL 7 and 8).
        # Contains 2 changes from the default env necessary for RHEL 7:
        #  - gcc 7
        #  - make 3.81
        # It should be sufficient to use this to download the kernels (with
        # rhel-kernel-get) and the tests can be run in the default dev shell.
        test-kernel-buildenv =
          with pkgs;
          let
            oldmake = import
              (builtins.fetchTarball {
                url = "https://github.com/NixOS/nixpkgs/archive/92487043aef07f620034af9caa566adecd4a252b.tar.gz";
                sha256 = "00fgvpj0aqmq45xmmiqr2kvdir6zigyasx130rp96hf35mab1n8c";
              })
              { inherit system; };
            gnumake381 = oldmake.gnumake381;

            pkgs-2311 = import nixpkgs-2311 { inherit system; };
            gcc7Stdenv = pkgs-2311.gcc7Stdenv;
          in

          gcc7Stdenv.mkDerivation {
            # gcc7Stdenv provides GCC 7, however it doesn't provide the mkShell
            # command so we need to use mkDerivation and clear phases (that's what
            # mkShell does).
            name = "test-kernel-buildenv";
            phases = [ ];

            nativeBuildInputs = lib.lists.remove gcc default.nativeBuildInputs;

            buildInputs = lib.lists.remove gnumake default.buildInputs ++ [
              gnumake381
            ];

            propagatedBuildInputs = default.propagatedBuildInputs;

            WITHOUT_RPYTHON = true;
          };
      };
    };
}
