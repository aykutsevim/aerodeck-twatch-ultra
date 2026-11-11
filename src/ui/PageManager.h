// AeroDeck page ring navigation (FR-002..FR-006). UI task only.
#pragma once
#include "ui/Page.h"

namespace PageManager {
constexpr int kMaxPages = 8;
void registerPage(Page* p);      // registration order = ring order; Home first
void begin();                    // shows Home
void next();
void prev();
void buttonPressed();            // FR-003 semantics
PageId current();
int count();
Page* page(int index);
// Called once when Home is first shown (boot timing, SC-001).
void onFirstHome(void (*cb)());
} // namespace PageManager
