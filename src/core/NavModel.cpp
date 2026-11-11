#include "core/NavModel.h"

namespace core {

int NavModel::next() const
{
    if (busy_ || n_ < 2) {
        return -1;
    }
    return (cur_ + 1) % n_;
}

int NavModel::prev() const
{
    if (busy_ || n_ < 2) {
        return -1;
    }
    return (cur_ + n_ - 1) % n_;
}

int NavModel::button()
{
    if (n_ < 2) {
        return -1;
    }
    if (busy_) {
        pendingHome_ = true;
        return -1;
    }
    return cur_ == 0 ? lastNonHome_ : 0;
}

void NavModel::transitionStarted(int target)
{
    busy_ = true;
    target_ = target;
}

int NavModel::transitionDone()
{
    busy_ = false;
    cur_ = target_;
    if (cur_ != 0) {
        lastNonHome_ = cur_;
    }
    if (pendingHome_) {
        pendingHome_ = false;
        if (cur_ != 0) {
            return 0;
        }
    }
    return -1;
}

} // namespace core
