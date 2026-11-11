// AeroDeck core: page-ring navigation state machine (FR-002/FR-003, edge cases). Host-tested.
// Index 0 is Home. Methods return the target page index, or -1 for "no navigation".
#pragma once

namespace core {

class NavModel {
public:
    explicit NavModel(int pages) : n_(pages) {}

    int next() const;          // swipe left
    int prev() const;          // swipe right
    int button();              // FR-003; while busy, queues "go home" and returns -1

    void transitionStarted(int target);
    int transitionDone();      // returns a queued target (Home) or -1

    int current() const { return cur_; }
    int lastNonHome() const { return lastNonHome_; }
    bool busy() const { return busy_; }

private:
    int n_;
    int cur_ = 0;
    int target_ = 0;
    int lastNonHome_ = 1;
    bool busy_ = false;
    bool pendingHome_ = false;
};

} // namespace core
