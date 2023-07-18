The flatpak we publish to flathub tracks the latest stable release.

You can also find here two more flatpaks, which the advanced user can use as they wish:

## Extra flatpak: Latest nightly

The following flatpak config builds from source:

```yml
```

## Extra flatpak: Built directly from source

The following flatpak config builds from source:

```yml
app-id: com.zquestclassic.ZQuest
runtime: org.freedesktop.Platform
runtime-version: '22.08'
sdk: org.freedesktop.Sdk
sdk-extensions: 
  - org.freedesktop.Sdk.Extension.llvm16
finish-args:
  - --device=all
  - --share=network
  - --share=ipc
  - --socket=x11
  - --socket=pulseaudio
cleanup:
  - /include
  - "*.a"
  - /lib/cmake
  - /lib/libpng
  - /lib/pkgconfig
command: zlauncher
rename-icon: zquest
modules:
  - name: zquest
    buildsystem: cmake-ninja
    config-opts:
      - -DCMAKE_C_COMPILER=clang
      - -DCMAKE_CXX_COMPILER=clang++
      - -DWANT_OPUS=OFF
      - -DWANT_OPENAL=OFF
      # libjpeg.so.62 => not found
      - -DWANT_JPEG=OFF
    build-options:
        append-path: /usr/lib/sdk/llvm16/bin
        prepend-ld-library-path: /usr/lib/sdk/llvm16/lib
        build-args:
          - --share=network
    post-install:
      - install -Dm644 com.zquestclassic.ZQuest.appdata.xml -t ${FLATPAK_DEST}/share/metainfo/
      - install -Dm644 com.zquestclassic.ZQuest.desktop -t ${FLATPAK_DEST}/share/applications/
      - icon_in="resources/zc.png";
        icon_out="zquest.png";
        for s in {32,64,128,192,256,512}; do
        convert "${icon_in}" -resize "${s}" "${icon_out}";
        install -p -Dm644 "${icon_out}" -t "${FLATPAK_DEST}/share/icons/hicolor/${s}x${s}/apps/";
        done;
      - find -L . -perm /a+x -type f -maxdepth 1 -name "*.so*" -exec cp -a {} ${FLATPAK_DEST}/lib/ \;
      - find -L . -perm /a+x -type f -maxdepth 1 -not -name "*.so*" -exec cp -a {} ${FLATPAK_DEST}/bin/ \;
    sources:
      - type: git
        url: https://github.com/ArmageddonGames/ZQuestClassic
        tag: 2.55-alpha-115
        commit: 812da2517f688635a4f5c440e22d037504535c85
        x-checker-data:
          type: git
          tag-pattern: ^2.55-.*(\d+)$
          sort-tags: false
      - type: file
        path: com.zquestclassic.ZQuest.appdata.xml
      - type: file
        path: com.zquestclassic.ZQuest.desktop
      - type: shell
        commands:
          - cp src/metadata/devsig.h.sig src/metadata/sigs/
          - cp src/metadata/compilersig.h.sig src/metadata/sigs/
    modules:
      - name: ImageMagick
        config-opts:
          - --disable-static
          - --disable-docs
          - --with-hdri
          - --with-pic
        sources:
          - type: archive
            url: https://github.com/ImageMagick/ImageMagick/archive/7.0.8-65.tar.gz
            sha256: 14afaf722d8964ed8de2ebd8184a229e521f1425e18e7274806f06e008bf9aa7
        cleanup:
          - '*'

```