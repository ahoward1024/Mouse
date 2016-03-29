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
global const tColor tcWhite     = tColorFromHex(COLOR_WHITE);
global const tColor tcBlack     = tColorFromHex(COLOR_BLACK);
global const tColor tcGrey      = tColorFromHex(COLOR_GREY);
global const tColor tcLightGrey = tColorFromHex(COLOR_LIGHTGREY);
global const tColor tcDarkGrey  = tColorFromHex(COLOR_DARKGREY);
global const tColor tcRed       = tColorFromHex(COLOR_RED);
global const tColor tcBlue      = tColorFromHex(COLOR_BLUE);
global const tColor tcGreen     = tColorFromHex(COLOR_GREEN);
global const tColor tcCyan      = tColorFromHex(COLOR_CYAN);
global const tColor tcMagenta   = tColorFromHex(COLOR_MAGENTA);
global const tColor tcYellow    = tColorFromHex(COLOR_YELLOW);

// STANDARD FCOLORS
global const fColor fcWhite     = fColorFromHex(COLOR_WHITE);
global const fColor fcBlack     = fColorFromHex(COLOR_BLACK);
global const fColor fcGrey      = fColorFromHex(COLOR_GREY);
global const fColor fcLightGrey = fColorFromHex(COLOR_LIGHTGREY);
global const fColor fcDarkGrey  = fColorFromHex(COLOR_DARKGREY);
global const fColor fcRed       = fColorFromHex(COLOR_RED);
global const fColor fcBlue      = fColorFromHex(COLOR_BLUE);
global const fColor fcGreen     = fColorFromHex(COLOR_GREEN);
global const fColor fcCyan      = fColorFromHex(COLOR_CYAN);
global const fColor fcMagenta   = fColorFromHex(COLOR_MAGENTA);
global const fColor fcYellow    = fColorFromHex(COLOR_YELLOW);

// CUSTOM COLORS
global const uint32 COLOR_VIDEOBLUE       = 0xFFEEA410;
global const uint32 COLOR_AUDIOGREEN      = 0xFF00C33D;
global const uint32 COLOR_SOLOBLUE        = 0xFFE5952D;
global const uint32 COLOR_MUTEORANGE      = 0xFF0060F2;
global const uint32 COLOR_SLIDEORANGE     = 0xFF4FC1F2;
global const uint32 COLOR_DROPDOWN        = 0xFF93897F;
global const uint32 COLOR_SCROLLBARS      = 0xFF564F46;
global const uint32 COLOR_SELECTEDTRACK   = 0xFFE2DDD9;
global const uint32 COLOR_VIEWCOLOR       = 0xFFB2ADA5;
global const uint32 COLOR_BACKGROUNDCOLOR = 0xFF3D3D3D;
global const uint32 COLOR_BORDER          = 0xFF8C7467;
global const uint32 COLOR_LINE            = 0xFFC5BEB7;
global const uint32 COLOR_MIDLINE         = 0xFFD8C5B8;
global const uint32 COLOR_ENDLINE         = 0xFFE5E1DF;
global const uint32 COLOR_PLAYHEAD        = 0xFF1177F4;

// CUSTOM TCOLORS
global const tColor tcVideoBlue       = tColorFromHex(COLOR_VIDEOBLUE);
global const tColor tcAudioGreen      = tColorFromHex(COLOR_AUDIOGREEN);
global const tColor tcSoloBlue        = tColorFromHex(COLOR_SOLOBLUE);
global const tColor tcMuteOrane       = tColorFromHex(COLOR_MUTEORANGE);
global const tColor tcSlideOrange     = tColorFromHex(COLOR_SLIDEORANGE);
global const tColor tcDropDown        = tColorFromHex(COLOR_DROPDOWN);
global const tColor tcScrollBars      = tColorFromHex(COLOR_SCROLLBARS);
global const tColor tcSelectedTrack   = tColorFromHex(COLOR_SELECTEDTRACK);
global const tColor tcView            = tColorFromHex(COLOR_VIEWCOLOR);
global const tColor tcBackground      = tColorFromHex(COLOR_BACKGROUNDCOLOR);
global const tColor tcBorder          = tColorFromHex(COLOR_BORDER);
global const tColor tcLine            = tColorFromHex(COLOR_LINE);
global const tColor tcMidline         = tColorFromHex(COLOR_MIDLINE);
global const tColor tcEndline         = tColorFromHex(COLOR_ENDLINE);
global const tColor tcPlayhead        = tColorFromHex(COLOR_PLAYHEAD);


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