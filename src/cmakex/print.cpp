#include "print.h"

namespace cmakex {
void log_exec(string_par command, const vector<string>& args)
{
    string u = command.str();
    string t;
    for (auto& s : args) {
        t.clear();
        bool quoted = false;
        for (auto c : s) {
            switch (c) {
                case ' ':
                    if (!quoted) {
                        quoted = true;
                        t.insert(t.begin(), '"');
                    }
                    break;
                case '\\':
                    t.append("\\\\");
                    break;
                case '"':
                    t.append("\\\"");
                    break;
                default:
                    t.push_back(c);
            }
        }
        if (quoted)
            t.push_back('"');
        u.push_back(' ');
        u.append(t);
    }
    print_out("$ %s", u.c_str());
}
}