// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the OpenColorIO Project.

#include <cstdio>
#include <iostream>
#include <fstream>
#include <sstream>

#include <OpenColorIO/OpenColorIO.h>

#include "expat.h"
#include "fileformats/ctf/CTFTransform.h"
#include "fileformats/ctf/CTFReaderHelper.h"
#include "fileformats/ctf/CTFReaderUtils.h"
#include "fileformats/xmlutils/XMLReaderHelper.h"
#include "fileformats/xmlutils/XMLReaderUtils.h"
#include "fileformats/xmlutils/XMLWriterUtils.h"
#include "ops/lut1d/Lut1DOp.h"
#include "ops/lut3d/Lut3DOp.h"
#include "ops/range/RangeOp.h"
#include "OpBuilders.h"
#include "ops/noop/NoOps.h"
#include "Platform.h"
#include "pystring/pystring.h"
#include "transforms/FileTransform.h"


/*

This file format reader supports the Academy/ASC Common LUT Format (CLF) and
the Autodesk Color Transform Format (CTF).

The Academy/ASC Common LUT format was an initiative to bring vendors together
to agree on a common LUT format for this industry.  Support for CLF is a
requirement in order to obtain ACES Logo Certification from the Academy (in
several product categories).  CLF files are expressed using XML.  The spec,
AMPAS S-2014-006, is available from:
<https://acescentral.com/t/aces-documentation/53>

The Autodesk CTF format is based on the Academy/ASC CLF format and adds several
operators that allow higher quality results by avoiding the need to bake
certain common functions into LUTs.  This ranges from simple power functions
to more complicated operators needed to implement very accurate yet compact
ACES Output Transforms.

Autodesk CTF was also designed to be able to losslessly serialize any OCIO
Processor to a self-contained XML file.  This opens up some useful workflow
options for sharing specific color transformations.  As a result, all native
OCIO ops have a lossless mapping into CTF as XML process nodes.  (This is
sometimes also useful for trouble-shooting.)

The CTF format is a superset of the CLF format, hence the use of a common
parser.  Aside from the file extension, the two formats may be distinguished
based on the version attribute in the root ProcessList element.  A CLF file
uses the attribute "compCLFversion" whereas a CTF file uses "version".

The parser has been carefully designed to assist users in trouble-shooting
problems with files that won't load.  A detailed error message is printed,
along with the line number (similar to a compiler).  There are also extensive
unit tests to ensure robustness.

Note:  One frequent point of confusion regarding the CLF syntax relates to the
inBitDepth and outBitDepth attributes in each process node.  These bit-depths
DO NOT specify the processing precision, nor do they specify the bit-depth of
the images that are input or output from the transform.  The only function of
these bit-depth attributes is to interpret the scaling of the parameter values
in a given process node.  This is helpful since, e.g., it avoids the need for
heuristics to guess whether LUT values are scaled to 10 or 12 bits.  These
attributes must always be present and must match at the interface between
adjacent process nodes.  That said, in some cases, one or both may not actually
affect the results if they are not required to interpret the scaling of the
parameters.  For example, the ASC_CDL parameters are always stored in
normalized form and hence the bit-depths, while required, do not affect their
interpretation.  On the other hand, the interpretation of the parameters in
a Matrix op is affected by both the in and out bit-depths.  It should be noted
that although the bit-depths imply a certain scaling, they never impose a
clamping or quantization, e.g. a LUT array with an outBitDepth of '10i' is free
to contain values outside of [0,1023] and to use fractional values.

For the OCIO implementation, we tried to avoid bringing the complexity of
proper bit-depth handling into the design of the ops.  Therefore, the objects
always store the values from LUTs, matrices, etc. in normalized form.  In other
words, as if the CLF file had all its bit-depths set to "32f".  However we do
provide FileBitDepth getters that will return the original scaling read from a
CLF file, and setters that will control the scaling of values to be written to
a CLF file.  These getters/setters are only provided for the transforms/ops
(LUT1D, LUT3D, Matrix, and Range) where a CLF file is allowed to store the
parameters in an unnormalized form.
*/

namespace OCIO_NAMESPACE
{

namespace
{

class LocalCachedFile : public CachedFile
{
public:
    LocalCachedFile ()
    {
    };
    ~LocalCachedFile() {};

    CTFReaderTransformPtr m_transform;
    std::string m_filePath;

};

typedef OCIO_SHARED_PTR<LocalCachedFile> LocalCachedFileRcPtr;

class LocalFileFormat : public FileFormat
{
public:

    ~LocalFileFormat() {}

    void getFormatInfo(FormatInfoVec & formatInfoVec) const override;

    CachedFileRcPtr read(std::istream & istream,
                         const std::string & fileName) const override;

    void buildFileOps(OpRcPtrVec & ops,
                      const Config & config,
                      const ConstContextRcPtr & context,
                      CachedFileRcPtr untypedCachedFile,
                      const FileTransform & fileTransform,
                      TransformDirection dir) const override;

    void bake(const Baker & baker,
              const std::string & formatName,
              std::ostream & ostream) const override;

    void write(const OpRcPtrVec & ops,
               const FormatMetadataImpl & metadata,
               const std::string & formatName,
               std::ostream & /*ostream*/) const override;
};

void LocalFileFormat::getFormatInfo(FormatInfoVec & formatInfoVec) const
{
    FormatInfo info;
    info.name = FILEFORMAT_CLF;
    info.extension = "clf";
    info.capabilities = FORMAT_CAPABILITY_READ |
                        FORMAT_CAPABILITY_BAKE |
                        FORMAT_CAPABILITY_WRITE;
    formatInfoVec.push_back(info);

    FormatInfo info2;
    info2.name = FILEFORMAT_CTF;
    info2.extension = "ctf";
    info2.capabilities = FORMAT_CAPABILITY_READ |
                         FORMAT_CAPABILITY_BAKE |
                         FORMAT_CAPABILITY_WRITE;
    formatInfoVec.push_back(info2);
}

class XMLParserHelper
{
public:
    XMLParserHelper() = delete;
    XMLParserHelper(const XMLParserHelper &) = delete;

    XMLParserHelper(const std::string & fileName)
        : m_parser(XML_ParserCreate(nullptr))
        , m_fileName(fileName)
    {
        XML_SetUserData(m_parser, this);
        XML_SetElementHandler(m_parser, StartElementHandler, EndElementHandler);
        XML_SetCharacterDataHandler(m_parser, CharacterDataHandler);

        std::string root, extension;
        pystring::os::path::splitext(root, extension, m_fileName);
        m_isCLF = pystring::lower(extension) == ".clf";
    }

    ~XMLParserHelper()
    {
        XML_ParserFree(m_parser);
    }

    void Parse(std::istream & istream)
    {
        std::string line;
        m_lineNumber = 0;
        while (istream.good())
        {
            std::getline(istream, line);
            // Add back the newline character. Parsing will copy the line in a
            // buffer up to the length of the line without the null termination.
            // Our code will be called back to parse the buffer into a number.
            // Code uses strtod. The buffer has to be delimited so that strtod
            // does not access it after its length.
            line.push_back('\n');
            ++m_lineNumber;

            Parse(line, !istream.good());
        }

        if (!m_elms.empty())
        {
            std::string error("CTF/CLF parsing error (no closing tag for '");
            error += m_elms.back()->getName().c_str();
            error += "). ";
            throwMessage(error);
        }

        const CTFReaderTransformPtr& pT = getTransform();
        if (pT.use_count() == 0)
        {
            static const std::string error(
                "CTF/CLF parsing error: Invalid transform. ");
            throwMessage(error);
        }

        if (pT->getOps().empty())
        {
            static const std::string error(
                "CTF/CLF parsing error: No color operator in file. ");
            throwMessage(error);
        }
    }

    void Parse(const std::string & buffer, bool lastLine)
    {
        const int done = lastLine?1:0;

        if (XML_STATUS_ERROR == XML_Parse(m_parser,
                                          buffer.c_str(),
                                          (int)buffer.size(), done))
        {
            XML_Error eXpatErrorCode = XML_GetErrorCode(m_parser);
            if (eXpatErrorCode == XML_ERROR_TAG_MISMATCH)
            {
                if (!m_elms.empty())
                {
                    // It could be an Op or an Attribute.
                    std::string error(
                        "CTF/CLF parsing error (no closing tag for '");
                    error += m_elms.back()->getName().c_str();
                    error += "'). ";
                    throwMessage(error);
                }
                else
                {
                    // Completely lost, something went wrong,
                    // but nothing detected with the stack.
                    static const std::string error(
                        "CTF/CLF parsing error (unbalanced element tags). ");
                    throwMessage(error);
                }
            }
            else
            {
                std::string error("CTF/CLF parsing error: ");
                error += XML_ErrorString(XML_GetErrorCode(m_parser));
                throwMessage(error);
            }
        }

    }

    CTFReaderTransformPtr getTransform()
    {
        return m_transform;
    }

private:

    void AddOpReader(CTFReaderOpElt::Type type, const char * xmlTag)
    {
        if (m_elms.size() != 1)
        {
            std::stringstream ss;
            ss << ": The " << xmlTag;
            ss << "'s parent can only be a Transform";

            m_elms.push_back(std::make_shared<XmlReaderDummyElt>(
                xmlTag,
                (m_elms.empty() ? 0 : m_elms.back()),
                getXmLineNumber(),
                getXmlFilename(),
                ss.str().c_str()));
        }
        else
        {
            ElementRcPtr pElt = m_elms.back();

            auto pT = std::dynamic_pointer_cast<CTFReaderTransformElt>(pElt);
            CTFReaderOpEltRcPtr pOp =
                CTFReaderOpElt::GetReader(type, pT->getVersion());

            if (!pOp)
            {
                std::stringstream ss;
                ss << "Unsupported transform file version '";
                ss << pT->getVersion() << "' for operator '" << xmlTag;
                throwMessage(ss.str());
            }

            pOp->setContext(xmlTag, m_transform,
                            getXmLineNumber(), getXmlFilename());

            m_elms.push_back(pOp);
        }
    }

    void throwMessage(const std::string & error) const
    {
        std::ostringstream os;
        os << "Error parsing CTF/CLF file (";
        os << m_fileName.c_str() << "). ";
        os << "Error is: " << error.c_str();
        os << ". At line (" << m_lineNumber << ")";
        throw Exception(os.str().c_str());
    }

    // Determines if the element name is supported in the current context.
    static bool SupportedElement(const char * name,
                                 ElementRcPtr & parent,
                                 const char * tag,
                                 const char * parentName,
                                 bool & recognizedName)
    {
        if (name && *name && tag && *tag)
        {
            if (0 == Platform::Strcasecmp(name, tag))
            {
                recognizedName |= true;

                if (!parentName || !strlen(parentName) ||
                    (parent &&
                     0 == Platform::Strcasecmp(parent->getName().c_str(),
                                               parentName)))
                {
                    return true;
                }
            }
        }

        return false;
    }

    static bool SupportedElement(const char * name,
                                 ElementRcPtr & parent,
                                 const std::vector<const char *> & tags,
                                 const char * parentName,
                                 bool & recognizedName)
    {
        if (name && *name)
        {
            const size_t numTags(tags.size());
            size_t i = 0;
            for (; i<numTags; ++i)
            {
                if (0 == Platform::Strcasecmp(name, tags[i]))
                {
                    recognizedName |= true;
                    break;
                }
            }

            // If found name in the tag list, test the parent name.
            if (i < numTags)
            {
                if (!parentName || !strlen(parentName) ||
                    (parent && 0 == Platform::Strcasecmp(parent->getName().c_str(), parentName)))
                {
                    return true;
                }
            }
        }

        return false;
    }

    // Start the parsing of one element.
    static void StartElementHandler(void * userData,
                                    const XML_Char * name,
                                    const XML_Char ** atts)
    {
        static const std::vector<const char *> rangeSubElements = {
            TAG_MIN_IN_VALUE,
            TAG_MAX_IN_VALUE,
            TAG_MIN_OUT_VALUE,
            TAG_MAX_OUT_VALUE
        };

        static const std::vector<const char *> sopSubElements = {
            TAG_SLOPE,
            TAG_OFFSET,
            TAG_POWER
        };

        XMLParserHelper * pImpl = (XMLParserHelper*)userData;

        if (!pImpl || !name || !*name)
        {
            if (!pImpl)
            {
                throw Exception("Internal CTF/CLF parser error.");
            }
            else
            {
                pImpl->throwMessage("Internal CTF/CLF parser error. ");
            }
        }

        if (!pImpl->m_elms.empty())
        {
            // Check if we are still processing a metadata structure.
            ElementRcPtr pElt = pImpl->m_elms.back();

            auto pMD = std::dynamic_pointer_cast<CTFReaderMetadataElt>(pElt);

            if (pMD)
            {
                pImpl->m_elms.push_back(
                    std::make_shared<CTFReaderMetadataElt>(
                        name,
                        pMD,
                        pImpl->m_lineNumber,
                        pImpl->m_fileName));

                pImpl->m_elms.back()->start(atts);
                return;
            }
        }

        // Handle the ProcessList element or its children (the ops).
        if (0 == Platform::Strcasecmp(name, TAG_PROCESS_LIST))
        {
            if (pImpl->m_transform.get())
            {
                ElementRcPtr pElt = pImpl->m_elms.front();
                auto pT = std::dynamic_pointer_cast<CTFReaderTransformElt>(pElt);

                pImpl->m_elms.push_back(
                    std::make_shared<XmlReaderDummyElt>(
                        name, pT,
                        pImpl->getXmLineNumber(),
                        pImpl->getXmlFilename(),
                        ": The Transform already exists"));
            }
            else
            {
                CTFReaderTransformEltRcPtr pT
                    = std::make_shared<CTFReaderTransformElt>(
                        name,
                        pImpl->getXmLineNumber(),
                        pImpl->getXmlFilename(),
                        pImpl->IsCLF());

                pImpl->m_elms.push_back(pT);
                pImpl->m_transform = pT->getTransform();
            }
        }

        // Handle all Ops.
        else
        {
            ElementRcPtr pElt;
            if (pImpl->m_elms.size())
            {
                pElt = pImpl->m_elms.back();
            }

            // Safety check to try and ensure that all new elements will get handled here.
            static_assert(CTFReaderOpElt::NoType == 13, "Need to handle new type here");

            // Will allow to give better error feedback to the user if the
            // element name is not handled. If any case recognizes the name,
            // but the element is not in the correct context (under the correct
            // parent), then the recognizedName boolean will be true.
            bool recognizedName = false;

            // For each possible element name, test against a tag name and a
            // current parent name to determine if the element should be handled.
            if (SupportedElement(name, pElt, TAG_ACES,
                                      TAG_PROCESS_LIST, recognizedName))
            {
                pImpl->AddOpReader(CTFReaderOpElt::ACESType, name);
            }
            else if (SupportedElement(name, pElt, TAG_CDL,
                                      TAG_PROCESS_LIST, recognizedName))
            {
                pImpl->AddOpReader(CTFReaderOpElt::CDLType, name);
            }
            else if (SupportedElement(name, pElt, TAG_EXPOSURE_CONTRAST,
                                      TAG_PROCESS_LIST, recognizedName))
            {
                pImpl->AddOpReader(CTFReaderOpElt::ExposureContrastType, name);
            }
            else if (SupportedElement(name, pElt, TAG_FIXED_FUNCTION,
                                      TAG_PROCESS_LIST, recognizedName))
            {
                pImpl->AddOpReader(CTFReaderOpElt::FixedFunctionType, name);
            }
            else if (SupportedElement(name, pElt, TAG_GAMMA,
                                      TAG_PROCESS_LIST, recognizedName))
            {
                pImpl->AddOpReader(CTFReaderOpElt::GammaType, name);
            }
            else if (SupportedElement(name, pElt, TAG_INVLUT1D,
                                      TAG_PROCESS_LIST, recognizedName))
            {
                pImpl->AddOpReader(CTFReaderOpElt::InvLut1DType, name);
            }
            else if (SupportedElement(name, pElt, TAG_INVLUT3D,
                                      TAG_PROCESS_LIST, recognizedName))
            {
                pImpl->AddOpReader(CTFReaderOpElt::InvLut3DType, name);
            }
            else if (SupportedElement(name, pElt, TAG_LOG,
                                      TAG_PROCESS_LIST, recognizedName))
            {
                pImpl->AddOpReader(CTFReaderOpElt::LogType, name);
            }
            else if (SupportedElement(name, pElt, TAG_LUT1D,
                                      TAG_PROCESS_LIST, recognizedName))
            {
                pImpl->AddOpReader(CTFReaderOpElt::Lut1DType, name);
            }
            else if (SupportedElement(name, pElt, TAG_LUT3D,
                                      TAG_PROCESS_LIST, recognizedName))
            {
                pImpl->AddOpReader(CTFReaderOpElt::Lut3DType, name);
            }
            else if (SupportedElement(name, pElt, TAG_MATRIX,
                                      TAG_PROCESS_LIST, recognizedName))
            {
                pImpl->AddOpReader(CTFReaderOpElt::MatrixType, name);
            }
            else if (SupportedElement(name, pElt, TAG_RANGE,
                                      TAG_PROCESS_LIST, recognizedName))
            {
                pImpl->AddOpReader(CTFReaderOpElt::RangeType, name);
            }
            else if (SupportedElement(name, pElt, TAG_REFERENCE,
                                      TAG_PROCESS_LIST, recognizedName))
            {
                pImpl->AddOpReader(CTFReaderOpElt::ReferenceType, name);
            }
            // TODO: handle other ops from syncolor.

            // Handle other elements that are transform-level metadata or parts of ops.
            else
            {
                auto pT = std::dynamic_pointer_cast<CTFReaderTransformElt>(pElt);

                auto pContainer =
                    std::dynamic_pointer_cast<XmlReaderContainerElt>(pElt);
                if (!pContainer)
                {
                    pImpl->m_elms.push_back(
                        std::make_shared<XmlReaderDummyElt>(
                            name,
                            pElt,
                            pImpl->getXmLineNumber(),
                            pImpl->getXmlFilename(),
                            nullptr));
                }
                else if (SupportedElement(name, pElt, TAG_ACES_PARAMS,
                                          TAG_ACES, recognizedName))
                {
                    pImpl->m_elms.push_back(
                        std::make_shared<CTFReaderACESParamsElt>(
                            name,
                            pContainer,
                            pImpl->getXmLineNumber(),
                            pImpl->getXmlFilename()));
                }
                else if (SupportedElement(name, pElt, TAG_ARRAY, TAG_LUT1D, recognizedName) ||
                         SupportedElement(name, pElt, TAG_ARRAY, TAG_INVLUT1D, recognizedName) ||
                         SupportedElement(name, pElt, TAG_ARRAY, TAG_LUT3D, recognizedName) ||
                         SupportedElement(name, pElt, TAG_ARRAY, TAG_INVLUT3D, recognizedName) ||
                         SupportedElement(name, pElt, TAG_ARRAY, TAG_MATRIX, recognizedName))
                {
                    auto pA = std::dynamic_pointer_cast<CTFArrayMgt>(pContainer);
                    if (!pA || pA->isCompleted())
                    {
                        if (!pA)
                        {
                            pImpl->m_elms.push_back(
                                std::make_shared<XmlReaderDummyElt>(
                                    name,
                                    (pImpl->m_elms.empty() ? 0 : pImpl->m_elms.back()),
                                    pImpl->getXmLineNumber(),
                                    pImpl->getXmlFilename(),
                                    ": Array not allowed in this element"));
                        }
                        else
                        {
                            pImpl->m_elms.push_back(
                                std::make_shared<XmlReaderDummyElt>(
                                    name,
                                    (pImpl->m_elms.empty() ? 0 : pImpl->m_elms.back()),
                                    pImpl->getXmLineNumber(),
                                    pImpl->getXmlFilename(),
                                    ": Only one Array allowed per op"));
                        }
                    }
                    else
                    {
                        pImpl->m_elms.push_back(
                            std::make_shared<CTFReaderArrayElt>(
                                name, pContainer,
                                pImpl->getXmLineNumber(),
                                pImpl->getXmlFilename()));
                    }
                }
                else if (SupportedElement(name, pElt, TAG_DESCRIPTION, "", recognizedName) ||
                         SupportedElement(name, pElt, METADATA_INPUT_DESCRIPTION, TAG_CDL, recognizedName) ||
                         SupportedElement(name, pElt, METADATA_VIEWING_DESCRIPTION, TAG_CDL, recognizedName))
                {
                    pImpl->m_elms.push_back(
                        std::make_shared<XmlReaderDescriptionElt>(
                            name,
                            pContainer,
                            pImpl->getXmLineNumber(),
                            pImpl->getXmlFilename()));
                }
                // Dynamic Property is valid under any operator parent. First
                // test if the tag is supported to set the recognizedName 
                // accordingly, without testing for parents. Test for the
                // parent type prior to testing the name.
                else if (SupportedElement(name, pElt, TAG_DYNAMIC_PARAMETER,
                                          "", recognizedName) &&
                         std::dynamic_pointer_cast<CTFReaderOpElt>(pContainer))
                {
                    pImpl->m_elms.push_back(
                        std::make_shared<CTFReaderDynamicParamElt>(
                            name,
                            pContainer,
                            pImpl->getXmLineNumber(),
                            pImpl->getXmlFilename()));
                }
                else if (SupportedElement(name, pElt, TAG_EC_PARAMS,
                                          TAG_EXPOSURE_CONTRAST, recognizedName))
                {
                    pImpl->m_elms.push_back(
                        std::make_shared<CTFReaderECParamsElt>(
                            name,
                            pContainer,
                            pImpl->getXmLineNumber(),
                            pImpl->getXmlFilename()));
                }
                else if (SupportedElement(name, pElt, TAG_GAMMA_PARAMS,
                                          TAG_GAMMA, recognizedName))
                {
                    CTFReaderGammaElt * pGamma = dynamic_cast<CTFReaderGammaElt*>(pContainer.get());
                    pImpl->m_elms.push_back(
                        pGamma->createGammaParamsElt(
                            name,
                            pContainer,
                            pImpl->getXmLineNumber(),
                            pImpl->getXmlFilename()));
                }
                else if (SupportedElement(name, pElt, TAG_INDEX_MAP, TAG_LUT1D, recognizedName) ||
                         SupportedElement(name, pElt, TAG_INDEX_MAP, TAG_LUT3D, recognizedName))
                {
                    auto pA = std::dynamic_pointer_cast<CTFIndexMapMgt>(pContainer);
                    if (!pA || pA->isCompletedIM())
                    {
                        if (!pA)
                        {
                            pImpl->m_elms.push_back(
                                std::make_shared<XmlReaderDummyElt>(
                                    name,
                                    (pImpl->m_elms.empty() ? 0 : pImpl->m_elms.back()),
                                    pImpl->getXmLineNumber(),
                                    pImpl->getXmlFilename(),
                                    ": IndexMap not allowed in this element"));
                        }
                        else
                        {
                            // Currently only support a single IndexMap per LUT.
                            pImpl->throwMessage("Only one IndexMap allowed per LUT. ");
                        }
                    }
                    else
                    {
                        pImpl->m_elms.push_back(
                            std::make_shared<CTFReaderIndexMapElt>(
                                name, pContainer,
                                pImpl->getXmLineNumber(),
                                pImpl->getXmlFilename()));
                    }

                }
                else if (SupportedElement(name, pElt, TAG_INFO, 
                                          TAG_PROCESS_LIST, recognizedName))
                {
                    pImpl->m_elms.push_back(
                        std::make_shared<CTFReaderInfoElt>(
                            name,
                            pContainer,
                            pImpl->getXmLineNumber(),
                            pImpl->getXmlFilename()));
                }
                else if (SupportedElement(name, pElt, METADATA_INPUT_DESCRIPTOR,
                                          TAG_PROCESS_LIST, recognizedName))
                {
                    pImpl->m_elms.push_back(
                        std::make_shared<CTFReaderInputDescriptorElt>(
                            name,
                            pContainer,
                            pImpl->getXmLineNumber(),
                            pImpl->getXmlFilename()));
                }
                else if (SupportedElement(name, pElt, TAG_LOG_PARAMS,
                                          TAG_LOG, recognizedName))
                {
                    auto pLog = std::dynamic_pointer_cast<CTFReaderLogElt>(pContainer);
                    const auto style = pLog->getCTFParams().m_style;
                    if (!(style == LogUtil::LOG_TO_LIN ||
                          style == LogUtil::LIN_TO_LOG))
                    {
                        pImpl->m_elms.push_back(
                            std::make_shared<XmlReaderDummyElt>(
                                name,
                                (pImpl->m_elms.empty() ? 0 : pImpl->m_elms.back()),
                                pImpl->getXmLineNumber(),
                                pImpl->getXmlFilename(),
                                ": Log Params not allowed in this element"));
                    }
                    else
                    {
                        pImpl->m_elms.push_back(
                            std::make_shared<CTFReaderLogParamsElt>(name,
                                pContainer,
                                pImpl->getXmLineNumber(),
                                pImpl->getXmlFilename()));
                    }
                }
                else if (SupportedElement(name, pElt, METADATA_OUTPUT_DESCRIPTOR,
                    TAG_PROCESS_LIST, recognizedName))
                {
                    pImpl->m_elms.push_back(
                        std::make_shared<CTFReaderOutputDescriptorElt>(
                            name,
                            pContainer,
                            pImpl->getXmLineNumber(),
                            pImpl->getXmlFilename()));
                }
                else if (SupportedElement(name, pElt, rangeSubElements,
                                          TAG_RANGE, recognizedName))
                {
                    pImpl->m_elms.push_back(
                        std::make_shared<CTFReaderRangeValueElt>(
                            name,
                            pContainer,
                            pImpl->getXmLineNumber(),
                            pImpl->getXmlFilename()));
                }
                else if (SupportedElement(name, pElt, TAG_SATNODE,
                                          TAG_CDL, recognizedName) ||
                         SupportedElement(name, pElt, TAG_SATNODEALT,
                                          TAG_CDL, recognizedName))
                {
                    auto pCDL =
                        std::dynamic_pointer_cast<CTFReaderCDLElt>(pContainer);

                    auto satNodeElt = std::make_shared<CTFReaderSatNodeElt>(
                        name,
                        pCDL,
                        pImpl->getXmLineNumber(),
                        pImpl->getXmlFilename());
                    pImpl->m_elms.push_back(satNodeElt);
                }
                else if (SupportedElement(name, pElt, TAG_SATURATION,
                                          TAG_SATNODE, recognizedName))
                {
                    pImpl->m_elms.push_back(
                        std::make_shared<XmlReaderSaturationElt>(
                            name,
                            pContainer,
                            pImpl->getXmLineNumber(),
                            pImpl->getXmlFilename()));
                }
                else if (SupportedElement(name, pElt, TAG_SOPNODE,
                                          TAG_CDL, recognizedName))
                {
                    auto pCDL =
                        std::dynamic_pointer_cast<CTFReaderCDLElt>(pContainer);

                    auto sopNodeElt = std::make_shared<CTFReaderSOPNodeElt>(
                        name,
                        pCDL,
                        pImpl->getXmLineNumber(),
                        pImpl->getXmlFilename());
                    pImpl->m_elms.push_back(sopNodeElt);
                }
                else if (SupportedElement(name, pElt, sopSubElements,
                                          TAG_SOPNODE, recognizedName))
                {
                    pImpl->m_elms.push_back(
                        std::make_shared<XmlReaderSOPValueElt>(
                            name,
                            pContainer,
                            pImpl->getXmLineNumber(),
                            pImpl->getXmlFilename()));
                }
                else
                {
                    if (recognizedName)
                    {
                        std::ostringstream oss;
                        oss << ": '" << name << "' not allowed in this element";

                        pImpl->m_elms.push_back(
                            std::make_shared<XmlReaderDummyElt>(
                                name,
                                (pImpl->m_elms.empty() ? 0 : pImpl->m_elms.back()),
                                pImpl->getXmLineNumber(),
                                pImpl->getXmlFilename(),
                                oss.str().c_str()));
                    }
                    else
                    {
                        pImpl->m_elms.push_back(
                            std::make_shared<XmlReaderDummyElt>(
                                name,
                                (pImpl->m_elms.empty() ? 0 : pImpl->m_elms.back()),
                                pImpl->getXmLineNumber(),
                                pImpl->getXmlFilename(),
                                ": Unknown element"));
                    }
                }
            }
        }

        pImpl->m_elms.back()->start(atts);
    }

    // End the parsing of one element.
    static void EndElementHandler(void * userData,
                                  const XML_Char * name)
    {
        XMLParserHelper * pImpl = (XMLParserHelper*)userData;
        if (!pImpl || !name || !*name)
        {
            throw Exception("CTF/CLF internal parsing error.");
        }

        // Is the expected element present?
        auto pElt(pImpl->m_elms.back());
        if (!pElt.get())
        {
            pImpl->throwMessage("CTF/CLF parsing error: Tag is missing. ");
        }

        // Is it the expected element?
        if (pElt->getName() != name)
        {
            std::stringstream ss;
            ss << "CTF/CLF parsing error: Tag '";
            ss << (name ? name : "");
            ss << "' is missing";
            pImpl->throwMessage(ss.str());
        }

        if (pElt->isDummy())
        {
            pImpl->m_elms.pop_back();
        }
        else if (pElt->isContainer())
        {
            pImpl->m_elms.pop_back();
        }
        else
        {
            // Is it a plain element?
            auto pPlainElt = std::dynamic_pointer_cast<XmlReaderPlainElt>(pElt);
            if (!pPlainElt)
            {
                std::stringstream ss;
                ss << "CTF/CLF parsing error: Attribute end '";
                ss << (name ? name : "");
                ss << "' is illegal. ";
                pImpl->throwMessage(ss.str());
            }

            pImpl->m_elms.pop_back();

            auto pParent = pImpl->m_elms.back();

            // Is it at the right location in the stack?
            if (!pParent || !pParent->isContainer() ||
                pParent != pPlainElt->getParent())
            {
                std::stringstream ss;
                ss << "CTF/CLF parsing error: Tag '";
                ss << (name ? name : "");
                ss << "'.";
                pImpl->throwMessage(ss.str());
            }
        }

        pElt->end();
    }

    // Handle of strings within an element.
    static void CharacterDataHandler(void * userData,
                                     const XML_Char * s, 
                                     int len)
    {
        XMLParserHelper * pImpl = (XMLParserHelper*)userData;
        if (!pImpl)
        {
            throw Exception("CTF/CLF internal parsing error.");
        }

        if (len == 0) return;
        if (len<0 || !s || !*s)
        {
            pImpl->throwMessage("CTF/CLF parsing error: attribute illegal. ");
        }
        // Parsing a single new line. This is valid.
        if (len == 1 && s[0] == '\n') return;

        auto pElt = pImpl->m_elms.back();
        if (!pElt)
        {
            std::ostringstream oss;
            oss << "CTF/CLF parsing error: missing end tag '";
            oss << std::string(s, len).c_str();
            oss << "'.";
            pImpl->throwMessage(oss.str());
        }

        auto pDescriptionElt =
            std::dynamic_pointer_cast<XmlReaderDescriptionElt>(pElt);
        if (pDescriptionElt)
        {
            pDescriptionElt->setRawData(s, len, pImpl->getXmLineNumber());
        }
        else
        {
            // Strip white spaces.
            size_t start = 0;
            size_t end = len;
            FindSubString(s, len, start, end);

            if (end>0)
            {
                // CTFReaderMetadataElt is a special element: it is used
                // to process container elements, but it is also used to
                // process the terminal/plain elements.
                auto pMetadataElt =
                    std::dynamic_pointer_cast<CTFReaderMetadataElt>(pElt);
                if (pMetadataElt)
                {
                    pMetadataElt->setRawData(s + start, end - start,
                                             pImpl->getXmLineNumber());
                }
                else
                {
                    if (pElt->isContainer())
                    {
                        std::ostringstream oss;
                        oss << "CTF/CLF parsing error: attribute illegal '";
                        oss << std::string(s, len).c_str();
                        oss << "'.";
                        pImpl->throwMessage(oss.str());
                    }

                    auto pPlainElt = 
                        std::dynamic_pointer_cast<XmlReaderPlainElt>(pElt);
                    if (!pPlainElt)
                    {
                        std::ostringstream oss;
                        oss << "CTF/CLF parsing error: attribute illegal '";
                        oss << std::string(s, len).c_str();
                        oss << "'.";
                        pImpl->throwMessage(oss.str());
                    }
                    pPlainElt->setRawData(s + start, end - start,
                                          pImpl->getXmLineNumber());
                }
            }
        }
    }

    unsigned int getXmLineNumber() const
    {
        return m_lineNumber;
    }

    const std::string & getXmlFilename() const
    {
        return m_fileName;
    }

    bool IsCLF() const
    {
        return m_isCLF;
    }

    XML_Parser m_parser;
    unsigned int m_lineNumber;
    std::string m_fileName;
    bool m_isCLF;
    XmlReaderElementStack m_elms; // Parsing stack
    CTFReaderTransformPtr m_transform;

};

bool isLoadableCTF(std::istream & istream)
{
    std::streampos curPos = istream.tellg();

    const unsigned limit(5 * 1024); // 5 kilobytes.
    const char *pattern = "<ProcessList";
    bool foundPattern = false;
    unsigned sizeProcessed(0);
    char line[limit + 1];

    // Error: file is not found, or cannot read.
    if (istream.good())
    {
        // Find ProcessList tag at beginning of file.
        while (istream.good() && !foundPattern && (sizeProcessed < limit))
        {
            istream.getline(line, limit);
            if (strstr(line, pattern)) foundPattern = true;
            sizeProcessed += (unsigned)strlen(line);
        }
    }

    // Restore pos in stream.
    istream.seekg(curPos);
    return foundPattern;
}

// Try and load the format.
// Raise an exception if it can't be loaded.
CachedFileRcPtr LocalFileFormat::read(
    std::istream & istream,
    const std::string & filePath) const
{
    if (!isLoadableCTF(istream))
    {
        std::ostringstream oss;
        oss << "Parsing error: '" << filePath << "' is not a CTF/CLF file.";
        throw Exception(oss.str().c_str());
    }

    XMLParserHelper parser(filePath);
    parser.Parse(istream);

    LocalCachedFileRcPtr cachedFile =
        LocalCachedFileRcPtr(new LocalCachedFile());

    // Keep transform.
    cachedFile->m_transform = parser.getTransform();
    cachedFile->m_filePath = filePath;

    return cachedFile;
}

// Helper called by LocalFileFormat::buildFileOps
void BuildOp(OpRcPtrVec & ops,
             const Config& config,
             const ConstContextRcPtr & context,
             const ConstOpDataRcPtr & opData,
             TransformDirection dir)
{
    if (opData->getType() == OpData::ReferenceType)
    {
        // Recursively resolve the op.
        ConstReferenceOpDataRcPtr ref = DynamicPtrCast<const ReferenceOpData>(opData);
        if (ref->getReferenceStyle() == REF_PATH)
        {
            dir = CombineTransformDirections(dir, ref->getDirection());
            FileTransformRcPtr fileTransform = FileTransform::Create();
            fileTransform->setInterpolation(INTERP_LINEAR);
            fileTransform->setDirection(TRANSFORM_DIR_FORWARD);
            fileTransform->setSrc(ref->getPath().c_str());
            FileTransform * pFileTranform = fileTransform.get();

            // This might call LocalFileFormat::buildFileOps if the file
            // is a CTF. BuildFileTransformOps is making sure there is no
            // cycling recursion.
            BuildFileTransformOps(ops, config, context, *pFileTranform, dir);
        }
    }
    else
    {
        CreateOpVecFromOpData(ops, opData, dir);
    }

}

void
LocalFileFormat::buildFileOps(OpRcPtrVec & ops,
                              const Config& config,
                              const ConstContextRcPtr & context,
                              CachedFileRcPtr untypedCachedFile,
                              const FileTransform& fileTransform,
                              TransformDirection dir) const
{
    LocalCachedFileRcPtr cachedFile = 
        DynamicPtrCast<LocalCachedFile>(untypedCachedFile);

    // This should never happen.
    if(!cachedFile)
    {
        throw Exception("Cannot build clf ops. Invalid cache type.");
    }

    const TransformDirection newDir 
        = CombineTransformDirections(dir, fileTransform.getDirection());

    if(newDir == TRANSFORM_DIR_UNKNOWN)
    {
        std::ostringstream os;
        os << "Cannot build file format transform,";
        os << " unspecified transform direction.";
        throw Exception(os.str().c_str());
    }

    FormatMetadataImpl & processorData = ops.getFormatMetadata();

    // Put CTF processList information into the FormatMetadata.
    cachedFile->m_transform->toMetadata(processorData);

    // Resolve reference path using context and load referenced files.
    const ConstOpDataVec & opDataVec = cachedFile->m_transform->getOps();
    if (newDir == TRANSFORM_DIR_FORWARD)
    {
        for (auto & opData : opDataVec)
        {
            BuildOp(ops, config, context, opData, newDir);
        }
    }
    else
    {
        for (int idx = (int)opDataVec.size() - 1; idx >= 0; --idx)
        {
            BuildOp(ops, config, context, opDataVec[idx], newDir);
        }
    }
}

// This baker is based on what was done for ResolveCube and HDL.  We enhanced
// it to use a half-domain Lut1D for the shaper to better represent transforms
// expecting linear inputs.
// TODO: The CLF format is more powerful than those older formats and there is
// no need to be limited to a Lut1D + Lut3D structure -- more ops could be used
// when necessary for a more accurate bake.
void LocalFileFormat::bake(const Baker & baker,
                           const std::string & formatName,
                           std::ostream & ostream) const
{
    constexpr int DEFAULT_1D_SIZE = 4096;
    constexpr int DEFAULT_3D_SIZE = 64;

    // NB: By default, the shaper uses a half-domain LUT1D, which is always 65536 entries.
    // If the user requests some other size, a typical (non-half-domain) LUT1D will be used.
    if (formatName != FILEFORMAT_CTF && formatName != FILEFORMAT_CLF)
    {
        std::ostringstream os;
        os << "Unknown CLF/CTF file format name, '";
        os << formatName << "'.";
        throw Exception(os.str().c_str());
    }

    //
    // Initialize config and data.
    //

    ConstConfigRcPtr config = baker.getConfig();

    int onedSize = baker.getCubeSize();
    if (onedSize == -1)
    {
        onedSize = DEFAULT_1D_SIZE;
    }
    else if (onedSize < 2)
    {
        std::ostringstream os;
        os << "1D LUT size must be higher than 2 (was " << onedSize << ")";
        throw Exception(os.str().c_str());
    }

    int cubeSize = baker.getCubeSize();
    if (cubeSize == -1)
    {
        cubeSize = DEFAULT_3D_SIZE;
    }
    cubeSize = std::max(2, cubeSize); // smallest cube is 2x2x2

    // Get spaces from baker.
    const std::string shaperSpace = baker.getShaperSpace();
    const std::string inputSpace = baker.getInputSpace();
    const std::string targetSpace = baker.getTargetSpace();
    const std::string looks = baker.getLooks();

    //
    // Determine required LUT type.
    //

    constexpr int CTF_1D = 1;    // 1D LUT version number
    constexpr int CTF_3D = 2;    // 3D LUT version number
    constexpr int CTF_1D_3D = 3; // 3D LUT with 1D prelut

    ConstProcessorRcPtr inputToTargetProc;
    if (!looks.empty())
    {
        LookTransformRcPtr transform = LookTransform::Create();
        transform->setLooks(looks.c_str());
        transform->setSrc(inputSpace.c_str());
        transform->setDst(targetSpace.c_str());
        inputToTargetProc = config->getProcessor(transform,
                                                 TRANSFORM_DIR_FORWARD);
    }
    else
    {
        inputToTargetProc = config->getProcessor(inputSpace.c_str(),
                                                 targetSpace.c_str());
    }

    int required_lut = -1;

    if (inputToTargetProc->hasChannelCrosstalk())
    {
        if (shaperSpace.empty())
        {
            // Has crosstalk, but no shaper, so need 3D LUT.
            required_lut = CTF_3D;
        }
        else
        {
            // Crosstalk with shaper-space.
            required_lut = CTF_1D_3D;
        }
    }
    else
    {
        // No crosstalk
        required_lut = CTF_1D;
    }

    if (required_lut == -1)
    {
        // Unnecessary paranoia.
        throw Exception("Internal logic error, LUT type was not determined");
    }

    //
    // Generate Shaper.
    //

    Lut1DOpDataRcPtr shaperLut;
    float fromInStart = 0.0f;
    float fromInEnd = 1.0f;

    if (required_lut == CTF_1D_3D)
    {
        auto inputToShaperProc = config->getProcessor(inputSpace.c_str(),
                                                      shaperSpace.c_str());

        if (inputToShaperProc->hasChannelCrosstalk())
        {
            // TODO: Automatically turn shaper into
            // non-crosstalked version?
            std::ostringstream os;
            os << "The specified shaperSpace, '" << baker.getShaperSpace();
            os << "' has channel crosstalk, which is not appropriate for";
            os << " shapers. Please select an alternate shaper space or";
            os << " omit this option.";
            throw Exception(os.str().c_str());
        }

        const auto shaperSizeRequest = baker.getShaperSize();
        if (shaperSizeRequest == -1)
        {
            // Generate the identity shaper values, then apply the transform.
            // Using a half-domain to accurately handle floating-point, linear-space inputs.
            shaperLut = std::make_shared<Lut1DOpData>(Lut1DOpData::LUT_INPUT_HALF_CODE, 65536);
        }
        else
        {
            // Calculate min/max value.
            // Get input value of 1.0 in shaper space, as this is the highest value that is
            // transformed by the cube (e.g for a generic lin-to-log transform, what the
            // log value 1.0 is in linear).
            ConstProcessorRcPtr proc = config->getProcessor(shaperSpace.c_str(),
                                                            inputSpace.c_str());
            ConstCPUProcessorRcPtr shaperToInputProc = proc->getDefaultCPUProcessor();

            float minval[3] = { 0.0f, 0.0f, 0.0f };
            float maxval[3] = { 1.0f, 1.0f, 1.0f };

            shaperToInputProc->applyRGB(minval);
            shaperToInputProc->applyRGB(maxval);

            fromInStart = std::min(std::min(minval[0], minval[1]), minval[2]);
            fromInEnd = std::max(std::max(maxval[0], maxval[1]), maxval[2]);

            shaperLut = std::make_shared<Lut1DOpData>(shaperSizeRequest);

            if (fromInStart != 0.f || fromInEnd != 1.0f)
            {
                for (int i = 0; i < shaperSizeRequest; ++i)
                {
                    const float x = (float)(double(i) / double(shaperSizeRequest - 1));
                    float cur_value = lerpf(fromInStart, fromInEnd, x);

                    shaperLut->getArray()[3 * i + 0] = cur_value;
                    shaperLut->getArray()[3 * i + 1] = cur_value;
                    shaperLut->getArray()[3 * i + 2] = cur_value;
                }
            }
        }
        const auto shaperSize = shaperLut->getArray().getLength();
        PackedImageDesc shaperImg(shaperLut->getArray().getValues().data(),
                                  shaperSize, 1, 3);
        ConstCPUProcessorRcPtr cpu = inputToShaperProc->getDefaultCPUProcessor();
        cpu->apply(shaperImg);
    }

    //
    // Generate 3DLUT.
    //

    std::vector<float> cubeData;
    if (required_lut == CTF_3D || required_lut == CTF_1D_3D)
    {
        cubeData.resize(cubeSize*cubeSize*cubeSize * 3);
        GenerateIdentityLut3D(&cubeData[0], cubeSize, 3, LUT3DORDER_FAST_BLUE);
        PackedImageDesc cubeImg(&cubeData[0], cubeSize*cubeSize*cubeSize, 1, 3);

        ConstProcessorRcPtr cubeProc;
        if (required_lut == CTF_1D_3D)
        {
            if (!looks.empty())
            {
                LookTransformRcPtr transform = LookTransform::Create();
                transform->setLooks(looks.c_str());
                transform->setSrc(shaperSpace.c_str());
                transform->setDst(targetSpace.c_str());
                cubeProc = config->getProcessor(transform,
                                                TRANSFORM_DIR_FORWARD);
            }
            else
            {
                cubeProc = config->getProcessor(shaperSpace.c_str(),
                    targetSpace.c_str());
            }
        }
        else
        {
            // No shaper, so cube goes from input-to-target.
            cubeProc = inputToTargetProc;
        }

        ConstCPUProcessorRcPtr cpu = cubeProc->getDefaultCPUProcessor();
        cpu->apply(cubeImg);
    }

    //
    // Generate 1DLUT
    //

    std::vector<float> onedData;
    if (required_lut == CTF_1D)
    {
        onedData.resize(onedSize * 3);
        GenerateIdentityLut1D(&onedData[0], onedSize, 3);
        PackedImageDesc onedImg(&onedData[0], onedSize, 1, 3);

        ConstCPUProcessorRcPtr cpu = inputToTargetProc->getDefaultCPUProcessor();
        cpu->apply(onedImg);
    }

    //
    // Write.
    //

    OpRcPtrVec ops;

    // 1D data.
    if (required_lut == CTF_1D)
    {
        Lut1DOpDataRcPtr lut1D = std::make_shared<Lut1DOpData>((unsigned long)onedSize);
        lut1D->getArray().getValues() = onedData;
        CreateLut1DOp(ops, lut1D, TRANSFORM_DIR_FORWARD);
    }
    else if (required_lut == CTF_1D_3D)
    {
        if (fromInStart != 0.f || fromInEnd != 1.0f)
        {
            CreateRangeOp(ops, fromInStart, fromInEnd, 0, 1, TRANSFORM_DIR_FORWARD);
        }
        CreateLut1DOp(ops, shaperLut, TRANSFORM_DIR_FORWARD);
    }

    // 3D data.
    if (required_lut == CTF_3D || required_lut == CTF_1D_3D)
    {
        Lut3DOpDataRcPtr lut3D = std::make_shared<Lut3DOpData>((unsigned long)cubeSize);
        lut3D->getArray().getValues() = cubeData;
        CreateLut3DOp(ops, lut3D, TRANSFORM_DIR_FORWARD);
    }

    write(ops, baker.getFormatMetadata(), formatName, ostream);
}

void LocalFileFormat::write(const OpRcPtrVec & ops,
                            const FormatMetadataImpl & metadata,
                            const std::string & formatName,
                            std::ostream & ostream) const
{
    bool isCLF = false;
    if (Platform::Strcasecmp(formatName.c_str(), FILEFORMAT_CLF) == 0)
    {
        isCLF = true;
    }
    else if (Platform::Strcasecmp(formatName.c_str(), FILEFORMAT_CTF) != 0)
    {
        // Neither a clf nor a ctf.
        std::ostringstream os;
        os << "Error: CLF/CTF writer does not also write format " << formatName << ".";
        throw Exception(os.str().c_str());
    }

    CTFReaderTransformPtr transform = std::make_shared<CTFReaderTransform>(ops, metadata);

    // Write XML Header.
    ostream << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>" << std::endl;
    XmlFormatter fmt(ostream);

    TransformWriter writer(fmt, transform, isCLF);
    writer.write();
}

} // end of anonymous namespace.

FileFormat * CreateFileFormatCLF()
{
    return new LocalFileFormat();
}


} // namespace OCIO_NAMESPACE
