{
  description = "Nix flake for dzen2 with devShell support";

  # inputs.nixpkgs.url = "github:NixOS/nixpkgs";
  inputs.nixpkgs.url = "github:NixOS/nixpkgs/abf9d678aa";

  outputs = { self, nixpkgs, ... }:
    let
      systems = [ "x86_64-linux" ];
    in rec {
      defaultOptions = {
        useXPM = true;
        useXFT = true;
        useXinerama = true;
        useXcursor = true;
        useXrandr = true;
      };

      # Function to create packages with options
      packageWithOptions = { options ? { } }:
        builtins.listToAttrs (map (system: {
          name = system;
          value = let
            pkgs = import nixpkgs { inherit system; };
            lib = pkgs.lib;
            actualOptions = lib.recursiveUpdate defaultOptions options;
            optionalBuildDeps = with pkgs;
              [  ]
              ++ lib.optional actualOptions.useXPM xorg.libXpm
              ++ lib.optional actualOptions.useXFT xorg.libXft
              ++ lib.optional actualOptions.useXinerama xorg.libXinerama
              ++ lib.optional actualOptions.useXcursor xorg.libXcursor
              ++ lib.optional actualOptions.useXrandr xorg.libXrandr;
          in pkgs.stdenv.mkDerivation rec {
            pname = "dzen2";
            version = "1.0.0";

            src = ./.;

            preConfigure = ''
              autoreconf -vfi
            '';

            configureFlags = [ ]
              ++ lib.optional actualOptions.useXPM "--enable-xpm"
              ++ lib.optional actualOptions.useXFT "--enable-xft"
              ++ lib.optional actualOptions.useXinerama "--enable-xinerama"
              ++ lib.optional actualOptions.useXcursor "--enable-xcursor"
              ++ lib.optional actualOptions.useXrandr "--enable-xrandr";

            nativeBuildInputs = [
              pkgs.autoconf
              pkgs.automake
              pkgs.pkg-config
            ];

            buildInputs = optionalBuildDeps;

            meta = with pkgs.lib; {
              description = "A lightweight and customizable status bar for X11";
              license = licenses.mit;
              homepage = "https://github.com/osv/dzen";
              maintainers = [ "Olexandr Sydorchuk <olexandr.syd@gmail.com>" ];
              platforms = platforms.unix;
            };
          };
        }) systems);

      # Define packages as attribute sets containing derivations
      packages = builtins.listToAttrs (map (system: {
        name = system;
        value = {
          default = (packageWithOptions { }).${system};
          # Expose packageWithOptions for custom options
          packageWithOptions = packageWithOptions;
        };
      }) systems);

      # Set the default package for the current system
      defaultPackage =
        builtins.mapAttrs (system: pkgSet: pkgSet.default) packages;

      # # Add the apps attribute
      apps = builtins.listToAttrs (map (system: {
        name = system;
        value = {
          dzen2 = {
            type = "app";
            program = "${packages.${system}.default}/bin/dzen2";
          };
        };
      }) systems);

      devShell = builtins.listToAttrs (map (system: {
        name = system;
        value = let pkgs = import nixpkgs { inherit system; };
        in pkgs.mkShell {
          buildInputs = with pkgs; [
            autoconf
            automake
            pkg-config
            gcc
            gdb
            xorg.libX11.dev
            xorg.libXft
            xorg.libXinerama
            xorg.libXpm
            xorg.libXcursor
            xorg.libXrandr
            xorg.xorgserver     # isolated Xorg/Xvfb integration tests
            xorg.xf86videodummy # RandR-capable dummy DDX for test_xrandr
            dejavu_fonts
            fontconfig

            valgrind            # I want check for memory leak, `printer-app | valgrind -s --leak-check=full --show-leak-kinds=all ./dzen2 ...`

            xorg.xwd            # for `make test`
            imagemagick         # for `make test`
            pandoc              # Update man pages: `make update-man`
            linuxKernel.packages.linux_xanmod_stable.perf
            clang-tools         # clang-tidy
            bc
          ];

          FONTCONFIG_FILE = pkgs.makeFontsConf { fontDirectories = [ pkgs.dejavu_fonts ]; };

          shellHook = ''
            export XRANDR_XORG_MODULE_PATH="${pkgs.xorg.xf86videodummy}/lib/xorg/modules,${pkgs.xorg.xorgserver}/lib/xorg/modules"
            echo "!! Entering the dzen2 development shell"
          '';
        };
      }) systems);
    };
}
