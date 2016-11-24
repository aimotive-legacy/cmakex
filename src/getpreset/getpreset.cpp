#ifdef _WIN32
#define _CRT_SECURE_NO_WARNINGS 1
#endif

#include "getpreset.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>

#include <yaml-cpp/yaml.h>
#include <nowide/cstdlib.hpp>

#include <adasworks/sx/check.h>
#include <adasworks/sx/log.h>
#include <adasworks/sx/string_par.h>
#include <adasworks/sx/stringf.h>

#include "filesystem.h"
#include "misc_utils.h"

using namespace adasworks;
namespace fs = filesystem;

using std::string;
using sx::string_par;
using std::vector;
using std::map;
using sx::stringf;
using namespace cmakex;

#ifndef LIBGETPRESET
void print_usage()
{
    fprintf(stderr, "%s\n",
            R"~(getpreset: Retrieve the value of a field of a specific preset. Usage:

    getpreset <preset-name-or-alias> <field>

or

    getpreset <preset-file.yaml>#<preset-name-or-alias> <field>

The second argument is the field to retrieve. The official fields
names are: `name`, `args` and `arch`.

Examples (return the `args` fields of the `vs2013` preset):

    getpreset vs2013 args
    getpreset some/dir/presets.yaml#vs2013 args

In the first case the `CMAKEX_PRESET_FILE` environment variable should
contain the path of the preset file.
)~");
}
#endif

// flatten a optionally nested sequence
vector<string> flatten(const YAML::Node n)
{
    if (!n || n.IsNull())
        return vector<string>();
    if (n.IsScalar())
        return vector<string>(1, n.as<string>());
    if (n.IsSequence()) {
        vector<string> vs;
        for (auto& x : n) {
            auto vsn = flatten(x);
            vs.insert(vs.end(), vsn.begin(), vsn.end());
        }
        return vs;
    }
    throw std::runtime_error(
        "Node can't be flattened because it contains a non-scalar, non-sequence node.");
}

#ifndef LIBGETPRESET
void test_flatten()
{
    CHECK(flatten(YAML::Load("")) == vector<string>());
    CHECK(flatten(YAML::Load("one")) == vector<string>({"one"}));
    CHECK(flatten(YAML::Load("[one, two]")) == vector<string>({"one", "two"}));
    CHECK(flatten(YAML::Load("[[one,three], two]")) == vector<string>({"one", "three", "two"}));
    CHECK(flatten(YAML::Load("[one, [two, three]]")) == vector<string>({"one", "two", "three"}));
    CHECK(flatten(YAML::Load("[one, [two, [three, four]]]")) ==
          vector<string>({"one", "two", "three", "four"}));
}
#endif

// split x at nonquoted spaces
vector<string> split(string_par x)
{
    vector<string> vs;
    string s = x.str();
    bool w_valid = false;
    string w;
    auto add_to_w = [&w, &w_valid](char c) {
        if (!w_valid) {
            w_valid = true;
            w.clear();
        }
        w += c;
    };
    bool in_quote = false;
    for (auto p = s.c_str(); *p; ++p) {
        char c = *p;

        // skip trailing spaces
        if (!w_valid && *p == ' ') {
            continue;
        }

        // handle escaped charactes: transmit backslash and next character
        if (c == '\\') {
            if (p[1] == 0) {
                fprintf(stderr, "Single backslash at end of the string `%s`.\n", s.c_str());
                exit(EXIT_FAILURE);
            }
            add_to_w('\\');
            add_to_w(p[1]);
            ++p;
            continue;
        }

        if (c == '"') {
            in_quote = !in_quote;
        } else if (c == ' ' && !in_quote) {
            CHECK(w_valid && !w.empty());
            vs.emplace_back(move(w));
            w_valid = false;
        } else
            add_to_w(c);
    }
    if (in_quote) {
        fprintf(stderr, "Unmatched double-quote in `%s`.", s.c_str());
        exit(EXIT_FAILURE);
    }
    if (w_valid) {
        CHECK(!w.empty());
        vs.emplace_back(move(w));
    }
    return vs;
}

#ifndef LIBGETPRESET
void test_split()
{
    CHECK(split("") == vector<string>());
    CHECK(split("one") == vector<string>({"one"}));
    CHECK(split("one two") == vector<string>({"one", "two"}));
    CHECK(split("one     two") == vector<string>({"one", "two"}));
    CHECK(split("one two three") == vector<string>({"one", "two", "three"}));
    CHECK(split(" one two three") == vector<string>({"one", "two", "three"}));
    CHECK(split(" one two three") == vector<string>({"one", "two", "three"}));
    CHECK(split("  one two three") == vector<string>({"one", "two", "three"}));
    CHECK(split("one    two    three ") == vector<string>({"one", "two", "three"}));
    CHECK(split("one   two  three   ") == vector<string>({"one", "two", "three"}));
    CHECK(split(" one two three ") == vector<string>({"one", "two", "three"}));
    CHECK(split("   one two three   ") == vector<string>({"one", "two", "three"}));
    CHECK(split("one \"two three\" four") == vector<string>({"one", "two three", "four"}));
    CHECK(split("one \"two   three\" four") == vector<string>({"one", "two   three", "four"}));
    CHECK(split("\"one two\" three four") == vector<string>({"one two", "three", "four"}));
    CHECK(split(" \"one two\" three four") == vector<string>({"one two", "three", "four"}));
    CHECK(split("  \"one two\" three four") == vector<string>({"one two", "three", "four"}));
    CHECK(split("one two \"three four\"") == vector<string>({"one", "two", "three four"}));
    CHECK(split("one two \"three four\" ") == vector<string>({"one", "two", "three four"}));
    CHECK(split("one two \"three four\"  ") == vector<string>({"one", "two", "three four"}));
    CHECK(split("\"one two\"") == vector<string>({"one two"}));
    CHECK(split(" \"one two\"") == vector<string>({"one two"}));
    CHECK(split("\"one two\" ") == vector<string>({"one two"}));
    CHECK(split(" \"one two\" ") == vector<string>({"one two"}));
    CHECK(split("  \"one two\"  ") == vector<string>({"one two"}));
    CHECK(split(R"~(-Done=\"two\")~") == vector<string>({R"~(-Done=\"two\")~"}));
}
#endif

string subs(string_par x, string_par var, string_par value)
{
    if (var.empty()) {
        fprintf(stderr, "A variable cannot be empty.\n");
        exit(EXIT_FAILURE);
    }
    for (auto c : {'$', '{', '}'}) {
        if (strchr(var.c_str(), c)) {
            fprintf(stderr, "Character '%c' found in variable name `%s`.\n", c, var.c_str());
            exit(EXIT_FAILURE);
        }
    }
    string r = x.str();
    string s = string("${") + var.c_str() + "}";
    for (;;) {
        auto pos = r.find(s);
        if (pos == string::npos)
            break;
        auto n = s.size();
        r = r.substr(0, pos) + value.str() + r.substr(pos + n);
    }
    return r;
}

#ifndef LIBGETPRESET
void test_subs()
{
    CHECK(subs("${a} a ${c}", "a", "1") == "1 a ${c}");
    CHECK(subs("${abc} a ${c}", "a", "1") == "${abc} a ${c}");
    CHECK(subs("${abc} abc ${c}", "abc", "1") == "1 abc ${c}");
    CHECK(subs("${abc} abc ${c}", "abc", "1") == "1 abc ${c}");
    CHECK(subs("${abc} abc ${c}", "c", "2") == "${abc} abc 2");
}

void do_test()
{
    test_flatten();
    test_split();
    test_subs();
}

int main_core(int argc, char* argv[])
{
#ifdef GETPRESET_DO_TEST
    do_test();
    return EXIT_SUCCESS;
#endif

    if (argc != 3) {
        print_usage();
        exit(EXIT_FAILURE);
    }

    string lookup_name = argv[1];
    string field = argv[2];

    vector<string> vv;
    try {
        string file;
        vector<string> names;
        std::tie(vv, file, names) = libgetpreset::getpreset(argv[1], argv[2]);
    } catch (const std::exception& e) {
        fprintf(stderr, "Error: %s\n", e.what());
        return EXIT_FAILURE;
    }

    for (auto& s : vv) {
        // add quotes as necessary
        if (s.empty() || s.find(' ') != string::npos)
            s = string("\"") + s + string("\"");
    }
    bool first = true;
    for (auto& s : vv) {
        if (!first)
            printf(" ");
        else
            first = false;
        printf("%s", s.c_str());
    }
    printf("\n");

    return EXIT_SUCCESS;
}
#endif

std::tuple<string, vector<string>> find_file_and_names(string_par path_name)
{
    // split along hashmarks and find out if the first one is a file
    auto names = split(path_name, '#');
    string file;
    if (names.empty())
        throwf("Empty path/preset argument");

    vector<string> tried_paths;

    if (fs::is_regular_file(names[0])) {
        if (names.size() == 1) {
            throwf(
                "The preset specifier (%s) is a valid YAML file but no preset names found after "
                "it.",
                path_for_log(path_name).c_str());
        }
        file = names[0];
        names.erase(names.begin());
    } else {
        if (names.size() > 1)
            tried_paths.emplace_back(names[0]);
        // check environment var
        auto x = nowide::getenv("CMAKEX_PRESET_FILE");
        if (x && strlen(x) == 0)
            x = nullptr;
        if (x) {
            if (fs::is_regular_file(x))
                file = x;
            else
                tried_paths.emplace_back(x);
        }
    }

    if (file.empty()) {
        CHECK(tried_paths.size() <= 2);
        string m = "No valid preset file found";
        if (!tried_paths.empty())
            m += stringf(". Tried: %s", path_for_log(tried_paths[0]).c_str());
        if (tried_paths.size() == 2)
            m += stringf(" and %s", path_for_log(tried_paths[1]).c_str());
        m += ".";
        throw std::runtime_error(m);
    }

    return std::make_tuple(std::move(file), std::move(names));
}

namespace libgetpreset {
tuple<vector<string>, string, vector<string>> getpreset(string_par path_name, string_par field)
{
    string file;
    vector<string> lookup_names;
    std::tie(file, lookup_names) = find_file_and_names(path_name);

    YAML::Node config;
    try {
        config = YAML::LoadFile(file);
    } catch (const std::exception& e) {
        throw std::runtime_error(stringf("Error loading preset file %s, reason: %s\n",
                                         path_for_log(file).c_str(), e.what()));
    }

    const YAML::Node variables = config["variables"];
    const YAML::Node presets = config["presets"];

    if (!presets)
        throw std::runtime_error(stringf("No presets found in %s.\n", path_for_log(file).c_str()));

    if (!presets.IsMap()) {
        throw std::runtime_error(stringf("The value of the `presets` key is not a map in %s.\n",
                                         path_for_log(file).c_str()));
    }

    vector<string> result;

    for (auto& lookup_name : lookup_names) {
        string preset_name;

        for (auto& n : presets) {
            string key = n.first.as<string>();
            if (key == lookup_name) {
                preset_name = key;
                break;
            }
            auto aliases = flatten(n.second["alias"]);
            for (auto& a : aliases) {
                if (a == lookup_name) {
                    preset_name = key;
                    break;
                }
            }
            if (!preset_name.empty())
                break;
        }

        if (preset_name.empty())
            throw std::runtime_error(stringf("The preset name or alias '%s' not found in %s.\n",
                                             lookup_name.c_str(), path_for_log(file).c_str()));

        auto preset = presets[preset_name];

        if (field == "name") {
            result.emplace_back(preset_name);
        } else if (field == "args") {
            auto args = preset["args"];
            vector<string> vv;
            if (args) {
                auto v = flatten(args);
                append_inplace(vv, v);
                map<string, string> vars;
                using pair_ss = std::pair<string, string>;
                string input_dir = fs::path(file).parent_path();
                vars.insert(pair_ss("CMAKE_CURRENT_LIST_DIR", input_dir));
                for (auto& n : variables) {
                    if (!n.first.IsScalar()) {
                        throw std::runtime_error(stringf("Non-scalar variable name found in %s.\n",
                                                         path_for_log(file).c_str()));
                    } else if (!n.second.IsScalar()) {
                        throw std::runtime_error(
                            stringf("The value of the variable '%s' is must be scalar in %s.\n",
                                    n.first.as<string>().c_str(), path_for_log(file).c_str()));
                    }
                    string k = n.first.as<string>();
                    string v = n.second.as<string>();
                    vars.insert(pair_ss(k, v));
                }
                // substitute variables
                for (auto& s : vv) {
                    bool changed = false;
                    do {
                        changed = false;
                        for (auto& kv_var : vars) {
                            auto t = subs(s, kv_var.first, kv_var.second);
                            if (t != s) {
                                s = t;
                                changed = true;
                            }
                        }
                    } while (changed);
                }
            }
            append_inplace(result, vv);
        } else if (field == "arch") {
            auto arch = preset["arch"];
            result.emplace_back(arch ? arch.as<string>().c_str() : preset_name.c_str());
        } else {
            throw std::runtime_error(stringf("Invalid field name: %s.\n", field.c_str()));
        }
    }

    return std::make_tuple(std::move(result), std::move(file), std::move(lookup_names));
}
}

#ifndef LIBGETPRESET
int main(int argc, char* argv[])
{
    int r = EXIT_FAILURE;
    try {
        r = main_core(argc, argv);
    } catch (const std::exception& e) {
        LOG_FATAL("Exception: %s", e.what());
    } catch (...) {
        LOG_FATAL("Unknown exception");
    }
    return r;
}
#endif
