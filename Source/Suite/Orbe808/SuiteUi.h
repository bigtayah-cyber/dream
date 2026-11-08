// Orbe 808 et Orbe Beat : alias vers le kit commun (Kit/Widgets.h).
#pragma once
#include <functional>
#include <juce_audio_utils/juce_audio_utils.h>
#include "../../Kit/Widgets.h"

namespace suiteui {

using kit::w::kLilac; using kit::w::kIce; using kit::w::kPeach;
using kit::w::noteName; using kit::w::DragHandle; using kit::w::rule; using kit::w::ledBar;
using Segmented = kit::w::Seg;
inline juce::String fr1(float v) { return kit::w::fr(v, 1); }

}  // namespace suiteui
