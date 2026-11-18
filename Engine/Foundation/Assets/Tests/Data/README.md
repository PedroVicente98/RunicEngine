# Runic::Assets › Tests/Data

Files the format tests read.

- `golden_square.rmesh`: a cooked mesh (a unit square of two triangles facing up, with its normals), written once by
  `WriteCookedMesh` when ASSET-1 defined format version 1. The test reads it and checks the writer still writes the same
  bytes: a change to the format that would make old cooked files unreadable fails it. A new format version gets its
  own golden file; this one stays, read by the reader for version 1 for as long as the Engine reads version 1.
