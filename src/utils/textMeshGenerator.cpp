#include "textMeshGenerator.h"

#include "generators.h"

#define STB_TRUETYPE_IMPLEMENTATION
#include <stb_truetype.h>

#include <tesselator.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>
#include <limits>
#include <memory>
#include <stdexcept>
#include <utility>

struct TessPoint {
  float x;
  float y;
};

using Contour = std::vector<TessPoint>;
using Contours = std::vector<Contour>;

glm::vec2 toGlm(TessPoint point) { return {point.x, point.y}; }

TessPoint toTess(glm::vec2 point) {
  return {
      .x = point.x,
      .y = point.y,
  };
}

bool nearlyEqual(glm::vec2 a, glm::vec2 b) {
  constexpr float epsilon = 0.001f;
  const glm::vec2 difference = a - b;

  return glm::dot(difference, difference) < epsilon * epsilon;
}

void appendPoint(Contour &contour, glm::vec2 point) {
  if (!contour.empty() && nearlyEqual(toGlm(contour.back()), point)) {
    return;
  }

  contour.push_back(toTess(point));
}

TextMeshGenerator::TextMeshGenerator(const std::filesystem::path &fontPath) {
  std::ifstream input{
      fontPath,
      std::ios::binary | std::ios::ate,
  };

  if (!input) {
    throw std::runtime_error("Could not open font: " + fontPath.string());
  }

  const std::streamsize byteCount = input.tellg();

  if (byteCount <= 0) {
    throw std::runtime_error("Font file is empty: " + fontPath.string());
  }

  input.seekg(0);

  fontBytes_.resize(static_cast<size_t>(byteCount));

  if (!input.read(reinterpret_cast<char *>(fontBytes_.data()), byteCount)) {
    throw std::runtime_error("Could not read font: " + fontPath.string());
  }

  const int fontOffset = stbtt_GetFontOffsetForIndex(fontBytes_.data(), 0);

  if (fontOffset < 0) {
    throw std::runtime_error("Font does not contain index 0");
  }

  if (stbtt_InitFont(&fontInfo_, fontBytes_.data(), fontOffset) == 0) {
    throw std::runtime_error("Could not initialize font");
  }
}

glm::vec2 evaluateQuadratic(glm::vec2 start, glm::vec2 control, glm::vec2 end,
                            float t) {
  const float inverseT = 1.0f - t;

  return inverseT * inverseT * start + 2.0f * inverseT * t * control +
         t * t * end;
}

void appendQuadratic(Contour &contour, glm::vec2 start, glm::vec2 control,
                     glm::vec2 end) {
  constexpr int subdivisions = 10;

  for (int i = 1; i <= subdivisions; ++i) {
    const float t = static_cast<float>(i) / static_cast<float>(subdivisions);

    appendPoint(contour, evaluateQuadratic(start, control, end, t));
  }
}

glm::vec2 evaluateCubic(glm::vec2 start, glm::vec2 control1, glm::vec2 control2,
                        glm::vec2 end, float t) {
  const float inverseT = 1.0f - t;

  return inverseT * inverseT * inverseT * start +
         3.0f * inverseT * inverseT * t * control1 +
         3.0f * inverseT * t * t * control2 + t * t * t * end;
}

void appendCubic(Contour &contour, glm::vec2 start, glm::vec2 control1,
                 glm::vec2 control2, glm::vec2 end) {
  constexpr int subdivisions = 12;

  for (int i = 1; i <= subdivisions; ++i) {
    const float t = static_cast<float>(i) / static_cast<float>(subdivisions);

    appendPoint(contour, evaluateCubic(start, control1, control2, end, t));
  }
}

Contours extractContours(const stbtt_fontinfo &font, int codepoint) {
  stbtt_vertex *commands = nullptr;

  const int commandCount = stbtt_GetCodepointShape(&font, codepoint, &commands);

  Contours contours;
  Contour currentContour;

  glm::vec2 currentPoint{};
  bool hasCurrentPoint = false;

  auto finishContour = [&] {
    if (currentContour.size() >= 2) {
      const glm::vec2 first = toGlm(currentContour.front());

      const glm::vec2 last = toGlm(currentContour.back());

      // libtess2 closes contours automatically.
      if (nearlyEqual(first, last)) {
        currentContour.pop_back();
      }
    }

    if (currentContour.size() >= 3) {
      contours.push_back(std::move(currentContour));
    }

    currentContour = {};
    hasCurrentPoint = false;
  };

  for (int i = 0; i < commandCount; ++i) {
    const stbtt_vertex &command = commands[i];

    const glm::vec2 end{
        static_cast<float>(command.x),
        static_cast<float>(command.y),
    };

    switch (command.type) {
    case STBTT_vmove:
      finishContour();

      currentPoint = end;
      hasCurrentPoint = true;
      appendPoint(currentContour, currentPoint);
      break;

    case STBTT_vline:
      if (!hasCurrentPoint) {
        break;
      }

      appendPoint(currentContour, end);
      currentPoint = end;
      break;

    case STBTT_vcurve: {
      if (!hasCurrentPoint) {
        break;
      }

      const glm::vec2 control{
          static_cast<float>(command.cx),
          static_cast<float>(command.cy),
      };

      appendQuadratic(currentContour, currentPoint, control, end);

      currentPoint = end;
      break;
    }

    case STBTT_vcubic: {
      if (!hasCurrentPoint) {
        break;
      }

      const glm::vec2 control1{
          static_cast<float>(command.cx),
          static_cast<float>(command.cy),
      };

      const glm::vec2 control2{
          static_cast<float>(command.cx1),
          static_cast<float>(command.cy1),
      };

      appendCubic(currentContour, currentPoint, control1, control2, end);

      currentPoint = end;
      break;
    }

    default:
      break;
    }
  }

  finishContour();

  if (commands != nullptr) {
    stbtt_FreeShape(&font, commands);
  }

  return contours;
}

TextMeshGenerator::CpuGlyphMesh
TextMeshGenerator::createGlyph(int codepoint) const {
  CpuGlyphMesh result{};

  int advance = 0;
  int leftSideBearing = 0;

  stbtt_GetCodepointHMetrics(&fontInfo_, codepoint, &advance, &leftSideBearing);

  result.advance = static_cast<float>(advance);

  Contours contours = extractContours(fontInfo_, codepoint);

  // Spaces and some control characters have metrics but no shape.
  if (contours.empty()) {
    return result;
  }

  using TessellatorPointer =
      std::unique_ptr<TESStesselator, decltype(&tessDeleteTess)>;

  TessellatorPointer tessellator{
      tessNewTess(nullptr),
      &tessDeleteTess,
  };

  if (!tessellator) {
    throw std::runtime_error("Could not create libtess2 tessellator");
  }

  for (const Contour &contour : contours) {
    tessAddContour(tessellator.get(), 2, contour.data(), sizeof(TessPoint),
                   static_cast<int>(contour.size()));
  }

  const int succeeded = tessTesselate(tessellator.get(), TESS_WINDING_NONZERO,
                                      TESS_POLYGONS, 3, 2, nullptr);

  if (succeeded == 0) {
    throw std::runtime_error("Could not tessellate codepoint " +
                             std::to_string(codepoint));
  }

  const int tessVertexCount = tessGetVertexCount(tessellator.get());

  const TESSreal *tessVertices = tessGetVertices(tessellator.get());

  result.vertices.reserve(static_cast<size_t>(tessVertexCount));

  for (int i = 0; i < tessVertexCount; ++i) {
    result.vertices.push_back(glm::vec2{
        static_cast<float>(tessVertices[i * 2]),
        static_cast<float>(tessVertices[i * 2 + 1]),
    });
  }

  const int triangleCount = tessGetElementCount(tessellator.get());

  const TESSindex *elements = tessGetElements(tessellator.get());

  result.indices.reserve(static_cast<size_t>(triangleCount) * 3);

  for (int triangle = 0; triangle < triangleCount; ++triangle) {
    const TESSindex *triangleIndices = elements + triangle * 3;

    if (triangleIndices[0] == TESS_UNDEF || triangleIndices[1] == TESS_UNDEF ||
        triangleIndices[2] == TESS_UNDEF) {
      continue;
    }

    result.indices.push_back(static_cast<uint32_t>(triangleIndices[0]));

    result.indices.push_back(static_cast<uint32_t>(triangleIndices[1]));

    result.indices.push_back(static_cast<uint32_t>(triangleIndices[2]));
  }

  return result;
}

const TextMeshGenerator::CpuGlyphMesh &
TextMeshGenerator::getGlyph(int codepoint) {
  const auto existing = glyphCache_.find(codepoint);

  if (existing != glyphCache_.end()) {
    return existing->second;
  }

  auto [inserted, wasInserted] =
      glyphCache_.emplace(codepoint, createGlyph(codepoint));

  return inserted->second;
}

Mesh TextMeshGenerator::generateMesh(std::string_view text, float height,
                                     glm::vec3 color) {
  if (text.empty()) {
    throw std::invalid_argument("Cannot generate a mesh from empty text");
  }

  if (height <= 0.0f) {
    throw std::invalid_argument("Text height must be greater than zero");
  }

  const float scale = stbtt_ScaleForPixelHeight(&fontInfo_, height);

  int ascent = 0;
  int descent = 0;
  int lineGap = 0;

  stbtt_GetFontVMetrics(&fontInfo_, &ascent, &descent, &lineGap);

  const float lineAdvance =
      static_cast<float>(ascent - descent + lineGap) * scale;

  std::vector<Vertex> vertices;
  std::vector<uint32_t> indices;

  glm::vec2 cursor{0.0f, 0.0f};

  for (size_t characterIndex = 0; characterIndex < text.size();
       ++characterIndex) {
    const unsigned char byte = static_cast<unsigned char>(text[characterIndex]);

    if (byte == '\n') {
      cursor.x = 0.0f;
      cursor.y -= lineAdvance;
      continue;
    }

    // Initial implementation: printable ASCII only.
    if (byte < 32 || byte > 126) {
      throw std::runtime_error("TextMeshGenerator currently supports "
                               "ASCII characters 32 through 126 only");
    }

    const int codepoint = byte;
    const CpuGlyphMesh &glyph = getGlyph(codepoint);

    if (vertices.size() + glyph.vertices.size() >
        std::numeric_limits<uint16_t>::max()) {
      throw std::runtime_error(
          "Text mesh exceeds the renderer's uint16 index limit");
    }

    const uint16_t baseVertex = static_cast<uint16_t>(vertices.size());

    vertices.reserve(vertices.size() + glyph.vertices.size());

    for (const glm::vec2 glyphPosition : glyph.vertices) {
      vertices.push_back(Vertex{
          .pos =
              {
                  cursor.x + glyphPosition.x * scale,
                  cursor.y + glyphPosition.y * scale,
                  0.0f,
              },
          .color = color,
          .normal = {0.0f, 0.0f, 1.0f},
      });
    }

    indices.reserve(indices.size() + glyph.indices.size());

    for (const uint32_t glyphIndex : glyph.indices) {
      const uint32_t combinedIndex =
          static_cast<uint32_t>(baseVertex) + glyphIndex;

      if (combinedIndex > std::numeric_limits<uint16_t>::max()) {
        throw std::runtime_error("Text mesh index exceeds uint16 capacity");
      }

      indices.push_back(static_cast<uint16_t>(combinedIndex));
    }

    cursor.x += glyph.advance * scale;

    if (characterIndex + 1 < text.size() && text[characterIndex + 1] != '\n') {
      const unsigned char nextByte =
          static_cast<unsigned char>(text[characterIndex + 1]);

      if (nextByte >= 32 && nextByte <= 126) {
        cursor.x += static_cast<float>(stbtt_GetCodepointKernAdvance(
                        &fontInfo_, codepoint, nextByte)) *
                    scale;
      }
    }
  }

  // A string containing only spaces/newlines has no triangles.
  // generateMesh() currently assumes non-empty vectors and uses [0].
  if (vertices.empty() || indices.empty()) {
    throw std::runtime_error("Text contains no drawable glyphs");
  }

  return ::generateMesh(vertices, indices);
}
