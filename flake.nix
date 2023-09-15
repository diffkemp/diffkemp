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
    };
}
