#pragma once

#include "types.h"

#include <glm/glm.hpp>

#include <stb_truetype.h>

#include <filesystem>
#include <string_view>
#include <unordered_map>
#include <vector>

class TextMeshGenerator {
public:
  explicit TextMeshGenerator(const std::filesystem::path &fontPath);

  TextMeshGenerator(const TextMeshGenerator &) = delete;
  TextMeshGenerator &operator=(const TextMeshGenerator &) = delete;
  TextMeshGenerator(TextMeshGenerator &&) = delete;
  TextMeshGenerator &operator=(TextMeshGenerator &&) = delete;

  // Generates text in the XY plane, facing +Z.
  //
  // height:
  //   Approximate world-space font height.
  //
  // baseline:
  //   The first line's baseline is y = 0.
  //
  // Limitations:
  //   ASCII only for this initial implementation.
  //   Throws if the result exceeds uint16_t index capacity.
  Mesh generateMesh(std::string_view text, float height, glm::vec3 color);

private:
  struct CpuGlyphMesh {
    std::vector<glm::vec2> vertices;
    std::vector<uint32_t> indices;

    float advance = 0.0f;
  };

  const CpuGlyphMesh &getGlyph(int codepoint);
  CpuGlyphMesh createGlyph(int codepoint) const;

  std::vector<unsigned char> fontBytes_;
  stbtt_fontinfo fontInfo_{};

  std::unordered_map<int, CpuGlyphMesh> glyphCache_;
};
