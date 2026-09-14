# Editor boundary

Future editor/debug tooling can consume the optional `Runic::Presentation`
dependencies. Editor code must never become a prerequisite for a headless
simulation. No renderer abstraction, ImGui backend, editor layer, or panels exist yet.
