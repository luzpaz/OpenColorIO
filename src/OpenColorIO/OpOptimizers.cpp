﻿// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the OpenColorIO Project.

#include <algorithm>
#include <iterator>
#include <sstream>

#include <OpenColorIO/OpenColorIO.h>

#include "BitDepthUtils.h"
#include "Logging.h"
#include "Op.h"
#include "ops/lut1d/Lut1DOp.h"
#include "ops/lut1d/Lut1DOpData.h"
#include "ops/range/RangeOpData.h"

namespace OCIO_NAMESPACE
{
namespace
{

bool HasFlag(OptimizationFlags flags, OptimizationFlags queryFlag)
{
    return (flags & queryFlag) == queryFlag;
}

bool IsPairInverseEnabled(OpData::Type type, OptimizationFlags flags)
{
    switch (type)
    {
    case OpData::CDLType:
        return HasFlag(flags, OPTIMIZATION_PAIR_IDENTITY_CDL);
    case OpData::ExposureContrastType:
        return HasFlag(flags, OPTIMIZATION_PAIR_IDENTITY_EXPOSURE_CONTRAST);
    case OpData::FixedFunctionType:
        return HasFlag(flags, OPTIMIZATION_PAIR_IDENTITY_FIXED_FUNCTION);
    case OpData::GammaType:
        return HasFlag(flags, OPTIMIZATION_PAIR_IDENTITY_GAMMA);
    case OpData::Lut1DType:
        return HasFlag(flags, OPTIMIZATION_PAIR_IDENTITY_LUT1D);
    case OpData::Lut3DType:
        return HasFlag(flags, OPTIMIZATION_PAIR_IDENTITY_LUT3D);
    case OpData::LogType:
        return HasFlag(flags, OPTIMIZATION_PAIR_IDENTITY_LOG);

    case OpData::ExponentType:
    case OpData::MatrixType:
    case OpData::RangeType:
        return false; // Use composition to optimize.

    default:
        // Other type are not controlled by a flag.
        return true;
    }
}

bool IsCombineEnabled(OpData::Type type, OptimizationFlags flags)
{
    // Some types are controlled by a flag.
    return (type == OpData::ExponentType && HasFlag(flags, OPTIMIZATION_COMP_EXPONENT)) ||
            (type == OpData::GammaType    && HasFlag(flags, OPTIMIZATION_COMP_GAMMA))    ||
            (type == OpData::Lut1DType    && HasFlag(flags, OPTIMIZATION_COMP_LUT1D))    ||
            (type == OpData::Lut3DType    && HasFlag(flags, OPTIMIZATION_COMP_LUT3D))    ||
            (type == OpData::MatrixType   && HasFlag(flags, OPTIMIZATION_COMP_MATRIX))   ||
            (type == OpData::RangeType    && HasFlag(flags, OPTIMIZATION_COMP_RANGE));
}

const int MAX_OPTIMIZATION_PASSES = 8;

void RemoveNoOpTypes(OpRcPtrVec & opVec)
{
    OpRcPtrVec::const_iterator iter = opVec.begin();
    while (iter != opVec.end())
    {
        ConstOpRcPtr o = (*iter);
        if (o->data()->getType() == OpData::NoOpType)
        {
            iter = opVec.erase(iter);
        }
        else
        {
            ++iter;
        }
    }
}

// Ops are preserved, dynamic properties are made non-dynamic.
void RemoveDynamicProperties(OpRcPtrVec & opVec)
{
    const size_t nbOps = opVec.size();
    for (size_t i = 0; i < nbOps; ++i)
    {
        auto & op = opVec[i];
        if (op->isDynamic())
        {
            // Optimization flag is tested before.
            auto replacedBy = op->clone();
            replacedBy->removeDynamicProperties();
            opVec[i] = replacedBy;
        }
    }
}

int RemoveNoOps(OpRcPtrVec & opVec)
{
    int count = 0;
    OpRcPtrVec::const_iterator iter = opVec.begin();
    while (iter != opVec.end())
    {
        if ((*iter)->isNoOp())
        {
            iter = opVec.erase(iter);
            ++count;
        }
        else
        {
            ++iter;
        }
    }
    return count;
}

int ReplaceIdentityOps(OpRcPtrVec & opVec, OptimizationFlags oFlags)
{
    int count = 0;

    // Remove any identity ops (other than gamma).
    const bool optIdentity = HasFlag(oFlags, OPTIMIZATION_IDENTITY);
    // Remove identity gamma ops (handled separately to give control over negative
    // alpha clamping).
    const bool optIdGamma = HasFlag(oFlags, OPTIMIZATION_IDENTITY_GAMMA);
    if (optIdentity || optIdGamma)
    {
        const size_t nbOps = opVec.size();
        for (size_t i = 0; i < nbOps; ++i)
        {
            ConstOpRcPtr op = opVec[i];
            const auto type = op->data()->getType();
            if (type != OpData::RangeType && // Do not replace a range identity.
                ((type == OpData::GammaType && optIdGamma) ||
                    (type != OpData::GammaType && optIdentity)) &&
                op->isIdentity())
            {
                // Optimization flag is tested before.
                auto replacedBy = op->getIdentityReplacement();
                opVec[i] = replacedBy;
                ++count;
            }
        }
    }
    return count;
}

int RemoveInverseOps(OpRcPtrVec & opVec, OptimizationFlags oFlags)
{
    int count      = 0;
    int firstindex = 0; // this must be a signed int

    while (firstindex < static_cast<int>(opVec.size() - 1))
    {
        ConstOpRcPtr op1 = opVec[firstindex];
        ConstOpRcPtr op2 = opVec[firstindex + 1];
        const auto type1 = op1->data()->getType();
        const auto type2 = op2->data()->getType();
        // The common case of inverse ops is to have a deep nesting:
        // ..., A, B, B', A', ...
        //
        // Consider the above, when firstindex reaches B:
        //
        //         |
        // ..., A, B, B', A', ...
        //
        // We will remove B and B'.
        // Firstindex remains pointing at the original location:
        //
        //         |
        // ..., A, A', ...
        //
        // We then decrement firstindex by 1,
        // to backstep and reconsider the A, A' case:
        //
        //      |            <-- firstindex decremented
        // ..., A, A', ...
        //

        if (type1 == type2 &&
            IsPairInverseEnabled(type1, oFlags) &&
            op1->isInverse(op2))
        {
            // When a pair of inverse ops is removed, we want the optimized ops to give the
            // same result as the original.  For certain ops such as Lut1D or Log this may
            // mean inserting a Range to emulate the clamping done by the original ops.
            auto replacedBy = op1->getIdentityReplacement();
            if (replacedBy->isNoOp())
            {
                opVec.erase(opVec.begin() + firstindex, opVec.begin() + firstindex + 2);
                firstindex = std::max(0, firstindex - 1);
            }
            else
            {
                // Forward + inverse does clamp.
                opVec[firstindex] = replacedBy;
                opVec.erase(opVec.begin() + firstindex + 1);
                ++firstindex;
            }
            ++count;
        }
        else
        {
            ++firstindex;
        }
    }

    return count;
}

int CombineOps(OpRcPtrVec & opVec, OptimizationFlags oFlags)
{
    int count      = 0;
    int firstindex = 0; // this must be a signed int

    OpRcPtrVec tmpops;

    while (firstindex < static_cast<int>(opVec.size() - 1))
    {
        ConstOpRcPtr op1 = opVec[firstindex];
        ConstOpRcPtr op2 = opVec[firstindex + 1];
        const auto type1 = op1->data()->getType();

        if (IsCombineEnabled(type1, oFlags) && op1->canCombineWith(op2))
        {
            tmpops.clear();
            op1->combineWith(tmpops, op2);

            // tmpops may have any number of ops in it. (0, 1, 2, ...)
            // (size 0 would occur only if the combination results in a no-op).
            //
            // No matter the number, we need to swap them in for the original ops.

            // Erase the initial two ops we've combined.
            opVec.erase(opVec.begin() + firstindex, opVec.begin() + firstindex + 2);

            // Insert the new ops (which may be empty) at this location.
            opVec.insert(opVec.begin() + firstindex, tmpops.begin(), tmpops.end());

            // Decrement firstindex by 1,
            // to backstep and reconsider the A, A' case.
            // See RemoveInverseOps for the full discussion of
            // why this is appropriate.
            firstindex = std::max(0, firstindex - 1);

            // We've done something so increment the count!
            ++count;
        }
        else
        {
            ++firstindex;
        }
    }

    return count;
}

int RemoveLeadingClampIdentity(OpRcPtrVec & opVec)
{
    int count = 0;
    OpRcPtrVec::const_iterator iter = opVec.begin();
    while (iter != opVec.end())
    {
        ConstOpRcPtr o = (*iter);
        auto oData = o->data();
        if (oData->getType() == OpData::RangeType && oData->isIdentity())
        {
            iter++;
            ++count;
        }
        else
        {
            break;
        }
    }
    if (count != 0)
    {
        OpRcPtrVec::const_iterator iter = opVec.begin() + count;
        opVec.erase(opVec.begin(), iter);
    }
    return count;
}

int RemoveTrailingClampIdentity(OpRcPtrVec & opVec)
{
    int count = 0;
    int current = static_cast<int>(opVec.size()) - 1;
    while (current >= 0)
    {
        ConstOpRcPtr o = opVec[current];
        auto oData = o->data();
        if (oData->getType() == OpData::RangeType && oData->isIdentity())
        {
            ++count;
            --current;
        }
        else
        {
            break;
        }
    }

    if (count != 0)
    {
        OpRcPtrVec::const_iterator iter = opVec.begin() + (current + 1);
        opVec.erase(iter, opVec.end());
    }
    return count;
}

// (Note: the term "separable" in mathematics refers to a multi-dimensional
// function where the dimensions are independent of each other.)
//
// The goal here is to speed up calculations by replacing the contiguous separable
// (channel independent) list of ops from the first op onwards with a single
// LUT1D whose domain is sampled for the target bit depth.  A typical use-case
// would be a list of ops that starts with a gamma that is processing integer 10i
// pixels.  Rather than convert to float and apply the power function on each
// pixel, it's better to build a 1024 entry LUT and just do a look-up.
//
unsigned FindSeparablePrefix(const OpRcPtrVec & ops)
{
    unsigned prefixLen = 0;

    // Loop over the ops until we get to one that cannot be combined.
    //
    // Note: For some ops such as Matrix and CDL, the separability depends upon
    //       the parameters.
    for (const auto & op : ops)
    {
        // In OCIO, the hasChannelCrosstalk method returns false for separable ops.
        if (op->hasChannelCrosstalk() || op->isDynamic())
        {
            break;
        }

        // Op is separable, keep going.
        prefixLen++;
    }

    // If the only op is a 1D LUT, there is actually nothing to optimize
    // so set the length to 0.  (This also avoids an infinite loop.)
    // (If it is an inverse 1D LUT, proceed since we want to replace it with a 1D LUT.)
    if (prefixLen == 1)
    {
        ConstOpRcPtr constOp0 = ops[0];
        auto opData = constOp0->data();
        if (opData->getType() == OpData::Lut1DType)
        {
            auto lutData = OCIO_DYNAMIC_POINTER_CAST<const Lut1DOpData>(opData);
            if (lutData->getDirection() == TRANSFORM_DIR_FORWARD)
            {
                return 0;
            }
        }
    }

    // Some ops are so fast that it may not make sense to replace just one of those.
    // E.g., if it's just a single matrix, it may not be faster to replace it with a LUT.
    // So make sure there are some more expensive ops to combine.
    unsigned expensiveOps = 0U;
    for (unsigned i = 0; i < prefixLen; ++i)
    {
        auto op = ops[i];

        if (op->hasChannelCrosstalk())
        {
            // Non-separable ops (should never get here).
            throw Exception("Non-separable op.");
        }

        ConstOpRcPtr constOp = op;
        switch (constOp->data()->getType())
        {
            // Potentially separable, but inexpensive ops.
            // TODO: Perhaps a LUT is faster once the conversion to float is considered?
            case OpData::MatrixType:
            case OpData::RangeType:
            {
                break;
            }

            // Potentially separable, and more expensive.
            default:
            {
                expensiveOps++;
                break;
            }
        }
    }

    if (expensiveOps == 0)
    {
        return 0;
    }

    // TODO: The main source of potential lossiness is where there is a 1D LUT
    // that has extended range values followed by something that clamps.  In
    // that case, the clamp would get baked into the LUT entries and therefore
    // result in a different interpolated value.  Could look for that case and
    // turn off the optimization.

    return prefixLen;
}

// Use functional composition to replace a string of separable ops at the head of
// the op list with a single 1D LUT that is built to do a look-up for the input bit-depth.
void OptimizeSeparablePrefix(OpRcPtrVec & ops, BitDepth in)
{
    if (ops.empty())
    {
        return;
    }

    // TODO: Investigate whether even the F32 case could be sped up via interpolating 
    //       in a half-domain Lut1D (e.g. replacing a string of exponent, log, etc.).
    if (in == BIT_DEPTH_F32 || in == BIT_DEPTH_UINT32)
    {
        return;
    }

    const unsigned prefixLen = FindSeparablePrefix(ops);
    if (prefixLen == 0)
    {
        return; // Nothing to do.
    }

    OpRcPtrVec prefixOps;
    for (unsigned i = 0; i < prefixLen; ++i)
    {
        prefixOps.push_back(ops[i]->clone());
    }

    // Make a domain for the LUT.  (Will be half-domain for target == 16f.)
    Lut1DOpDataRcPtr newDomain = Lut1DOpData::MakeLookupDomain(in);

    // Send the domain through the prefix ops.
    // Note: This sets the outBitDepth of newDomain to match prefixOps.
    Lut1DOpData::ComposeVec(newDomain, prefixOps);

    // Remove the prefix ops.
    ops.erase(ops.begin(), ops.begin() + prefixLen);

    // Insert the new LUT to replace the prefix ops.
    OpRcPtrVec lutOps;
    CreateLut1DOp(lutOps, newDomain, TRANSFORM_DIR_FORWARD);

    ops.insert(ops.begin(), lutOps.begin(), lutOps.end());
}
} // namespace

void OptimizeOpVec(OpRcPtrVec & ops,
                    const BitDepth & inBitDepth,
                    const BitDepth & outBitDepth,
                    OptimizationFlags oFlags)
{
    if (ops.empty())
        return;

    if (IsDebugLoggingEnabled())
    {
        LogDebug("Optimizing Op Vec...");
        LogDebug(SerializeOpVec(ops, 4));
    }

    // NoOpType can be removed.
    RemoveNoOpTypes(ops);

    if (oFlags == OPTIMIZATION_NONE)
    {
        return;
    }

    // Keep dynamic ops using their default values. Remove the ability to modify
    // them dynamically.
    const auto removeDynamic = HasFlag(oFlags, OPTIMIZATION_NO_DYNAMIC_PROPERTIES);
    if (removeDynamic)
    {
        RemoveDynamicProperties(ops);
    }

    // As the input and output bit-depths represent the color processing
    // request and they may be altered by the following optimizations,
    // preserve their values.

    const auto originalSize = ops.size();
    int total_noops         = 0;
    int total_identityops   = 0;
    int total_inverseops    = 0;
    int total_combines      = 0;
    int passes              = 0;

    const bool optimizeIdentity = HasFlag(oFlags, OPTIMIZATION_IDENTITY);

    while (passes <= MAX_OPTIMIZATION_PASSES)
    {
        int noops       = optimizeIdentity ? RemoveNoOps(ops) : 0;
        int identityops = ReplaceIdentityOps(ops, oFlags);
        int inverseops  = RemoveInverseOps(ops, oFlags);
        int combines    = CombineOps(ops, oFlags);

        if (noops + identityops + inverseops + combines == 0)
        {
            // No optimization progress was made, so stop trying.
            break;
        }

        total_noops += noops;
        total_identityops += identityops;
        total_inverseops += inverseops;
        total_combines += combines;

        ++passes;
    }

    if (!ops.empty())
    {
        if (!IsFloatBitDepth(inBitDepth))
        {
            RemoveLeadingClampIdentity(ops);
        }
        if (!IsFloatBitDepth(outBitDepth))
        {
            RemoveTrailingClampIdentity(ops);
        }

        if(HasFlag(oFlags, OPTIMIZATION_COMP_SEPARABLE_PREFIX))
        {
            OptimizeSeparablePrefix(ops, inBitDepth);
        }
    }

    OpRcPtrVec::size_type finalSize = ops.size();

    if (passes == MAX_OPTIMIZATION_PASSES)
    {
        std::ostringstream os;
        os << "The max number of passes, " << passes << ", ";
        os << "was reached during optimization. This is likely a sign ";
        os << "that either the complexity of the color transform is ";
        os << "very high, or that some internal optimizers are in conflict ";
        os << "(undo-ing / redo-ing the other's results).";
        LogDebug(os.str());
    }

    if (IsDebugLoggingEnabled())
    {
        std::ostringstream os;
        os << "Optimized ";
        os << originalSize << "->" << finalSize << ", ";
        os << passes << " passes, ";
        os << total_noops << " noops removed, ";
        os << total_identityops << " identity ops replaced, ";
        os << total_inverseops << " inverse ops removed\n";
        os << total_combines << " ops combines\n";
        os << SerializeOpVec(ops, 4);
        LogDebug(os.str());
    }
}

} // namespace OCIO_NAMESPACE

