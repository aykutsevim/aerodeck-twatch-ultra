#include "core/CommandParser.h"
#include <cstring>

namespace core {

static bool isSep(char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; }

int CommandParser::tokenize(char* line, const char** argv, int max)
{
    int n = 0;
    char* p = line;
    while (*p) {
        while (*p && isSep(*p)) {
            *p++ = '\0';
        }
        if (!*p) {
            break;
        }
        if (n >= max) {
            return max + 1;
        }
        argv[n++] = p;
        while (*p && !isSep(*p)) {
            p++;
        }
    }
    return n;
}

bool CommandParser::add(const char* pattern, uint8_t minArgs, Handler fn)
{
    if (!pattern || !fn || n_ >= kMaxCommands) {
        return false;
    }
    cmds_[n_++] = {pattern, minArgs, fn};
    return true;
}

// Number of pattern tokens if pattern is a token-prefix of argv[0..argc), else 0.
static int matchPrefix(const char* pattern, const char* const* argv, int argc)
{
    char buf[CommandParser::kMaxLine];
    strncpy(buf, pattern, sizeof buf - 1);
    buf[sizeof buf - 1] = '\0';
    const char* pt[CommandParser::kMaxTokens];
    int pn = CommandParser::tokenize(buf, pt, CommandParser::kMaxTokens);
    if (pn <= 0 || pn > CommandParser::kMaxTokens || pn > argc) {
        return 0;
    }
    for (int i = 0; i < pn; i++) {
        if (strcmp(pt[i], argv[i]) != 0) {
            return 0;
        }
    }
    return pn;
}

bool CommandParser::dispatch(const char* line, Reply& reply) const
{
    if (!line) {
        reply.err("unknown command");
        return false;
    }
    if (strlen(line) > kMaxLine) {
        reply.err("line too long");
        return false;
    }
    char buf[kMaxLine + 1];
    strcpy(buf, line);
    const char* argv[kMaxTokens];
    int argc = tokenize(buf, argv, kMaxTokens);
    if (argc > kMaxTokens) {
        reply.err("too many arguments");
        return false;
    }
    if (argc < 2 || strcmp(argv[0], "ad") != 0) {
        reply.err("unknown command");
        return false;
    }
    const char* const* rest = argv + 1;
    int restc = argc - 1;

    const Cmd* best = nullptr;
    int bestLen = 0;
    bool prefixMatchedButShort = false;
    for (size_t i = 0; i < n_; i++) {
        int len = matchPrefix(cmds_[i].pattern, rest, restc);
        if (len == 0) {
            continue;
        }
        if (restc - len < cmds_[i].minArgs) {
            prefixMatchedButShort = true;
            continue;
        }
        if (len > bestLen) {
            best = &cmds_[i];
            bestLen = len;
        }
    }
    if (!best) {
        reply.err(prefixMatchedButShort ? "missing arguments" : "unknown command");
        return false;
    }
    best->fn(restc - bestLen, rest + bestLen, reply);
    return true;
}

} // namespace core
