#!/bin/bash
# Create DMG Installer
mkdir -p dist/dmg_root
ln -s /Applications dist/dmg_root/Applications
cp -R "dist/Mix Analyzer.app" dist/dmg_root/
hdiutil create -volname "Funky Moose Mix Analyzer Pro" -srcfolder dist/dmg_root -ov -format UDZO "dist/Funky_Moose_Mix_Analyzer_Pro.dmg"
rm -rf dist/dmg_root
