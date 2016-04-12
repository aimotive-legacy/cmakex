#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>

#include <yaml-cpp/yaml.h>

#include <adasworks/sx/check.h>
#include <adasworks/sx/log.h>
#include <adasworks/sx/string_par.h>
#include <adasworks/sx/stringf.h>

namespace adasworks {

using std::string;
using sx::string_par;
using std::vector;
using std::map;

bool file_exists(string_par x)
{
    FILE* f = fopen(x.c_str(), "r");
    if (f)
        fclose(f);
    return f;
}

string find_input_path(const char* argv0)
{
    const char* primary_filename = "cmakex-preset.yml";
    const char* secondary_filename = ".cmakex-preset.yml";
    const char* cpf = getenv("CMAKEX_PRESET_FILE");
    if (cpf && strlen(cpf) > 0)
        return cpf;
    else if (file_exists(primary_filename))
        return primary_filename;
    else if (file_exists(secondary_filename))
        return secondary_filename;
    else {
        const char* home = getenv("HOME");
        const char* homedrive = getenv("HOMEDRIVE");
        const char* homepath = getenv("HOMEPATH");
        string ha, hb;
        if (home)
            ha = string(home) + "/";
        if (homedrive && homepath)
            hb = string(homedrive) + homepath + "\\";
        if (file_exists(ha + primary_filename))
            return ha + primary_filename;
        else if (file_exists(ha + secondary_filename))
            return ha + secondary_filename;
        else if (file_exists(hb + primary_filename))
            return hb + primary_filename;
        else if (file_exists(hb + secondary_filename))
            return hb + secondary_filename;
    }

    string d = string(strlen(argv0) == 0 ? "." : argv0) + "/";
    if (file_exists(d + primary_filename))
        return d + primary_filename;
    else if (file_exists(d + secondary_filename))
        return d + secondary_filename;

    return {};
}

void print_usage()
{
    fprintf(stderr, "%s\n",
            R"~(getpreset: Retrieve the value of a field of a specific preset. Usage:

    getpreset <preset-name-or-alias> <field>

The first argument is the the name or alias (alternative name) of the preset.
The second argument is the name of the field to retrieve.

Example (return the `args` fields of the `vs2013` preset):

    getpreset vs2013 args

The preset file will be loaded from the first valid location from the
following list:

- the file pointed by the `CMAKEX_PRESET_FILE` environment variable
- the `cmakex-preset.yml` or `.cmakex-preset.yml` in the current working directory
- the `cmakex-preset.yml` or `.cmakex-preset.yml` in the current user's home
  directory (retrieved from `$HOME` or `%HOMEDRIVE%\%HOMEPATH%` variables)
- the `cmakex-preset.yml` or `.cmakex-preset.yml` in the `getpreset`
  executable's directory
)~");
}

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

// split x at nonquoted spaces
vector<string> split(string_par x)
{
    vector<string> vs;
    string s = x.str();
    int N = s.size();
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
                fprintf(stderr, "Single backslash at end of the string \"%s\".\n", s.c_str());
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

string subs(string_par x, string_par var, string_par value)
{
    if (var.empty()) {
        fprintf(stderr, "A variable cannot be empty.\n");
        exit(EXIT_FAILURE);
    }
    for (auto c : {'$', '{', '}'}) {
        if (strchr(var.c_str(), c)) {
            fprintf(stderr, "Character '%c' found in variable name \"%s\".\n", c, var.c_str());
        }
        if (strchr(value.c_str(), c)) {
            fprintf(stderr, "Character '%c' found in variable value \"%s\".\n", c, value.c_str());
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
#if GETPRESET_DO_TEST
    do_test();
    return EXIT_SUCCESS;
#endif

    if (argc != 3) {
        print_usage();
        exit(EXIT_FAILURE);
    }

    string lookup_name = argv[1];
    string field = argv[2];

    string input_path = find_input_path(argv[0]);
    if (input_path.empty()) {
        fprintf(stderr, "Preset file not found.\n");
        exit(EXIT_FAILURE);
    }

    YAML::Node config;

    try {
        config = YAML::LoadFile(input_path);
    } catch (const std::exception& e) {
        fprintf(stderr, "Error loading preset file \"%s\", reason: %s\n", input_path.c_str(),
                e.what());
        exit(EXIT_FAILURE);
    }

    config = YAML::Load("presets: { {a: ize},{b: bize}}");

    const YAML::Node variables = config["variables"];
    const YAML::Node presets = config["presets"];

    if (!presets) {
        fprintf(stderr, "No presets found in \"%s\".\n", input_path.c_str());
        exit(EXIT_FAILURE);
    }

    if (!presets.IsMap()) {
        fprintf(stderr, "The value of the `presets` key is not a map in \"%s\".\n",
                input_path.c_str());
        exit(EXIT_FAILURE);
    }

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

    if (preset_name.empty()) {
        fprintf(stderr, "The preset name or alias \"%s\" not found in \"%s\".\n",
                lookup_name.c_str(), input_path.c_str());
        exit(EXIT_FAILURE);
    }

    auto preset = presets[preset_name];

    if (field == "name") {
        printf("%s\n", preset_name.c_str());
    } else if (field == "args") {
        auto args = preset["args"];
        if (args) {
            auto v = flatten(args);
            vector<string> vv;
            for (auto& s : v) {
                auto ss = split(s);
                if (!ss.empty())
                    vv.insert(vv.end(), ss.begin(), ss.end());
            }
            map<string, string> vars;
            using pair_ss = std::pair<string, string>;
            string input_dir = input_path;
            while (!input_dir.empty() && input_dir.back() != '/' && input_dir.back() != '\\')
                input_dir.pop_back();
            vars.insert(pair_ss("CMAKE_CURRENT_LIST_DIR", input_dir));
            for (auto& n : variables) {
                if (!n.first.IsScalar()) {
                    fprintf(stderr, "Non-scalar variable name found in \"%s\".\n",
                            input_path.c_str());
                    return EXIT_FAILURE;
                } else if (!n.second.IsScalar()) {
                    fprintf(stderr,
                            "The value of the variable \"%s\" is must be scalar in \"%s\".\n",
                            n.first.as<string>().c_str(), input_path.c_str());
                    return EXIT_FAILURE;
                }
                string k = n.first.as<string>();
                string v = n.second.as<string>();
                vars.insert(pair_ss(k, v));
            }
            // substitute variables
            for (auto& s : vv) {
                for (auto& kv_var : vars) {
                    s = subs(s, kv_var.first, kv_var.second);
                }
            }
            for (auto& s : vv) {
                // remove quotes
                while (s.size() >= 2 && s.front() == '"' && s.back() == '"')
                    s = s.substr(1, s.size() - 2);
                // check any double quotes left
                if (s.find("\"") != string::npos) {
                    fprintf(stderr, "The string `%s` contains internal double-quotes in file %s.",
                            s.c_str(), input_path.c_str());
                    exit(EXIT_FAILURE);
                }
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
        } else {
            printf("\n");
        }
    } else if (field == "arch") {
        auto arch = preset["arch"];
        printf("%s\n", arch ? arch.as<string>().c_str() : preset_name.c_str());
    } else {
        fprintf(stderr, "Invalid field name: %s.\n", field.c_str());
        exit(EXIT_FAILURE);
    }

    return EXIT_SUCCESS;
}
}

int main(int argc, char* argv[])
{
    int r = EXIT_FAILURE;
    try {
        r = adasworks::main_core(argc, argv);
    } catch (const std::exception& e) {
        LOG_FATAL("Exception: %s", e.what());
    } catch (...) {
        LOG_FATAL("Unknown exception");
    }
    return r;
}
