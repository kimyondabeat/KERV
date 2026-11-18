# Build KERV for Mac (AU + VST3 + app)

Needs: a Mac with Xcode (free, App Store) and CMake (`brew install cmake`).

```
cmake -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64" -DCMAKE_OSX_DEPLOYMENT_TARGET=10.13
cmake --build build --config Release --parallel 4
bash mac/make_pkg.sh build/KERV_artefacts/Release
```

This makes `KERV_v1.0_Mac.pkg`, which installs:
- KERV.component  -> /Library/Audio/Plug-Ins/Components (Logic Pro, GarageBand, any AU host)
- KERV.vst3       -> /Library/Audio/Plug-Ins/VST3
- KERV.app        -> /Applications

Or push this folder to GitHub: `.github/workflows/build-mac.yml` builds the same .pkg
on GitHub's Mac machines (Actions tab > artifact "KERV-v1.0-Mac").
