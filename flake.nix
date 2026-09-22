{
  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/?ref=nixpkgs-unstable";
  };

  outputs =
    { self, nixpkgs }:
    let
      pkgs = nixpkgs.legacyPackages.x86_64-linux;
      system = "x86_64-linux";
      lib = pkgs.lib;
    in
    {
      packages.${system}.default = pkgs.stdenv.mkDerivation {
        pname = "yapper";
        version = "0.1.0";

        src = lib.fileset.toSource {
          root = ./.;
          fileset = lib.fileset.unions [
            ./CMakeLists.txt
            ./include
          ];
        };

        dontBuild = true;

        nativeBuildInputs = with pkgs; [
          cmake
        ];
      };

      devShells.${system}.default = pkgs.mkShell {
        packages = with pkgs; [
          gcc16

          cmake
          ninja

          llvmPackages_latest.clang-tools
          neocmakelsp

          catch2_3
        ];
      };
    };

}
