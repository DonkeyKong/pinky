#pragma once

#include "cpp/Color.hpp"
#include "cpp/Logging.hpp"

#include <stdint.h>
#include <unordered_map>
#include <vector>
#include <cmath>
#include <algorithm>
#include <random>

enum class ColorName : uint8_t
{
  White,
  Red,
  Orange,
  Yellow,
  Green,
  Blue,
  Black,
  Clean
};

typedef uint8_t IndexedColor;

struct IndexLabIntPair
{
  IndexedColor index;
  Vec3i labInt;
};

struct IndexedColorMap
{
  IndexedColorMap() = default;
  IndexedColorMap(std::vector<std::tuple<ColorName,IndexedColor,RGBColor>> mapping);
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
private:
  void calculateFastDeltaELookup();
  std::vector<IndexedColor> indexedColors_;
  std::vector<ColorName> namedColors_;
  std::unordered_map<IndexedColor,ColorName> indexToName;
  std::unordered_map<IndexedColor,RGBColor> indexToRgb;
  std::unordered_map<IndexedColor,LabColor> indexToLab;
  std::unordered_map<ColorName,IndexedColor> nameToIndex;
  std::unordered_map<ColorName,RGBColor> nameToRgb;
  std::unordered_map<ColorName,LabColor> nameToLab;
  std::vector<IndexLabIntPair> fastDeltaELookup;
  int size_;
};


IndexedColorMap::IndexedColorMap(std::vector<std::tuple<ColorName,IndexedColor,RGBColor>> mapping)
{
  if (mapping.size() > 254)
  {
    DEBUG_LOG("Cannot create IndexedColorMap with more than 254 mappings!");
    return;
  }

  // Create exhaustive mappings
  for (const auto& [name, index, rbg] : mapping)
  {
    indexedColors_.push_back(index);
    namedColors_.push_back(name);
    indexToName[index] = name;
    indexToRgb[index] = rbg;
    indexToLab[index] = rbg.toLab();
    nameToIndex[name] = index;
    nameToRgb[name] = rbg;
    nameToLab[name] = indexToLab[index];
  }

  size_ = indexedColors_.size();
  calculateFastDeltaELookup();
}

void IndexedColorMap::calculateFastDeltaELookup()
{
  int i = 0;
  fastDeltaELookup.resize(size_);
  for (const auto& [indexedColor, refColor] : indexToLab)
  {
    fastDeltaELookup[i++] = {indexedColor, {(int)refColor.L, (int)refColor.a, (int)refColor.b}};
  }
}

template <typename T>
static T remap(T value, T inMin, T inMax, T outMin, T outMax)
{
  double t = (double)(value - inMin)/(double)(inMax-inMin);
  double out = t * (double)(outMax-outMin) + (double)(outMin);
  return std::clamp((T)std::clamp(out, (double)outMin, (double)outMax), outMin, outMax);
}

void IndexedColorMap::normalizePaletteByRgb(bool pinBlack, bool pinWhite)
{
  auto max = pinWhite ? toRGBColor(toIndexedColor(ColorName::White)).getBrightestChannel() : (uint8_t)255;
  auto min = pinBlack ? toRGBColor(toIndexedColor(ColorName::Black)).getDarkestChannel() : (uint8_t)0;

  for (auto index : indexedColors_)
  {
    auto name = indexToName[index];
    RGBColor colorRgb = indexToRgb[index];
    colorRgb.R = remap(colorRgb.R, min, max, (uint8_t)0, (uint8_t)255);
    colorRgb.G = remap(colorRgb.G, min, max, (uint8_t)0, (uint8_t)255);
    colorRgb.B = remap(colorRgb.B, min, max, (uint8_t)0, (uint8_t)255);
    LabColor colorLab = colorRgb.toLab();

    // Be sure to update all the following indices
    // indexToRgb, indexToLab, nameToRgb, nameToLab
    indexToRgb[index] = colorRgb;
    nameToRgb[name] = colorRgb;
    indexToLab[index] = colorLab;
    nameToLab[name] = colorLab;
  }
  calculateFastDeltaELookup();
}

void IndexedColorMap::normalizePaletteByLab(bool pinBlack, bool pinWhite)
{
  float max = pinWhite ? toLabColor(toIndexedColor(ColorName::White)).L : 100.0f;
  float min = pinBlack ? toLabColor(toIndexedColor(ColorName::Black)).L : 0.0f;

  for (auto index : indexedColors_)
  {
    auto name = indexToName[index];
    LabColor colorLab = indexToLab[index];
    colorLab.L = remap(colorLab.L, min, max, 0.0f, 100.0f);
    RGBColor colorRgb = colorLab.toRGB();

    // Be sure to update all the following indices
    // indexToRgb, indexToLab, nameToRgb, nameToLab
    indexToRgb[index] = colorRgb;
    nameToRgb[name] = colorRgb;
    indexToLab[index] = colorLab;
    nameToLab[name] = colorLab;
  }
  calculateFastDeltaELookup();
}

const std::vector<IndexedColor>& IndexedColorMap::indexedColors() const 
{
  return indexedColors_;
}

const std::vector<ColorName>& IndexedColorMap::namedColors() const 
{
  return namedColors_;
}

IndexedColor IndexedColorMap::toIndexedColor(const LabColor& inputColor, LabColor& error) const
{
  // Find the indexed color with the minimum deltaE from the specified color
  Vec3i inputColorInt {(int)inputColor.L, (int)inputColor.a, (int)inputColor.b};
  int minDeltaE = std::numeric_limits<int>::max();
  IndexedColor minIndexColor = 0;

  for (auto& paletteColor : fastDeltaELookup)
  {
    Vec3i d = inputColorInt-paletteColor.labInt;
    int dE = d.X*d.X+d.Y*d.Y+d.Z*d.Z;
    if (dE < minDeltaE)
    {
      minDeltaE = dE;
      minIndexColor = paletteColor.index;
    }
  }
  error = inputColor - indexToLab.at(minIndexColor);
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
