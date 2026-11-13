// AeroDeck core: `ad ...` serial command tokenizer + dispatch table (host-tested).
// Contract: specs/001-platform-foundation/contracts/serial_commands.md
#pragma once
#include <cstddef>
#include <cstdint>

namespace core {

struct Reply {
    virtual void ok() = 0;
    virtual void err(const char* reason) = 0;
    virtual void line(const char* text) = 0;
    virtual ~Reply() = default;
};

// argv excludes "ad" and the matched pattern tokens.
using Handler = void (*)(int argc, const char* const* argv, Reply& reply);

class CommandParser {
public:
    static constexpr size_t kMaxLine = 300;   // S3: `ad wx decode "<METAR>"` (raw reports up to 255 chars)
    static constexpr int kMaxTokens = 12;   // "ad simfail" + 8 names + headroom
    static constexpr size_t kMaxCommands = 72;   // S7: 57 registered

    // pattern: space-separated tokens after "ad", e.g. "set list".
    bool add(const char* pattern, uint8_t minArgs, Handler fn);

    // Parses and dispatches one line. Replies ERR on failure. Returns true if a handler ran.
    bool dispatch(const char* line, Reply& reply) const;

    size_t count() const { return n_; }
    const char* pattern(size_t i) const { return i < n_ ? cmds_[i].pattern : nullptr; }

    static constexpr int kUnterminatedQuote = -1;
    // Splits in place on spaces/tabs/CR/LF; "double-quoted" tokens may contain spaces (\" and \ escapes).
    // Returns token count (≤ max), max+1 if there are more, or kUnterminatedQuote.
    static int tokenize(char* line, const char** argv, int max);

private:
    struct Cmd {
        const char* pattern;
        uint8_t minArgs;
        Handler fn;
    };
    Cmd cmds_[kMaxCommands] = {};
    size_t n_ = 0;
};

} // namespace core
