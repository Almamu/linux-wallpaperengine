{
  description = "linux-wallpaperengine development shell";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
  };

  outputs = { self, nixpkgs }:
    let
      system = "x86_64-linux";
      pkgs = import nixpkgs { inherit system; };

      runtimeLibs = with pkgs; [
        # Core graphics
        libGL
        libGLU
        glew
        freeglut
        glfw

        # X11
        libx11
        libxrandr
        libxinerama
        libxcursor
        libxi
        libxcomposite
        libxdamage
        libxext
        libxxf86vm
        libxcb

        # Wayland
        wayland
        libxkbcommon
        egl-wayland

        # Audio
        SDL2
        libpulseaudio

        # Video / codecs
        ffmpeg
        mpv

        # Compression
        lz4
        zlib

        # Text rendering
        freetype

        # IPC
        dbus

        # CEF runtime dependencies
        cups
        at-spi2-core
        nss
        nspr
        glib
        pango
        cairo
        gtk3
        libdrm
        mesa
        alsa-lib
        expat
        systemd # libudev
        atk
      ];
    in
    {
      devShells.${system}.default = pkgs.mkShell {
        nativeBuildInputs = with pkgs; [
          cmake
          pkg-config
          gcc
          gnumake
          wayland-scanner
        ];

        buildInputs = runtimeLibs ++ (with pkgs; [
          glm
          wayland-protocols
          fftw
        ]);

        LD_LIBRARY_PATH = pkgs.lib.makeLibraryPath runtimeLibs;

        shellHook = ''
          echo "linux-wallpaperengine dev shell"
          echo "Build with:"
          echo "  cmake -B build -DCMAKE_BUILD_TYPE=Release -Wno-dev -DCMAKE_EXE_LINKER_FLAGS=-Wl,--allow-shlib-undefined -DCMAKE_SHARED_LINKER_FLAGS=-Wl,--allow-shlib-undefined"
          echo "  cmake --build build -j$(nproc)"
        '';
      };
    };
}
