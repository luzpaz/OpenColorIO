// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the OpenColorIO Project.

#include <algorithm>
#include <sstream>

#include <OpenColorIO/OpenColorIO.h>

#include "BitDepthUtils.h"
#include "MathUtils.h"
#include "ops/log/LogOpData.h"
#include "ops/log/LogUtils.h"
#include "ops/matrix/MatrixOpData.h"
#include "ops/range/RangeOpData.h"
#include "Platform.h"

namespace OCIO_NAMESPACE
{

namespace DefaultValues
{
static const double logSlope[3]  = { 1.0, 1.0, 1.0 };
static const double linSlope[3]  = { 1.0, 1.0, 1.0 };
static const double linOffset[3]  = { 0.0, 0.0, 0.0 };
static const double logOffset[3] = { 0.0, 0.0, 0.0 };
const std::streamsize FLOAT_DECIMALS = 7;
}

namespace
{
// Validate number of parameters and their respective range and value.
void ValidateParams(const LogOpData::Params & params, TransformDirection direction)
{
    const size_t expectedSize = 4;
    if (params.size() != expectedSize)
    {
        throw Exception("Log: expecting 4 parameters.");
    }

    if (direction != TRANSFORM_DIR_UNKNOWN)
    {

        if (IsScalarEqualToZero(params[LIN_SIDE_SLOPE]))
        {
            std::ostringstream oss;
            oss << "Log: Invalid linear slope value '";
            oss << params[LIN_SIDE_SLOPE];
            oss << "', linear slope cannot be 0.";
            throw Exception(oss.str().c_str());
        }
        if (IsScalarEqualToZero(params[LOG_SIDE_SLOPE]))
        {
            std::ostringstream oss;
            oss << "Log: Invalid log slope value '";
            oss << params[LOG_SIDE_SLOPE];
            oss << "', log slope cannot be 0.";
            throw Exception(oss.str().c_str());
        }
    }
    else
    {
        throw Exception("Log: Invalid direction.");
    }
}
}

LogOpData::LogOpData(double base, TransformDirection direction)
    : OpData()
    , m_base(base)
    , m_direction(direction)
{
    if (m_direction == TRANSFORM_DIR_UNKNOWN)
    {
        throw Exception("Cannot create Log op, unspecified transform direction.");
    }

    setParameters(DefaultValues::logSlope, DefaultValues::logOffset,
                  DefaultValues::linSlope, DefaultValues::linOffset);
}

LogOpData::LogOpData(double base,
                     const double(&logSlope)[3],
                     const double(&logOffset)[3],
                     const double(&linSlope)[3],
                     const double(&linOffset)[3],
                     TransformDirection direction)
    : OpData()
    , m_base(base)
    , m_direction(direction)
{
    if (m_direction == TRANSFORM_DIR_UNKNOWN)
    {
        throw Exception("Cannot create Log op, unspecified transform direction.");
    }
    setParameters(logSlope, logOffset, linSlope, linOffset);
}

LogOpData::LogOpData(TransformDirection dir,
                     double base,
                     const Params & redParams,
                     const Params & greenParams,
                     const Params & blueParams)
    : OpData()
    , m_redParams(redParams)
    , m_greenParams(greenParams)
    , m_blueParams(blueParams)
    , m_base(base)
    , m_direction(dir)
{
    if (m_direction == TRANSFORM_DIR_UNKNOWN)
    {
        throw Exception("Cannot create Log op, unspecified transform direction.");
    }
}

void LogOpData::setBase(double base)
{
    m_base = base;
}

double LogOpData::getBase() const
{
    return m_base;
}

void LogOpData::setValue(LogAffineParameter val, const double(&values)[3])
{
    m_redParams[val]   = values[0];
    m_greenParams[val] = values[1];
    m_blueParams[val]  = values[2];

}

void LogOpData::getValue(LogAffineParameter val, double(&values)[3]) const
{
    values[0] = m_redParams[val];
    values[1] = m_greenParams[val];
    values[2] = m_blueParams[val];
}

void LogOpData::setParameters(const double(&logSlope)[3],
                              const double(&logOffset)[3],
                              const double(&linSlope)[3],
                              const double(&linOffset)[3])
{
    m_redParams.resize(4);
    m_greenParams.resize(4);
    m_blueParams.resize(4);

    setValue(LOG_SIDE_SLOPE, logSlope);
    setValue(LOG_SIDE_OFFSET, logOffset);
    setValue(LIN_SIDE_SLOPE, linSlope);
    setValue(LIN_SIDE_OFFSET, linOffset);
}

void LogOpData::getParameters(double(&logSlope)[3],
                              double(&logOffset)[3],
                              double(&linSlope)[3],
                              double(&linOffset)[3]) const
{
    getValue(LOG_SIDE_SLOPE, logSlope);
    getValue(LOG_SIDE_OFFSET, logOffset);
    getValue(LIN_SIDE_SLOPE, linSlope);
    getValue(LIN_SIDE_OFFSET, linOffset);
}

LogOpData::~LogOpData()
{
}

void LogOpData::validate() const
{
    OpData::validate();

    ValidateParams(m_redParams, m_direction);
    ValidateParams(m_greenParams, m_direction);
    ValidateParams(m_blueParams, m_direction);

    if (m_base == 1.0)
    {
        std::ostringstream oss;
        oss << "Log: Invalid base value '";
        oss << m_base;
        oss << "', base cannot be 1.";
        throw Exception(oss.str().c_str());
    }
    else if (m_base <= 0.0)
    {
        std::ostringstream oss;
        oss << "Log: Invalid base value '";
        oss << m_base;
        oss << "', base must be greater than 0.";
        throw Exception(oss.str().c_str());
    }
}

bool LogOpData::isIdentity() const
{
    return false;
}

// Although a LogOp is never an identity, we still want to be able to replace a pair of logs that
// is effectively an identity (FWD/INV pairs) with an op that will emulate any clamping imposed
// by the original pair.
OpDataRcPtr LogOpData::getIdentityReplacement() const
{
    OpDataRcPtr resOp;
    if (isLog2() || isLog10())
    {
        if (m_direction == TRANSFORM_DIR_FORWARD)
        {
            // The first op logarithm is not defined for negative values.
            resOp = std::make_shared<RangeOpData>(
                0.,
                // Don't clamp high end.
                RangeOpData::EmptyValue(),
                0.,
                RangeOpData::EmptyValue());
        }
        else
        {
            // In principle, the power function is defined over the entire domain.
            // However, in practice the input to the following logarithm is clamped
            // to a very small positive number and this imposes a limit.
            // E.g., log10(FLOAT_MIN) = -37.93, but this is so small that it makes
            // more sense to consider it an exact inverse.
            resOp = std::make_shared<MatrixOpData>();
        }
    }
    else
    {
        if (m_direction == TRANSFORM_DIR_FORWARD)
        {
            // Minimum value allowed is -linOffset/linSlope so that linSlope*x+linOffset > 0.
            const double minValue = -m_redParams[LIN_SIDE_OFFSET] / m_redParams[LIN_SIDE_SLOPE];
            resOp = std::make_shared<RangeOpData>(minValue,
                                                  // Don't clamp high end.
                                                  RangeOpData::EmptyValue(),
                                                  minValue,
                                                  RangeOpData::EmptyValue());

        }
        else
        {
            resOp = std::make_shared<MatrixOpData>();
        }
    }
    return resOp;
}

bool LogOpData::isNoOp() const
{
    return false;
}

void LogOpData::finalize()
{
    AutoMutex lock(m_mutex);

    validate();

    std::ostringstream cacheIDStream;
    cacheIDStream << getID() << " ";

    cacheIDStream << TransformDirectionToString(m_direction) << " ";

    cacheIDStream << "Base "         << getBaseString(DefaultValues::FLOAT_DECIMALS) << " ";
    cacheIDStream << "LogSlope "     << getLogSlopeString(DefaultValues::FLOAT_DECIMALS) << " ";
    cacheIDStream << "LogOffset "    << getLogOffsetString(DefaultValues::FLOAT_DECIMALS) << " ";
    cacheIDStream << "LinearSlope "  << getLinSlopeString(DefaultValues::FLOAT_DECIMALS) << " ";
    cacheIDStream << "LinearOffset " << getLinOffsetString(DefaultValues::FLOAT_DECIMALS);

    m_cacheID = cacheIDStream.str();
}

bool LogOpData::operator==(const OpData& other) const
{
    if (!OpData::operator==(other)) return false;

    const LogOpData* log = static_cast<const LogOpData*>(&other);

    return (m_direction == log->m_direction
            && m_base == log->m_base
            && m_redParams == log->m_redParams
            && m_greenParams == log->m_greenParams
            && m_blueParams == log->m_blueParams);
}

LogOpDataRcPtr LogOpData::clone() const
{
    auto clone = std::make_shared<LogOpData>(m_direction,
                                             getBase(),
                                             getRedParams(),
                                             getGreenParams(),
                                             getBlueParams());
    clone->getFormatMetadata() = getFormatMetadata();
    return clone;
}

LogOpDataRcPtr LogOpData::inverse() const
{
    LogOpDataRcPtr invOp = clone();

    invOp->setDirection(GetInverseTransformDirection(m_direction));
    invOp->validate();

    // Note that any existing metadata could become stale at this point but
    // trying to update it is also challenging since inverse() is sometimes
    // called even during the creation of new ops.
    return invOp;
}

bool LogOpData::isInverse(ConstLogOpDataRcPtr & r) const
{
    if (GetInverseTransformDirection(m_direction) == r->m_direction
        && allComponentsEqual() && r->allComponentsEqual()
        && getRedParams() == r->getRedParams()
        && getBase() == r->getBase())
    {
        return true;
    }
 
    // Note:  Actually the R/G/B channels would not need to be equal for an
    // inverse, however, the identity replacement would get more complicated if
    // we allowed that case.  Since it is not a typical use-case, we don't
    // consider it an inverse since it is not easy to optimize out.
    return false;
}

bool LogOpData::allComponentsEqual() const
{
    // Comparing doubles is generally not a good idea, but in this case
    // it is ok to be strict.  Since the same operations are applied to
    // all components, if they started equal, they should remain equal.
    return m_redParams == m_greenParams && m_redParams == m_blueParams;
}

template <int index>
std::string getParameterString(const LogOpData & log, std::streamsize precision)
{
    static_assert(index >= 0 && index < 4, "Index has to be in [0..3]");
    std::ostringstream o;
    o.precision(precision);

    if (log.allComponentsEqual())
    {
        o << log.getRedParams()[index];
    }
    else
    {
        o << log.getRedParams()[index] << ", ";
        o << log.getGreenParams()[index] << ", ";
        o << log.getBlueParams()[index];
    }
    return o.str();
}

std::string LogOpData::getBaseString(std::streamsize precision) const
{
    std::ostringstream o;
    o.precision(precision);
    o << getBase();
    return o.str();
}

std::string LogOpData::getLogSlopeString(std::streamsize precision) const
{
    return getParameterString<LOG_SIDE_SLOPE>(*this, precision);
}

std::string LogOpData::getLinSlopeString(std::streamsize precision) const
{
    return getParameterString<LIN_SIDE_SLOPE>(*this, precision);
}

std::string LogOpData::getLinOffsetString(std::streamsize precision) const
{
    return getParameterString<LIN_SIDE_OFFSET>(*this, precision);
}

std::string LogOpData::getLogOffsetString(std::streamsize precision) const
{
    return getParameterString<LOG_SIDE_OFFSET>(*this, precision);
}

bool LogOpData::isLogBase(double base) const
{
    if (allComponentsEqual())
    {
        if (m_redParams[LOG_SIDE_SLOPE] == 1.0
            && m_redParams[LIN_SIDE_SLOPE] == 1.0
            && m_redParams[LIN_SIDE_OFFSET] == 0.0
            && m_redParams[LOG_SIDE_OFFSET] == 0.0
            && m_base == base)
        {
            return true;
        }
    }
    return false;
}

bool LogOpData::isLog2() const
{
    return isLogBase(2.0);
}

bool LogOpData::isLog10() const
{
    return isLogBase(10.0);
}

} // namespace OCIO_NAMESPACE

