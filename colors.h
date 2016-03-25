#ifndef COLORS_H
#define COLORS_H

// COLOR STRUCTURES
struct fColor
{
  float32 r;
  float32 g;
  float32 b;
  float32 a;
};

struct tColor
{
  uint8 r;
  uint8 g;
  uint8 b;
  uint8 a;
};

// FUNCTION DECLARATIONS
internal uint32 roundFlot32ToUin32(float32 number);
internal uint32 hColorFromFloat(float32 r, float32 g, float32 b, float32 a);
internal fColor fColorFromHex(uint32 color);
internal tColor tColorFromHex(uint32 color);

// STANDARD COLORS                    0xAABBGGRR (SDL GFX SPECIFIED)
global const uint32 COLOR_WHITE     = 0xFFFFFFFF;
global const uint32 COLOR_BLACK     = 0xFF000000;
global const uint32 COLOR_GREY      = 0xFF808080;
global const uint32 COLOR_LIGHTGREY = 0xFFC0C0C0;
global const uint32 COLOR_DARKGREY  = 0xFF404040;
global const uint32 COLOR_RED       = 0xFF0000FF;
global const uint32 COLOR_GREEN     = 0xFF00FF00;
global const uint32 COLOR_BLUE      = 0xFFFF0000;
global const uint32 COLOR_CYAN      = 0xFFFFFF00;
global const uint32 COLOR_MAGENTA   = 0xFFFF00FF;
global const uint32 COLOR_YELLOW    = 0xFF00FFFF;

// STANDARD TCOLORS
global tColor tcWhite     = tColorFromHex(COLOR_WHITE);
global tColor tcBlack     = tColorFromHex(COLOR_BLACK);
global tColor tcGrey      = tColorFromHex(COLOR_GREY);
global tColor tcLightGrey = tColorFromHex(COLOR_LIGHTGREY);
global tColor tcDarkGrey  = tColorFromHex(COLOR_DARKGREY);
global tColor tcRed       = tColorFromHex(COLOR_RED);
global tColor tcBlue      = tColorFromHex(COLOR_BLUE);
global tColor tcGreen     = tColorFromHex(COLOR_GREEN);
global tColor tcCyan      = tColorFromHex(COLOR_CYAN);
global tColor tcMagenta   = tColorFromHex(COLOR_MAGENTA);
global tColor tcYellow    = tColorFromHex(COLOR_YELLOW);

// STANDARD FCOLORS
global fColor fcWhite     = fColorFromHex(COLOR_WHITE);
global fColor fcBlack     = fColorFromHex(COLOR_BLACK);
global fColor fcGrey      = fColorFromHex(COLOR_GREY);
global fColor fcLightGrey = fColorFromHex(COLOR_LIGHTGREY);
global fColor fcDarkGrey  = fColorFromHex(COLOR_DARKGREY);
global fColor fcRed       = fColorFromHex(COLOR_RED);
global fColor fcBlue      = fColorFromHex(COLOR_BLUE);
global fColor fcGreen     = fColorFromHex(COLOR_GREEN);
global fColor fcCyan      = fColorFromHex(COLOR_CYAN);
global fColor fcMagenta   = fColorFromHex(COLOR_MAGENTA);
global fColor fcYellow    = fColorFromHex(COLOR_YELLOW);

// CUSTOM COLORS
global const uint32 COLOR_VIDEOBLUE       = 0xFFEEA410;
global const uint32 COLOR_AUDIOGREEN      = 0xFF00C33D;
global const uint32 COLOR_SOLOBLUE        = 0xFFE5952D;
global const uint32 COLOR_MUTEORANGE      = 0xFF0060F2;
global const uint32 COLOR_SLIDEORANGE     = 0xFF4FC1F2;
global const uint32 COLOR_DROPDOWN        = 0xFF93897F;
global const uint32 COLOR_SCROLLINGBARS   = 0xFF564F46;
global const uint32 COLOR_SELECTEDTRACK   = 0xFFE2DDD9;
global const uint32 COLOR_TIMELINECOLOR   = 0xFFB2ADA5;
global const uint32 COLOR_BACKGROUNDCOLOR = 0xFF3D3D3D;
global const uint32 COLOR_BORDER          = 0xFF93897F;
global const uint32 COLOR_LINE            = 0xFFC5BEB7;
global const uint32 COLOR_MIDLINE         = 0xFFD8C5B8;
global const uint32 COLOR_ENDLINE         = 0xFFE5E1DF;
global const uint32 COLOR_PLAYHEAD        = 0xFF1177F4;

// CUSTOM TCOLORS
global tColor bgc = tColorFromHex(COLOR_BACKGROUNDCOLOR);
global tColor tlcolor = tColorFromHex(COLOR_TIMELINECOLOR);


// FUNCTION DEFINITIONS
internal uint32 roundFloat32ToUint32(float32 number)
{
  uint32 result = (uint32)(number + 0.5f);
  return result;
}

internal uint32 hColorFromFloat(float32 r, float32 g, float32 b, float32 a)
{
  uint32 result = ((roundFloat32ToUint32(r * 255.0f) << 0) |
                   (roundFloat32ToUint32(g * 255.0f) << 8)  |
                   (roundFloat32ToUint32(b * 255.0f) << 16)  |
                   (roundFloat32ToUint32(a * 255.0f) << 24));
}

internal fColor fColorFromHex(uint32 color)
{
  fColor result = {};
  result.r = (uint8)(color >> 0) / 255.0f;
  result.g = (uint8)(color >> 8)  / 255.0f;
  result.b = (uint8)(color >> 16)  / 255.0f;
  result.a = (uint8)(color >> 24) / 255.0f;

  return result;
}

internal tColor tColorFromHex(uint32 color)
{
  tColor result;
  result.r = (uint8)(color >> 0);
  result.g = (uint8)(color >> 8);
  result.b = (uint8)(color >> 16);
  result.a = (uint8)(color >> 24);
  return result;
}

#endif