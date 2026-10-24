{
    description = "C++ CMake dev env";

    inputs = {
        nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
        flake-utils.url = "github:numtide/flake-utils";
    };

    outputs = { self, nixpkgs, flake-utils }:
        flake-utils.lib.eachDefaultSystem (system:
            let
                pkgs = nixpkgs.legacyPackages.${system};
            in {
                devShells.default = pkgs.mkShell {
                    nativeBuildInputs = [ pkgs.pkg-config ];
                    buildInputs = with pkgs; [
                        clang
                        cmake
                        SDL2
                        ffmpeg # to convert ppm files to pkg-config
                        # for f in *.ppm; do ffmpeg -i "$f" "${f%.ppm}.png"; done
                    ];
                };
            }
        );
}
