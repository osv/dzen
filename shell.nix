{ pkgs ? import <nixpkgs> {} }:

with pkgs;

mkShell {
  buildInputs = [
    autoconf
    automake
    pkg-config
    gcc
    xorg.libX11.dev
    xorg.libXft
    xorg.libXinerama
    xorg.libXpm
    pkg

    valgrind

    imagemagick
  ];

  shellHook = ''
    echo "Welcome to the development shell"
  '';
}