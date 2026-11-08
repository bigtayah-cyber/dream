# Regenere Source/Kit/FontsData.h a partir des TTF de ce dossier (polices OFL, voir OFL-*.txt).
# Usage : python3 assets/fonts/gen_fonts_h.py
import os
here = os.path.dirname(os.path.abspath(__file__))
fonts = [("sansRegular", "IBMPlexSansCondensed-Regular.ttf"), ("sansSemiBold", "IBMPlexSansCondensed-SemiBold.ttf"),
         ("monoMedium", "IBMPlexMono-Medium.ttf"), ("lcd", "ShareTechMono-Regular.ttf"), ("seg7", "DSEG7Classic-Regular.ttf")]
out = ["// Fichier genere par assets/fonts/gen_fonts_h.py : ne pas modifier a la main.",
       "// Polices sous licence SIL Open Font License 1.1 (textes dans assets/fonts/OFL-*.txt).",
       "#pragma once", "#include <cstddef>", "namespace kit { namespace fontdata {"]
for name, fn in fonts:
    data = open(os.path.join(here, fn), "rb").read()
    out.append("// %s (%d octets)" % (fn, len(data)))
    out.append("inline constexpr unsigned char %s[] = {" % name)
    for i in range(0, len(data), 40):
        out.append(",".join(str(b) for b in data[i:i + 40]) + ",")
    out.append("};")
    out.append("inline constexpr std::size_t %sSize = sizeof(%s);" % (name, name))
out.append("} }  // namespace kit::fontdata")
open(os.path.join(here, "..", "..", "Source", "Kit", "FontsData.h"), "w").write("\n".join(out) + "\n")
