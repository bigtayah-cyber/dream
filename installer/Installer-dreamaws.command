#!/bin/bash
# Installe les plugins dreamaws (VST3 + AU) pour l'utilisateur courant, puis lève la quarantaine macOS.
cd "$(dirname "$0")" || exit 1
VST="$HOME/Library/Audio/Plug-Ins/VST3"
AU="$HOME/Library/Audio/Plug-Ins/Components"
mkdir -p "$VST" "$AU"
for B in *.vst3 *.component; do
  [ -d "$B" ] || continue
  case "$B" in
    *.vst3) D="$VST" ;;
    *) D="$AU" ;;
  esac
  rm -rf "$D/$B" && cp -R "$B" "$D/" && xattr -cr "$D/$B" && echo "OK  $B"
done
echo
echo "Installation terminée. Dans FL Studio : Options > Manage plugins > Find installed plugins."
read -n 1 -s -r -p "Appuie sur une touche pour fermer."
