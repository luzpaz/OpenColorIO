// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the OpenColorIO Project.

#include <iostream>
#include <set>
#include <sstream>

#include <OpenColorIO/OpenColorIO.h>

#include "ParseUtils.h"
#include "pystring/pystring.h"

namespace OCIO_NAMESPACE
{

struct Element
{
    std::string str;
    char c;
};

static struct Element elts[]
    = { { "&quot;", '"' },
        { "&apos;", '\''},
        { "&lt;",   '<' },
        { "&gt;",   '>' },
        { "&amp;",  '&' },
        { "",       ' ' } };

std::string ConvertSpecialCharToXmlToken(const std::string& str)
{
    std::string res;
    for (std::string::const_iterator it(str.begin());
        it != str.end();
        it++)
    {
        bool found = false;
        for (unsigned idx = 0; !elts[idx].str.empty(); ++idx)
        {
            const Element& elt = elts[idx];
            if (*it == elt.c)
            {
                found = true;
                res += elt.str;
                break;
            }
        }
        if (!found)
        {
            res += *it;
        }
    }
    return res;
}

const char * BoolToString(bool val)
{
    if(val) return "true";
    return "false";
}

bool BoolFromString(const char * s)
{
    std::string str = pystring::lower(s);
    if((str == "true") || (str=="yes")) return true;
    return false;
}

const char * LoggingLevelToString(LoggingLevel level)
{
    if(level == LOGGING_LEVEL_NONE) return "none";
    else if(level == LOGGING_LEVEL_WARNING) return "warning";
    else if(level == LOGGING_LEVEL_INFO) return "info";
    else if(level == LOGGING_LEVEL_DEBUG) return "debug";
    return "unknown";
}

LoggingLevel LoggingLevelFromString(const char * s)
{
    std::string str = pystring::lower(s);
    if(str == "0" || str == "none") return LOGGING_LEVEL_NONE;
    else if(str == "1" || str == "warning") return LOGGING_LEVEL_WARNING;
    else if(str == "2" || str == "info") return LOGGING_LEVEL_INFO;
    else if(str == "3" || str == "debug") return LOGGING_LEVEL_DEBUG;
    return LOGGING_LEVEL_UNKNOWN;
}

const char * TransformDirectionToString(TransformDirection dir)
{
    if(dir == TRANSFORM_DIR_FORWARD) return "forward";
    else if(dir == TRANSFORM_DIR_INVERSE) return "inverse";
    return "unknown";
}

TransformDirection TransformDirectionFromString(const char * s)
{
    std::string str = pystring::lower(s);
    if(str == "forward") return TRANSFORM_DIR_FORWARD;
    else if(str == "inverse") return TRANSFORM_DIR_INVERSE;
    return TRANSFORM_DIR_UNKNOWN;
}

TransformDirection CombineTransformDirections(TransformDirection d1,
                                                TransformDirection d2)
{
    // Any unknowns always combine to be unknown.
    if(d1 == TRANSFORM_DIR_UNKNOWN || d2 == TRANSFORM_DIR_UNKNOWN)
        return TRANSFORM_DIR_UNKNOWN;

    if(d1 == TRANSFORM_DIR_FORWARD && d2 == TRANSFORM_DIR_FORWARD)
        return TRANSFORM_DIR_FORWARD;

    if(d1 == TRANSFORM_DIR_INVERSE && d2 == TRANSFORM_DIR_INVERSE)
        return TRANSFORM_DIR_FORWARD;

    return TRANSFORM_DIR_INVERSE;
}

TransformDirection GetInverseTransformDirection(TransformDirection dir)
{
    if(dir == TRANSFORM_DIR_FORWARD) return TRANSFORM_DIR_INVERSE;
    else if(dir == TRANSFORM_DIR_INVERSE) return TRANSFORM_DIR_FORWARD;
    return TRANSFORM_DIR_UNKNOWN;
}

const char * ColorSpaceDirectionToString(ColorSpaceDirection dir)
{
    if(dir == COLORSPACE_DIR_TO_REFERENCE) return "to_reference";
    else if(dir == COLORSPACE_DIR_FROM_REFERENCE) return "from_reference";
    return "unknown";
}

ColorSpaceDirection ColorSpaceDirectionFromString(const char * s)
{
    std::string str = pystring::lower(s);
    if(str == "to_reference") return COLORSPACE_DIR_TO_REFERENCE;
    else if(str == "from_reference") return COLORSPACE_DIR_FROM_REFERENCE;
    return COLORSPACE_DIR_UNKNOWN;
}

const char * BitDepthToString(BitDepth bitDepth)
{
    if(bitDepth == BIT_DEPTH_UINT8) return "8ui";
    else if(bitDepth == BIT_DEPTH_UINT10) return "10ui";
    else if(bitDepth == BIT_DEPTH_UINT12) return "12ui";
    else if(bitDepth == BIT_DEPTH_UINT14) return "14ui";
    else if(bitDepth == BIT_DEPTH_UINT16) return "16ui";
    else if(bitDepth == BIT_DEPTH_UINT32) return "32ui";
    else if(bitDepth == BIT_DEPTH_F16) return "16f";
    else if(bitDepth == BIT_DEPTH_F32) return "32f";
    return "unknown";
}

BitDepth BitDepthFromString(const char * s)
{
    std::string str = pystring::lower(s);
    if(str == "8ui") return BIT_DEPTH_UINT8;
    else if(str == "10ui") return BIT_DEPTH_UINT10;
    else if(str == "12ui") return BIT_DEPTH_UINT12;
    else if(str == "14ui") return BIT_DEPTH_UINT14;
    else if(str == "16ui") return BIT_DEPTH_UINT16;
    else if(str == "32ui") return BIT_DEPTH_UINT32;
    else if(str == "16f") return BIT_DEPTH_F16;
    else if(str == "32f") return BIT_DEPTH_F32;
    return BIT_DEPTH_UNKNOWN;
}

bool BitDepthIsFloat(BitDepth bitDepth)
{
    if(bitDepth == BIT_DEPTH_F16) return true;
    else if(bitDepth == BIT_DEPTH_F32) return true;
    return false;
}

int BitDepthToInt(BitDepth bitDepth)
{
    if(bitDepth == BIT_DEPTH_UINT8) return 8;
    else if(bitDepth == BIT_DEPTH_UINT10) return 10;
    else if(bitDepth == BIT_DEPTH_UINT12) return 12;
    else if(bitDepth == BIT_DEPTH_UINT14) return 14;
    else if(bitDepth == BIT_DEPTH_UINT16) return 16;
    else if(bitDepth == BIT_DEPTH_UINT32) return 32;

    return 0;
}

const char * AllocationToString(Allocation alloc)
{
    if(alloc == ALLOCATION_UNIFORM) return "uniform";
    else if(alloc == ALLOCATION_LG2) return "lg2";
    return "unknown";
}

Allocation AllocationFromString(const char * s)
{
    std::string str = pystring::lower(s);
    if(str == "uniform") return ALLOCATION_UNIFORM;
    else if(str == "lg2") return ALLOCATION_LG2;
    return ALLOCATION_UNKNOWN;
}

const char * InterpolationToString(Interpolation interp)
{
    if(interp == INTERP_NEAREST) return "nearest";
    else if(interp == INTERP_LINEAR) return "linear";
    else if(interp == INTERP_TETRAHEDRAL) return "tetrahedral";
    else if(interp == INTERP_BEST) return "best";
    else if (interp == INTERP_DEFAULT) return "default";
    return "unknown";
}

Interpolation InterpolationFromString(const char * s)
{
    std::string str = pystring::lower(s);
    if(str == "nearest") return INTERP_NEAREST;
    else if(str == "linear") return INTERP_LINEAR;
    else if(str == "tetrahedral") return INTERP_TETRAHEDRAL;
    else if(str == "best") return INTERP_BEST;
    return INTERP_UNKNOWN;
}

const char * GpuLanguageToString(GpuLanguage language)
{
    if(language == GPU_LANGUAGE_CG) return "cg";
    else if(language == GPU_LANGUAGE_GLSL_1_0)  return "glsl_1.0";
    else if(language == GPU_LANGUAGE_GLSL_1_3)  return "glsl_1.3";
    else if(language == GPU_LANGUAGE_GLSL_4_0)  return "glsl_4.0";
    else if(language == GPU_LANGUAGE_HLSL_DX11) return "hlsl_dx11";
    return "unknown";
}

GpuLanguage GpuLanguageFromString(const char * s)
{
    std::string str = pystring::lower(s);
    if(str == "cg") return GPU_LANGUAGE_CG;
    else if(str == "glsl_1.0") return GPU_LANGUAGE_GLSL_1_0;
    else if(str == "glsl_1.3") return GPU_LANGUAGE_GLSL_1_3;
    else if(str == "glsl_4.0") return GPU_LANGUAGE_GLSL_4_0;
    else if(str == "hlsl_dx11") return GPU_LANGUAGE_HLSL_DX11;
    return GPU_LANGUAGE_UNKNOWN;
}

const char * EnvironmentModeToString(EnvironmentMode mode)
{
    if(mode == ENV_ENVIRONMENT_LOAD_PREDEFINED) return "loadpredefined";
    else if(mode == ENV_ENVIRONMENT_LOAD_ALL) return "loadall";
    return "unknown";
}

EnvironmentMode EnvironmentModeFromString(const char * s)
{
    std::string str = pystring::lower(s);
    if(str == "loadpredefined") return ENV_ENVIRONMENT_LOAD_PREDEFINED;
    else if(str == "loadall") return ENV_ENVIRONMENT_LOAD_ALL;
    return ENV_ENVIRONMENT_UNKNOWN;
}

const char * RangeStyleToString(RangeStyle style)
{
    if(style == RANGE_NO_CLAMP) return "noClamp";
    else if(style == RANGE_CLAMP) return "Clamp";
    return "Clamp";
}

RangeStyle RangeStyleFromString(const char * style)
{
    if(style && *style)
    {
        const std::string str = pystring::lower(style);
        if(str == "noclamp") return RANGE_NO_CLAMP;
        else if(str == "clamp") return RANGE_CLAMP;
    }

    std::string msg("Wrong Range style: ");
    msg += (style && *style) ? style : "<null>";

    throw Exception(msg.c_str());
}

const char * FixedFunctionStyleToString(FixedFunctionStyle style)
{
    switch(style)
    {
        case FIXED_FUNCTION_ACES_RED_MOD_03:     return "ACES_RedMod03";
        case FIXED_FUNCTION_ACES_RED_MOD_10:     return "ACES_RedMod10";
        case FIXED_FUNCTION_ACES_GLOW_03:        return "ACES_Glow03";
        case FIXED_FUNCTION_ACES_GLOW_10:        return "ACES_Glow10";
        case FIXED_FUNCTION_ACES_DARK_TO_DIM_10: return "ACES_DarkToDim10";
        case FIXED_FUNCTION_REC2100_SURROUND:    return "REC2100_Surround";
    }

    // Default style is meaningless.
    throw Exception("Unknown Fixed FunctionOp style");
}

FixedFunctionStyle FixedFunctionStyleFromString(const char * style)
{
    std::string str = pystring::lower(style);

    if(str == "aces_redmod03")           return FIXED_FUNCTION_ACES_RED_MOD_03;
    else if(str == "aces_redmod10")      return FIXED_FUNCTION_ACES_RED_MOD_10;
    else if(str == "aces_glow03")        return FIXED_FUNCTION_ACES_GLOW_03;
    else if(str == "aces_glow10")        return FIXED_FUNCTION_ACES_GLOW_10;
    else if(str == "aces_darktodim10")   return FIXED_FUNCTION_ACES_DARK_TO_DIM_10;
    else if(str == "rec2100_surround")   return FIXED_FUNCTION_REC2100_SURROUND;

    // Default style is meaningless.
    std::stringstream ss;
    ss << "Unknown Fixed FunctionOp style: " << style;

    throw Exception(ss.str().c_str());
}

namespace
{
static constexpr const char * EC_STYLE_VIDEO = "video";
static constexpr const char * EC_STYLE_LOGARITHMIC = "log";
static constexpr const char * EC_STYLE_LINEAR = "linear";
}

const char * ExposureContrastStyleToString(ExposureContrastStyle style)
{
    switch (style)
    {
    case EXPOSURE_CONTRAST_VIDEO:       return EC_STYLE_VIDEO;
    case EXPOSURE_CONTRAST_LOGARITHMIC: return EC_STYLE_LOGARITHMIC;
    case EXPOSURE_CONTRAST_LINEAR:      return EC_STYLE_LINEAR;
    }

    // Default style is meaningless.
    throw Exception("Unknown exposure contrast style");
}

ExposureContrastStyle ExposureContrastStyleFromString(const char * style)
{
    std::string str = pystring::lower(style);

    if      (str == EC_STYLE_LINEAR)      return EXPOSURE_CONTRAST_LINEAR;
    else if (str == EC_STYLE_VIDEO)       return EXPOSURE_CONTRAST_VIDEO;
    else if (str == EC_STYLE_LOGARITHMIC) return EXPOSURE_CONTRAST_LOGARITHMIC;

    // Default style is meaningless.
    std::stringstream ss;
    ss << "Unknown exposure contrast style: " << style;

    throw Exception(ss.str().c_str());
}

const char * ROLE_DEFAULT = "default";
const char * ROLE_REFERENCE = "reference";
const char * ROLE_DATA = "data";
const char * ROLE_COLOR_PICKING = "color_picking";
const char * ROLE_SCENE_LINEAR = "scene_linear";
const char * ROLE_COMPOSITING_LOG = "compositing_log";
const char * ROLE_COLOR_TIMING = "color_timing";
const char * ROLE_TEXTURE_PAINT = "texture_paint";
const char * ROLE_MATTE_PAINT = "matte_paint";

namespace
{
const int FLOAT_DECIMALS = 7;
const int DOUBLE_DECIMALS = 16;
}

std::string FloatToString(float value)
{
    std::ostringstream pretty;
    pretty.precision(FLOAT_DECIMALS);
    pretty << value;
    return pretty.str();
}

std::string FloatVecToString(const float * fval, unsigned int size)
{
    if(size<=0) return "";

    std::ostringstream pretty;
    pretty.precision(FLOAT_DECIMALS);
    for(unsigned int i=0; i<size; ++i)
    {
        if(i!=0) pretty << " ";
        pretty << fval[i];
    }

    return pretty.str();
}

bool StringToFloat(float * fval, const char * str)
{
    if(!str) return false;

    std::istringstream inputStringstream(str);
    float x;
    if(!(inputStringstream >> x))
    {
        return false;
    }

    if(fval) *fval = x;
    return true;
}

bool StringToInt(int * ival, const char * str, bool failIfLeftoverChars)
{
    if(!str) return false;
    if(!ival) return false;

    std::istringstream i(str);
    char c=0;
    if (!(i >> *ival) || (failIfLeftoverChars && i.get(c))) return false;
    return true;
}

std::string DoubleToString(double value)
{
    std::ostringstream pretty;
    pretty.precision(DOUBLE_DECIMALS);
    pretty << value;
    return pretty.str();
}

std::string DoubleVecToString(const double * val, unsigned int size)
{
    if (size <= 0) return "";

    std::ostringstream pretty;
    pretty.precision(DOUBLE_DECIMALS);
    for (unsigned int i = 0; i<size; ++i)
    {
        if (i != 0) pretty << " ";
        pretty << val[i];
    }

    return pretty.str();
}

bool StringVecToFloatVec(std::vector<float> &floatArray,
                            const StringVec &lineParts)
{
    floatArray.resize(lineParts.size());

    for(unsigned int i=0; i<lineParts.size(); i++)
    {
        std::istringstream inputStringstream(lineParts[i]);
        float x;
        if(!(inputStringstream >> x))
        {
            return false;
        }
        floatArray[i] = x;
    }

    return true;
}

// This will resize intArray to the size of lineParts.
// Returns true if all lineParts have been recognized as int.
// Content of intArray will be unknown if function returns false.
bool StringVecToIntVec(std::vector<int> &intArray,
                        const StringVec &lineParts)
{
    intArray.resize(lineParts.size());

    for(unsigned int i=0; i<lineParts.size(); i++)
    {
        int x;
        // When reading a vector of string as int, ints that
        // are followed by other characters (ex. "3d") are
        // not considered as int.
        if ( !StringToInt(&x, lineParts[i].c_str(), true) )
        {
            return false;
        }
        intArray[i] = x;
    }

    return true;
}

////////////////////////////////////////////////////////////////////////////

// read the next non empty line, and store it in 'line'
// return 'true' on success

bool nextline(std::istream &istream, std::string &line)
{
    while ( istream.good() )
    {
        std::getline(istream, line);
        if(line.size() > 0 && line[line.size() - 1] == '\r')
        {
            line.resize(line.size() - 1);
        }
        if(!pystring::strip(line).empty())
        {
            return true;
        }
    }

    line = "";
    return false;
}

bool StrEqualsCaseIgnore(const std::string & a, const std::string & b)
{
    return (pystring::lower(a) == pystring::lower(b));
}

// If a ',' is in the string, split on it
// If a ':' is in the string, split on it
// Otherwise, assume a single string.
// Also, strip whitespace from all parts.

void SplitStringEnvStyle(StringVec & outputvec, const char * str)
{
    if(!str) return;

    std::string s = pystring::strip(str);
    if(pystring::find(s, ",") > -1)
    {
        pystring::split(s, outputvec, ",");
    }
    else if(pystring::find(s, ":") > -1)
    {
        pystring::split(s, outputvec, ":");
    }
    else
    {
        outputvec.push_back(s);
    }

    for(unsigned int i=0; i<outputvec.size(); ++i)
    {
        outputvec[i] = pystring::strip(outputvec[i]);
    }
}

std::string JoinStringEnvStyle(const StringVec & outputvec)
{
    return pystring::join(", ", outputvec);
}

// Return a vector of strings that are both in vec1 and vec2.
// Case is ignored to find strings.
// Ordering and capitalization from vec1 are preserved.
StringVec IntersectStringVecsCaseIgnore(const StringVec & vec1,
                                        const StringVec & vec2)
{
    StringVec newvec;
    std::set<std::string> allvalues;

    // Seed the set with all values from vec2
    for(unsigned int i=0; i<vec2.size(); ++i)
    {
        allvalues.insert(pystring::lower(vec2[i]));
    }

    for(unsigned int i=0; i<vec1.size(); ++i)
    {
        std::string key = pystring::lower(vec1[i]);
        if(allvalues.find(key) != allvalues.end())
        {
            newvec.push_back(vec1[i]);
        }
    }

    return newvec;
}

int FindInStringVecCaseIgnore(const StringVec & vec, const std::string & str)
{
    std::string teststr = pystring::lower(str);
    for(unsigned int i=0; i<vec.size(); ++i)
    {
        if(pystring::lower(vec[i]) == teststr) return static_cast<int>(i);
    }

    return -1;
}
} // namespace OCIO_NAMESPACE

