#!/bin/bash
# Builds KERV_v1.0_Mac.pkg: AU -> /Library/Audio/Plug-Ins/Components, VST3 -> /Library/Audio/Plug-Ins/VST3, app -> /Applications
set -e
A="$1"
W=$(mktemp -d)
mkdir -p "$W/au" "$W/vst3" "$W/app"
cp -R "$A/AU/KERV.component" "$W/au/"
cp -R "$A/VST3/KERV.vst3" "$W/vst3/"
cp -R "$A/Standalone/KERV.app" "$W/app/"
pkgbuild --root "$W/au"   --install-location "/Library/Audio/Plug-Ins/Components" --identifier com.kimyondabeat.kerv.au   --version 1.0.0 "$W/au.pkg"
pkgbuild --root "$W/vst3" --install-location "/Library/Audio/Plug-Ins/VST3"       --identifier com.kimyondabeat.kerv.vst3 --version 1.0.0 "$W/vst3.pkg"
pkgbuild --root "$W/app"  --install-location "/Applications"                      --identifier com.kimyondabeat.kerv.app  --version 1.0.0 "$W/app.pkg"
cat > "$W/dist.xml" <<XML
<?xml version="1.0" encoding="utf-8"?>
<installer-gui-script minSpecVersion="2">
  <title>KERV - Kompa Drum Engine v1.0</title>
  <options customize="allow" require-scripts="false" hostArchitectures="arm64,x86_64"/>
  <choices-outline>
    <line choice="au"/><line choice="vst3"/><line choice="app"/>
  </choices-outline>
  <choice id="au"   title="KERV Audio Unit (Logic, GarageBand...)"><pkg-ref id="com.kimyondabeat.kerv.au"/></choice>
  <choice id="vst3" title="KERV VST3 plugin"><pkg-ref id="com.kimyondabeat.kerv.vst3"/></choice>
  <choice id="app"  title="KERV app"><pkg-ref id="com.kimyondabeat.kerv.app"/></choice>
  <pkg-ref id="com.kimyondabeat.kerv.au">au.pkg</pkg-ref>
  <pkg-ref id="com.kimyondabeat.kerv.vst3">vst3.pkg</pkg-ref>
  <pkg-ref id="com.kimyondabeat.kerv.app">app.pkg</pkg-ref>
</installer-gui-script>
XML
productbuild --distribution "$W/dist.xml" --package-path "$W" KERV_v1.0_Mac.pkg
echo "Built KERV_v1.0_Mac.pkg"
