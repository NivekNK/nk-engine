{
  description = "Reproducible build and development environment for NK Engine";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";

    # Keep pure Nix builds independent from the state of the local Git
    # submodules. These revisions match the gitlinks in this repository.
    glm-src = {
      url = "github:g-truc/glm/0af55ccecd98d4e5a8d1fad7de25ba429d60e863";
      flake = false;
    };
    googletest-src = {
      url = "github:google/googletest/b514bdc898e2951020cbdca1304b75f5950d1f59";
      flake = false;
    };
    rapidhash-src = {
      # Tag: rapidhash_v3
      url = "github:Nicoshev/rapidhash/bc4b4baa48a15ff52ff4725e1ccdcda62815221c";
      flake = false;
    };
    tinyobjloader-src = {
      # Tag: v2.0.0rc13
      url = "github:tinyobjloader/tinyobjloader/2945a967c5303b2c8c14174117c45f3302591150";
      flake = false;
    };
    libspng-src = {
      # Tag: v0.7.4
      url = "github:randy408/libspng/fb768002d4288590083a476af628e51c3f1d47cd";
      flake = false;
    };
    zlib-src = {
      # Tag: v1.3.2
      url = "github:madler/zlib/da607da739fa6047df13e66a2af6b8bec7c2a498";
      flake = false;
    };
  };

  outputs =
    inputs@{ nixpkgs, self, ... }:
    let
      supportedSystems = [
        "x86_64-linux"
        "aarch64-linux"
      ];
      forAllSystems = nixpkgs.lib.genAttrs supportedSystems;

      perSystem =
        system:
        let
          pkgs = import nixpkgs { inherit system; };
          inherit (pkgs) lib;
          mesaArch = lib.head (lib.splitString "-" system);
          lavapipeIcd = "${pkgs.mesa}/share/vulkan/icd.d/lvp_icd.${mesaArch}.json";
          # Keep explicit Vulkan overrides authoritative. Mesa from the same
          # Nix closure avoids mixing host and Nix libc/driver dependencies.
          vulkanRuntimeEnv = ''
            if [[ -z "''${VK_DRIVER_FILES:-}" && -z "''${VK_ICD_FILENAMES:-}" ]]; then
              case "''${NK_VULKAN_DRIVER:-auto}" in
                auto)
                  export VK_ADD_DRIVER_FILES="${pkgs.mesa}/share/vulkan/icd.d''${VK_ADD_DRIVER_FILES:+:$VK_ADD_DRIVER_FILES}"
                  ;;
                software) export VK_DRIVER_FILES="${lavapipeIcd}" ;;
                system) ;;
                *) echo "NK_VULKAN_DRIVER must be auto, software, or system." >&2; exit 2 ;;
              esac
            fi
            if [[ -d /run/opengl-driver/lib ]]; then
              export LD_LIBRARY_PATH="/run/opengl-driver/lib''${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
            fi
          '';

          nk-engine = pkgs.stdenv.mkDerivation {
            pname = "nk-engine";
            version = "0.1.0";
            src = self;

            nativeBuildInputs = with pkgs; [
              cmake
              makeWrapper
              ninja
              pkg-config
              shader-slang
              wayland-protocols
              wayland-scanner
            ];

            buildInputs = with pkgs; [
              libffi
              libxkbcommon
              libxcb
              libxau
              libxdmcp
              vulkan-headers
              vulkan-loader
              libxcb-keysyms
              wayland
              xorgproto
            ];

            # Local path flakes do not copy Git submodule contents by default.
            # Populate the exact revisions in the isolated build tree.
            postPatch = ''
              rm -rf engine/vendor/glm
              rm -rf engine/vendor/libspng
              rm -rf engine/vendor/rapidhash
              rm -rf engine/vendor/tinyobjloader
              rm -rf engine/vendor/zlib
              rm -rf tests/vendor/googletest

              mkdir -p engine/vendor tests/vendor
              cp -R ${inputs."glm-src"} engine/vendor/glm
              cp -R ${inputs."libspng-src"} engine/vendor/libspng
              cp -R ${inputs."rapidhash-src"} engine/vendor/rapidhash
              cp -R ${inputs."tinyobjloader-src"} engine/vendor/tinyobjloader
              cp -R ${inputs."zlib-src"} engine/vendor/zlib
              cp -R ${inputs."googletest-src"} tests/vendor/googletest
              chmod -R u+w \
                engine/vendor/glm \
                engine/vendor/libspng \
                engine/vendor/rapidhash \
                engine/vendor/tinyobjloader \
                engine/vendor/zlib \
                tests/vendor/googletest
            '';

            preConfigure = ''
              export NK_ENGINE_SOURCE_ROOT="$PWD"
            '';

            cmakeBuildType = "Release";
            cmakeFlags = [
              "-GNinja"
              "-DCMAKE_POLICY_VERSION_MINIMUM=3.5"
              "-DSLANGC=${lib.getExe' pkgs.shader-slang "slangc"}"
              "-DGLM_BUILD_TESTS=OFF"
              "-DGLM_BUILD_INSTALL=OFF"
              "-DBUILD_GMOCK=OFF"
              "-DINSTALL_GTEST=OFF"
            ];

            # The distributable package is the editor. Tests remain available
            # from the development build without blocking this target.
            buildPhase = ''
              runHook preBuild
              cmake --build . --target editor --parallel "$NIX_BUILD_CORES"
              runHook postBuild
            '';

            # The upstream CMake project does not currently define install()
            # rules, so install the editor and its generated assets explicitly.
            installPhase = ''
              runHook preInstall

              install -Dm755 \
                "$NK_ENGINE_SOURCE_ROOT/bin/Linux-Release/editor" \
                "$out/libexec/nk-engine/editor"
              mkdir -p "$out/share/nk-engine"
              cp -R \
                "$NK_ENGINE_SOURCE_ROOT/bin/Linux-Release/assets" \
                "$out/share/nk-engine/assets"

              makeWrapper \
                "$out/libexec/nk-engine/editor" \
                "$out/bin/nk-editor" \
                --chdir "$out/share/nk-engine" \
                --run ${lib.escapeShellArg vulkanRuntimeEnv}

              runHook postInstall
            '';

            meta = {
              description = "C++ game engine built with Vulkan";
              homepage = "https://github.com/NivekNK/nk-engine";
              license = lib.licenses.mit;
              mainProgram = "nk-editor";
              platforms = supportedSystems;
            };
          };

          build-command = pkgs.writeShellApplication {
            name = "nk-build";
            runtimeInputs = with pkgs; [
              cmake
              coreutils
              ninja
              pkg-config
              shader-slang
              stdenv.cc
              libffi
              wayland
              wayland-protocols
              wayland-scanner
              libxkbcommon
            ];
            text = ''
              project_root="''${NK_ENGINE_ROOT:-$PWD}"
              while [[ "$project_root" != "/" && ! -f "$project_root/CMakeLists.txt" ]]; do
                project_root="$(dirname -- "$project_root")"
              done

              if [[ ! -f "$project_root/CMakeLists.txt" || ! -f "$project_root/.scripts/build.sh" ]]; then
                echo "nk-build: run this command inside the NK Engine repository." >&2
                exit 1
              fi

              export CC="${pkgs.stdenv.cc}/bin/cc"
              export CXX="${pkgs.stdenv.cc}/bin/c++"
              export SLANGC="${lib.getExe' pkgs.shader-slang "slangc"}"
              export Vulkan_INCLUDE_DIR="${lib.getDev pkgs.vulkan-headers}/include"
              export Vulkan_LIBRARY="${lib.getLib pkgs.vulkan-loader}/lib/libvulkan.so"
              export CMAKE_PREFIX_PATH="${pkgs.xorgproto}''${CMAKE_PREFIX_PATH:+:$CMAKE_PREFIX_PATH}"
              export PKG_CONFIG_PATH="${
                lib.makeSearchPath "lib/pkgconfig" [
                  (lib.getDev pkgs.libxcb)
                  (lib.getDev pkgs.libxcb-keysyms)
                  (lib.getDev pkgs.libxau)
                  (lib.getDev pkgs.libxdmcp)
                  (lib.getDev pkgs.libffi)
                  (lib.getDev pkgs.libxkbcommon)
                  (lib.getDev pkgs.wayland)
                ]
              }:${
                lib.makeSearchPath "share/pkgconfig" [
                  pkgs.wayland-protocols
                ]
              }''${PKG_CONFIG_PATH:+:$PKG_CONFIG_PATH}"

              exec ${lib.getExe pkgs.bashNonInteractive} "$project_root/.scripts/build.sh" "$@"
            '';
          };

          run-command = pkgs.writeShellApplication {
            name = "nk-run";
            runtimeInputs = with pkgs; [
              coreutils
            ];
            text = ''
              project_root="''${NK_ENGINE_ROOT:-$PWD}"
              while [[ "$project_root" != "/" && ! -f "$project_root/CMakeLists.txt" ]]; do
                project_root="$(dirname -- "$project_root")"
              done

              if [[ ! -f "$project_root/.scripts/run.sh" ]]; then
                echo "nk-run: run this command inside the NK Engine repository." >&2
                exit 1
              fi

              export VK_LAYER_PATH="${pkgs.vulkan-validation-layers}/share/vulkan/explicit_layer.d"
              ${vulkanRuntimeEnv}

              exec ${lib.getExe pkgs.bashNonInteractive} "$project_root/.scripts/run.sh" "$@"
            '';
          };
        in
        {
          inherit
            nk-engine
            build-command
            lavapipeIcd
            vulkanRuntimeEnv
            run-command
            pkgs
            ;
        };
    in
    {
      packages = forAllSystems (
        system:
        let
          project = perSystem system;
        in
        {
          default = project.nk-engine;
          inherit (project) nk-engine;
        }
      );

      apps = forAllSystems (
        system:
        let
          project = perSystem system;
        in
        {
          default = {
            type = "app";
            program = "${project.nk-engine}/bin/nk-editor";
            meta.description = "Build and run the packaged NK Engine editor";
          };
          build = {
            type = "app";
            program = "${project.build-command}/bin/nk-build";
            meta.description = "Configure and build NK Engine in the working tree";
          };
          run = {
            type = "app";
            program = "${project.run-command}/bin/nk-run";
            meta.description = "Run an existing local NK Engine build";
          };
          editor = {
            type = "app";
            program = "${project.nk-engine}/bin/nk-editor";
            meta.description = "Build and run the packaged NK Engine editor";
          };
        }
      );

      devShells = forAllSystems (
        system:
        let
          project = perSystem system;
          inherit (project) pkgs vulkanRuntimeEnv;
          inherit (pkgs) lib;
        in
        {
          default = pkgs.mkShell {
            packages = with pkgs; [
              bashInteractive
              cmake
              gdb
              git
              ninja
              pkg-config
              shader-slang
              vulkan-tools
              wayland-protocols
              wayland-scanner
            ];
            buildInputs = with pkgs; [
              libffi
              libxkbcommon
              libxcb
              libxau
              libxdmcp
              vulkan-headers
              vulkan-loader
              vulkan-validation-layers
              libxcb-keysyms
              wayland
              xorgproto
            ];

            shellHook = ''
              export SLANGC="${lib.getExe' pkgs.shader-slang "slangc"}"
              export Vulkan_INCLUDE_DIR="${lib.getDev pkgs.vulkan-headers}/include"
              export Vulkan_LIBRARY="${lib.getLib pkgs.vulkan-loader}/lib/libvulkan.so"
              export VK_LAYER_PATH="${pkgs.vulkan-validation-layers}/share/vulkan/explicit_layer.d"
              ${vulkanRuntimeEnv}

              echo "NK Engine development shell"
              echo "  Build: .scripts/build.sh [Debug|RelWithDebInfo|Release]"
              echo "  Run:   .scripts/run.sh   [Debug|RelWithDebInfo|Release]"
            '';
          };
        }
      );

      formatter = forAllSystems (system: (perSystem system).pkgs.nixfmt);
    };
}
