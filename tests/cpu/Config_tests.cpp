// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the OpenColorIO Project.

#include <pystring/pystring.h>
#include <sys/stat.h>

#include "Config.cpp"

#include "UnitTest.h"
#include "UnitTestLogUtils.h"
#include "UnitTestUtils.h"

namespace OCIO = OCIO_NAMESPACE;

#if 0
OCIO_ADD_TEST(Config, test_searchpath_filesystem)
{

    OCIO::EnvMap env = OCIO::GetEnvMap();
    std::string OCIO_TEST_AREA("$OCIO_TEST_AREA");
    EnvExpand(&OCIO_TEST_AREA, &env);

    OCIO::ConfigRcPtr config = OCIO::Config::Create();

    // basic get/set/expand
    config->setSearchPath("."
                          ":$OCIO_TEST1"
                          ":/$OCIO_JOB/${OCIO_SEQ}/$OCIO_SHOT/ocio");

    OCIO_CHECK_ASSERT(strcmp(config->getSearchPath(),
        ".:$OCIO_TEST1:/$OCIO_JOB/${OCIO_SEQ}/$OCIO_SHOT/ocio") == 0);
    OCIO_CHECK_ASSERT(strcmp(config->getSearchPath(true),
        ".:foobar:/meatballs/cheesecake/mb-cc-001/ocio") == 0);

    // find some files
    config->setSearchPath(".."
                          ":$OCIO_TEST1"
                          ":${OCIO_TEST_AREA}/test_search/one"
                          ":$OCIO_TEST_AREA/test_search/two");

    // setup for search test
    std::string base_dir("$OCIO_TEST_AREA/test_search/");
    EnvExpand(&base_dir, &env);
    mkdir(base_dir.c_str(), 0777);

    std::string one_dir("$OCIO_TEST_AREA/test_search/one/");
    EnvExpand(&one_dir, &env);
    mkdir(one_dir.c_str(), 0777);

    std::string two_dir("$OCIO_TEST_AREA/test_search/two/");
    EnvExpand(&two_dir, &env);
    mkdir(two_dir.c_str(), 0777);

    std::string lut1(one_dir+"somelut1.lut");
    std::ofstream somelut1(lut1.c_str());
    somelut1.close();

    std::string lut2(two_dir+"somelut2.lut");
    std::ofstream somelut2(lut2.c_str());
    somelut2.close();

    std::string lut3(two_dir+"somelut3.lut");
    std::ofstream somelut3(lut3.c_str());
    somelut3.close();

    std::string lutdotdot(OCIO_TEST_AREA+"/lutdotdot.lut");
    std::ofstream somelutdotdot(lutdotdot.c_str());
    somelutdotdot.close();

    // basic search test
    OCIO_CHECK_ASSERT(strcmp(config->findFile("somelut1.lut"),
        lut1.c_str()) == 0);
    OCIO_CHECK_ASSERT(strcmp(config->findFile("somelut2.lut"),
        lut2.c_str()) == 0);
    OCIO_CHECK_ASSERT(strcmp(config->findFile("somelut3.lut"),
        lut3.c_str()) == 0);
    OCIO_CHECK_ASSERT(strcmp(config->findFile("lutdotdot.lut"),
        lutdotdot.c_str()) == 0);

}
#endif

OCIO_ADD_TEST(Config, internal_raw_profile)
{
    std::istringstream is;
    is.str(OCIO::INTERNAL_RAW_PROFILE);

    OCIO_CHECK_NO_THROW(OCIO::Config::CreateFromStream(is));
}

OCIO_ADD_TEST(Config, create_raw_config)
{
    OCIO::ConstConfigRcPtr config;
    OCIO_CHECK_NO_THROW(config = OCIO::Config::CreateRaw());
    OCIO_CHECK_NO_THROW(config->sanityCheck());
    OCIO_CHECK_EQUAL(config->getNumColorSpaces(), 1);
    OCIO_CHECK_EQUAL(std::string(config->getColorSpaceNameByIndex(0)), std::string("raw"));

    OCIO::ConstProcessorRcPtr proc;
    OCIO_CHECK_NO_THROW(proc = config->getProcessor("raw", "raw"));
    OCIO_CHECK_NO_THROW(proc->getDefaultCPUProcessor());
}

OCIO_ADD_TEST(Config, simple_config)
{

    constexpr char SIMPLE_PROFILE[] =
        "ocio_profile_version: 1\n"
        "resource_path: luts\n"
        "strictparsing: false\n"
        "luma: [0.2126, 0.7152, 0.0722]\n"
        "roles:\n"
        "  default: raw\n"
        "  scene_linear: lnh\n"
        "displays:\n"
        "  sRGB:\n"
        "  - !<View> {name: Film1D, colorspace: loads_of_transforms}\n"
        "  - !<View> {name: Ln, colorspace: lnh}\n"
        "  - !<View> {name: Raw, colorspace: raw}\n"
        "colorspaces:\n"
        "  - !<ColorSpace>\n"
        "      name: raw\n"
        "      family: raw\n"
        "      equalitygroup: \n"
        "      bitdepth: 32f\n"
        "      description: |\n"
        "        A raw color space. Conversions to and from this space are no-ops.\n"
        "      isdata: true\n"
        "      allocation: uniform\n"
        "  - !<ColorSpace>\n"
        "      name: lnh\n"
        "      family: ln\n"
        "      equalitygroup: \n"
        "      bitdepth: 16f\n"
        "      description: |\n"
        "        The show reference space. This is a sensor referred linear\n"
        "        representation of the scene with primaries that correspond to\n"
        "        scanned film. 0.18 in this space corresponds to a properly\n"
        "        exposed 18% grey card.\n"
        "      isdata: false\n"
        "      allocation: lg2\n"
        "  - !<ColorSpace>\n"
        "      name: loads_of_transforms\n"
        "      family: vd8\n"
        "      equalitygroup: \n"
        "      bitdepth: 8ui\n"
        "      description: 'how many transforms can we use?'\n"
        "      isdata: false\n"
        "      allocation: uniform\n"
        "      to_reference: !<GroupTransform>\n"
        "        direction: forward\n"
        "        children:\n"
        "          - !<FileTransform>\n"
        "            src: diffusemult.spimtx\n"
        "            interpolation: unknown\n"
        "          - !<ColorSpaceTransform>\n"
        "            src: raw\n"
        "            dst: lnh\n"
        "          - !<ExponentTransform>\n"
        "            value: [2.2, 2.2, 2.2, 1]\n"
        "          - !<MatrixTransform>\n"
        "            matrix: [1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1]\n"
        "            offset: [0, 0, 0, 0]\n"
        "          - !<CDLTransform>\n"
        "            slope: [1, 1, 1]\n"
        "            offset: [0, 0, 0]\n"
        "            power: [1, 1, 1]\n"
        "            saturation: 1\n"
        "\n";

    std::istringstream is;
    is.str(SIMPLE_PROFILE);
    OCIO::ConstConfigRcPtr config;
    OCIO_CHECK_NO_THROW(config = OCIO::Config::CreateFromStream(is));
    OCIO_CHECK_NO_THROW(config->sanityCheck());
}

OCIO_ADD_TEST(Config, roles)
{

    std::string SIMPLE_PROFILE =
    "ocio_profile_version: 1\n"
    "strictparsing: false\n"
    "roles:\n"
    "  compositing_log: lgh\n"
    "  default: raw\n"
    "  scene_linear: lnh\n"
    "colorspaces:\n"
    "  - !<ColorSpace>\n"
    "      name: raw\n"
    "  - !<ColorSpace>\n"
    "      name: lnh\n"
    "  - !<ColorSpace>\n"
    "      name: lgh\n"
    "\n";

    std::istringstream is;
    is.str(SIMPLE_PROFILE);
    OCIO::ConstConfigRcPtr config;
    OCIO_CHECK_NO_THROW(config = OCIO::Config::CreateFromStream(is));

    OCIO_CHECK_EQUAL(config->getNumRoles(), 3);

    OCIO_CHECK_ASSERT(config->hasRole("compositing_log") == true);
    OCIO_CHECK_ASSERT(config->hasRole("cheese") == false);
    OCIO_CHECK_ASSERT(config->hasRole("") == false);

    OCIO_CHECK_ASSERT(strcmp(config->getRoleName(2), "scene_linear") == 0);
    OCIO_CHECK_ASSERT(strcmp(config->getRoleName(0), "compositing_log") == 0);
    OCIO_CHECK_ASSERT(strcmp(config->getRoleName(1), "default") == 0);
    OCIO_CHECK_ASSERT(strcmp(config->getRoleName(10), "") == 0);
    OCIO_CHECK_ASSERT(strcmp(config->getRoleName(-4), "") == 0);

}

OCIO_ADD_TEST(Config, serialize_group_transform)
{
    // The unit test validates that a group transform is correctly serialized.

    OCIO::ConfigRcPtr config = OCIO::Config::Create();
    {
        OCIO::ColorSpaceRcPtr cs = OCIO::ColorSpace::Create();
        cs->setName("testing");
        cs->setFamily("test");
        OCIO::FileTransformRcPtr transform1 = \
            OCIO::FileTransform::Create();
        OCIO::GroupTransformRcPtr groupTransform = OCIO::GroupTransform::Create();
        groupTransform->appendTransform(transform1);
        cs->setTransform(groupTransform, OCIO::COLORSPACE_DIR_FROM_REFERENCE);
        config->addColorSpace(cs);
        config->setRole( OCIO::ROLE_COMPOSITING_LOG, cs->getName() );
    }
    {
        OCIO::ColorSpaceRcPtr cs = OCIO::ColorSpace::Create();
        cs->setName("testing2");
        cs->setFamily("test");
        OCIO::ExponentTransformRcPtr transform1 = \
            OCIO::ExponentTransform::Create();
        OCIO::GroupTransformRcPtr groupTransform = OCIO::GroupTransform::Create();
        groupTransform->appendTransform(transform1);
        cs->setTransform(groupTransform, OCIO::COLORSPACE_DIR_TO_REFERENCE);
        config->addColorSpace(cs);
        config->setRole( OCIO::ROLE_COMPOSITING_LOG, cs->getName() );
    }

    std::ostringstream os;
    config->serialize(os);

    std::string PROFILE_OUT =
    "ocio_profile_version: 1\n"
    "\n"
    "search_path: \"\"\n"
    "strictparsing: true\n"
    "luma: [0.2126, 0.7152, 0.0722]\n"
    "\n"
    "roles:\n"
    "  compositing_log: testing2\n"
    "\n"
    "displays:\n"
    "  {}\n"
    "\n"
    "active_displays: []\n"
    "active_views: []\n"
    "\n"
    "colorspaces:\n"
    "  - !<ColorSpace>\n"
    "    name: testing\n"
    "    family: test\n"
    "    equalitygroup: \"\"\n"
    "    bitdepth: unknown\n"
    "    isdata: false\n"
    "    allocation: uniform\n"
    "    from_reference: !<GroupTransform>\n"
    "      children:\n"
    "        - !<FileTransform> {src: \"\", interpolation: unknown}\n"
    "\n"
    "  - !<ColorSpace>\n"
    "    name: testing2\n"
    "    family: test\n"
    "    equalitygroup: \"\"\n"
    "    bitdepth: unknown\n"
    "    isdata: false\n"
    "    allocation: uniform\n"
    "    to_reference: !<GroupTransform>\n"
    "      children:\n"
    "        - !<ExponentTransform> {value: [1, 1, 1, 1]}\n";

    std::vector<std::string> osvec;
    pystring::splitlines(os.str(), osvec);
    std::vector<std::string> PROFILE_OUTvec;
    pystring::splitlines(PROFILE_OUT, PROFILE_OUTvec);

    OCIO_CHECK_EQUAL(osvec.size(), PROFILE_OUTvec.size());
    for(unsigned int i = 0; i < PROFILE_OUTvec.size(); ++i)
        OCIO_CHECK_EQUAL(osvec[i], PROFILE_OUTvec[i]);
}

OCIO_ADD_TEST(Config, serialize_searchpath)
{
    {
        OCIO::ConfigRcPtr config = OCIO::Config::Create();

        std::ostringstream os;
        config->serialize(os);

        std::string PROFILE_OUT =
            "ocio_profile_version: 1\n"
            "\n"
            "search_path: \"\"\n"
            "strictparsing: true\n"
            "luma: [0.2126, 0.7152, 0.0722]\n"
            "\n"
            "roles:\n"
            "  {}\n"
            "\n"
            "displays:\n"
            "  {}\n"
            "\n"
            "active_displays: []\n"
            "active_views: []\n"
            "\n"
            "colorspaces:\n"
            "  []";

        std::vector<std::string> osvec;
        pystring::splitlines(os.str(), osvec);
        std::vector<std::string> PROFILE_OUTvec;
        pystring::splitlines(PROFILE_OUT, PROFILE_OUTvec);

        OCIO_CHECK_EQUAL(osvec.size(), PROFILE_OUTvec.size());
        for (unsigned int i = 0; i < PROFILE_OUTvec.size(); ++i)
            OCIO_CHECK_EQUAL(osvec[i], PROFILE_OUTvec[i]);
    }

    {
        OCIO::ConfigRcPtr config = OCIO::Config::Create();

        std::string searchPath("a:b:c");
        config->setSearchPath(searchPath.c_str());

        std::ostringstream os;
        config->serialize(os);

        std::vector<std::string> osvec;
        pystring::splitlines(os.str(), osvec);

        const std::string expected1{ "search_path: a:b:c" };
        OCIO_CHECK_EQUAL(osvec[2], expected1);

        config->setMajorVersion(2);
        os.clear();
        os.str("");
        config->serialize(os);

        osvec.clear();
        pystring::splitlines(os.str(), osvec);

        const std::string expected2[] = { "search_path:", "  - a", "  - b", "  - c" };
        OCIO_CHECK_EQUAL(osvec[2], expected2[0]);
        OCIO_CHECK_EQUAL(osvec[3], expected2[1]);
        OCIO_CHECK_EQUAL(osvec[4], expected2[2]);
        OCIO_CHECK_EQUAL(osvec[5], expected2[3]);

        std::istringstream is;
        is.str(os.str());
        OCIO::ConstConfigRcPtr configRead;
        OCIO_CHECK_NO_THROW(configRead = OCIO::Config::CreateFromStream(is));

        OCIO_CHECK_EQUAL(configRead->getNumSearchPaths(), 3);
        OCIO_CHECK_EQUAL(std::string(configRead->getSearchPath()), searchPath);
        OCIO_CHECK_EQUAL(std::string(configRead->getSearchPath(0)), std::string("a"));
        OCIO_CHECK_EQUAL(std::string(configRead->getSearchPath(1)), std::string("b"));
        OCIO_CHECK_EQUAL(std::string(configRead->getSearchPath(2)), std::string("c"));

        os.clear();
        os.str("");
        config->clearSearchPaths();
        const std::string sp0{ "a path with a - in it/" };
        const std::string sp1{ "/absolute/linux/path" };
        const std::string sp2{ "C:\\absolute\\windows\\path" };
        const std::string sp3{ "!<path> using /yaml/symbols" };
        config->addSearchPath(sp0.c_str());
        config->addSearchPath(sp1.c_str());
        config->addSearchPath(sp2.c_str());
        config->addSearchPath(sp3.c_str());
        config->serialize(os);

        osvec.clear();
        pystring::splitlines(os.str(), osvec);

        const std::string expected3[] = { "search_path:",
                                          "  - a path with a - in it/",
                                          "  - /absolute/linux/path",
                                          "  - C:\\absolute\\windows\\path",
                                          "  - \"!<path> using /yaml/symbols\"" };
        OCIO_CHECK_EQUAL(osvec[2], expected3[0]);
        OCIO_CHECK_EQUAL(osvec[3], expected3[1]);
        OCIO_CHECK_EQUAL(osvec[4], expected3[2]);
        OCIO_CHECK_EQUAL(osvec[5], expected3[3]);
        OCIO_CHECK_EQUAL(osvec[6], expected3[4]);

        is.clear();
        is.str(os.str());
        OCIO_CHECK_NO_THROW(configRead = OCIO::Config::CreateFromStream(is));

        OCIO_CHECK_EQUAL(configRead->getNumSearchPaths(), 4);
        OCIO_CHECK_EQUAL(std::string(configRead->getSearchPath(0)), sp0);
        OCIO_CHECK_EQUAL(std::string(configRead->getSearchPath(1)), sp1);
        OCIO_CHECK_EQUAL(std::string(configRead->getSearchPath(2)), sp2);
        OCIO_CHECK_EQUAL(std::string(configRead->getSearchPath(3)), sp3);
    }
}

OCIO_ADD_TEST(Config, sanity_check)
{
    {
    std::string SIMPLE_PROFILE =
    "ocio_profile_version: 1\n"
    "colorspaces:\n"
    "  - !<ColorSpace>\n"
    "      name: raw\n"
    "  - !<ColorSpace>\n"
    "      name: raw\n"
    "strictparsing: false\n"
    "roles:\n"
    "  default: raw\n"
    "displays:\n"
    "  sRGB:\n"
    "  - !<View> {name: Raw, colorspace: raw}\n"
    "\n";

    std::istringstream is;
    is.str(SIMPLE_PROFILE);
    OCIO::ConstConfigRcPtr config;
    OCIO_CHECK_THROW(config = OCIO::Config::CreateFromStream(is), OCIO::Exception);
    }

    {
    std::string SIMPLE_PROFILE =
    "ocio_profile_version: 1\n"
    "colorspaces:\n"
    "  - !<ColorSpace>\n"
    "      name: raw\n"
    "strictparsing: false\n"
    "roles:\n"
    "  default: raw\n"
    "displays:\n"
    "  sRGB:\n"
    "  - !<View> {name: Raw, colorspace: raw}\n"
    "\n";

    std::istringstream is;
    is.str(SIMPLE_PROFILE);
    OCIO::ConstConfigRcPtr config;
    OCIO_CHECK_NO_THROW(config = OCIO::Config::CreateFromStream(is));

    OCIO_CHECK_NO_THROW(config->sanityCheck());
    }
}


OCIO_ADD_TEST(config, env_check)
{
    std::string SIMPLE_PROFILE =
    "ocio_profile_version: 1\n"
    "environment:\n"
    "  SHOW: super\n"
    "  SHOT: test\n"
    "  SEQ: foo\n"
    "  test: bar${cheese}\n"
    "  cheese: chedder\n"
    "colorspaces:\n"
    "  - !<ColorSpace>\n"
    "      name: raw\n"
    "strictparsing: false\n"
    "roles:\n"
    "  default: raw\n"
    "displays:\n"
    "  sRGB:\n"
    "  - !<View> {name: Raw, colorspace: raw}\n"
    "\n";

    std::string SIMPLE_PROFILE2 =
    "ocio_profile_version: 1\n"
    "colorspaces:\n"
    "  - !<ColorSpace>\n"
    "      name: raw\n"
    "strictparsing: false\n"
    "roles:\n"
    "  default: raw\n"
    "displays:\n"
    "  sRGB:\n"
    "  - !<View> {name: Raw, colorspace: raw}\n"
    "\n";

    OCIO::Platform::Setenv("SHOW", "bar");
    OCIO::Platform::Setenv("TASK", "lighting");

    std::istringstream is;
    is.str(SIMPLE_PROFILE);
    OCIO::ConstConfigRcPtr config;
    OCIO_CHECK_NO_THROW(config = OCIO::Config::CreateFromStream(is));
    OCIO_CHECK_EQUAL(config->getNumEnvironmentVars(), 5);
    OCIO_CHECK_ASSERT(strcmp(config->getCurrentContext()->resolveStringVar("test${test}"),
        "testbarchedder") == 0);
    OCIO_CHECK_ASSERT(strcmp(config->getCurrentContext()->resolveStringVar("${SHOW}"),
        "bar") == 0);
    OCIO_CHECK_ASSERT(strcmp(config->getEnvironmentVarDefault("SHOW"), "super") == 0);

    OCIO::ConfigRcPtr edit = config->createEditableCopy();
    edit->clearEnvironmentVars();
    OCIO_CHECK_EQUAL(edit->getNumEnvironmentVars(), 0);

    edit->addEnvironmentVar("testing", "dupvar");
    edit->addEnvironmentVar("testing", "dupvar");
    edit->addEnvironmentVar("foobar", "testing");
    edit->addEnvironmentVar("blank", "");
    edit->addEnvironmentVar("dontadd", NULL);
    OCIO_CHECK_EQUAL(edit->getNumEnvironmentVars(), 3);
    edit->addEnvironmentVar("foobar", NULL); // remove
    OCIO_CHECK_EQUAL(edit->getNumEnvironmentVars(), 2);
    edit->clearEnvironmentVars();

    edit->addEnvironmentVar("SHOW", "super");
    edit->addEnvironmentVar("SHOT", "test");
    edit->addEnvironmentVar("SEQ", "foo");
    edit->addEnvironmentVar("test", "bar${cheese}");
    edit->addEnvironmentVar("cheese", "chedder");

    // As a warning message is expected, please mute it.
    OCIO::MuteLogging mute;

    // Test
    OCIO::LoggingLevel loglevel = OCIO::GetLoggingLevel();
    OCIO::SetLoggingLevel(OCIO::LOGGING_LEVEL_DEBUG);
    is.str(SIMPLE_PROFILE2);
    OCIO::ConstConfigRcPtr noenv;
    OCIO_CHECK_NO_THROW(noenv = OCIO::Config::CreateFromStream(is));
    OCIO_CHECK_ASSERT(strcmp(noenv->getCurrentContext()->resolveStringVar("${TASK}"),
        "lighting") == 0);
    OCIO::SetLoggingLevel(loglevel);

    OCIO_CHECK_EQUAL(edit->getEnvironmentMode(), OCIO::ENV_ENVIRONMENT_LOAD_PREDEFINED);
    edit->setEnvironmentMode(OCIO::ENV_ENVIRONMENT_LOAD_ALL);
    OCIO_CHECK_EQUAL(edit->getEnvironmentMode(), OCIO::ENV_ENVIRONMENT_LOAD_ALL);
}

OCIO_ADD_TEST(Config, role_without_colorspace)
{
    OCIO::ConfigRcPtr config = OCIO::Config::Create()->createEditableCopy();
    config->setRole("reference", "UnknownColorSpace");

    std::ostringstream os;
    OCIO_CHECK_THROW(config->serialize(os), OCIO::Exception);
}

OCIO_ADD_TEST(Config, env_colorspace_name)
{
    const std::string MY_OCIO_CONFIG =
        "ocio_profile_version: 1\n"
        "\n"
        "search_path: luts\n"
        "strictparsing: true\n"
        "luma: [0.2126, 0.7152, 0.0722]\n"
        "\n"
        "roles:\n"
        "  compositing_log: lgh\n"
        "  default: raw\n"
        "  scene_linear: lnh\n"
        "\n"
        "displays:\n"
        "  sRGB:\n"
        "    - !<View> {name: Raw, colorspace: raw}\n"
        "\n"
        "active_displays: []\n"
        "active_views: []\n"
        "\n"
        "colorspaces:\n"
        "  - !<ColorSpace>\n"
        "    name: raw\n"
        "    family: \"\"\n"
        "    equalitygroup: \"\"\n"
        "    bitdepth: unknown\n"
        "    isdata: false\n"
        "    allocation: uniform\n"
        "\n"
        "  - !<ColorSpace>\n"
        "    name: lnh\n"
        "    family: \"\"\n"
        "    equalitygroup: \"\"\n"
        "    bitdepth: unknown\n"
        "    isdata: false\n"
        "    allocation: uniform\n"
        "\n"
        "  - !<ColorSpace>\n"
        "    name: lgh\n"
        "    family: \"\"\n"
        "    equalitygroup: \"\"\n"
        "    bitdepth: unknown\n"
        "    isdata: false\n"
        "    allocation: uniform\n"
        "    allocationvars: [-0.125, 1.125]\n";


    {
        // Test when the env. variable is missing

        const std::string 
            myConfigStr = MY_OCIO_CONFIG
                + "    from_reference: !<ColorSpaceTransform> {src: raw, dst: $MISSING_ENV}\n";

        std::istringstream is;
        is.str(myConfigStr);

        OCIO::ConstConfigRcPtr config;
        OCIO_CHECK_NO_THROW(config = OCIO::Config::CreateFromStream(is));
        OCIO_CHECK_THROW(config->sanityCheck(), OCIO::Exception);
        OCIO_CHECK_THROW(config->getProcessor("raw", "lgh"), OCIO::Exception);
    }

    {
        // Test when the env. variable exists but its content is wrong
        OCIO::Platform::Setenv("OCIO_TEST", "FaultyColorSpaceName");

        const std::string 
            myConfigStr = MY_OCIO_CONFIG
                + "    from_reference: !<ColorSpaceTransform> {src: raw, dst: $OCIO_TEST}\n";

        std::istringstream is;
        is.str(myConfigStr);

        OCIO::ConstConfigRcPtr config;
        OCIO_CHECK_NO_THROW(config = OCIO::Config::CreateFromStream(is));
        OCIO_CHECK_THROW(config->sanityCheck(), OCIO::Exception);
        OCIO_CHECK_THROW(config->getProcessor("raw", "lgh"), OCIO::Exception);
    }

    {
        // Test when the env. variable exists and its content is right
        OCIO::Platform::Setenv("OCIO_TEST", "lnh");

        const std::string 
            myConfigStr = MY_OCIO_CONFIG
                + "    from_reference: !<ColorSpaceTransform> {src: raw, dst: $OCIO_TEST}\n";

        std::istringstream is;
        is.str(myConfigStr);

        OCIO::ConstConfigRcPtr config;
        OCIO_CHECK_NO_THROW(config = OCIO::Config::CreateFromStream(is));
        OCIO_CHECK_NO_THROW(config->sanityCheck());
        OCIO_CHECK_NO_THROW(config->getProcessor("raw", "lgh"));
    }

    {
        // Check that the serialization preserves the env. variable
        OCIO::Platform::Setenv("OCIO_TEST", "lnh");

        const std::string 
            myConfigStr = MY_OCIO_CONFIG
                + "    from_reference: !<ColorSpaceTransform> {src: raw, dst: $OCIO_TEST}\n";

        std::istringstream is;
        is.str(myConfigStr);

        OCIO::ConstConfigRcPtr config;
        OCIO_CHECK_NO_THROW(config = OCIO::Config::CreateFromStream(is));
        OCIO_CHECK_NO_THROW(config->sanityCheck());

        std::stringstream ss;
        ss << *config.get();
        OCIO_CHECK_EQUAL(ss.str(), myConfigStr);
    }
}

OCIO_ADD_TEST(Config, version)
{
    const std::string SIMPLE_PROFILE =
        "ocio_profile_version: 2\n"
        "colorspaces:\n"
        "  - !<ColorSpace>\n"
        "      name: raw\n"
        "strictparsing: false\n"
        "roles:\n"
        "  default: raw\n"
        "displays:\n"
        "  sRGB:\n"
        "  - !<View> {name: Raw, colorspace: raw}\n"
        "\n";

    std::istringstream is;
    is.str(SIMPLE_PROFILE);
    OCIO::ConfigRcPtr config;
    OCIO_CHECK_NO_THROW(config = OCIO::Config::CreateFromStream(is)->createEditableCopy());

    OCIO_CHECK_NO_THROW(config->sanityCheck());

    OCIO_CHECK_NO_THROW(config->setMajorVersion(1));
    OCIO_CHECK_THROW(config->setMajorVersion(20000), OCIO::Exception);

    {
        OCIO_CHECK_NO_THROW(config->setMinorVersion(2));
        OCIO_CHECK_NO_THROW(config->setMinorVersion(20));

        std::stringstream ss;
        ss << *config.get();   
        pystring::startswith(
            pystring::lower(ss.str()), "ocio_profile_version: 2.20");
    }

    {
        OCIO_CHECK_NO_THROW(config->setMinorVersion(0));

        std::stringstream ss;
        ss << *config.get();   
        pystring::startswith(
            pystring::lower(ss.str()), "ocio_profile_version: 2");
    }

    {
        OCIO_CHECK_NO_THROW(config->setMinorVersion(1));

        std::stringstream ss;
        ss << *config.get();   
        pystring::startswith(
            pystring::lower(ss.str()), "ocio_profile_version: 1");
    }
}

OCIO_ADD_TEST(Config, version_faulty_1)
{
    const std::string SIMPLE_PROFILE =
        "ocio_profile_version: 2.0.1\n"
        "colorspaces:\n"
        "  - !<ColorSpace>\n"
        "      name: raw\n"
        "strictparsing: false\n"
        "roles:\n"
        "  default: raw\n"
        "displays:\n"
        "  sRGB:\n"
        "  - !<View> {name: Raw, colorspace: raw}\n"
        "\n";

    std::istringstream is;
    is.str(SIMPLE_PROFILE);
    OCIO::ConstConfigRcPtr config;
    OCIO_CHECK_THROW(config = OCIO::Config::CreateFromStream(is), OCIO::Exception);
}

namespace
{

const std::string PROFILE_V1 = "ocio_profile_version: 1\n";

const std::string PROFILE_V2 = "ocio_profile_version: 2\n";

const std::string SIMPLE_PROFILE =
    "\n"
    "search_path: luts\n"
    "strictparsing: true\n"
    "luma: [0.2126, 0.7152, 0.0722]\n"
    "\n"
    "roles:\n"
    "  default: raw\n"
    "  scene_linear: lnh\n"
    "\n"
    "displays:\n"
    "  sRGB:\n"
    "    - !<View> {name: Raw, colorspace: raw}\n"
    "\n"
    "active_displays: []\n"
    "active_views: []\n"
    "\n"
    "colorspaces:\n"
    "  - !<ColorSpace>\n"
    "    name: raw\n"
    "    family: \"\"\n"
    "    equalitygroup: \"\"\n"
    "    bitdepth: unknown\n"
    "    isdata: false\n"
    "    allocation: uniform\n"
    "\n"
    "  - !<ColorSpace>\n"
    "    name: lnh\n"
    "    family: \"\"\n"
    "    equalitygroup: \"\"\n"
    "    bitdepth: unknown\n"
    "    isdata: false\n"
    "    allocation: uniform\n";

};

OCIO_ADD_TEST(Config, range_serialization)
{
    {
        const std::string strEnd =
            "    from_reference: !<RangeTransform> {}\n";
        const std::string str = PROFILE_V2 + SIMPLE_PROFILE + strEnd;

        std::istringstream is;
        is.str(str);

        OCIO::ConstConfigRcPtr config;
        OCIO_CHECK_NO_THROW(config = OCIO::Config::CreateFromStream(is));
        OCIO_CHECK_NO_THROW(config->sanityCheck());

        std::stringstream ss;
        ss << *config.get();
        OCIO_CHECK_EQUAL(ss.str(), str);
    }

    {
        const std::string strEnd =
            "    from_reference: !<RangeTransform> {direction: inverse}\n";
        const std::string str = PROFILE_V2 + SIMPLE_PROFILE + strEnd;

        std::istringstream is;
        is.str(str);

        OCIO::ConstConfigRcPtr config;
        OCIO_CHECK_NO_THROW(config = OCIO::Config::CreateFromStream(is));
        OCIO_CHECK_NO_THROW(config->sanityCheck());

        std::stringstream ss;
        ss << *config.get();
        OCIO_CHECK_EQUAL(ss.str(), str);
    }

    {
        const std::string strEnd =
            "    from_reference: !<RangeTransform> {style: noClamp}\n";
        const std::string str = PROFILE_V2 + SIMPLE_PROFILE + strEnd;

        std::istringstream is;
        is.str(str);

        OCIO::ConstConfigRcPtr config;
        OCIO_CHECK_NO_THROW(config = OCIO::Config::CreateFromStream(is));
        OCIO_CHECK_NO_THROW(config->sanityCheck());

        std::stringstream ss;
        ss << *config.get();
        OCIO_CHECK_EQUAL(ss.str(), str);
    }

    {
        const std::string strEnd =
            "    from_reference: !<RangeTransform> {style: noClamp, direction: inverse}\n";
        const std::string str = PROFILE_V2 + SIMPLE_PROFILE + strEnd;

        std::istringstream is;
        is.str(str);

        OCIO::ConstConfigRcPtr config;
        OCIO_CHECK_NO_THROW(config = OCIO::Config::CreateFromStream(is));
        OCIO_CHECK_NO_THROW(config->sanityCheck());

        std::stringstream ss;
        ss << *config.get();
        OCIO_CHECK_EQUAL(ss.str(), str);
    }

    {
        // Test Range with clamp style (i.e. default one)
        const std::string strEnd =
            "    from_reference: !<RangeTransform> {minInValue: -0.0109, "
            "maxInValue: 1.0505, minOutValue: 0.0009, maxOutValue: 2.5001, "
            "direction: inverse}\n";
        const std::string str = PROFILE_V2 + SIMPLE_PROFILE + strEnd;

        std::istringstream is;
        is.str(str);

        OCIO::ConstConfigRcPtr config;
        OCIO_CHECK_NO_THROW(config = OCIO::Config::CreateFromStream(is));
        OCIO_CHECK_NO_THROW(config->sanityCheck());

        std::stringstream ss;
        ss << *config.get();
        OCIO_CHECK_EQUAL(ss.str(), str);
    }

    {
        // Test Range with clamp style
        const std::string in_strEnd =
            "    from_reference: !<RangeTransform> {minInValue: -0.0109, "
            "maxInValue: 1.0505, minOutValue: 0.0009, maxOutValue: 2.5001, "
            "style: Clamp, direction: inverse}\n";
        const std::string in_str = PROFILE_V2 + SIMPLE_PROFILE + in_strEnd;

        std::istringstream is;
        is.str(in_str);

        OCIO::ConstConfigRcPtr config;
        OCIO_CHECK_NO_THROW(config = OCIO::Config::CreateFromStream(is));
        OCIO_CHECK_NO_THROW(config->sanityCheck());

        // Clamp style is not saved
        const std::string out_strEnd =
            "    from_reference: !<RangeTransform> {minInValue: -0.0109, "
            "maxInValue: 1.0505, minOutValue: 0.0009, maxOutValue: 2.5001, "
            "direction: inverse}\n";
        const std::string out_str = PROFILE_V2 + SIMPLE_PROFILE + out_strEnd;

        std::stringstream ss;
        ss << *config.get();
        OCIO_CHECK_EQUAL(ss.str(), out_str);
    }

    {
        const std::string strEnd =
            "    from_reference: !<RangeTransform> "
            "{minInValue: 0, maxOutValue: 1}\n";
        const std::string str = PROFILE_V2 + SIMPLE_PROFILE + strEnd;

        std::istringstream is;
        is.str(str);
        OCIO::ConstConfigRcPtr config;
        OCIO_CHECK_NO_THROW(config = OCIO::Config::CreateFromStream(is));
        OCIO_CHECK_THROW_WHAT(config->sanityCheck(), 
                              OCIO::Exception, 
                              "must be both set or both missing");

        std::stringstream ss;
        ss << *config.get();
        OCIO_CHECK_EQUAL(ss.str(), str);
    }

    {
        // maxInValue has an illegal second number.
        const std::string strEndFail =
            "    from_reference: !<RangeTransform> {minInValue: -0.01, "
            "maxInValue: 1.05  10, minOutValue: 0.0009, maxOutValue: 2.5}\n";
        const std::string strEnd =
            "    from_reference: !<RangeTransform> {minInValue: -0.01, "
            "maxInValue: 1.05, minOutValue: 0.0009, maxOutValue: 2.5}\n";

        const std::string str = PROFILE_V2 + SIMPLE_PROFILE + strEndFail;
        const std::string strSaved = PROFILE_V2 + SIMPLE_PROFILE + strEnd;

        std::istringstream is;
        is.str(str);
        OCIO::ConstConfigRcPtr config;
        OCIO_CHECK_THROW_WHAT(OCIO::Config::CreateFromStream(is),
                              OCIO::Exception, "parsing double failed");

        is.str(strSaved);
        OCIO_CHECK_NO_THROW(config = OCIO::Config::CreateFromStream(is));
        OCIO_CHECK_NO_THROW(config->sanityCheck());

        // Re-serialize and test that it matches the expected text.
        std::stringstream ss;
        ss << *config.get();
        OCIO_CHECK_EQUAL(ss.str(), strSaved);
    }

    {
        // maxInValue & maxOutValue have no value, they will not be defined.
        const std::string strEnd =
            "    from_reference: !<RangeTransform> {minInValue: -0.01, "
            "maxInValue: , minOutValue: 0.0009, maxOutValue: }\n";
        const std::string strEndSaved =
            "    from_reference: !<RangeTransform> {minInValue: -0.01, "
            "minOutValue: 0.0009}\n";
        const std::string str = PROFILE_V2 + SIMPLE_PROFILE + strEnd;
        const std::string strSaved = PROFILE_V2 + SIMPLE_PROFILE + strEndSaved;

        std::istringstream is;
        is.str(str);
        OCIO::ConstConfigRcPtr config;
        OCIO_CHECK_NO_THROW(config = OCIO::Config::CreateFromStream(is));
        OCIO_CHECK_NO_THROW(config->sanityCheck());

        // Re-serialize and test that it matches the expected text.
        std::stringstream ss;
        ss << *config.get();
        OCIO_CHECK_EQUAL(ss.str(), strSaved);
    }

    {
        const std::string strEnd =
            "    from_reference: !<RangeTransform> "
            "{minInValue: 0.12345678901234, maxOutValue: 1.23456789012345}\n";
        const std::string str = PROFILE_V2 + SIMPLE_PROFILE + strEnd;

        std::istringstream is;
        is.str(str);
        OCIO::ConstConfigRcPtr config;
        OCIO_CHECK_NO_THROW(config = OCIO::Config::CreateFromStream(is));
        OCIO_CHECK_THROW_WHAT(config->sanityCheck(),
            OCIO::Exception,
            "must be both set or both missing");

        std::stringstream ss;
        ss << *config.get();
        OCIO_CHECK_EQUAL(ss.str(), str);
    }

    {
        const std::string strEnd =
            "    from_reference: !<RangeTransform> {minInValue: -0.01, "
            "maxInValue: 1.05, minOutValue: 0.0009, maxOutValue: 2.5}\n";
        const std::string str = PROFILE_V2 + SIMPLE_PROFILE + strEnd;

        std::istringstream is;
        is.str(str);
        OCIO::ConstConfigRcPtr config;
        OCIO_CHECK_NO_THROW(config = OCIO::Config::CreateFromStream(is));
        OCIO_CHECK_NO_THROW(config->sanityCheck());

        // Re-serialize and test that it matches the original text.
        std::stringstream ss;
        ss << *config.get();
        OCIO_CHECK_EQUAL(ss.str(), str);
    }

    {
        const std::string strEnd =
            "    from_reference: !<RangeTransform> {minOutValue: 0.0009, "
            "maxOutValue: 2.5}\n";
        const std::string str = PROFILE_V2 + SIMPLE_PROFILE + strEnd;

        std::istringstream is;
        is.str(str);
        OCIO::ConstConfigRcPtr config;
        OCIO_CHECK_NO_THROW(config = OCIO::Config::CreateFromStream(is));
        OCIO_CHECK_THROW_WHAT(config->sanityCheck(),
                              OCIO::Exception,
                              "must be both set or both missing");

        std::stringstream ss;
        ss << *config.get();
        OCIO_CHECK_EQUAL(ss.str(), str);
    }

    {
        const std::string strEnd =
            "    from_reference: !<GroupTransform>\n"
            "      children:\n"
            "        - !<RangeTransform> {minInValue: -0.01, maxInValue: 1.05, "
            "minOutValue: 0.0009, maxOutValue: 2.5}\n"
            "        - !<RangeTransform> {minOutValue: 0.0009, maxOutValue: 2.1}\n"
            "        - !<RangeTransform> {minOutValue: 0.1, maxOutValue: 0.9}\n";
        const std::string str = PROFILE_V2 + SIMPLE_PROFILE + strEnd;

        std::istringstream is;
        is.str(str);
        OCIO::ConstConfigRcPtr config;
        OCIO_CHECK_NO_THROW(config = OCIO::Config::CreateFromStream(is));
        OCIO_CHECK_THROW_WHAT(config->sanityCheck(),
                              OCIO::Exception,
                              "must be both set or both missing");

        // Re-serialize and test that it matches the original text.
        std::stringstream ss;
        ss << *config.get();
        OCIO_CHECK_EQUAL(ss.str(), str);
    }

    // Some faulty cases

    {
        const std::string strEnd =
            "    from_reference: !<GroupTransform>\n"
            "      children:\n"
            // missing { (and mInValue is wrong -> that's a warning)
            "        - !<RangeTransform> mInValue: -0.01, maxInValue: 1.05, "
            "minOutValue: 0.0009, maxOutValue: 2.5}\n";
        const std::string str = PROFILE_V2 + SIMPLE_PROFILE + strEnd;

        std::istringstream is;
        is.str(str);
        OCIO_CHECK_THROW_WHAT(OCIO::Config::CreateFromStream(is),
                              OCIO::Exception,
                              "Loading the OCIO profile failed");
    }

    {
        const std::string strEnd =
            // The comma is missing after the minInValue value.
            "    from_reference: !<RangeTransform> {minInValue: -0.01 "
            "maxInValue: 1.05, minOutValue: 0.0009, maxOutValue: 2.5}\n";
        const std::string str = PROFILE_V2 + SIMPLE_PROFILE + strEnd;

        std::istringstream is;
        is.str(str);
        OCIO_CHECK_THROW_WHAT(OCIO::Config::CreateFromStream(is),
                              OCIO::Exception,
                              "Loading the OCIO profile failed");
    }

    {
        const std::string strEnd =
            "    from_reference: !<RangeTransform> {minInValue: -0.01, "
            // The comma is missing between the minOutValue value and
            // the maxOutValue tag.
            "maxInValue: 1.05, minOutValue: 0.0009maxOutValue: 2.5}\n";
        const std::string str = PROFILE_V2 + SIMPLE_PROFILE + strEnd;

        std::istringstream is;
        is.str(str);
        OCIO_CHECK_THROW_WHAT(OCIO::Config::CreateFromStream(is),
                              OCIO::Exception,
                              "Loading the OCIO profile failed");
    }
}

OCIO_ADD_TEST(Config, exponent_serialization)   
{   
    {   
        const std::string strEnd =  
            "    from_reference: !<ExponentTransform> " 
            "{value: [1.101, 1.202, 1.303, 1.404]}\n";  
        const std::string str = PROFILE_V1 + SIMPLE_PROFILE + strEnd;

        std::istringstream is; 
        is.str(str);    
        OCIO::ConstConfigRcPtr config;  
        OCIO_CHECK_NO_THROW(config = OCIO::Config::CreateFromStream(is));   
        OCIO_CHECK_NO_THROW(config->sanityCheck()); 

        std::stringstream ss;  
        ss << *config.get();    
        OCIO_CHECK_EQUAL(ss.str(), str);    
    }   

     {  
        const std::string strEnd =  
            "    from_reference: !<ExponentTransform> " 
            "{value: [1.101, 1.202, 1.303, 1.404], direction: inverse}\n";  
        const std::string str = PROFILE_V1 + SIMPLE_PROFILE + strEnd;

        std::istringstream is; 
        is.str(str);    
        OCIO::ConstConfigRcPtr config;  
        OCIO_CHECK_NO_THROW(config = OCIO::Config::CreateFromStream(is));   
        OCIO_CHECK_NO_THROW(config->sanityCheck()); 

        std::stringstream ss;  
        ss << *config.get();    
        OCIO_CHECK_EQUAL(ss.str(), str);    
    }   

    // Errors

    {
        // Some gamma values are missing.
        const std::string strEnd =
            "    from_reference: !<ExponentTransform> "
            "{value: [1.1, 1.2, 1.3]}\n";
        const std::string str = PROFILE_V1 + SIMPLE_PROFILE + strEnd;

        std::istringstream is;
        is.str(str);
        OCIO::ConstConfigRcPtr config;
        OCIO_CHECK_THROW_WHAT(config = OCIO::Config::CreateFromStream(is),
                              OCIO::Exception,
                              "'value' values must be 4 floats. Found '3'");
    }
}

OCIO_ADD_TEST(Config, exponent_with_linear_serialization)
{
    {
        const std::string strEnd =
            "    from_reference: !<ExponentWithLinearTransform> "
            "{gamma: [1.1, 1.2, 1.3, 1.4], offset: [0.101, 0.102, 0.103, 0.1]}\n";
        const std::string str = PROFILE_V2 + SIMPLE_PROFILE + strEnd;

        std::istringstream is;
        is.str(str);
        OCIO::ConstConfigRcPtr config;
        OCIO_CHECK_NO_THROW(config = OCIO::Config::CreateFromStream(is));
        OCIO_CHECK_NO_THROW(config->sanityCheck());

        std::stringstream ss;
        ss << *config.get();
        OCIO_CHECK_EQUAL(ss.str(), str);
    }

    {
        const std::string strEnd =
            "    from_reference: !<ExponentWithLinearTransform> "
            "{gamma: [1.1, 1.2, 1.3, 1.4], offset: [0.101, 0.102, 0.103, 0.1], direction: inverse}\n";
        const std::string str = PROFILE_V2 + SIMPLE_PROFILE + strEnd;

        std::istringstream is;
        is.str(str);
        OCIO::ConstConfigRcPtr config;
        OCIO_CHECK_NO_THROW(config = OCIO::Config::CreateFromStream(is));
        OCIO_CHECK_NO_THROW(config->sanityCheck());

        std::stringstream ss;
        ss << *config.get();
        OCIO_CHECK_EQUAL(ss.str(), str);
    }

    // Errors

    {
        const std::string strEnd =
            "    from_reference: !<ExponentWithLinearTransform> {}\n";
        const std::string str = PROFILE_V2 + SIMPLE_PROFILE + strEnd;

        std::istringstream is;
        is.str(str);
        OCIO::ConstConfigRcPtr config;
        OCIO_CHECK_THROW_WHAT(config = OCIO::Config::CreateFromStream(is),
                              OCIO::Exception,
                              "ExponentWithLinear parse error, gamma and offset fields are missing");
    }

    {
        // Offset values are missing.
        const std::string strEnd =
            "    from_reference: !<ExponentWithLinearTransform> "
            "{gamma: [1.1, 1.2, 1.3, 1.4]}\n";
        const std::string str = PROFILE_V2 + SIMPLE_PROFILE + strEnd;

        std::istringstream is;
        is.str(str);
        OCIO::ConstConfigRcPtr config;
        OCIO_CHECK_THROW_WHAT(config = OCIO::Config::CreateFromStream(is),
                              OCIO::Exception,
                              "ExponentWithLinear parse error, offset field is missing");
    }

    {
        // Gamma values are missing.
        const std::string strEnd =
            "    from_reference: !<ExponentWithLinearTransform> "
            "{offset: [1.1, 1.2, 1.3, 1.4]}\n";
        const std::string str = PROFILE_V2 + SIMPLE_PROFILE + strEnd;

        std::istringstream is;
        is.str(str);
        OCIO::ConstConfigRcPtr config;
        OCIO_CHECK_THROW_WHAT(config = OCIO::Config::CreateFromStream(is),
                              OCIO::Exception,
                              "ExponentWithLinear parse error, gamma field is missing");
    }

    {
        // Some gamma values are missing.
        const std::string strEnd =
            "    from_reference: !<ExponentWithLinearTransform> "
            "{gamma: [1.1, 1.2, 1.3]}\n";
        const std::string str = PROFILE_V2 + SIMPLE_PROFILE + strEnd;

        std::istringstream is;
        is.str(str);
        OCIO::ConstConfigRcPtr config;
        OCIO_CHECK_THROW_WHAT(config = OCIO::Config::CreateFromStream(is),
                              OCIO::Exception,
                              "ExponentWithLinear parse error, gamma field must be 4 floats");
    }
    {
        // Some offset values are missing.
        const std::string strEnd =
            "    from_reference: !<ExponentWithLinearTransform> "
            "{gamma: [1.1, 1.2, 1.3, 1.4], offset: [0.101, 0.102]}\n";
        const std::string str = PROFILE_V2 + SIMPLE_PROFILE + strEnd;

        std::istringstream is;
        is.str(str);
        OCIO::ConstConfigRcPtr config;
        OCIO_CHECK_THROW_WHAT(config = OCIO::Config::CreateFromStream(is),
                              OCIO::Exception,
                              "ExponentWithLinear parse error, offset field must be 4 floats");
    }
}

OCIO_ADD_TEST(Config, exponent_vs_config_version)
{
    // The config i.e. SIMPLE_PROFILE is a version 2.

    std::istringstream is;
    OCIO::ConstConfigRcPtr config;
    OCIO::ConstProcessorRcPtr processor;

    // OCIO config file version == 1  and exponent == 1

    const std::string strEnd =
        "    from_reference: !<ExponentTransform> {value: [1, 1, 1, 1]}\n";
    const std::string str = PROFILE_V1 + SIMPLE_PROFILE + strEnd;

    is.str(str);
    OCIO_CHECK_NO_THROW(config = OCIO::Config::CreateFromStream(is));
    OCIO_CHECK_NO_THROW(config->sanityCheck());

    OCIO_CHECK_NO_THROW(processor = config->getProcessor("raw", "lnh"));

    OCIO::ConstCPUProcessorRcPtr cpuProcessor;
    OCIO_CHECK_NO_THROW(cpuProcessor = processor->getDefaultCPUProcessor());

    float img1[4] = { -0.5f, 0.0f, 1.0f, 1.0f };
    OCIO_CHECK_NO_THROW(cpuProcessor->applyRGBA(img1));

    OCIO_CHECK_EQUAL(img1[0], -0.5f);
    OCIO_CHECK_EQUAL(img1[1],  0.0f);
    OCIO_CHECK_EQUAL(img1[2],  1.0f);
    OCIO_CHECK_EQUAL(img1[3],  1.0f);

    // OCIO config file version == 1  and exponent != 1

    const std::string strEnd2 =
        "    from_reference: !<ExponentTransform> {value: [2, 2, 2, 1]}\n";
    const std::string str2 = PROFILE_V1 + SIMPLE_PROFILE + strEnd2;

    is.str(str2);
    OCIO_CHECK_NO_THROW(config = OCIO::Config::CreateFromStream(is));
    OCIO_CHECK_NO_THROW(config->sanityCheck());

    OCIO_CHECK_NO_THROW(processor = config->getProcessor("raw", "lnh"));
    OCIO_CHECK_NO_THROW(cpuProcessor = processor->getDefaultCPUProcessor());

    float img2[4] = { -0.5f, 0.0f, 1.0f, 1.0f };
    OCIO_CHECK_NO_THROW(cpuProcessor->applyRGBA(img2));

    OCIO_CHECK_EQUAL(img2[0],  0.0f);
    OCIO_CHECK_EQUAL(img2[1],  0.0f);
    OCIO_CHECK_EQUAL(img2[2],  1.0f);
    OCIO_CHECK_EQUAL(img2[3],  1.0f);

    // OCIO config file version > 1  and exponent == 1

    std::string str3 = PROFILE_V2 + SIMPLE_PROFILE + strEnd;
    is.str(str3);
    OCIO_CHECK_NO_THROW(config = OCIO::Config::CreateFromStream(is));
    OCIO_CHECK_NO_THROW(config->sanityCheck());

    OCIO_CHECK_NO_THROW(processor = config->getProcessor("raw", "lnh"));
    OCIO_CHECK_NO_THROW(cpuProcessor = processor->getDefaultCPUProcessor());

    float img3[4] = { -0.5f, 0.0f, 1.0f, 1.0f };
    OCIO_CHECK_NO_THROW(cpuProcessor->applyRGBA(img3));

    OCIO_CHECK_EQUAL(img3[0], 0.0f);
    OCIO_CHECK_EQUAL(img3[1], 0.0f);
    OCIO_CHECK_CLOSE(img3[2], 1.0f, 2e-5f); // Because of SSE optimizations.
    OCIO_CHECK_CLOSE(img3[3], 1.0f, 2e-5f); // Because of SSE optimizations.

    // OCIO config file version > 1  and exponent != 1

    std::string str4 = PROFILE_V2 + SIMPLE_PROFILE + strEnd2;
    is.str(str4);
    OCIO_CHECK_NO_THROW(config = OCIO::Config::CreateFromStream(is));
    OCIO_CHECK_NO_THROW(config->sanityCheck());

    OCIO_CHECK_NO_THROW(processor = config->getProcessor("raw", "lnh"));
    OCIO_CHECK_NO_THROW(cpuProcessor = processor->getDefaultCPUProcessor());

    float img4[4] = { -0.5f, 0.0f, 1.0f, 1.0f };
    OCIO_CHECK_NO_THROW(cpuProcessor->applyRGBA(img4));

    OCIO_CHECK_EQUAL(img4[0], 0.0f);
    OCIO_CHECK_EQUAL(img4[1], 0.0f);
    OCIO_CHECK_CLOSE(img4[2], 1.0f, 3e-5f); // Because of SSE optimizations.
    OCIO_CHECK_CLOSE(img4[3], 1.0f, 2e-5f); // Because of SSE optimizations.
}

OCIO_ADD_TEST(Config, categories)
{
    static const std::string MY_OCIO_CONFIG =
        "ocio_profile_version: 1\n"
        "\n"
        "search_path: luts\n"
        "strictparsing: true\n"
        "luma: [0.2126, 0.7152, 0.0722]\n"
        "\n"
        "roles:\n"
        "  default: raw1\n"
        "  scene_linear: raw1\n"
        "\n"
        "displays:\n"
        "  sRGB:\n"
        "    - !<View> {name: Raw, colorspace: raw1}\n"
        "\n"
        "active_displays: []\n"
        "active_views: []\n"
        "\n"
        "colorspaces:\n"
        "  - !<ColorSpace>\n"
        "    name: raw1\n"
        "    family: \"\"\n"
        "    equalitygroup: \"\"\n"
        "    bitdepth: unknown\n"
        "    isdata: false\n"
        "    categories: [rendering, linear]\n"
        "    allocation: uniform\n"
        "    allocationvars: [-0.125, 1.125]\n"
        "\n"
        "  - !<ColorSpace>\n"
        "    name: raw2\n"
        "    family: \"\"\n"
        "    equalitygroup: \"\"\n"
        "    bitdepth: unknown\n"
        "    isdata: false\n"
        "    categories: [rendering]\n"
        "    allocation: uniform\n"
        "    allocationvars: [-0.125, 1.125]\n";

    std::istringstream is;
    is.str(MY_OCIO_CONFIG);

    OCIO::ConstConfigRcPtr config;
    OCIO_CHECK_NO_THROW(config = OCIO::Config::CreateFromStream(is));
    OCIO_CHECK_NO_THROW(config->sanityCheck());

    // Test the serialization & deserialization.

    std::stringstream ss;
    ss << *config.get();
    OCIO_CHECK_EQUAL(ss.str(), MY_OCIO_CONFIG);

    // Test the config content.

    OCIO::ColorSpaceSetRcPtr css = config->getColorSpaces(nullptr);
    OCIO_CHECK_EQUAL(css->getNumColorSpaces(), 2);
    OCIO::ConstColorSpaceRcPtr cs = css->getColorSpaceByIndex(0);
    OCIO_CHECK_EQUAL(cs->getNumCategories(), 2);
    OCIO_CHECK_EQUAL(std::string(cs->getCategory(0)), std::string("rendering"));
    OCIO_CHECK_EQUAL(std::string(cs->getCategory(1)), std::string("linear"));

    css = config->getColorSpaces("linear");
    OCIO_CHECK_EQUAL(css->getNumColorSpaces(), 1);
    cs = css->getColorSpaceByIndex(0);
    OCIO_CHECK_EQUAL(cs->getNumCategories(), 2);
    OCIO_CHECK_EQUAL(std::string(cs->getCategory(0)), std::string("rendering"));
    OCIO_CHECK_EQUAL(std::string(cs->getCategory(1)), std::string("linear"));

    css = config->getColorSpaces("rendering");
    OCIO_CHECK_EQUAL(css->getNumColorSpaces(), 2);

    OCIO_CHECK_EQUAL(config->getNumColorSpaces(), 2);
    OCIO_CHECK_EQUAL(std::string(config->getColorSpaceNameByIndex(0)), std::string("raw1"));
    OCIO_CHECK_EQUAL(std::string(config->getColorSpaceNameByIndex(1)), std::string("raw2"));
    OCIO_CHECK_EQUAL(config->getIndexForColorSpace("raw1"), 0);
    OCIO_CHECK_EQUAL(config->getIndexForColorSpace("raw2"), 1);
    cs = config->getColorSpace("raw1");
    OCIO_CHECK_EQUAL(std::string(cs->getName()), std::string("raw1"));
    cs = config->getColorSpace("raw2");
    OCIO_CHECK_EQUAL(std::string(cs->getName()), std::string("raw2"));
}

OCIO_ADD_TEST(Config, display)
{
    // Guard to automatically unset the env. variable.
    class Guard
    {
    public:
        Guard() = default;
        ~Guard()
        {
            OCIO::Platform::Setenv(OCIO::OCIO_ACTIVE_DISPLAYS_ENVVAR, "");
        }
    } guard;


    static const std::string SIMPLE_PROFILE_HEADER =
        "ocio_profile_version: 2\n"
        "\n"
        "search_path: luts\n"
        "strictparsing: true\n"
        "luma: [0.2126, 0.7152, 0.0722]\n"
        "\n"
        "roles:\n"
        "  default: raw\n"
        "  scene_linear: lnh\n"
        "\n"
        "displays:\n"
        "  sRGB_2:\n"
        "    - !<View> {name: Raw, colorspace: raw}\n"
        "  sRGB_F:\n"
        "    - !<View> {name: Raw, colorspace: raw}\n"
        "  sRGB_1:\n"
        "    - !<View> {name: Raw, colorspace: raw}\n"
        "  sRGB_3:\n"
        "    - !<View> {name: Raw, colorspace: raw}\n"
        "  sRGB_B:\n"
        "    - !<View> {name: Raw, colorspace: raw}\n"
        "  sRGB_A:\n"
        "    - !<View> {name: Raw, colorspace: raw}\n"
        "\n";

    static const std::string SIMPLE_PROFILE_FOOTER =
        "\n"
        "colorspaces:\n"
        "  - !<ColorSpace>\n"
        "    name: raw\n"
        "    family: \"\"\n"
        "    equalitygroup: \"\"\n"
        "    bitdepth: unknown\n"
        "    isdata: false\n"
        "    allocation: uniform\n"
        "\n"
        "  - !<ColorSpace>\n"
        "    name: lnh\n"
        "    family: \"\"\n"
        "    equalitygroup: \"\"\n"
        "    bitdepth: unknown\n"
        "    isdata: false\n"
        "    allocation: uniform\n";

    {
        const std::string myProfile = 
            SIMPLE_PROFILE_HEADER
            +
            "active_displays: []\n"
            "active_views: []\n"
            + SIMPLE_PROFILE_FOOTER;

        std::istringstream is(myProfile);
        OCIO::ConstConfigRcPtr config;
        OCIO_CHECK_NO_THROW(config = OCIO::Config::CreateFromStream(is));
        OCIO_CHECK_NO_THROW(config->sanityCheck());

        OCIO_REQUIRE_EQUAL(config->getNumDisplays(), 6);
        OCIO_CHECK_EQUAL(std::string(config->getDisplay(0)), std::string("sRGB_2"));
        OCIO_CHECK_EQUAL(std::string(config->getDisplay(1)), std::string("sRGB_F"));
        OCIO_CHECK_EQUAL(std::string(config->getDisplay(2)), std::string("sRGB_1"));
        OCIO_CHECK_EQUAL(std::string(config->getDisplay(3)), std::string("sRGB_3"));
        OCIO_CHECK_EQUAL(std::string(config->getDisplay(4)), std::string("sRGB_B"));
        OCIO_CHECK_EQUAL(std::string(config->getDisplay(5)), std::string("sRGB_A"));
        OCIO_CHECK_EQUAL(std::string(config->getDefaultDisplay()), "sRGB_2");

        std::stringstream ss;
        ss << *config.get();
        OCIO_CHECK_EQUAL(ss.str(), myProfile);
    }

    {
        const std::string myProfile = 
            SIMPLE_PROFILE_HEADER
            +
            "active_displays: [sRGB_1]\n"
            "active_views: []\n"
            + SIMPLE_PROFILE_FOOTER;

        std::istringstream is(myProfile);
        OCIO::ConstConfigRcPtr config;
        OCIO_CHECK_NO_THROW(config = OCIO::Config::CreateFromStream(is));
        OCIO_CHECK_NO_THROW(config->sanityCheck());

        OCIO_REQUIRE_EQUAL(config->getNumDisplays(), 1);
        OCIO_CHECK_EQUAL(std::string(config->getDisplay(0)), std::string("sRGB_1"));
        OCIO_CHECK_EQUAL(std::string(config->getDefaultDisplay()), "sRGB_1");
    }

    {
        const std::string myProfile = 
            SIMPLE_PROFILE_HEADER
            +
            "active_displays: [sRGB_2, sRGB_1]\n"
            "active_views: []\n"
            + SIMPLE_PROFILE_FOOTER;

        std::istringstream is(myProfile);
        OCIO::ConstConfigRcPtr config;
        OCIO_CHECK_NO_THROW(config = OCIO::Config::CreateFromStream(is));

        OCIO_REQUIRE_EQUAL(config->getNumDisplays(), 2);
        OCIO_CHECK_EQUAL(std::string(config->getDisplay(0)), std::string("sRGB_2"));
        OCIO_CHECK_EQUAL(std::string(config->getDisplay(1)), std::string("sRGB_1"));
        OCIO_CHECK_EQUAL(std::string(config->getDefaultDisplay()), "sRGB_2");
    }

    {
        const std::string myProfile = 
            SIMPLE_PROFILE_HEADER
            +
            "active_displays: []\n"
            "active_views: []\n"
            + SIMPLE_PROFILE_FOOTER;

        OCIO::Platform::Setenv(OCIO::OCIO_ACTIVE_DISPLAYS_ENVVAR, " sRGB_3, sRGB_2");

        std::istringstream is(myProfile);
        OCIO::ConstConfigRcPtr config;
        OCIO_CHECK_NO_THROW(config = OCIO::Config::CreateFromStream(is));
        OCIO_CHECK_NO_THROW(config->sanityCheck());

        OCIO_REQUIRE_EQUAL(config->getNumDisplays(), 2);
        OCIO_CHECK_EQUAL(std::string(config->getDisplay(0)), std::string("sRGB_3"));
        OCIO_CHECK_EQUAL(std::string(config->getDisplay(1)), std::string("sRGB_2"));
        OCIO_CHECK_EQUAL(std::string(config->getDefaultDisplay()), "sRGB_3");
    }

    {
        const std::string myProfile = 
            SIMPLE_PROFILE_HEADER
            +
            "active_displays: [sRGB_2, sRGB_1]\n"
            "active_views: []\n"
            + SIMPLE_PROFILE_FOOTER;

        OCIO::Platform::Setenv(OCIO::OCIO_ACTIVE_DISPLAYS_ENVVAR, " sRGB_3, sRGB_2");

        std::istringstream is(myProfile);
        OCIO::ConstConfigRcPtr config;
        OCIO_CHECK_NO_THROW(config = OCIO::Config::CreateFromStream(is));
        OCIO_CHECK_NO_THROW(config->sanityCheck());

        OCIO_REQUIRE_EQUAL(config->getNumDisplays(), 2);
        OCIO_CHECK_EQUAL(std::string(config->getDisplay(0)), std::string("sRGB_3"));
        OCIO_CHECK_EQUAL(std::string(config->getDisplay(1)), std::string("sRGB_2"));
        OCIO_CHECK_EQUAL(std::string(config->getDefaultDisplay()), "sRGB_3");
    }

    {
        OCIO::Platform::Setenv(OCIO::OCIO_ACTIVE_DISPLAYS_ENVVAR, ""); // No value

        const std::string myProfile = 
            SIMPLE_PROFILE_HEADER
            +
            "active_displays: [sRGB_2, sRGB_1]\n"
            "active_views: []\n"
            + SIMPLE_PROFILE_FOOTER;

        std::istringstream is(myProfile);
        OCIO::ConstConfigRcPtr config;
        OCIO_CHECK_NO_THROW(config = OCIO::Config::CreateFromStream(is));
        OCIO_CHECK_NO_THROW(config->sanityCheck());

        OCIO_REQUIRE_EQUAL(config->getNumDisplays(), 2);
        OCIO_CHECK_EQUAL(std::string(config->getDisplay(0)), std::string("sRGB_2"));
        OCIO_CHECK_EQUAL(std::string(config->getDisplay(1)), std::string("sRGB_1"));
        OCIO_CHECK_EQUAL(std::string(config->getDefaultDisplay()), "sRGB_2");
    }

    {
        // No value, but misleading space.

        OCIO::Platform::Setenv(OCIO::OCIO_ACTIVE_DISPLAYS_ENVVAR, " ");

        const std::string myProfile = 
            SIMPLE_PROFILE_HEADER
            +
            "active_displays: [sRGB_2, sRGB_1]\n"
            "active_views: []\n"
            + SIMPLE_PROFILE_FOOTER;

        std::istringstream is(myProfile);
        OCIO::ConstConfigRcPtr config;
        OCIO_CHECK_NO_THROW(config = OCIO::Config::CreateFromStream(is));
        OCIO_CHECK_NO_THROW(config->sanityCheck());

        OCIO_REQUIRE_EQUAL(config->getNumDisplays(), 2);
        OCIO_CHECK_EQUAL(std::string(config->getDisplay(0)), std::string("sRGB_2"));
        OCIO_CHECK_EQUAL(std::string(config->getDisplay(1)), std::string("sRGB_1"));
        OCIO_CHECK_EQUAL(std::string(config->getDefaultDisplay()), "sRGB_2");
    }

    {
        // Test an unknown display name using the env. variable.

        OCIO::Platform::Setenv(OCIO::OCIO_ACTIVE_DISPLAYS_ENVVAR, "ABCDEF");

        const std::string myProfile = 
            SIMPLE_PROFILE_HEADER
            +
            "active_displays: [sRGB_2, sRGB_1]\n"
            "active_views: []\n"
            + SIMPLE_PROFILE_FOOTER;

        std::istringstream is(myProfile);
        OCIO::ConstConfigRcPtr config;
        OCIO_CHECK_NO_THROW(config = OCIO::Config::CreateFromStream(is));
        OCIO_CHECK_THROW_WHAT(config->sanityCheck(),
                              OCIO::Exception,
                              "The content of the env. variable for the list of active displays [ABCDEF] is invalid.");
    }

    {
        // Test an unknown display name using the env. variable.

        OCIO::Platform::Setenv(OCIO::OCIO_ACTIVE_DISPLAYS_ENVVAR, "sRGB_2, sRGB_1, ABCDEF");

        const std::string myProfile = 
            SIMPLE_PROFILE_HEADER
            +
            "active_displays: [sRGB_2, sRGB_1]\n"
            "active_views: []\n"
            + SIMPLE_PROFILE_FOOTER;

        std::istringstream is(myProfile);
        OCIO::ConstConfigRcPtr config;
        OCIO_CHECK_NO_THROW(config = OCIO::Config::CreateFromStream(is));
        OCIO_CHECK_THROW_WHAT(config->sanityCheck(),
                              OCIO::Exception,
                              "The content of the env. variable for the list of active displays"\
                              " [sRGB_2, sRGB_1, ABCDEF] contains invalid display name(s).");
    }

    {
        // Test an unknown display name in the config active displays.

        OCIO::Platform::Setenv(OCIO::OCIO_ACTIVE_DISPLAYS_ENVVAR, ""); // Unset the env. variable.

        const std::string myProfile = 
            SIMPLE_PROFILE_HEADER
            +
            "active_displays: [ABCDEF]\n"
            "active_views: []\n"
            + SIMPLE_PROFILE_FOOTER;

        std::istringstream is(myProfile);
        OCIO::ConstConfigRcPtr config;
        OCIO_CHECK_NO_THROW(config = OCIO::Config::CreateFromStream(is));
        OCIO_CHECK_THROW_WHAT(config->sanityCheck(),
                              OCIO::Exception,
                              "The list of active displays [ABCDEF] from the config file is invalid.");
    }

    {
        // Test an unknown display name in the config active displays.

        OCIO::Platform::Setenv(OCIO::OCIO_ACTIVE_DISPLAYS_ENVVAR, ""); // Unset the env. variable.

        const std::string myProfile = 
            SIMPLE_PROFILE_HEADER
            +
            "active_displays: [sRGB_2, sRGB_1, ABCDEF]\n"
            "active_views: []\n"
            + SIMPLE_PROFILE_FOOTER;

        std::istringstream is(myProfile);
        OCIO::ConstConfigRcPtr config;
        OCIO_CHECK_NO_THROW(config = OCIO::Config::CreateFromStream(is));
        OCIO_CHECK_THROW_WHAT(config->sanityCheck(),
                              OCIO::Exception,
                              "The list of active displays [sRGB_2, sRGB_1, ABCDEF] "\
                              "from the config file contains invalid display name(s)");
    }
}

OCIO_ADD_TEST(Config, view)
{
    // Guard to automatically unset the env. variable.
    class Guard
    {
    public:
        Guard() = default;
        ~Guard()
        {
            OCIO::Platform::Setenv(OCIO::OCIO_ACTIVE_VIEWS_ENVVAR, "");
        }
    } guard;


    static const std::string SIMPLE_PROFILE_HEADER =
        "ocio_profile_version: 1\n"
        "\n"
        "search_path: luts\n"
        "strictparsing: true\n"
        "luma: [0.2126, 0.7152, 0.0722]\n"
        "\n"
        "roles:\n"
        "  default: raw\n"
        "  scene_linear: lnh\n"
        "\n"
        "displays:\n"
        "  sRGB_1:\n"
        "    - !<View> {name: View_1, colorspace: raw}\n"
        "    - !<View> {name: View_2, colorspace: raw}\n"
        "  sRGB_2:\n"
        "    - !<View> {name: View_2, colorspace: raw}\n"
        "    - !<View> {name: View_3, colorspace: raw}\n"
        "  sRGB_3:\n"
        "    - !<View> {name: View_3, colorspace: raw}\n"
        "    - !<View> {name: View_1, colorspace: raw}\n"
        "\n";

    static const std::string SIMPLE_PROFILE_FOOTER =
        "\n"
        "colorspaces:\n"
        "  - !<ColorSpace>\n"
        "    name: raw\n"
        "    family: \"\"\n"
        "    equalitygroup: \"\"\n"
        "    bitdepth: unknown\n"
        "    isdata: false\n"
        "    allocation: uniform\n"
        "\n"
        "  - !<ColorSpace>\n"
        "    name: lnh\n"
        "    family: \"\"\n"
        "    equalitygroup: \"\"\n"
        "    bitdepth: unknown\n"
        "    isdata: false\n"
        "    allocation: uniform\n";

    {
        std::string myProfile = 
            SIMPLE_PROFILE_HEADER
            +
            "active_displays: []\n"
            "active_views: []\n"
            + SIMPLE_PROFILE_FOOTER;

        std::istringstream is(myProfile);
        OCIO::ConstConfigRcPtr config;
        OCIO_CHECK_NO_THROW(config = OCIO::Config::CreateFromStream(is));
        OCIO_CHECK_EQUAL(std::string(config->getDefaultView("sRGB_1")), "View_1");
        OCIO_REQUIRE_EQUAL(config->getNumViews("sRGB_1"), 2);
        OCIO_CHECK_EQUAL(std::string(config->getView("sRGB_1", 0)), "View_1");
        OCIO_CHECK_EQUAL(std::string(config->getView("sRGB_1", 1)), "View_2");
        OCIO_CHECK_EQUAL(std::string(config->getDefaultView("sRGB_2")), "View_2");
        OCIO_REQUIRE_EQUAL(config->getNumViews("sRGB_2"), 2);
        OCIO_CHECK_EQUAL(std::string(config->getView("sRGB_2", 0)), "View_2");
        OCIO_CHECK_EQUAL(std::string(config->getView("sRGB_2", 1)), "View_3");
        OCIO_CHECK_EQUAL(std::string(config->getDefaultView("sRGB_3")), "View_3");
        OCIO_REQUIRE_EQUAL(config->getNumViews("sRGB_3"), 2);
        OCIO_CHECK_EQUAL(std::string(config->getView("sRGB_3", 0)), "View_3");
        OCIO_CHECK_EQUAL(std::string(config->getView("sRGB_3", 1)), "View_1");
    }

    {
        std::string myProfile = 
            SIMPLE_PROFILE_HEADER
            +
            "active_displays: []\n"
            "active_views: [View_3]\n"
            + SIMPLE_PROFILE_FOOTER;

        std::istringstream is(myProfile);
        OCIO::ConstConfigRcPtr config;
        OCIO_CHECK_NO_THROW(config = OCIO::Config::CreateFromStream(is));
        OCIO_CHECK_EQUAL(std::string(config->getDefaultView("sRGB_1")), "View_1");
        OCIO_REQUIRE_EQUAL(config->getNumViews("sRGB_1"), 2);
        OCIO_CHECK_EQUAL(std::string(config->getView("sRGB_1", 0)), "View_1");
        OCIO_CHECK_EQUAL(std::string(config->getView("sRGB_1", 1)), "View_2");
        OCIO_CHECK_EQUAL(std::string(config->getDefaultView("sRGB_2")), "View_3");
        OCIO_REQUIRE_EQUAL(config->getNumViews("sRGB_2"), 1);
        OCIO_CHECK_EQUAL(std::string(config->getView("sRGB_2", 0)), "View_3");
        OCIO_CHECK_EQUAL(std::string(config->getDefaultView("sRGB_3")), "View_3");
        OCIO_REQUIRE_EQUAL(config->getNumViews("sRGB_3"), 1);
        OCIO_CHECK_EQUAL(std::string(config->getView("sRGB_3", 0)), "View_3");
    }

    {
        std::string myProfile = 
            SIMPLE_PROFILE_HEADER
            +
            "active_displays: []\n"
            "active_views: [View_3, View_2, View_1]\n"
            + SIMPLE_PROFILE_FOOTER;

        std::istringstream is(myProfile);
        OCIO::ConstConfigRcPtr config;
        OCIO_CHECK_NO_THROW(config = OCIO::Config::CreateFromStream(is));
        OCIO_CHECK_EQUAL(std::string(config->getDefaultView("sRGB_1")), "View_2");
        OCIO_REQUIRE_EQUAL(config->getNumViews("sRGB_1"), 2);
        OCIO_CHECK_EQUAL(std::string(config->getView("sRGB_1", 0)), "View_2");
        OCIO_CHECK_EQUAL(std::string(config->getView("sRGB_1", 1)), "View_1");
        OCIO_CHECK_EQUAL(std::string(config->getDefaultView("sRGB_2")), "View_3");
        OCIO_REQUIRE_EQUAL(config->getNumViews("sRGB_2"), 2);
        OCIO_CHECK_EQUAL(std::string(config->getView("sRGB_2", 0)), "View_3");
        OCIO_CHECK_EQUAL(std::string(config->getView("sRGB_2", 1)), "View_2");
        OCIO_CHECK_EQUAL(std::string(config->getDefaultView("sRGB_3")), "View_3");
        OCIO_REQUIRE_EQUAL(config->getNumViews("sRGB_3"), 2);
        OCIO_CHECK_EQUAL(std::string(config->getView("sRGB_3", 0)), "View_3");
        OCIO_CHECK_EQUAL(std::string(config->getView("sRGB_3", 1)), "View_1");
    }

    {
        std::string myProfile = 
            SIMPLE_PROFILE_HEADER
            +
            "active_displays: []\n"
            "active_views: []\n"
            + SIMPLE_PROFILE_FOOTER;

        OCIO::Platform::Setenv(OCIO::OCIO_ACTIVE_VIEWS_ENVVAR, " View_3, View_2");

        std::istringstream is(myProfile);
        OCIO::ConstConfigRcPtr config;
        OCIO_CHECK_NO_THROW(config = OCIO::Config::CreateFromStream(is));
        OCIO_CHECK_EQUAL(std::string(config->getDefaultView("sRGB_1")), "View_2");
        OCIO_REQUIRE_EQUAL(config->getNumViews("sRGB_1"), 1);
        OCIO_CHECK_EQUAL(std::string(config->getView("sRGB_1", 0)), "View_2");
        OCIO_CHECK_EQUAL(std::string(config->getDefaultView("sRGB_2")), "View_3");
        OCIO_REQUIRE_EQUAL(config->getNumViews("sRGB_2"), 2);
        OCIO_CHECK_EQUAL(std::string(config->getView("sRGB_2", 0)), "View_3");
        OCIO_CHECK_EQUAL(std::string(config->getView("sRGB_2", 1)), "View_2");
        OCIO_CHECK_EQUAL(std::string(config->getDefaultView("sRGB_3")), "View_3");
        OCIO_REQUIRE_EQUAL(config->getNumViews("sRGB_3"), 1);
        OCIO_CHECK_EQUAL(std::string(config->getView("sRGB_3", 0)), "View_3");
    }

    {
        std::string myProfile = 
            SIMPLE_PROFILE_HEADER
            +
            "active_displays: []\n"
            "active_views: []\n"
            + SIMPLE_PROFILE_FOOTER;

        OCIO::Platform::Setenv(OCIO::OCIO_ACTIVE_VIEWS_ENVVAR, ""); // No value.

        std::istringstream is(myProfile);
        OCIO::ConstConfigRcPtr config;
        OCIO_CHECK_NO_THROW(config = OCIO::Config::CreateFromStream(is));
        OCIO_CHECK_EQUAL(std::string(config->getDefaultView("sRGB_1")), "View_1");
        OCIO_REQUIRE_EQUAL(config->getNumViews("sRGB_1"), 2);
        OCIO_CHECK_EQUAL(std::string(config->getView("sRGB_1", 0)), "View_1");
        OCIO_CHECK_EQUAL(std::string(config->getView("sRGB_1", 1)), "View_2");
        OCIO_CHECK_EQUAL(std::string(config->getDefaultView("sRGB_2")), "View_2");
        OCIO_REQUIRE_EQUAL(config->getNumViews("sRGB_2"), 2);
        OCIO_CHECK_EQUAL(std::string(config->getView("sRGB_2", 0)), "View_2");
        OCIO_CHECK_EQUAL(std::string(config->getView("sRGB_2", 1)), "View_3");
        OCIO_CHECK_EQUAL(std::string(config->getDefaultView("sRGB_3")), "View_3");
        OCIO_REQUIRE_EQUAL(config->getNumViews("sRGB_3"), 2);
        OCIO_CHECK_EQUAL(std::string(config->getView("sRGB_3", 0)), "View_3");
        OCIO_CHECK_EQUAL(std::string(config->getView("sRGB_3", 1)), "View_1");
    }

    {
        std::string myProfile = 
            SIMPLE_PROFILE_HEADER
            +
            "active_displays: []\n"
            "active_views: []\n"
            + SIMPLE_PROFILE_FOOTER;

        OCIO::Platform::Setenv(OCIO::OCIO_ACTIVE_VIEWS_ENVVAR, " "); // No value, but misleading space

        std::istringstream is(myProfile);
        OCIO::ConstConfigRcPtr config;
        OCIO_CHECK_NO_THROW(config = OCIO::Config::CreateFromStream(is));
        OCIO_CHECK_EQUAL(std::string(config->getDefaultView("sRGB_1")), "View_1");
        OCIO_REQUIRE_EQUAL(config->getNumViews("sRGB_1"), 2);
        OCIO_CHECK_EQUAL(std::string(config->getView("sRGB_1", 0)), "View_1");
        OCIO_CHECK_EQUAL(std::string(config->getView("sRGB_1", 1)), "View_2");
        OCIO_CHECK_EQUAL(std::string(config->getDefaultView("sRGB_2")), "View_2");
        OCIO_REQUIRE_EQUAL(config->getNumViews("sRGB_2"), 2);
        OCIO_CHECK_EQUAL(std::string(config->getView("sRGB_2", 0)), "View_2");
        OCIO_CHECK_EQUAL(std::string(config->getView("sRGB_2", 1)), "View_3");
        OCIO_CHECK_EQUAL(std::string(config->getDefaultView("sRGB_3")), "View_3");
        OCIO_REQUIRE_EQUAL(config->getNumViews("sRGB_3"), 2);
        OCIO_CHECK_EQUAL(std::string(config->getView("sRGB_3", 0)), "View_3");
        OCIO_CHECK_EQUAL(std::string(config->getView("sRGB_3", 1)), "View_1");
    }
}

OCIO_ADD_TEST(Config, display_view_order)
{
    constexpr const char * SIMPLE_CONFIG { R"(
        ocio_profile_version: 2

        displays:
          sRGB_B:
            - !<View> {name: View_2, colorspace: raw}
            - !<View> {name: View_1, colorspace: raw}
          sRGB_D:
            - !<View> {name: View_2, colorspace: raw}
            - !<View> {name: View_3, colorspace: raw}
          sRGB_A:
            - !<View> {name: View_3, colorspace: raw}
            - !<View> {name: View_1, colorspace: raw}
          sRGB_C:
            - !<View> {name: View_4, colorspace: raw}
            - !<View> {name: View_1, colorspace: raw}

        colorspaces:
          - !<ColorSpace>
            name: raw
            allocation: uniform

          - !<ColorSpace>
            name: lnh
            allocation: uniform
        )" };

    std::istringstream is(SIMPLE_CONFIG);
    OCIO::ConstConfigRcPtr config;
    OCIO_CHECK_NO_THROW(config = OCIO::Config::CreateFromStream(is));
    OCIO_CHECK_NO_THROW(config->sanityCheck());

    OCIO_REQUIRE_EQUAL(config->getNumDisplays(), 4);

    // When active_displays is not defined, the displays are returned in config order.

    OCIO_CHECK_EQUAL(std::string(config->getDefaultDisplay()), "sRGB_B");

    OCIO_CHECK_EQUAL(std::string(config->getDisplay(0)), "sRGB_B");
    OCIO_CHECK_EQUAL(std::string(config->getDisplay(1)), "sRGB_D");
    OCIO_CHECK_EQUAL(std::string(config->getDisplay(2)), "sRGB_A");
    OCIO_CHECK_EQUAL(std::string(config->getDisplay(3)), "sRGB_C");

    // When active_views is not defined, the views are returned in config order.

    OCIO_CHECK_EQUAL(std::string(config->getDefaultView("sRGB_B")), "View_2");

    OCIO_REQUIRE_EQUAL(config->getNumViews("sRGB_B"), 2);
    OCIO_CHECK_EQUAL(std::string(config->getView("sRGB_B", 0)), "View_2");
    OCIO_CHECK_EQUAL(std::string(config->getView("sRGB_B", 1)), "View_1");
}

OCIO_ADD_TEST(Config, log_serialization)
{
    const std::string PROFILE_V1 = "ocio_profile_version: 1\n";
    const std::string PROFILE_V2 = "ocio_profile_version: 2\n";
    static const std::string SIMPLE_PROFILE =
        "\n"
        "search_path: luts\n"
        "strictparsing: true\n"
        "luma: [0.2126, 0.7152, 0.0722]\n"
        "\n"
        "roles:\n"
        "  default: raw\n"
        "  scene_linear: lnh\n"
        "\n"
        "displays:\n"
        "  sRGB:\n"
        "    - !<View> {name: Raw, colorspace: raw}\n"
        "\n"
        "active_displays: []\n"
        "active_views: []\n"
        "\n"
        "colorspaces:\n"
        "  - !<ColorSpace>\n"
        "    name: raw\n"
        "    family: \"\"\n"
        "    equalitygroup: \"\"\n"
        "    bitdepth: unknown\n"
        "    isdata: false\n"
        "    allocation: uniform\n"
        "\n"
        "  - !<ColorSpace>\n"
        "    name: lnh\n"
        "    family: \"\"\n"
        "    equalitygroup: \"\"\n"
        "    bitdepth: unknown\n"
        "    isdata: false\n"
        "    allocation: uniform\n";

    {
        // Log with default base value and default direction.
        const std::string strEnd =
            "    from_reference: !<LogTransform> {}\n";
        const std::string str = PROFILE_V1 + SIMPLE_PROFILE + strEnd;

        std::istringstream is;
        is.str(str);

        OCIO::ConstConfigRcPtr config;
        OCIO_CHECK_NO_THROW(config = OCIO::Config::CreateFromStream(is));
        OCIO_CHECK_NO_THROW(config->sanityCheck());

        std::stringstream ss;
        ss << *config.get();
        OCIO_CHECK_EQUAL(ss.str(), str);
    }

    {
        // Log with default base value.
        const std::string strEnd =
            "    from_reference: !<LogTransform> {direction: inverse}\n";
        const std::string str = PROFILE_V1 + SIMPLE_PROFILE + strEnd;

        std::istringstream is;
        is.str(str);

        OCIO::ConstConfigRcPtr config;
        OCIO_CHECK_NO_THROW(config = OCIO::Config::CreateFromStream(is));
        OCIO_CHECK_NO_THROW(config->sanityCheck());

        std::stringstream ss;
        ss << *config.get();
        OCIO_CHECK_EQUAL(ss.str(), str);
    }

    {
        // Log with specified base value.
        const std::string strEnd =
            "    from_reference: !<LogTransform> {base: 5}\n";
        const std::string str = PROFILE_V1 + SIMPLE_PROFILE + strEnd;

        std::istringstream is;
        is.str(str);

        OCIO::ConstConfigRcPtr config;
        OCIO_CHECK_NO_THROW(config = OCIO::Config::CreateFromStream(is));
        OCIO_CHECK_NO_THROW(config->sanityCheck());

        std::stringstream ss;
        ss << *config.get();
        OCIO_CHECK_EQUAL(ss.str(), str);
    }

    {
        // Log with specified base value and direction.
        const std::string strEnd =
            "    from_reference: !<LogTransform> {base: 7, direction: inverse}\n";
        const std::string str = PROFILE_V1 + SIMPLE_PROFILE + strEnd;

        std::istringstream is;
        is.str(str);

        OCIO::ConstConfigRcPtr config;
        OCIO_CHECK_NO_THROW(config = OCIO::Config::CreateFromStream(is));
        OCIO_CHECK_NO_THROW(config->sanityCheck());

        std::stringstream ss;
        ss << *config.get();
        OCIO_CHECK_EQUAL(ss.str(), str);
    }

    {
        // LogAffine with specified values 3 components.
        const std::string strEnd =
            "    from_reference: !<LogAffineTransform> {"
            "base: 10, "
            "logSideSlope: [1.3, 1.4, 1.5], "
            "logSideOffset: [0, 0, 0.1], "
            "linSideSlope: [1, 1, 1.1], "
            "linSideOffset: [0.1234567890123, 0.5, 0.1]}\n";
            const std::string str = PROFILE_V2 + SIMPLE_PROFILE + strEnd;

        std::istringstream is;
        is.str(str);

        OCIO::ConstConfigRcPtr config;
        OCIO_CHECK_NO_THROW(config = OCIO::Config::CreateFromStream(is));
        OCIO_CHECK_NO_THROW(config->sanityCheck());

        std::stringstream ss;
        ss << *config.get();
        OCIO_CHECK_EQUAL(ss.str(), str);
    }

    {
        // LogAffine with default value for base.
        const std::string strEnd =
            "    from_reference: !<LogAffineTransform> {"
            "logSideSlope: [1, 1, 1.1], "
            "logSideOffset: [0.1234567890123, 0.5, 0.1], "
            "linSideSlope: [1.3, 1.4, 1.5], "
            "linSideOffset: [0, 0, 0.1]}\n";

        const std::string str = PROFILE_V2 + SIMPLE_PROFILE + strEnd;

        std::istringstream is;
        is.str(str);

        OCIO::ConstConfigRcPtr config;
        OCIO_CHECK_NO_THROW(config = OCIO::Config::CreateFromStream(is));
        OCIO_CHECK_NO_THROW(config->sanityCheck());

        std::stringstream ss;
        ss << *config.get();
        OCIO_CHECK_EQUAL(ss.str(), str);
    }

    {
        // LogAffine with single value for linSideOffset.
        const std::string strEnd =
            "    from_reference: !<LogAffineTransform> {"
            "base: 10, "
            "logSideSlope: [1, 1, 1.1], "
            "logSideOffset: [0.1234567890123, 0.5, 0.1], "
            "linSideSlope: [1.3, 1.4, 1.5], "
            "linSideOffset: 0.5}\n";
        const std::string str = PROFILE_V2 + SIMPLE_PROFILE + strEnd;

        std::istringstream is;
        is.str(str);

        OCIO::ConstConfigRcPtr config;
        OCIO_CHECK_NO_THROW(config = OCIO::Config::CreateFromStream(is));
        OCIO_CHECK_NO_THROW(config->sanityCheck());

        std::stringstream ss;
        ss << *config.get();
        OCIO_CHECK_EQUAL(ss.str(), str);
    }

    {
        // LogAffine with single value for linSideSlope.
        const std::string strEnd =
            "    from_reference: !<LogAffineTransform> {"
            "logSideSlope: [1, 1, 1.1], "
            "linSideSlope: 1.3, "
            "linSideOffset: [0, 0, 0.1]}\n";
        const std::string str = PROFILE_V2 + SIMPLE_PROFILE + strEnd;

        std::istringstream is;
        is.str(str);

        OCIO::ConstConfigRcPtr config;
        OCIO_CHECK_NO_THROW(config = OCIO::Config::CreateFromStream(is));
        OCIO_CHECK_NO_THROW(config->sanityCheck());

        std::stringstream ss;
        ss << *config.get();
        OCIO_CHECK_EQUAL(ss.str(), str);
    }

    {
        // LogAffine with single value for logSideOffset.
        const std::string strEnd =
            "    from_reference: !<LogAffineTransform> {"
            "logSideSlope: [1, 1, 1.1], "
            "logSideOffset: 0.5, "
            "linSideSlope: [1.3, 1, 1], "
            "linSideOffset: [0, 0, 0.1]}\n";
        const std::string str = PROFILE_V2 + SIMPLE_PROFILE + strEnd;

        std::istringstream is;
        is.str(str);

        OCIO::ConstConfigRcPtr config;
        OCIO_CHECK_NO_THROW(config = OCIO::Config::CreateFromStream(is));
        OCIO_CHECK_NO_THROW(config->sanityCheck());

        std::stringstream ss;
        ss << *config.get();
        OCIO_CHECK_EQUAL(ss.str(), str);
    }

    {
        // LogAffine with single value for logSideSlope.
        const std::string strEnd =
            "    from_reference: !<LogAffineTransform> {"
            "logSideSlope: 1.1, "
            "logSideOffset: [0.5, 0, 0], "
            "linSideSlope: [1.3, 1, 1], "
            "linSideOffset: [0, 0, 0.1]}\n";
        const std::string str = PROFILE_V2 + SIMPLE_PROFILE + strEnd;

        std::istringstream is;
        is.str(str);

        OCIO::ConstConfigRcPtr config;
        OCIO_CHECK_NO_THROW(config = OCIO::Config::CreateFromStream(is));
        OCIO_CHECK_NO_THROW(config->sanityCheck());

        std::stringstream ss;
        ss << *config.get();
        OCIO_CHECK_EQUAL(ss.str(), str);
    }

    {
        // LogAffine with default value for logSideSlope.
        const std::string strEnd =
            "    from_reference: !<LogAffineTransform> {"
            "logSideOffset: [0.1234567890123, 0.5, 0.1], "
            "linSideSlope: [1.3, 1.4, 1.5], "
            "linSideOffset: [0.1, 0, 0]}\n";
        const std::string str = PROFILE_V2 + SIMPLE_PROFILE + strEnd;

        std::istringstream is;
        is.str(str);

        OCIO::ConstConfigRcPtr config;
        OCIO_CHECK_NO_THROW(config = OCIO::Config::CreateFromStream(is));
        OCIO_CHECK_NO_THROW(config->sanityCheck());

        std::stringstream ss;
        ss << *config.get();
        OCIO_CHECK_EQUAL(ss.str(), str);
    }

    {
        // LogAffine with default value for all but base.
        const std::string strEnd =
            "    from_reference: !<LogAffineTransform> {base: 10}\n";
        const std::string str = PROFILE_V2 + SIMPLE_PROFILE + strEnd;

        std::istringstream is;
        is.str(str);

        OCIO::ConstConfigRcPtr config;
        OCIO_CHECK_NO_THROW(config = OCIO::Config::CreateFromStream(is));
        OCIO_CHECK_NO_THROW(config->sanityCheck());

        std::stringstream ss;
        ss << *config.get();
        OCIO_CHECK_EQUAL(ss.str(), str);
    }

    {
        // LogAffine with wrong size for logSideSlope.
        const std::string strEnd =
            "    from_reference: !<LogAffineTransform> {"
            "logSideSlope: [1, 1], "
            "logSideOffset: [0.1234567890123, 0.5, 0.1]}\n";
        const std::string str = PROFILE_V2 + SIMPLE_PROFILE + strEnd;

        std::istringstream is;
        is.str(str);

        OCIO::ConstConfigRcPtr config;
        OCIO_CHECK_THROW_WHAT(config = OCIO::Config::CreateFromStream(is),
                              OCIO::Exception,
                              "logSideSlope value field must have 3 components");
    }

    {
        // LogAffine with 3 values for base.
        const std::string strEnd =
            "    from_reference: !<LogAffineTransform> {"
            "base: [2, 2, 2], "
            "logSideOffset: [0.1234567890123, 0.5, 0.1]}\n";
        const std::string str = PROFILE_V2 + SIMPLE_PROFILE + strEnd;

        std::istringstream is;
        is.str(str);

        OCIO::ConstConfigRcPtr config;
        OCIO_CHECK_THROW_WHAT(config = OCIO::Config::CreateFromStream(is),
            OCIO::Exception,
            "base must be a single double");
    }
}

OCIO_ADD_TEST(Config, key_value_error)
{
    // Check the line number contained in the parser error messages.

    const std::string SHORT_PROFILE =
        "ocio_profile_version: 2\n"
        "strictparsing: false\n"
        "roles:\n"
        "  default: raw\n"
        "displays:\n"
        "  sRGB:\n"
        "  - !<View> {name: Raw, colorspace: raw}\n"
        "\n"
        "colorspaces:\n"
        "  - !<ColorSpace>\n"
        "    name: raw\n"
        "    to_reference: !<MatrixTransform> \n"
        "                      {\n"
        "                           matrix: [1, 0, 0, 0, 0, 1]\n" // Missing values.
        "                      }\n"
        "    allocation: uniform\n"
        "\n";

    std::istringstream is;
    is.str(SHORT_PROFILE);

    OCIO_CHECK_THROW_WHAT(OCIO::Config::CreateFromStream(is),
                          OCIO::Exception,
                          "Error: Loading the OCIO profile failed. At line 14, the value "
                          "parsing of the key 'matrix' from 'MatrixTransform' failed: "
                          "'matrix' values must be 16 numbers. Found '6'.");
}

namespace
{

// Redirect the std::cerr to catch the warning.
class Guard
{
public:      
    Guard()      
        :   m_oldBuf(std::cerr.rdbuf())      
    {        
        std::cerr.rdbuf(m_ss.rdbuf());       
    }        

    ~Guard()         
    {        
        std::cerr.rdbuf(m_oldBuf);       
        m_oldBuf = nullptr;      
    }        

    std::string output() { return m_ss.str(); }      

private:         
    std::stringstream m_ss;      
    std::streambuf *  m_oldBuf;      

    Guard(const Guard&) = delete;        
    Guard operator=(const Guard&) = delete;      
};

};

OCIO_ADD_TEST(Config, unknown_key_error)
{
    const std::string SHORT_PROFILE =
        "ocio_profile_version: 2\n"
        "strictparsing: false\n"
        "roles:\n"
        "  default: raw\n"
        "displays:\n"
        "  sRGB:\n"
        "  - !<View> {name: Raw, colorspace: raw}\n"
        "\n"
        "colorspaces:\n"
        "  - !<ColorSpace>\n"
        "    name: raw\n"
        "    dummyKey: dummyValue\n"
        "    to_reference: !<MatrixTransform> {offset: [1, 0, 0, 0]}\n"
        "    allocation: uniform\n"
        "\n";

    std::istringstream is;
    is.str(SHORT_PROFILE);

    Guard g;
    OCIO_CHECK_NO_THROW(OCIO::Config::CreateFromStream(is));
    OCIO_CHECK_EQUAL(g.output(), 
                     "[OpenColorIO Warning]: At line 12, unknown key 'dummyKey' in 'ColorSpace'.\n");
}

OCIO_ADD_TEST(Config, fixed_function_serialization)
{
    static const std::string SIMPLE_PROFILE =
        "ocio_profile_version: 2\n"
        "\n"
        "search_path: luts\n"
        "strictparsing: true\n"
        "luma: [0.2126, 0.7152, 0.0722]\n"
        "\n"
        "roles:\n"
        "  default: raw\n"
        "  scene_linear: raw\n"
        "\n"
        "displays:\n"
        "  sRGB:\n"
        "    - !<View> {name: Raw, colorspace: raw}\n"
        "\n"
        "active_displays: []\n"
        "active_views: []\n"
        "\n"
        "colorspaces:\n"
        "  - !<ColorSpace>\n"
        "    name: raw\n"
        "    family: \"\"\n"
        "    equalitygroup: \"\"\n"
        "    bitdepth: unknown\n"
        "    isdata: false\n"
        "    allocation: uniform\n";

    {
        const std::string strEnd =
            "    from_reference: !<GroupTransform>\n"
            "      children:\n"
            "        - !<FixedFunctionTransform> {style: ACES_RedMod03}\n"
            "        - !<FixedFunctionTransform> {style: ACES_RedMod03, direction: inverse}\n"
            "        - !<FixedFunctionTransform> {style: ACES_RedMod10}\n"
            "        - !<FixedFunctionTransform> {style: ACES_RedMod10, direction: inverse}\n"
            "        - !<FixedFunctionTransform> {style: ACES_Glow03}\n"
            "        - !<FixedFunctionTransform> {style: ACES_Glow03, direction: inverse}\n"
            "        - !<FixedFunctionTransform> {style: ACES_Glow10}\n"
            "        - !<FixedFunctionTransform> {style: ACES_Glow10, direction: inverse}\n"
            "        - !<FixedFunctionTransform> {style: ACES_DarkToDim10}\n"
            "        - !<FixedFunctionTransform> {style: ACES_DarkToDim10, direction: inverse}\n"
            "        - !<FixedFunctionTransform> {style: REC2100_Surround, params: [0.75]}\n"
            "        - !<FixedFunctionTransform> {style: REC2100_Surround, params: [0.75], direction: inverse}\n";

        const std::string str = SIMPLE_PROFILE + strEnd;

        std::istringstream is;
        is.str(str);

        OCIO::ConstConfigRcPtr config;
        OCIO_CHECK_NO_THROW(config = OCIO::Config::CreateFromStream(is));
        OCIO_CHECK_NO_THROW(config->sanityCheck());

        std::stringstream ss;
        ss << *config.get();
        OCIO_CHECK_EQUAL(ss.str(), str);
    }

    {
        const std::string strEnd =
            "    from_reference: !<GroupTransform>\n"
            "      children:\n"
            "        - !<FixedFunctionTransform> {style: ACES_DarkToDim10, params: [0.75]}\n";

        const std::string str = SIMPLE_PROFILE + strEnd;

        std::istringstream is;
        is.str(str);

        OCIO::ConstConfigRcPtr config;
        OCIO_CHECK_NO_THROW(config = OCIO::Config::CreateFromStream(is));
        OCIO_CHECK_THROW_WHAT(config->sanityCheck(), OCIO::Exception,
            "The style 'ACES_DarkToDim10 (Forward)' must have zero parameters but 1 found.");
    }

    {
        const std::string strEnd =
            "    from_reference: !<GroupTransform>\n"
            "      children:\n"
            "        - !<FixedFunctionTransform> {style: REC2100_Surround, direction: inverse}\n";

        const std::string str = SIMPLE_PROFILE + strEnd;

        std::istringstream is;
        is.str(str);

        OCIO::ConstConfigRcPtr config;
        OCIO_CHECK_NO_THROW(config = OCIO::Config::CreateFromStream(is));
        OCIO_CHECK_THROW_WHAT(config->sanityCheck(), OCIO::Exception, 
                              "The style 'REC2100_Surround' must "
                              "have one parameter but 0 found.");
    }
}

OCIO_ADD_TEST(Config, exposure_contrast_serialization)
{
    static const std::string SIMPLE_PROFILE =
        "ocio_profile_version: 2\n"
        "\n"
        "search_path: luts\n"
        "strictparsing: true\n"
        "luma: [0.2126, 0.7152, 0.0722]\n"
        "\n"
        "roles:\n"
        "  default: raw\n"
        "  scene_linear: raw\n"
        "\n"
        "displays:\n"
        "  sRGB:\n"
        "    - !<View> {name: Raw, colorspace: raw}\n"
        "\n"
        "active_displays: []\n"
        "active_views: []\n"
        "\n"
        "colorspaces:\n"
        "  - !<ColorSpace>\n"
        "    name: raw\n"
        "    family: \"\"\n"
        "    equalitygroup: \"\"\n"
        "    bitdepth: unknown\n"
        "    isdata: false\n"
        "    allocation: uniform\n";

    {
        const std::string strEnd =
            "    from_reference: !<GroupTransform>\n"
            "      children:\n"
            "        - !<ExposureContrastTransform> {style: video, exposure: 1.5,"
                       " contrast: 0.5, gamma: 1.1, pivot: 0.18}\n"
            "        - !<ExposureContrastTransform> {style: video,"
                       " exposure: {value: 1.5, dynamic: true}, contrast: 0.5,"
                       " gamma: 1.1, pivot: 0.18}\n"
            "        - !<ExposureContrastTransform> {style: video, exposure: -1.4,"
                       " contrast: 0.6, gamma: 1.2, pivot: 0.2,"
                       " direction: inverse}\n"
            "        - !<ExposureContrastTransform> {style: log, exposure: 1.5,"
                       " contrast: 0.6, gamma: 1.2, pivot: 0.18}\n"
            "        - !<ExposureContrastTransform> {style: log, exposure: 1.5,"
                       " contrast: 0.5, gamma: 1.1, pivot: 0.18,"
                       " direction: inverse}\n"
            "        - !<ExposureContrastTransform> {style: log, exposure: 1.5,"
                       " contrast: {value: 0.6, dynamic: true}, gamma: 1.2,"
                       " pivot: 0.18}\n"
            "        - !<ExposureContrastTransform> {style: linear, exposure: 1.5,"
                       " contrast: 0.5, gamma: 1.1, pivot: 0.18}\n"
            "        - !<ExposureContrastTransform> {style: linear, exposure: 1.5,"
                       " contrast: 0.5, gamma: 1.1, pivot: 0.18,"
                       " direction: inverse}\n"
            "        - !<ExposureContrastTransform> {style: linear, exposure: 1.5,"
                       " contrast: 0.5, gamma: {value: 1.1, dynamic: true},"
                       " pivot: 0.18}\n";

        const std::string str = SIMPLE_PROFILE + strEnd;

        std::istringstream is;
        is.str(str);

        OCIO::ConstConfigRcPtr config;
        OCIO_CHECK_NO_THROW(config = OCIO::Config::CreateFromStream(is));
        OCIO_CHECK_NO_THROW(config->sanityCheck());

        std::stringstream ss;
        ss << *config.get();
        OCIO_CHECK_EQUAL(ss.str(), str);
    }

    {
        const std::string strEnd =
            "    from_reference: !<GroupTransform>\n"
            "      children:\n";

        const std::string strEndEC =
            "        - !<ExposureContrastTransform> {style: video,"
                       " exposure: {value: 1.5},"
                       " contrast: {value: 0.5, dynamic: false},"
                       " gamma: {value: 1.1}, pivot: 0.18}\n";

        const std::string strEndECExpected =
            "        - !<ExposureContrastTransform> {style: video, exposure: 1.5,"
                       " contrast: 0.5, gamma: 1.1, pivot: 0.18}\n";

        const std::string str = SIMPLE_PROFILE + strEnd + strEndEC;

        std::istringstream is;
        is.str(str);

        OCIO::ConstConfigRcPtr config;
        OCIO_CHECK_NO_THROW(config = OCIO::Config::CreateFromStream(is));
        OCIO_CHECK_NO_THROW(config->sanityCheck());

        const std::string strExpected = SIMPLE_PROFILE + strEnd + strEndECExpected;

        std::stringstream ss;
        ss << *config.get();
        OCIO_CHECK_EQUAL(ss.str(), strExpected);

    }

    {
        const std::string strEnd =
            "    from_reference: !<GroupTransform>\n"
            "      children:\n"
            "        - !<ExposureContrastTransform> {style: wrong}\n";

        const std::string str = SIMPLE_PROFILE + strEnd;

        std::istringstream is;
        is.str(str);

        OCIO_CHECK_THROW_WHAT(OCIO::Config::CreateFromStream(is),
                              OCIO::Exception,
                              "Unknown exposure contrast style");
    }
}

OCIO_ADD_TEST(Config, matrix_serialization)
{
    const std::string strEnd =
        "    from_reference: !<GroupTransform>\n"
        "      children:\n"
                 // Check the value serialization.
        "        - !<MatrixTransform> {matrix: [0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15],"\
                                     " offset: [-1, -2, -3, -4]}\n"
                 // Check the value precision.
        "        - !<MatrixTransform> {offset: [0.123456789876, 1.23456789876, 12.3456789876, 123.456789876]}\n"
        "        - !<MatrixTransform> {matrix: [0.123456789876, 1.23456789876, 12.3456789876, 123.456789876, "\
                                                "1234.56789876, 12345.6789876, 123456.789876, 1234567.89876, "\
                                                "0, 0, 1, 0, 0, 0, 0, 1]}\n";

    const std::string str = PROFILE_V1 + SIMPLE_PROFILE + strEnd;

    std::istringstream is;
    is.str(str);

    OCIO::ConstConfigRcPtr config;
    OCIO_CHECK_NO_THROW(config = OCIO::Config::CreateFromStream(is));
    OCIO_CHECK_NO_THROW(config->sanityCheck());

    std::stringstream ss;
    ss << *config.get();
    OCIO_CHECK_EQUAL(ss.str(), str);
}

OCIO_ADD_TEST(Config, add_color_space)
{
    // The unit test validates that the color space is correctly added to the configuration.

    // Note that the new C++11 u8 notation for UTF-8 string literals is used
    // to partially validate non-english language support.

    const std::string str
        = PROFILE_V2 + SIMPLE_PROFILE
            + u8"    from_reference: !<MatrixTransform> {offset: [-1, -2, -3, -4]}\n";

    std::istringstream is;
    is.str(str);

    OCIO::ConfigRcPtr config;
    OCIO_CHECK_NO_THROW(config = OCIO::Config::CreateFromStream(is)->createEditableCopy());
    OCIO_CHECK_NO_THROW(config->sanityCheck());
    OCIO_CHECK_EQUAL(config->getNumColorSpaces(), 2);

    OCIO::ColorSpaceRcPtr cs;
    OCIO_CHECK_NO_THROW(cs = OCIO::ColorSpace::Create());
    cs->setName(u8"astéroïde");                           // Color space name with accents.
    cs->setDescription(u8"é À Â Ç É È ç -- $ € 円 £ 元"); // Some accents and some money symbols.

    OCIO::FixedFunctionTransformRcPtr tr;
    OCIO_CHECK_NO_THROW(tr = OCIO::FixedFunctionTransform::Create());

    OCIO_CHECK_NO_THROW(cs->setTransform(tr, OCIO::COLORSPACE_DIR_TO_REFERENCE));

    constexpr char csName[] = u8"astéroïde";

    OCIO_CHECK_EQUAL(config->getIndexForColorSpace(csName), -1);
    OCIO_CHECK_NO_THROW(config->addColorSpace(cs));
    OCIO_CHECK_EQUAL(config->getIndexForColorSpace(csName), 2);

    const std::string res 
        = str
        + u8"\n"
        + u8"  - !<ColorSpace>\n"
        + u8"    name: " + csName + u8"\n"
        + u8"    family: \"\"\n"
        + u8"    equalitygroup: \"\"\n"
        + u8"    bitdepth: unknown\n"
        + u8"    description: |\n"
        + u8"      é À Â Ç É È ç -- $ € 円 £ 元\n"
        + u8"    isdata: false\n"
        + u8"    allocation: uniform\n"
        + u8"    to_reference: !<FixedFunctionTransform> {style: ACES_RedMod03}\n";

    std::stringstream ss;
    ss << *config.get();
    OCIO_CHECK_EQUAL(ss.str(), res);

    OCIO_CHECK_NO_THROW(config->removeColorSpace(csName));
    OCIO_CHECK_EQUAL(config->getNumColorSpaces(), 2);
    OCIO_CHECK_EQUAL(config->getIndexForColorSpace(csName), -1);

    OCIO_CHECK_NO_THROW(config->clearColorSpaces());
    OCIO_CHECK_EQUAL(config->getNumColorSpaces(), 0);
}

OCIO_ADD_TEST(Config, faulty_config_file)
{
    std::istringstream is("/usr/tmp/not_existing.ocio");

    OCIO::ConstConfigRcPtr config;
    OCIO_CHECK_THROW_WHAT(config = OCIO::Config::CreateFromStream(is),
                          OCIO::Exception,
                          "Error: Loading the OCIO profile failed.");
}

