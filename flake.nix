{
  description = "TRAD Strike Sokol/c-ecs shooter";

  inputs.nixpkgs.url = "github:NixOS/nixpkgs/nixpkgs-unstable";
  inputs.cecs = {
    url = "path:/Volumes/Storage/code/c-ecs";
    flake = false;
  };

  outputs = { self, nixpkgs, cecs }:
    let
      systems = [ "aarch64-darwin" "x86_64-darwin" "x86_64-linux" "aarch64-linux" ];
      forAllSystems = nixpkgs.lib.genAttrs systems;
    in {
      devShells = forAllSystems (system:
        let
          pkgs = import nixpkgs { inherit system; };
          darwinFrameworks = pkgs.lib.optionals pkgs.stdenv.isDarwin [
            pkgs.apple-sdk_15
          ];
          linuxLibs = pkgs.lib.optionals pkgs.stdenv.isLinux [
            pkgs.xorg.libX11
            pkgs.xorg.libXi
            pkgs.xorg.libXcursor
            pkgs.libGL
          ];
        in {
          default = pkgs.mkShell {
            packages = [
              pkgs.clang
              pkgs.emscripten
              pkgs.gnumake
              pkgs.python3
            ] ++ darwinFrameworks ++ linuxLibs;
          };
        });

      packages = forAllSystems (system:
        let
          pkgs = import nixpkgs { inherit system; };
          darwinFrameworks = pkgs.lib.optionals pkgs.stdenv.isDarwin [
            pkgs.apple-sdk_15
          ];
          linuxLibs = pkgs.lib.optionals pkgs.stdenv.isLinux [
            pkgs.xorg.libX11
            pkgs.xorg.libXi
            pkgs.xorg.libXcursor
            pkgs.libGL
          ];
        in {
          default = pkgs.stdenv.mkDerivation {
            pname = "trad-strike";
            version = "0.1.0";
            src = ./.;
            nativeBuildInputs = [ pkgs.python3 pkgs.gnumake ];
            buildInputs = darwinFrameworks ++ linuxLibs;
            buildPhase = ''
              runHook preBuild
              make native CECS_DIR=${cecs}
              runHook postBuild
            '';
            installPhase = ''
              runHook preInstall
              mkdir -p $out/bin
              cp build/trad $out/bin/trad
              runHook postInstall
            '';
          };
        });
    };
}
