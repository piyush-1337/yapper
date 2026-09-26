{
  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixpkgs-unstable";
  };

  outputs =
    { self, nixpkgs }:
    let
      systems = [
        "x86_64-linux"
        "aarch64-linux"
      ];

      forEachSystem = nixpkgs.lib.genAttrs systems;
    in
    {
      packages = forEachSystem (
        system:
        let
          pkgs = nixpkgs.legacyPackages.${system};
          lib = pkgs.lib;

          packageFiles = lib.fileset.unions [
            ./CMakeLists.txt
            ./include
          ];
        in
        {
          default = pkgs.stdenv.mkDerivation {
            pname = "yapper";
            version = "0.1.0";

            src = lib.fileset.toSource {
              root = ./.;
              fileset = packageFiles;
            };

            dontBuild = true;

            nativeBuildInputs = [
              pkgs.cmake
            ];
          };
        }
      );

      checks = forEachSystem (
        system:
        let
          pkgs = nixpkgs.legacyPackages.${system};
          lib = pkgs.lib;

          packageFiles = lib.fileset.unions [
            ./CMakeLists.txt
            ./include
          ];

          testFiles = lib.fileset.unions [
            packageFiles
            ./tests
          ];
        in
        {
          tests = pkgs.stdenv.mkDerivation {
            pname = "yapper-tests";
            version = "0.1.0";

            src = lib.fileset.toSource {
              root = ./.;
              fileset = testFiles;
            };

            nativeBuildInputs = with pkgs; [
              gcc16
              cmake
              ninja
              catch2_3
            ];

            configurePhase = ''
              cmake -S . -B build -G Ninja \
                -DBUILD_TESTS=ON \
                -DBUILD_EXAMPLES=OFF \
                -DCMAKE_BUILD_TYPE=Release
            '';

            buildPhase = ''
              cmake --build build
            '';

            doCheck = true;

            checkPhase = ''
              ctest --test-dir build --output-on-failure
            '';

            installPhase = ''
              mkdir -p $out
            '';
          };
        }
      );

      devShells = forEachSystem (
        system:
        let
          pkgs = nixpkgs.legacyPackages.${system};
        in
        {
          default = pkgs.mkShell {
            packages = with pkgs; [
              gcc16
              cmake
              ninja
              llvmPackages_latest.clang-tools
              neocmakelsp
              catch2_3
            ];
          };
        }
      );
    };
}
