#pragma once

#include "cpp/Color.hpp"
#include "cpp/Logging.hpp"

#include <stdint.h>
#include <unordered_map>
#include <vector>
#include <cmath>
#include <algorithm>
#include <random>

// When adding colors here, be sure to also add them to the function
// ColorNameToSaturatedRGBColor as well (if they are actual colors 
// and not control values like "clean")
enum class ColorName : uint8_t
{
  White,
  Magenta,
  Red,
  Orange,
  Yellow,
  Green,
  Cyan,
  Blue,
  Black,
  Clean  // Note: not "clear". Has no set visual appearance.
};

RGBColor ColorNameToSaturatedRGBColor(ColorName name)
{
  switch (name)
  {
    case ColorName::White: return {255, 255, 255};
    case ColorName::Magenta: return {255, 0, 255};
    case ColorName::Red: return {255, 0, 0};
    case ColorName::Orange: return {255, 127, 0};
    case ColorName::Yellow: return {255, 255, 0};
    case ColorName::Green: return {0, 255, 0};
    case ColorName::Cyan: return {0, 255, 255};
    case ColorName::Blue: return {0, 0, 255};
    case ColorName::Black: return {0, 0, 0};
    default: return {0,0,0};
  }
}

typedef uint8_t IndexedColor;
using ColorMapArgList = std::vector<std::tuple<ColorName,IndexedColor,RGBColor>>;

struct IndexedColorMap
{
  IndexedColorMap() = default;
  IndexedColorMap(ColorMapArgList mapping, bool monochrome = false);
  // Set the colors Black and White to (0,0,0) and (255,255,255) respectively, then rescale the rest
  void normalizePaletteByRgb(bool pinBlack = true, bool pinWhite = true);
  void normalizePaletteByLab(bool pinBlack = true, bool pinWhite = true);
  IndexedColor toIndexedColor(const LabColor& color, LabColor& error) const;
  IndexedColor toIndexedColor(const RGBColor& color) const;
  IndexedColor toIndexedColor(const LabColor& color) const;
  IndexedColor toIndexedColor(const ColorName) const;
  RGBColor toRGBColor(const IndexedColor indexedColor) const;
  LabColor toLabColor(const IndexedColor indexedColor) const;
  uint8_t size() const;
  const std::vector<IndexedColor>& indexedColors() const;
  const std::vector<ColorName>& namedColors() const;
  bool hasDestinationColor(ColorName color) const
  {
    return nameToIndex.count(color) > 0;
  }
private:
  bool monochrome_ = false;
  std::vector<IndexedColor> indexedColors_;
  std::vector<ColorName> namedColors_;
  std::unordered_map<IndexedColor,ColorName> indexToName;
  std::unordered_map<IndexedColor,RGBColor> indexToRgb;
  std::unordered_map<IndexedColor,LabColor> indexToLab;
  std::unordered_map<ColorName,IndexedColor> nameToIndex;
  std::unordered_map<ColorName,RGBColor> nameToRgb;
  std::unordered_map<ColorName,LabColor> nameToLab;
};


IndexedColorMap::IndexedColorMap(ColorMapArgList mapping, bool monochrome) :
  monochrome_{monochrome}
{
  if (mapping.size() > 254)
  {
    DEBUG_LOG("Cannot create IndexedColorMap with more than 254 mappings!");
    return;
  }

  // Create exhaustive mappings
  for (const auto& [name, index, rgb] : mapping)
  {
    indexedColors_.push_back(index);
    namedColors_.push_back(name);
    indexToName[index] = name;
    nameToIndex[name] = index;

    LabColor lab = rgb.toLab();
    if (monochrome_)
    {
      uint8_t mono = (uint8_t)remapClamp(lab.L, 0.0f, 100.0f, 0.0f, 255.0f);
      indexToRgb[index] = RGBColor{mono, mono, mono};
      indexToLab[index] = LabColor{lab.L, 0, 0};
    }
    else
    {
      indexToRgb[index] = rgb;
      indexToLab[index] = lab;
    }
    nameToRgb[name] = indexToRgb[index];
    nameToLab[name] = indexToLab[index];
  }
}

void IndexedColorMap::normalizePaletteByRgb(bool pinBlack, bool pinWhite)
{
  auto max = pinWhite ? toRGBColor(toIndexedColor(ColorName::White)).getBrightestChannel() : (uint8_t)255;
  auto min = pinBlack ? toRGBColor(toIndexedColor(ColorName::Black)).getDarkestChannel() : (uint8_t)0;

  for (auto index : indexedColors_)
  {
    auto name = indexToName[index];
    RGBColor colorRgb = indexToRgb[index];
    colorRgb.R = remapClamp(colorRgb.R, min, max, (uint8_t)0, (uint8_t)255);
    colorRgb.G = remapClamp(colorRgb.G, min, max, (uint8_t)0, (uint8_t)255);
    colorRgb.B = remapClamp(colorRgb.B, min, max, (uint8_t)0, (uint8_t)255);
    LabColor colorLab = colorRgb.toLab();

    // Be sure to update all the following indices
    // indexToRgb, indexToLab, nameToRgb, nameToLab
    indexToRgb[index] = colorRgb;
    nameToRgb[name] = colorRgb;
    indexToLab[index] = colorLab;
    nameToLab[name] = colorLab;
  }
}

void IndexedColorMap::normalizePaletteByLab(bool pinBlack, bool pinWhite)
{
  float max = pinWhite ? toLabColor(toIndexedColor(ColorName::White)).L : 100.0f;
  float min = pinBlack ? toLabColor(toIndexedColor(ColorName::Black)).L : 0.0f;

  for (auto index : indexedColors_)
  {
    auto name = indexToName[index];
    LabColor colorLab = indexToLab[index];
    colorLab.L = remapClamp(colorLab.L, min, max, 0.0f, 100.0f);
    RGBColor colorRgb = colorLab.toRGB();

    // Be sure to update all the following indices
    // indexToRgb, indexToLab, nameToRgb, nameToLab
    indexToRgb[index] = colorRgb;
    nameToRgb[name] = colorRgb;
    indexToLab[index] = colorLab;
    nameToLab[name] = colorLab;
  }
}

const std::vector<IndexedColor>& IndexedColorMap::indexedColors() const 
{
  return indexedColors_;
}

const std::vector<ColorName>& IndexedColorMap::namedColors() const 
{
  return namedColors_;
}

IndexedColor IndexedColorMap::toIndexedColor(const LabColor& color, LabColor& error) const
{
  // Find the indexed color with the minimum deltaE from the specified color
  float minDeltaE = std::numeric_limits<float>::infinity();
  IndexedColor minIndexColor = 0;
  LabColor minLabColor;

  for (const auto& [indexedColor, refColor] : indexToLab)
  {
    float dE = monochrome_? std::abs(refColor.L-color.L) : refColor.deltaE(color);
    if (dE < minDeltaE)
    {
      minDeltaE = dE;
      minIndexColor = indexedColor;
      minLabColor = refColor;
    }
  }

  error = monochrome_ ? LabColor{color.L-minLabColor.L,0,0} : (color - minLabColor);
  return minIndexColor;
}

IndexedColor IndexedColorMap::toIndexedColor(const RGBColor& color) const
{
  LabColor error;
  return toIndexedColor(color.toLab(), error);
}

IndexedColor IndexedColorMap::toIndexedColor(const LabColor& color) const
{
  LabColor error;
  return toIndexedColor(color, error);
}

RGBColor IndexedColorMap::toRGBColor(const IndexedColor indexedColor) const
{
  if (indexToRgb.count(indexedColor) > 0)
    return indexToRgb.at(indexedColor);
  return RGBColor();
}

LabColor IndexedColorMap::toLabColor(const IndexedColor indexedColor) const
{
  if (indexToLab.count(indexedColor) > 0)
    return indexToLab.at(indexedColor);
  return LabColor();
}

IndexedColor IndexedColorMap::toIndexedColor(const ColorName namedColor) const
{
  if (nameToIndex.count(namedColor) > 0)
    return nameToIndex.at(namedColor);
  if (namedColor == ColorName::Clean)
    return (IndexedColor)nameToIndex.size();
  return 255;
}

uint8_t IndexedColorMap::size() const
{
  return (uint8_t) indexedColors_.size();
}
