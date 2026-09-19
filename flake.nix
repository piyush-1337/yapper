{
  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/?ref=nixpkgs-unstable";
  };

  outputs =
    { self, nixpkgs }:
    let
      pkgs = nixpkgs.legacyPackages.x86_64-linux;
      system = "x86_64-linux";
    in
    {
      devShells.${system}.default = pkgs.mkShell {
        packages = with pkgs; [
          gcc16

          cmake
          ninja

          llvmPackages_latest.clang-tools
          neocmakelsp
        ];
      };
    };

}
