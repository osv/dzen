{
  description = "Nix flake for dzen2 with devShell support";

  # inputs.nixpkgs.url = "github:NixOS/nixpkgs";
  inputs.nixpkgs.url = "github:NixOS/nixpkgs/51063ed4";

  outputs = { self, nixpkgs, ... }:
    let
      systems = [ "x86_64-linux" ];
    in rec {
      defaultOptions = {
        useXPM = true;
        useXFT = true;
        useXinerama = true;
        useXcursor = true;
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
              ++ lib.optional actualOptions.useXcursor xorg.libXcursor;
          in pkgs.stdenv.mkDerivation rec {
            pname = "dzen2";
            version = "1.0.0";

            src = ./.;

            preConfigure = ''
              autoreconf -vfi
            '';

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
              maintainers = [ "Olexander Sydorchuk <olexandr.syd@gmail.com>" ];
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
            xorg.libX11.dev
            xorg.libXft
            xorg.libXinerama
            xorg.libXpm
            xorg.libXcursor
            pkg

            valgrind

            xorg.xwd
            imagemagick
          ];

          shellHook = ''
            echo "!! Entering the dzen2 development shell"
          '';
        };
      }) systems);
    };
}
