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
    xorg.libXcursor
    pkg

    valgrind

    xorg.xwd
    imagemagick

    # for updating man page
    pandoc
  ];

  shellHook = ''
    echo "Welcome to the development shell"
  '';
}
