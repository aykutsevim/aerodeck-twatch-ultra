#include "core/AlertSet.h"
#include <cstring>

namespace core {

namespace {
void setText(char* dst, const char* src)
{
    strncpy(dst, src ? src : "", 24);
    dst[24] = '\0';
}

// a before b?
bool before(const Alert& a, const Alert& b)
{
    if (a.level != b.level) {
        return uint8_t(a.level) > uint8_t(b.level);
    }
    return a.raisedSeq < b.raisedSeq;
}
} // namespace

int AlertSet::find(AlertId id) const
{
    for (size_t i = 0; i < n_; i++) {
        if (a_[i].id == id) {
            return int(i);
        }
    }
    return -1;
}

RaiseResult AlertSet::raise(AlertId id, AlertLevel level, const char* text)
{
    const char* t = text ? text : "";
    const int i = find(id);
    if (i < 0) {
        if (n_ >= kCap) {
            return RaiseResult::Full;
        }
        Alert& a = a_[n_++];
        a.id = id;
        a.level = level;
        setText(a.text, t);
        a.acked = false;
        a.raisedSeq = ++seq_;
        return RaiseResult::Added;
    }
    Alert& a = a_[i];
    char nt[25];
    setText(nt, t);
    if (uint8_t(level) > uint8_t(a.level)) {
        a.level = level;
        memcpy(a.text, nt, sizeof a.text);
        a.acked = false;
        return RaiseResult::Rearmed;
    }
    if (level != a.level || strcmp(nt, a.text) != 0) {
        a.level = level;
        memcpy(a.text, nt, sizeof a.text);
        return RaiseResult::Updated;
    }
    return RaiseResult::Same;
}

bool AlertSet::clear(AlertId id)
{
    const int i = find(id);
    if (i < 0) {
        return false;
    }
    a_[i] = a_[--n_];
    return true;
}

uint8_t AlertSet::ackAll()
{
    uint8_t n = 0;
    for (size_t i = 0; i < n_; i++) {
        if (!a_[i].acked) {
            a_[i].acked = true;
            n++;
        }
    }
    return n;
}

bool AlertSet::ack(AlertId id)
{
    const int i = find(id);
    if (i < 0) {
        return false;
    }
    a_[i].acked = true;
    return true;
}

bool AlertSet::contains(AlertId id) const { return find(id) >= 0; }

bool AlertSet::anyUnacked() const
{
    for (size_t i = 0; i < n_; i++) {
        if (!a_[i].acked) {
            return true;
        }
    }
    return false;
}

bool AlertSet::anyUnackedWarning() const
{
    for (size_t i = 0; i < n_; i++) {
        if (!a_[i].acked && a_[i].level == AlertLevel::Warning) {
            return true;
        }
    }
    return false;
}

size_t AlertSet::sorted(Alert* out, size_t max) const
{
    if (!out || max == 0) {
        return 0;
    }
    Alert tmp[kCap];
    memcpy(tmp, a_, sizeof(Alert) * n_);
    // insertion sort (n <= 24)
    for (size_t i = 1; i < n_; i++) {
        Alert x = tmp[i];
        size_t j = i;
        while (j > 0 && before(x, tmp[j - 1])) {
            tmp[j] = tmp[j - 1];
            j--;
        }
        tmp[j] = x;
    }
    const size_t m = n_ < max ? n_ : max;
    memcpy(out, tmp, sizeof(Alert) * m);
    return m;
}

} // namespace core
