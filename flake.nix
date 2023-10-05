{
  description = "Static analyser of semantic differences in large C projects";

  inputs = { nixpkgs.url = "github:NixOS/nixpkgs/release-23.05"; };

  outputs = { self, nixpkgs, ... }:
    let
      system = "x86_64-linux";
      pkgs = import nixpkgs { inherit system; };
    in
    {
      formatter.${system} = pkgs.nixpkgs-fmt;

      packages.${system}.default = with pkgs;
        python3Packages.buildPythonPackage {
          pname = "diffkemp";
          version = "0.5.0";

          src = self;

          nativeBuildInputs = with llvmPackages_16; [ cmake gcc libllvm ninja ];

          buildInputs = with llvmPackages_16; [
            clangNoLibcxx
            cscope
            diffutils
            gtest
            gnumake
          ];

          propagatedBuildInputs = with python3Packages; [
            cffi
            pyyaml
            setuptools
          ];

          # Including cmake in nativeBuildInputs automatically runs it during
          # configurePhase so we just need to set correct flags.
          cmakeFlags = [ "-DCMAKE_BUILD_TYPE=Debug" "-GNinja" ];

          # We're mixing Ninja and Python build here so we need to manually define
          # buildPhase and installPhase to make sure both are built. CMake has
          # switched dir to build/ so let's switch back and define ninjaFlags.
          ninjaFlags = [ "-C" "build" ];

          buildPhase = ''
            cd ..
            ninjaBuildPhase
            setuptoolsBuildPhase
          '';

          installPhase = ''
            ninjaInstallPhase
            pipInstallPhase
            install -m 0755 bin/diffkemp $out/bin/diffkemp
          '';
        };

      devShells.${system} = {
        default = with pkgs;
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
            inputsFrom = [ self.packages.${system}.default ];

            buildInputs = [
              bc
              bison
              bzip2
              cpio
              flex
              gdb
              gmp
              kmod
              openssl
              rhel_kernel_get
              rpm
              xz
            ];

            propagatedBuildInputs = with python3Packages; [
              pytest
              pytest-mock
            ];
          };
      };

      # Environment for downloading and preparing test kernels (RHEL 7 and 8).
      # Contains 2 changes from the default env necessary for RHEL 7:
      #  - gcc 7
      #  - make 3.81
      # It should be sufficient to use this to download the kernels (with
      # rhel-kernel-get) and the tests can be run in the default dev shell.
      test-kernel-buildenv = with pkgs;
        let
          oldmake = import
            (builtins.fetchTarball {
              url = "https://github.com/NixOS/nixpkgs/archive/92487043aef07f620034af9caa566adecd4a252b.tar.gz";
              sha256 = "00fgvpj0aqmq45xmmiqr2kvdir6zigyasx130rp96hf35mab1n8c";
            })
            { inherit system; };
          gnumake381 = oldmake.gnumake381;

          default = self.devShells.${system}.default;
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

          dontUseSetuptoolsShellHook = true;
        };
    };
}
