#include "filesystem.h"

#include <adasworks/sx/check.h>

#include "cmakex_utils.h"
#include "getpreset.h"
#include "misc_utils.h"
#include "print.h"
#include "process_command_line.h"

namespace cmakex {

namespace fs = filesystem;

const char* cmakex_version_string = STRINGIZE(CMAKEX_VERSION_STRING);

const char* usage_text =
    "Execute multiple `cmake` commands with a single `cmakex` command.\n"
    "For detailed help, see README.md\n"
    "\n"
    "Usage: cmakex <subcommand> [options...]\n"
    "\n"
    "The first (compulsory) parameter is the subcommand word which can be:\n"
    "\n"
    "- mix of the characters c, b, i, t, d, r, w to execute multiple `cmake` commands\n"
    "- 'help' or '--help' to display this message\n"
    "- 'version' or '--version' to display the version\n"
    "\n"
    "Execute Multiple `cmake` Commands\n"
    "=================================\n"
    "\n"
    "    cmakex [c][b][i][t][d][r][w] [cmake-options] [--deps]\n"
    "\n"
    "Specify one or more of the characters c, b, i, t to execute one or more of\n"
    "these steps:\n"
    "\n"
    "- `c`: CMake configure step (`cmake ...`)\n"
    "- `b`: CMake build step (`cmake --build ...`)\n"
    "- `i`: CMake install step (`cmake --build ... --target install`)\n"
    "- `t`: CMake test step (`ctest ...`)\n"
    "\n"
    "The remaining characters control the configurations. You can specify zero, one,\n"
    "or more of d, r, w for the configurations: Debug, Release, RelWithDebInfo.\n"
    "\n"
    "CMake Options\n"
    "-------------\n"
    "\n"
    "After the command word you can specify:\n"
    "\n"
    "- `-H` and `-B` to specify source and build directories. Note that unlike cmake,\n"
    "  cmakex accepts the `-H <path>` and `-B <path>` forms, too.\n"
    "- <source-dir> or <existing-binary-dir>\n"
    "- most of the cmake configuring options (see below)\n"
    "- `--target <tgt>` (also multiple times)\n"
    "- `--config <cfg>` (also multiple times)\n"
    "- `--clean-first`\n"
    "- double-dash \"--\" followed by options to the native build tool\n"
    "\n"
    "Allowed cmake options: \n"
    "\n"
    "  -C, -D, -U, -G, -T, -A, -N, all the -W* options\n"
    "  --debug-trycompile, --debug-output, --trace, --trace-expand\n"
    "  --warn-uninitialized, --warn-unused-vars, --no-warn-unused-cli,\n"
    "  --check-system-vars, --graphwiz=\n"
    "\n"
    "\n"
    "Additional Options\n"
    "------------------\n"
    "\n"
    "- `--deps`  download or install dependencies first\n"
    "            If the current source directory has a `deps.cmake` file it will\n"
    "            be processed first.\n"
    "\n"
    "Examples:\n"
    "=========\n"
    "\n"
    "Configure, install and test a project from scrach, for `Debug` and `Release`\n"
    "configurations, clean build:\n"
    "\n"
    "    cd project_source_dir\n"
    "    cmakex citdr -H. -Bb -DCMAKE_INSTALL_PREFIX=$PWD/out\n"
    "\n"
    "Install the 'Debug' and 'Release' configs:\n"
    "\n"
    "    cmakex cidr -H source_dir -B build_dir -DMY_OPTION=something\n"
    "\n"
    "To test a project which has not been configured yet:\n"
    "\n"
    "    cmakex cbtr -H source_dir -B build\n"
    "\n"
    "Test the 'Release' config (no configure and build):\n"
    "\n"
    "    cmakex tr -H source_dir -B build_dir\n"
    "\n";

void display_usage_and_exit(int exit_code)
{
    fprintf(stderr, "cmakex v%s\n\n", cmakex_version_string);
    fprintf(stderr, "%s", usage_text);
    exit(exit_code);
}

void display_version_and_exit(int exit_code)
{
    fprintf(stderr, "cmakex v%s\n\n", cmakex_version_string);
    exit(exit_code);
}

bool is_valid_binary_dir(string_par x)
{
    return fs::is_regular_file(x.str() + "/" + k_cmakex_cache_filename) ||
           fs::is_regular_file(x.str() + "/CMakeCache.txt");
}

bool is_valid_source_dir(string_par x)
{
    return fs::is_regular_file(x.str() + "/CMakeLists.txt");
}

fs::path strip_trailing_dot(const fs::path& x)
{
    string s = x.string();
    if (s.size() >= 2) {
        auto t = s.substr(s.size() - 2);
        if (t == "/." || t == "\\.")
            return s.substr(0, s.size() - 2);
    }
    return x;
}

command_line_args_cmake_mode_t process_command_line_1(int argc, char* argv[])
{
    command_line_args_cmake_mode_t pars;
    if (argc <= 1)
        display_usage_and_exit(EXIT_SUCCESS);

    for (int argix = 1; argix < argc; ++argix) {
        string arg = argv[argix];
        if ((argix == 1 && arg == "help") || arg == "--help")
            display_usage_and_exit(EXIT_SUCCESS);
        if ((argix == 1 && arg == "version") || arg == "--version")
            display_version_and_exit(EXIT_SUCCESS);
    }

    string command = argv[1];

    for (auto c : command) {
        switch (c) {
            case 'c':
                pars.flag_c = true;
                break;
            case 'b':
                pars.flag_b = true;
                break;
            case 'i':
                pars.build_targets.emplace_back("install");
                break;
            case 't':
                pars.flag_t = true;
                break;
            case 'd':
                pars.configs.emplace_back("Debug");
                break;
            case 'r':
                pars.configs.emplace_back("Release");
                break;
            case 'w':
                pars.configs.emplace_back("RelWithDebInfo");
                break;
            default:
                badpars_exit(stringf("Invalid character in subcommand: %c", c));
        }
    }
    for (int argix = 2; argix < argc; ++argix) {
        string arg = argv[argix];

        if (arg == "--") {
            pars.native_tool_args.assign(argv + argix + 1, argv + argc);
            break;
        }
        if (arg == "--target") {
            if (++argix >= argc)
                badpars_exit("Missing target name after '--target'");
            pars.build_targets.emplace_back(argv[argix]);
        } else if (arg == "--config") {
            if (++argix >= argc)
                badpars_exit("Missing config name after '--config'");
            string c = trim(argv[argix]);
            if (c.empty())
                badpars_exit("Invalid empty argument for '--config'");
            pars.configs.emplace_back(c);
        } else if (is_one_of(arg, {"--clean-first", "--use-stderr"}))
            pars.build_args.emplace_back(arg);
        else if (arg == "--deps" || starts_with(arg, "--deps=")) {
            if (pars.deps_mode != dm_main_only)
                badpars_exit("'--deps' or '--deps-only' specified multiple times");
            pars.deps_mode = dm_deps_and_main;
            if (starts_with(arg, "--deps=")) {
                pars.deps_script = make_string(butleft(arg, strlen("--deps=")));
                if (pars.deps_script.empty())
                    badpars_exit("Missing path after '--deps='");
            }
        } else if (arg == "--deps-only" || starts_with(arg, "--deps-only=")) {
            if (pars.deps_mode != dm_main_only)
                badpars_exit("'--deps' or '--deps-only' specified multiple times");
            pars.deps_mode = dm_deps_only;
            if (starts_with(arg, "--deps-only=")) {
                pars.deps_script = make_string(butleft(arg, strlen("--deps-only=")));
                if (pars.deps_script.empty())
                    badpars_exit("Missing path after '--deps-only='");
            }
        } else if (starts_with(arg, "-H")) {
            if (arg == "-H") {
                // unlike cmake, here we support the '-H <path>' style, too
                if (++argix >= argc)
                    badpars_exit("Missing path after '-H'");
                pars.arg_H = argv[argix];
            } else
                pars.arg_H = make_string(butleft(arg, 2));
        } else if (starts_with(arg, "-B")) {
            if (arg == "-B") {
                // unlike cmake, here we support the '-B <path>' style, too
                if (++argix >= argc)
                    badpars_exit("Missing path after '-B'");
                pars.arg_B = argv[argix];
            } else
                pars.arg_B = make_string(butleft(arg, 2));
        } else if (arg == "-p") {
            if (++argix >= argc)
                badpars_exit("Missing argument after '-p'");
            if (!pars.arg_p.empty())
                badpars_exit("Multiple '-p' options.");
            pars.arg_p = argv[argix];
        } else if (!starts_with(arg, '-')) {
            pars.free_args.emplace_back(arg);
        } else {
            if (arg.size() == 2 && is_one_of(arg, {"-C", "-D", "-U", "-G", "-T", "-A"})) {
                if (++argix >= argc)
                    badpars_exit(stringf("Missing argument after '%s'", arg.c_str()));
                arg += argv[argix];
            }

            try {
                auto pca = parse_cmake_arg(arg);
            } catch (...) {
                badpars_exit(stringf("Invalid option: '%s'", arg.c_str()));
            }

            pars.cmake_args.emplace_back(arg);
        }  // last else
    }      // foreach arg
    return pars;
}

// todo multiple presets

tuple<processed_command_line_args_cmake_mode_t, cmakex_cache_t> process_command_line_2(
    const command_line_args_cmake_mode_t& cla)

{
    processed_command_line_args_cmake_mode_t pcla;
    *static_cast<base_command_line_args_cmake_mode_t*>(&pcla) = cla;  // slice to common base

    // complete relative paths for CMAKE_INSTALL_PREFIX, CMAKE_PREFIX_PATH
    for (auto& a : pcla.cmake_args) {
        auto b = parse_cmake_arg(a);
        if ((b.name == "CMAKE_PREFIX_PATH" || b.name == "CMAKE_INSTALL_PREFIX") &&
            b.switch_ == "-D") {
            auto v = cmakex_prefix_path_to_vector(b.value);
            for (auto& p : v)
                p = fs::absolute(p);
            b.value = join(v, string{system_path_separator()});
            a = format_cmake_arg(b);
        }
    }

    // resolve preset
    if (!cla.arg_p.empty()) {
        string msg;
        string file, alias;
        vector<string> preset_args, name;
        try {
            tie(name, file, alias) = libgetpreset::getpreset(cla.arg_p, "name");
            tie(preset_args, file, alias) = libgetpreset::getpreset(cla.arg_p, "args");
        } catch (const exception& e) {
            msg = e.what();
        } catch (...) {
            msg = "Unknown error";
        }
        if (!msg.empty())
            throwf("Invalid '-p' argument, reason: %s", msg.c_str());
        CHECK(name.size() == 1);
        log_info("Using preset '%s'%s from \"%s\"", name.front().c_str(),
                 alias == name[0] ? "" : stringf(" (alias: %s)", alias.c_str()).c_str(),
                 file.c_str());
        prepend_inplace(pcla.cmake_args, preset_args);
    }

    if (cla.free_args.empty()) {
        if (cla.arg_H.empty()) {
            if (cla.arg_B.empty()) {
                badpars_exit(
                    "No source or binary directories specified. CMake would set both to the "
                    "current directory but cmakex requires the source and binary directories to be "
                    "different.");
            } else
                pcla.binary_dir = cla.arg_B;
        } else {
            if (cla.arg_B.empty())
                pcla.source_dir = cla.arg_H;
            else {
                pcla.source_dir = cla.arg_H;
                pcla.binary_dir = cla.arg_B;
            }
        }
    } else if (cla.free_args.size() == 1) {
        string d = cla.free_args[0];
        bool b = is_valid_binary_dir(d);
        bool s = is_valid_source_dir(d);
        if (b) {
            if (s) {
                badpars_exit(
                    stringf("The directory \"%s\" is both a valid source and a valid binary "
                            "directory. This is not permitted with cmakex",
                            d.c_str()));
            } else {
                pcla.binary_dir = d;
                if (!cla.arg_B.empty() && !fs::equivalent(d, cla.arg_B))
                    badpars_exit(
                        stringf("The binary directory specified by the free argument \"%s\" is "
                                "different from '-B' option: \"%s\"",
                                d.c_str(), cla.arg_B.c_str()));
                if (!cla.arg_H.empty())
                    pcla.source_dir = cla.arg_H;
            }
        } else {
            if (s) {
                pcla.source_dir = d;
                if (!cla.arg_H.empty() && !fs::equivalent(d, cla.arg_H))
                    badpars_exit(
                        stringf("The source directory specified by the free argument \"%s\" is "
                                "different from '-H' option: \"%s\"",
                                d.c_str(), cla.arg_H.c_str()));
                if (!cla.arg_B.empty())
                    pcla.binary_dir = cla.arg_H;
            } else {
                badpars_exit(
                    stringf("The directory \"%s\" is neither a valid source (no CMakeLists.txt), "
                            "nor a valid binary directory.",
                            d.c_str()));
            }
        }
    } else if (cla.free_args.size() > 1) {
        badpars_exit(
            stringf("More than one free arguments: %s", join(cla.free_args, ", ").c_str()));
    }
    if (pcla.binary_dir.empty())
        pcla.binary_dir = fs::current_path().string();

    // at this point we have a new or an existing binary dir
    CHECK(!pcla.binary_dir.empty());

    if (!pcla.source_dir.empty()) {
        if (fs::equivalent(pcla.source_dir, pcla.binary_dir))
            badpars_exit(
                stringf("The source and binary dirs are identical: \"%s\", this is not permitted "
                        "with cmakex",
                        fs::canonical(pcla.source_dir.c_str()).c_str()));
    }

    // extract source dir from existing binary dir
    cmakex_config_t cfg(pcla.binary_dir);

    cmake_cache_t cmake_cache;
    bool binary_dir_has_cmake_cache =
        false;  // silencing warning, otherwise it always will be set before read
    bool cmake_cache_checked = false;
    auto check_cmake_cache = [&pcla, &binary_dir_has_cmake_cache, &cmake_cache_checked,
                              &cmake_cache]() {
        if (cmake_cache_checked)
            return;

        const string cmake_cache_path = pcla.binary_dir + "/CMakeCache.txt";

        binary_dir_has_cmake_cache = fs::is_regular_file(cmake_cache_path);
        cmake_cache_checked = true;

        if (!binary_dir_has_cmake_cache)
            return;

        cmake_cache = read_cmake_cache(cmake_cache_path);
    };

    cmakex_cache_t cmakex_cache = cfg.cmakex_cache();

    if (cmakex_cache.valid) {
        string source_dir = cmakex_cache.home_directory;

        if (!pcla.source_dir.empty() && !fs::equivalent(source_dir, pcla.source_dir)) {
            badpars_exit(
                stringf("The source dir specified \"%s\" is different from the one stored in the "
                        "%s: \"%s\"",
                        pcla.source_dir.c_str(), k_cmakex_cache_filename, source_dir.c_str()));
        }
        if (pcla.source_dir.empty())
            pcla.source_dir = source_dir;
    } else {
        check_cmake_cache();
        if (binary_dir_has_cmake_cache) {
            string source_dir = cmake_cache.vars["CMAKE_HOME_DIRECTORY"];
            if (!pcla.source_dir.empty() && !fs::equivalent(source_dir, pcla.source_dir)) {
                badpars_exit(stringf(
                    "The source dir specified \"%s\" is different from the one stored in the "
                    "CMakeCache.txt: \"%s\"",
                    pcla.source_dir.c_str(), source_dir.c_str()));
            }
            if (pcla.source_dir.empty())
                pcla.source_dir = source_dir;
        }
    }

    string home_directory;

    if (pcla.deps_mode != dm_deps_only) {
        if (pcla.source_dir.empty())
            badpars_exit(
                stringf("No source dir has been specified and the binary dir \"%s\" is not valid "
                        "(contains no CMakeCache.txt or %s)",
                        pcla.binary_dir.c_str(), k_cmakex_cache_filename));

        CHECK(!pcla.source_dir.empty());

        if (!is_valid_source_dir(pcla.source_dir))
            badpars_exit(stringf("The source dir \"%s\" is not valid (no CMakeLists.txt)",
                                 pcla.source_dir.c_str()));

        home_directory = fs::canonical(pcla.source_dir).string();
    }

    if (cmakex_cache.valid) {
        if (cmakex_cache.home_directory.empty())
            cmakex_cache.home_directory = home_directory;
        else
            CHECK(fs::equivalent(cmakex_cache.home_directory, home_directory));
    } else {
        cmakex_cache.home_directory = home_directory;
        string cmake_generator;
        check_cmake_cache();
        if (binary_dir_has_cmake_cache) {
            cmake_generator = cmake_cache.vars["CMAKE_GENERATOR"];
            THROW_IF(
                cmake_generator.empty(),
                "The file %s/CMakeCache.txt contains invalid (empty) CMAKE_GENERATOR variable.",
                pcla.binary_dir.c_str());
        } else {
            // new binary dir
            cmake_generator = extract_generator_from_cmake_args(pcla.cmake_args);
        }
        cmakex_cache.multiconfig_generator = is_generator_multiconfig(cmake_generator);
        if (binary_dir_has_cmake_cache)
            cmakex_cache.per_config_bin_dirs = false;  // building upon an existing non-cmakex build
        else
            cmakex_cache.per_config_bin_dirs =
                cmakex_cache.multiconfig_generator && cfg.per_config_bin_dirs();
        cmakex_cache.valid = true;
    }

    string cmake_build_type;
    bool defines_cmake_build_type = false;
    bool undefines_cmake_build_type = false;
    for (auto it = pcla.cmake_args.begin(); it != pcla.cmake_args.end(); /* no incr */) {
        auto pca = parse_cmake_arg(*it);
        if (pca.name == "CMAKE_BUILD_TYPE") {
            if (pca.switch_ == "-D") {
                cmake_build_type = trim(pca.value);
                undefines_cmake_build_type = false;
                defines_cmake_build_type = true;
            } else if (pca.switch_ == "-U") {
                cmake_build_type.clear();
                undefines_cmake_build_type = true;
                defines_cmake_build_type = false;
            }
            it = pcla.cmake_args.erase(it);
        } else
            ++it;
    }

    if (pcla.configs.empty()) {
        // for multiconfig generator not specifying a config defaults to Debug
        // for singleconfig, it's a special NoConfig configuration
        pcla.configs.assign(1, cmakex_cache.multiconfig_generator ? "Debug" : "");
    } else {
        for (auto& c : pcla.configs) {
            CHECK(!c.empty());  // this is an internal error
        }
    }

    // CMAKE_BUILD_TYPE rules
    // - if no config has been specified, this is only possible with single-config generators,
    // because multiconfig defaults to Debug. In this case, configs = {""} but it will be
    // overwritten with the config the CMAKE_BUILD_TYPE specifies
    // - otherwise it must be the same as the config specified otherwise
    // in all cases CMAKE_BUILD_TYPE itself will be removed from the command line

    CHECK(!pcla.configs.empty());  // previous lines must ensure this

    if (pcla.configs.size() == 1 && pcla.configs.front().empty()) {
        if (defines_cmake_build_type && !cmake_build_type.empty())
            pcla.configs.assign(1, cmake_build_type);
    } else {  // non-empty configs specified
        for (auto& c : pcla.configs) {
            CHECK(!c.empty());
            if (undefines_cmake_build_type)
                throwf(
                    "Incompatible configuration settings: CMAKE_BUILD_TYPE is removed with "
                    "'-U' but a configuration setting requires it to be defined to '%s'",
                    c.c_str());
            else if (defines_cmake_build_type && cmake_build_type != c)
                throwf(
                    "Incompatible configuration settings: CMAKE_BUILD_TYPE is defined to '%s' "
                    "but a configuration setting requires it to be defined to '%s'",
                    cmake_build_type.c_str(), c.c_str());
        }
    }

    if (!pcla.source_dir.empty()) {
        if (fs::path(pcla.source_dir).is_absolute())
            log_info("Using source dir: \"%s\"", pcla.source_dir.c_str());
        else {
            log_info(
                "Using source dir: \"%s\" -> \"%s\"", pcla.source_dir.c_str(),
                strip_trailing_dot(fs::lexically_normal(fs::absolute(pcla.source_dir))).c_str());
        }
    }

    const char* using_or_creating =
        is_valid_binary_dir(pcla.binary_dir) ? "Using existing" : "Creating";

    if (fs::path(pcla.binary_dir).is_absolute())
        log_info("%s binary dir: \"%s\"", using_or_creating, pcla.binary_dir.c_str());
    else
        log_info("%s binary dir: \"%s\" -> \"%s\"", using_or_creating, pcla.binary_dir.c_str(),
                 strip_trailing_dot(fs::lexically_normal(fs::absolute(pcla.binary_dir))).c_str());

    pcla.cmake_args = normalize_cmake_args(pcla.cmake_args);

    return make_tuple(move(pcla), move(cmakex_cache));
}
}
