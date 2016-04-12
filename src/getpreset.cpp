#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>

#include <yaml-cpp/yaml.h>

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

vector<string> flatten(string_par input_path,
                       string_par preset_name,
                       string_par key,
                       const YAML::Node n)
{
    vector<string> vs;
    for (auto& x : n) {
        if (x.IsScalar())
            vs.push_back(x.as<string>());
        else if (n.IsSequence()) {
            auto vsn = flatten(input_path, preset_name, key, x);
            vs.insert(vs.end(), vsn.begin(), vsn.end());
        } else {
            fprintf(stderr, "The key \"%s\" in preset \"%s\" can't be flattened (\"%s\".\n",
                    key.c_str(), preset_name.c_str(), input_path.c_str());
            exit(EXIT_FAILURE);
        }
    }
    return vs;
}

vector<string> split(string_par x)
{
    vector<string> vs;
    string s = x.str();
    int start = 0;
    int N = s.size();
    for (; start < N;) {
        if (s[start] == ' ') {
            ++start;
            continue;
        }
        bool in_quote = false;
        int end = start;
        for (; end < N; ++end) {
            if (s[end] == '"')
                in_quote = !in_quote;
            else if (s[end] == ' ') {
                if (!in_quote) {
                    vs.emplace_back(s.begin() + start, s.begin() + end);
                    start = end + 1;
                    break;
                }
            }
        }
        if (end == N) {
            if (start < end) {
                if (in_quote) {
                    fprintf(stderr, "Unmatched double-quote in `%s`.", s.c_str());
                    exit(EXIT_FAILURE);
                }
            }
            vs.emplace_back(s.begin() + start, s.begin() + end);
            break;
        }
    }
    return vs;
}

string subs(string_par x, string_par var, string_par value)
{
    string r;
    return r;
}

int main_core(int argc, char* argv[])
{
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
        auto aliases = flatten(input_path, preset_name, "alias", n.second["alias"]);
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
            auto v = flatten(input_path, preset_name, "args", args);
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
