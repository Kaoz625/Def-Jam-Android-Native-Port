#!/usr/bin/env bash
# [android] Build the Def Jam Android APK (arm64-v8a) on a Linux box from this
# repository and the player's own USA default.xbe. The bash counterpart of
# android/Preparar-Dependencias-Android.ps1 + android/Compilar-DefJam-Android.ps1,
# written for the Dell's WSL distro "Blazing" (Ubuntu 24.04), usable on any
# x86-64 Linux with python3.12, git, curl, unzip.
#
#   bash android/dell-build.sh [stage...]
#
# Stages (default: all, in this order; each skips what is already done):
#   tools   JDK 17, Gradle 8.7, Android SDK 34 / build-tools 34.0.0,
#           NDK $NDK_VERSION and CMake $CMAKE_VERSION under $W/tools
#   src     clone/update this repo's branch into $W/src   (REPO, BRANCH)
#   deps    android/vendor: SDL $SDL_TAG and Dear ImGui v1.92.9b sources, and
#           shaderc built for arm64 from the NDK's own sources (ndk-build)
#   gen     the lifted game code: scripts/pipeline-state.py analyze + lift on
#           $XBE (the player's own USA default.xbe; never committed)
#   patch   contatg's generated-code patches that still apply (see below)
#   build   gradle assembleRelease (signed with the debug keystore kept in
#           $W/keys/android-user, see ANDROID_USER_HOME)
#   check   aapt2 badging, apksigner verify, readelf of every .so; copies the
#           APK to $W/out/defjam-android-<rev>.apk with its .sha256
#
# Environment: W (/root/defjam-android), XBE (default $W/xbe/default.xbe),
# REPO, BRANCH, SRC (use this checkout instead of cloning), NATIVE_JOBS
# (parallel C/C++ compiles inside Gradle's CMake build, default 6: the
# lifted files take GBs of RAM each and the Dell has 15 GB).
set -euo pipefail

W=${W:-/root/defjam-android}
REPO=${REPO:-https://github.com/Kaoz625/Def-Jam-Android-Native-Port.git}
BRANCH=${BRANCH:-blz/android-1.1.2}
SRC=${SRC:-$W/src}
XBE=${XBE:-$W/xbe/default.xbe}
NATIVE_JOBS=${NATIVE_JOBS:-6}
T=$W/tools
JDK=$T/jdk-17
GRADLE_VERSION=8.7
GRADLE=$T/gradle-$GRADLE_VERSION/bin/gradle
SDK=$T/android-sdk
NDK_VERSION=${NDK_VERSION:-28.2.13676358}
CMAKE_VERSION=${CMAKE_VERSION:-3.31.6}
BUILD_TOOLS=34.0.0
SDL_TAG=${SDL_TAG:-release-3.4.16}
IMGUI_VERSION=1.92.9b
IMGUI_SHA256=21d8a0a565e85dce943e375db00812c2f3f0ab21f3f0f7964e364a63422d7f99

export JAVA_HOME=$JDK
export ANDROID_HOME=$SDK ANDROID_SDK_ROOT=$SDK
# The debug keystore Gradle signs release with (android/app/build.gradle) lives
# here instead of ~/.android, so this build has its own key.
export ANDROID_USER_HOME=$W/keys/android-user
export PATH=$JDK/bin:$PATH

log() { echo "[$(date +%H:%M:%S)] $*"; }
mkdir -p "$W" "$W/logs" "$W/dl" "$T" "$W/keys" "$W/out"

fetch() {   # url file
    [[ -s $2 ]] && return 0
    curl -fL --retry 3 -o "$2.part" "$1" && mv "$2.part" "$2"
}

stage_tools() {
    if [[ ! -x $JDK/bin/java ]]; then
        log "tools: Temurin JDK 17"
        fetch "https://api.adoptium.net/v3/binary/latest/17/ga/linux/x64/jdk/hotspot/normal/eclipse" "$W/dl/jdk17.tar.gz"
        rm -rf "$JDK" && mkdir -p "$JDK" && tar -xzf "$W/dl/jdk17.tar.gz" -C "$JDK" --strip-components=1
    fi
    if [[ ! -x $GRADLE ]]; then
        log "tools: Gradle $GRADLE_VERSION"
        fetch "https://services.gradle.org/distributions/gradle-$GRADLE_VERSION-bin.zip" "$W/dl/gradle.zip"
        (cd "$T" && unzip -q -o "$W/dl/gradle.zip")
    fi
    local sdkm=$SDK/cmdline-tools/latest/bin/sdkmanager
    if [[ ! -x $sdkm ]]; then
        log "tools: Android command-line tools"
        fetch "https://dl.google.com/android/repository/commandlinetools-linux-11076708_latest.zip" "$W/dl/cmdline-tools.zip"
        rm -rf "$SDK/cmdline-tools" && mkdir -p "$SDK/cmdline-tools"
        (cd "$SDK/cmdline-tools" && unzip -q "$W/dl/cmdline-tools.zip" && mv cmdline-tools latest)
    fi
    local want=("platforms;android-34" "build-tools;$BUILD_TOOLS" "ndk;$NDK_VERSION" "cmake;$CMAKE_VERSION")
    local missing=()
    [[ -d $SDK/platforms/android-34 ]] || missing+=("${want[0]}")
    [[ -x $SDK/build-tools/$BUILD_TOOLS/apksigner ]] || missing+=("${want[1]}")
    [[ -f $SDK/ndk/$NDK_VERSION/source.properties ]] || missing+=("${want[2]}")
    [[ -x $SDK/cmake/$CMAKE_VERSION/bin/cmake ]] || missing+=("${want[3]}")
    if [[ ${#missing[@]} -gt 0 ]]; then
        log "tools: sdkmanager ${missing[*]}"
        yes | "$sdkm" --sdk_root="$SDK" --licenses > "$W/logs/sdk-licenses.log" 2>&1 || true
        "$sdkm" --sdk_root="$SDK" "${missing[@]}" > "$W/logs/sdkmanager.log" 2>&1 ||
            { tail -30 "$W/logs/sdkmanager.log"; exit 1; }
    fi
    log "tools: $("$JDK/bin/java" -version 2>&1 | head -1); gradle $GRADLE_VERSION; NDK $(grep Pkg.Revision "$SDK/ndk/$NDK_VERSION/source.properties" | cut -d= -f2 | tr -d ' '); cmake $CMAKE_VERSION"
}

stage_src() {
    if [[ $SRC != "$W/src" ]]; then log "src: using $SRC as it is"; return; fi
    if [[ ! -d $SRC/.git ]]; then
        log "src: cloning $REPO $BRANCH"
        git clone --branch "$BRANCH" "$REPO" "$SRC"
    else
        log "src: updating to origin/$BRANCH"
        git -C "$SRC" fetch origin "$BRANCH"
        git -C "$SRC" checkout -q -f -B "$BRANCH" "origin/$BRANCH"   # tracked files only; gen and vendor stay
    fi
    log "src: $(git -C "$SRC" log --oneline -1)"
}

stage_deps() {
    local v=$SRC/android/vendor
    mkdir -p "$v"
    if [[ ! -f $v/SDL/CMakeLists.txt ]]; then
        log "deps: SDL $SDL_TAG"
        fetch "https://github.com/libsdl-org/SDL/archive/refs/tags/$SDL_TAG.tar.gz" "$W/dl/SDL-$SDL_TAG.tar.gz"
        rm -rf "$v/SDL" && mkdir -p "$v/SDL" && tar -xzf "$W/dl/SDL-$SDL_TAG.tar.gz" -C "$v/SDL" --strip-components=1
    fi
    if [[ ! -f $v/imgui/imgui.cpp ]]; then
        log "deps: Dear ImGui v$IMGUI_VERSION"
        fetch "https://github.com/ocornut/imgui/archive/refs/tags/v$IMGUI_VERSION.tar.gz" "$W/dl/imgui-$IMGUI_VERSION.tar.gz"
        echo "$IMGUI_SHA256  $W/dl/imgui-$IMGUI_VERSION.tar.gz" | sha256sum -c --strict --quiet
        rm -rf "$v/imgui" && mkdir -p "$v/imgui" && tar -xzf "$W/dl/imgui-$IMGUI_VERSION.tar.gz" -C "$v/imgui" --strip-components=1
    fi
    local lib=$v/shaderc/lib/arm64-v8a/libshaderc.a ndk=$SDK/ndk/$NDK_VERSION
    if [[ ! -f $lib ]]; then
        local ss=$ndk/sources/third_party/shaderc
        [[ -f $ss/Android.mk ]] || { echo "NDK $NDK_VERSION has no sources/third_party/shaderc" >&2; exit 1; }
        # Built in a copy: the NDK itself is left as installed.
        log "deps: shaderc arm64 from NDK $NDK_VERSION (ndk-build, a few minutes)"
        rm -rf "$W/shaderc-build" && cp -r "$ndk/sources/third_party" "$W/shaderc-build"
        (cd "$W/shaderc-build/shaderc" && "$ndk/ndk-build" NDK_PROJECT_PATH=. APP_BUILD_SCRIPT=Android.mk \
            "APP_STL:=c++_shared" APP_ABI=arm64-v8a APP_PLATFORM=android-29 libshaderc_combined -j"$(nproc)") \
            > "$W/logs/shaderc.log" 2>&1 || { tail -30 "$W/logs/shaderc.log"; exit 1; }
        mkdir -p "$(dirname "$lib")" "$v/shaderc/include"
        cp "$W/shaderc-build/shaderc/libs/c++_shared/arm64-v8a/libshaderc.a" "$lib"
        cp -r "$W/shaderc-build/shaderc/include/shaderc" "$v/shaderc/include/"
    fi
    printf '%s' "$NDK_VERSION" > "$v/.ndk-version"
    log "deps: SDL, ImGui, shaderc ready (NDK $NDK_VERSION)"
}

stage_gen() {
    local tk=$SRC/tools/xboxrecomp venv=$W/venv
    [[ -f $XBE ]] || { echo "no default.xbe at $XBE (the player's own USA dump)" >&2; exit 2; }
    echo "31cc0d11f7c656b1a6f337fdf7d9bbaac9df11c324a2fb881f7eb639d4be00c2  $XBE" | sha256sum -c --strict --quiet ||
        { echo "$XBE is not the USA default.xbe this port lifts" >&2; exit 2; }
    if [[ ! -x $venv/bin/python ]]; then
        log "gen: python venv (pyxbe, capstone)"
        python3.12 -m venv "$venv"
        "$venv/bin/pip" install -q pyxbe capstone
    fi
    if [[ -f $SRC/src/recomp/gen/recomp_dispatch.c && ${FORCE_GEN:-0} != 1 ]]; then
        log "gen: src/recomp/gen is there (FORCE_GEN=1 lifts again)"
        return
    fi
    mkdir -p "$tk/game_files"
    cp -f "$XBE" "$tk/game_files/default.xbe"
    log "gen: analyze (scripts/pipeline-state.py analyze)"
    (cd "$SRC" && "$venv/bin/python" scripts/pipeline-state.py analyze --toolkit "$tk") > "$W/logs/analyze.log" 2>&1 ||
        { tail -40 "$W/logs/analyze.log"; exit 1; }
    log "gen: lift (scripts/pipeline-state.py lift)"
    (cd "$SRC" && "$venv/bin/python" scripts/pipeline-state.py lift --toolkit "$tk" --split 1000 --) > "$W/logs/lift.log" 2>&1 ||
        { tail -40 "$W/logs/lift.log"; exit 1; }
    log "gen: $(ls "$SRC/src/recomp/gen" | wc -l) files in src/recomp/gen"
}

# contatg's 1.4 build ran five scripts over the generated C. On 1.1.2:
#  - android-subtitles37.py: replaced by upstream's sub_000DA8A0 wrapper in
#    src/recomp_manual.c (the script would refuse the _gen name); not run.
#  - android-rollback131.py: its five hooks are entry hooks / a wrapper in
#    src/recomp_manual.c now (lifter v0.16 `_enter`); not run.
#  - android-makespace-backoff.py, android-online-batch130.py,
#    android-native120140.py --remove: run when the generated function still
#    matches the body they were written for; a mismatch is reported and the
#    function is left as lifted (all three are performance/instrumentation).
stage_patch() {
    local gen=$SRC/src/recomp/gen py=$W/venv/bin/python s name extra
    for s in "android-makespace-backoff.py" "android-online-batch130.py" "android-native120140.py --remove"; do
        read -r name extra <<< "$s"
        if (cd "$SRC" && $py "scripts/$name" "$gen" $extra) > "$W/logs/patch-${name%.py}.log" 2>&1; then
            log "patch: $s: $(tail -1 "$W/logs/patch-${name%.py}.log")"
        else
            log "patch: $s NOT applied: $(tail -1 "$W/logs/patch-${name%.py}.log")"
        fi
    done
}

stage_build() {
    log "build: gradle assembleRelease (native jobs $NATIVE_JOBS)"
    mkdir -p "$ANDROID_USER_HOME"
    printf 'sdk.dir=%s\n' "$SDK" > "$SRC/android/local.properties"
    (cd "$SRC/android" && "$GRADLE" --no-daemon --console=plain \
        -Dorg.gradle.jvmargs="-Xmx3g -XX:MaxMetaspaceSize=512m -Dfile.encoding=UTF-8" \
        -PdefjamNdkVersion="$NDK_VERSION" -PdefjamNativeJobs="$NATIVE_JOBS" assembleRelease) \
        > "$W/logs/gradle.log" 2>&1 ||
        { grep -E "error:|FAILED|undefined|What went wrong" -A3 "$W/logs/gradle.log" | head -80; exit 1; }
    log "build: $(ls -la "$SRC/android/app/build/outputs/apk/release/"*.apk)"
}

stage_check() {
    local apk=$SRC/android/app/build/outputs/apk/release/app-release.apk bt=$SDK/build-tools/$BUILD_TOOLS
    local rev out x=$W/apk-x
    [[ -f $apk ]] || { echo "no APK at $apk" >&2; exit 1; }
    rev=$(git -C "$SRC" rev-parse --short HEAD)
    out=$W/out/defjam-android-$rev.apk
    cp -f "$apk" "$out"
    (cd "$W/out" && sha256sum "$(basename "$out")" > "$(basename "$out").sha256")
    {
        echo "== $out ($(du -h "$out" | cut -f1), $(stat -c %s "$out") bytes)"
        echo "== sha256 $(cut -d' ' -f1 "$out.sha256")"
        echo "== aapt2 dump badging"; "$bt/aapt2" dump badging "$out" | grep -E "^(package|sdkVersion|targetSdkVersion|native-code|application-label|launchable-activity)"
        echo "== apksigner verify"; "$bt/apksigner" verify --verbose --print-certs "$out" | grep -vE "^WARNING: META-INF"
        rm -rf "$x" && mkdir -p "$x" && (cd "$x" && unzip -q "$out" 'lib/*')
        echo "== native libraries"
        for so in "$x"/lib/*/*.so; do
            echo "-- ${so#$x/} $(stat -c %s "$so") bytes: $(readelf -h "$so" | awk -F: '/Machine/{gsub(/^ +/,"",$2);print $2}')"
            readelf -d "$so" | awk '/NEEDED/{print "   NEEDED " $NF}'
        done
    } | tee "$W/out/defjam-android-$rev.check.txt"
}

stages=("$@")
[[ ${#stages[@]} -gt 0 ]] || stages=(tools src deps gen patch build check)
for s in "${stages[@]}"; do "stage_$s"; done
