CONT=$(diskutil info / | awk -F': *' '/Part of Whole/{print $2}')
PS=$(diskutil list "$CONT" | awk '/Physical Store/{print $NF; exit}')
WDEFI="$(echo "$PS" | sed 's/s[0-9]*$//')s1"
sudo diskutil mount "$WDEFI" >/dev/null
DST=$(diskutil info "$WDEFI" | awk -F': *' '/Mount Point/{print $2}')
SRC=""
for p in $(diskutil list external physical | awk '/EFI|Microsoft Basic Data/{print $NF}'); do
  sudo diskutil mount "$p" >/dev/null 2>&1
  mp=$(diskutil info "$p" | awk -F': *' '/Mount Point/{print $2}')
  [ -f "$mp/EFI/OC/OpenCore.efi" ] && SRC="$mp" && break
done
echo "來源USB=$SRC / 目標WD=$DST"
if [ -f "$SRC/EFI/OC/OpenCore.efi" ] && [ -n "$DST" ]; then
  sudo rm -rf "$DST/EFI"
  sudo /bin/cp -R "$SRC/EFI" "$DST/"
  sudo dot_clean "$DST" 2>/dev/null
  echo "完成，硬碟 EFI/OC:"; ls "$DST/EFI/OC"
else
  echo "找不到來源(USB OpenCore)或目標，停"
fi