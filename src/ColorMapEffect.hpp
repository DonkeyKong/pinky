#pragma once

#include <IndexedColor.hpp>
#include <memory>

enum class ColorMapEffect : int
{
  None,
  BlackWhite,
  BlackWhiteRed,
  BlackWhiteYellow,
  Saturated,
  WhiteGreenDuotone,
  YellowBlackDuotone,
  RedBlueDuotone,
  WhiteYellowRedBlack,
  GrayscaleRainbow,
};

enum class ColorMapEffectOptions : int
{
  None = 0,
  ConvertInputToMonochrome = 0b00000001,    // When mapping, first convert all colors to greyscale
  AllowMissingOutputChannels = 0b00000010,  // Drop output channels that are missing instead of returning nullptr (unsupported effect)
};

std::shared_ptr<IndexedColorMap> applyToBaseMap(const IndexedColorMap& base, const ColorMapArgList& newMapping, ColorMapEffectOptions options = ColorMapEffectOptions::None)
{
  // Mapping is basically a way of saying which color channels on the display
  // map to which RGB values in an image. The actual color index is ignored and pulled from
  // the baseDisplayMap. 

  // If dropped channels are not allowed, make sure the base mapping
  // contains all the color channels we are mapping
  if (!((int)options & (int)ColorMapEffectOptions::AllowMissingOutputChannels))
  {
    for (const auto& val : newMapping)
    {
      if (!base.hasDestinationColor(std::get<0>(val)))
      {
        return nullptr;
      }
    }
  }

  // Populate the final named color / index / RGB mapping
  ColorMapArgList mappingWithIndex;
  for (const auto& val : newMapping)
  {
    if (base.hasDestinationColor(std::get<0>(val)))
    {
      mappingWithIndex.push_back({
        std::get<0>(val),
        base.toIndexedColor(std::get<0>(val)),
        std::get<2>(val)
      });
    }
  }

  // Figure out if this is a monochrome mapping
  bool monochrome = (((int)options & (int) ColorMapEffectOptions::ConvertInputToMonochrome) != 0);

  return std::make_shared<IndexedColorMap>(mappingWithIndex, monochrome);
}

std::shared_ptr<IndexedColorMap> getColorMapWithEffect(const IndexedColorMap& base, ColorMapEffect effect)
{
  switch (effect)
  {
    case ColorMapEffect::BlackWhite:
      return applyToBaseMap(
        base,
        {
          {ColorName::White, 0, {255,255,255}},
          {ColorName::Black, 0, {0,0,0}}
        }
      );
    case ColorMapEffect::BlackWhiteRed:
      return applyToBaseMap(
        base,
        {
          {ColorName::White, 0, {255,255,255}},
          {ColorName::Black, 0, {0,0,0}},
          {ColorName::Red, 0, {255,0,0}}
        }
      );
    case ColorMapEffect::BlackWhiteYellow:
      return applyToBaseMap(
        base,
        {
          {ColorName::White, 0, {255,255,255}},
          {ColorName::Black, 0, {0,0,0}},
          {ColorName::Yellow, 0, {255,255,0}}
        }
    );
    case ColorMapEffect::Saturated:
      {
        ColorMapArgList saturatedColors;
        for (const auto& colorName : base.namedColors())
        {
          saturatedColors.push_back({
            colorName,
            base.toIndexedColor(colorName),
            ColorNameToSaturatedRGBColor(colorName)
          });
        }
        return std::make_shared<IndexedColorMap>(saturatedColors);
      }
    case ColorMapEffect::WhiteGreenDuotone:
      return applyToBaseMap(
        base,
        {
          {ColorName::White, 0, {255, 255, 255}},
          {ColorName::Green, 0, {0, 0, 0}}
        },
        ColorMapEffectOptions::ConvertInputToMonochrome
      );
    case ColorMapEffect::YellowBlackDuotone:
      return applyToBaseMap(
        base,
        {
          {ColorName::Yellow, 0, {255, 255, 255}},
          {ColorName::Black, 0, {0, 0, 0}}
        },
        ColorMapEffectOptions::ConvertInputToMonochrome
      );
    case ColorMapEffect::RedBlueDuotone:
      return applyToBaseMap(
        base,
        {
          {ColorName::Blue, 0, {0, 0, 0}},
          {ColorName::Red, 0, {255, 255, 255}}
        },
        ColorMapEffectOptions::ConvertInputToMonochrome
      );
    case ColorMapEffect::WhiteYellowRedBlack:
      return applyToBaseMap(
        base,
        {
          {ColorName::Black, 0, {0, 0, 0}},
          {ColorName::White, 0, {255, 255, 255}},
          {ColorName::Red, 0, {80, 80, 80}},
          {ColorName::Yellow, 0, {168, 168, 168}},
        },
        ColorMapEffectOptions::ConvertInputToMonochrome
      );
    case ColorMapEffect::GrayscaleRainbow:
      return applyToBaseMap(
        base,
        {
          {ColorName::Black, 0, {0, 0, 0}},
          {ColorName::Blue, 0, {42, 42, 42}},
          {ColorName::Green, 0, {84, 84, 84}},
          {ColorName::Red, 0, {126, 126, 126}},
          {ColorName::Orange, 0, {168, 168, 168}},
          {ColorName::Yellow, 0, {210, 210, 210}},
          {ColorName::White, 0, {255, 255, 255}},
        },
        ColorMapEffectOptions::ConvertInputToMonochrome
      );
    default:
      return nullptr;
  }
}