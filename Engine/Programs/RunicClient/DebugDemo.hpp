#pragma once

#include "ExtractorRegistry.hpp"

// RunicClient's debug demo (GDB-2, slice 2's runnable result): a grid, colored shapes with opacity
// and text around the world's origin, written into each RenderFrame through the debug-draw API and
// drawn by Godot. From slice 5 it is one of the Presentation layer's extractors, so it draws
// relative to the frame's origin like any other. A neutral scene with no game content.

namespace Runic::Programs {

inline constexpr std::string_view kDebugDemoExtractorName = "engine:debug_demo";

class DebugDemoExtractor final : public Client::Extractor {
  public:
    void Extract(const Client::ExtractionView &view, RenderData::RenderFrame &frame) override;
};

} // namespace Runic::Programs
