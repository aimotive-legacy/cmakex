#include "filesystem.h"

#include <adasworks/sx/check.h>

#include "cmakex_utils.h"
#include "misc_utils.h"
#include "process_command_line.h"

namespace cmakex {

namespace fs = filesystem;

const char* cmakex_version_string = STRINGIZE(CMAKEX_VERSION_STRING);

const char* usage_text =
    "Execute multiple `cmake` commands with a single `cmakex` command.\n"
    "For detailed help, see README.md\n"
    "\n"
    "Usage: cmakex <subcommand>] [options...]\n"
    "\n"
    "The first (compulsory) parameter is the subcommand word which can be:\n"
    "\n"
    "- mix of the characters c, b, i, t, d, r, w to execute multiple `cmake` commands\n"
    "- 'help' to display this message\n"
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
    "            If -H specifies a dependency, its dependencies will be rebuilt\n"
    "            first.\n"
    "\n"
    "Examples:\n"
    "=========\n"
    "\n"
    "Configure, install and test a project from scrach, for `Debug` and `Release`\n"
    "configurations, clean build:\n"
    "\n"
    "    cd project_source_dir\n"
    "    cmakex citdr -H. -Bb -DCMAKE_INSTALL_PREFIX=$PWD/out --clean-first\n"
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

bool evaluate_binary_dir(string_par x)
{
    return fs::is_regular_file(x.str() + "/CMakeCache.txt");
}

cmakex_pars_t process_command_line(int argc, char* argv[])
{
    cmakex_pars_t pars;
    if (argc <= 1)
        display_usage_and_exit(EXIT_SUCCESS);
    string command = argv[1];
    if (command == "help")
        display_usage_and_exit(argc == 2 ? EXIT_SUCCESS : EXIT_FAILURE);

    pars.subcommand = cmakex_pars_t::subcommand_cmake_steps;
    for (auto c : command) {
        switch (c) {
            case 'c':
                pars.c = true;
                break;
            case 'b':
                pars.b = true;
                break;
            case 'i':
                pars.build_targets.emplace_back("install");
                break;
            case 't':
                pars.t = true;
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

    auto set_source_dir = [&pars](string_par x) -> void {
        evaluate_source_dir(x);
        if (!pars.source_dir.empty())
            badpars_exit(stringf("Multiple source paths specified: \"%s\", then \"%s\"",
                                 pars.source_dir.c_str(), x.c_str()));
        pars.source_dir = x.c_str();
    };

    auto set_binary_dir = [&pars](string_par x) -> void {
        if (!pars.binary_dir.empty())
            badpars_exit(
                stringf("Multiple binary (build) directories specified: \"%s\", then \"%s\"",
                        pars.binary_dir.c_str(), x.c_str()));
        pars.binary_dir_valid = evaluate_binary_dir(x);
        pars.binary_dir = x.c_str();
    };

    for (int argix = 2; argix < argc; ++argix) {
        string arg = argv[argix];
        if (!pars.native_tool_args.empty() || arg == "--") {
            pars.native_tool_args.emplace_back(arg);
            continue;
        }

        if (arg == "-P") {
            if (++argix >= argc)
                badpars_exit("Missing argument after -P");
            pars.add_pkgs.emplace_back(argv[argix]);
        } else if (starts_with(arg, "-P")) {
            pars.add_pkgs.emplace_back(make_string(butleft(arg, strlen("-P"))));
        } else if (arg == "--target") {
            if (++argix >= argc)
                badpars_exit("Missing target name after '--target'");
            pars.build_targets.emplace_back(argv[argix]);
        } else if (arg == "--config") {
            if (++argix >= argc)
                badpars_exit("Missing config name after '--config'");
            pars.configs.emplace_back(argv[argix]);
        } else if (is_one_of(arg, {"--clean-first", "--use-stderr"}))
            pars.build_args.emplace_back(arg);
        else if (arg == "--deps") {
            pars.deps = true;
        } else {
            bool found = false;
            for (const char* c : {"-C", "-D", "-U", "-G", "-T", "-A"}) {
                if (arg == c) {
                    if (++argix >= argc)
                        badpars_exit(stringf("Missing argument after '%s'", c));
                    pars.config_args.emplace_back(string(c) + argv[argix]);
                    found = true;
                    pars.config_args_besides_binary_dir = true;
                    break;
                }
                if (starts_with(arg, c)) {
                    pars.config_args.emplace_back(arg);
                    found = true;
                    break;
                }
            }
            if (found)
                continue;

            for (auto c : {"-Wno-dev", "-Wdev", "-Werror=dev", "Wno-error=dev", "-Wdeprecated",
                           "-Wno-deprecated", "-Werror=deprecated", "-Wno-error=deprecated", "-N",
                           "--debug-trycompile", "--debug-output", "--trace", "--trace-expand",
                           "--warn-uninitialized", "--warn-unused-vars", "--no-warn-unused-cli",
                           "--check-system-vars"}) {
                if (arg == c) {
                    pars.config_args.emplace_back(arg);
                    found = true;
                    pars.config_args_besides_binary_dir = true;
                    break;
                }
            }

            if (found)
                continue;

            if (starts_with(arg, "--graphwiz=")) {
                pars.config_args.emplace_back(arg);
                pars.config_args_besides_binary_dir = true;
                continue;
            }

            if (starts_with(arg, "-H")) {
                if (arg == "-H") {
                    // unlike cmake, here we support the '-H <path>' style, too
                    if (++argix >= argc)
                        badpars_exit("Missing path after '-H'");
                    set_source_dir(argv[argix]);
                } else
                    set_source_dir(make_string(butleft(arg, 2)));
                pars.config_args_besides_binary_dir = true;
            } else if (starts_with(arg, "-B")) {
                if (arg == "-B") {
                    // unlike cmake, here we support the '-B <path>' style, too
                    if (++argix >= argc)
                        badpars_exit("Missing path after '-B'");
                    set_binary_dir(argv[argix]);
                } else
                    set_binary_dir(make_string(butleft(arg, 2)));
            } else if (!starts_with(arg, '-')) {
                if (evaluate_source_dir(arg, true)) {
                    set_source_dir(arg);
                    pars.config_args_besides_binary_dir = true;
                } else if (evaluate_binary_dir(arg)) {
                    set_binary_dir(arg);
                } else {
                    badpars_exit(
                        stringf("%s is neither a valid source dir (no CMakeLists.txt), nor a "
                                "'*.cmake' build script, nor "
                                "an existing binary dir (no CMakeCache.txt)",
                                arg.c_str()));
                }
            } else
                badpars_exit(stringf("Invalid option: '%s'", arg.c_str()));
        }  // else: not one of specific args
    }      // foreach arg
    if (pars.binary_dir.empty()) {
        if (pars.source_dir.empty())
            badpars_exit(
                "Neither a source directory and nor a valid binary directory (path to an existing "
                "build) specified.");
        pars.binary_dir = fs::current_path();
        pars.binary_dir_valid = evaluate_binary_dir(pars.binary_dir);
    }

    if (!pars.add_pkgs.empty()) {
        if (!pars.source_dir.empty())
            badpars_exit(
                "Don't specify source directory or build script (with or without '-H') together "
                "with the '-P' option. Packages are assigned automatic source directories "
                "under the 'cmakex_deps_clone_prefix' directory (see cmakex-config)");
    }

    // at this point we have a new or an existing binary dir
    CHECK(!pars.binary_dir.empty());

    // extract source dir from existing binary dir
    string cmake_cache_file = pars.binary_dir + "/CMakeCache.txt";
    if (fs::is_regular_file(cmake_cache_file)) {
        // read CMAKE_HOME_DIRECTORY
        string cmake_home_directory;
        bool found = false;
        auto f = must_fopen(cmake_cache_file, "r");
        while (!feof(f)) {
            auto line = must_fgetline_if_not_eof(f);
            if (starts_with(line, "CMAKE_HOME_DIRECTORY:") ||
                starts_with(line, "CMAKE_HOME_DIRECTORY=")) {
                auto equal_pos = line.find('=');
                if (equal_pos != string::npos) {
                    cmake_home_directory = line.substr(equal_pos + 1);
                    found = true;
                    break;
                }
            }
        }
        if (found) {
            if (fs::is_directory(cmake_home_directory)) {
                if (pars.source_dir.empty())
                    pars.source_dir = cmake_home_directory;
                else {
                    if (pars.source_dir != cmake_home_directory)
                        throwf(
                            "The source dir specified is different than the one found in the "
                            "CMakeCache.txt: \"%s\" and \"%s\"",
                            pars.source_dir.c_str(), cmake_home_directory.c_str());
                }
            } else
                throwf(
                    "CMAKE_HOME_DIRECTORY (\"%s\") is not an existing directory (read from "
                    "(\"%s\")",
                    cmake_home_directory.c_str(), cmake_cache_file.c_str());
        } else
            throwf("No CMAKE_HOME_DIRECTORY found in \"%s\"", cmake_cache_file.c_str());
    }

    return pars;
}
}
