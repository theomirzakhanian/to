#include "environment.h"

ToGenerator::~ToGenerator() {
    // Signal the thread to exit cleanly
    if (thread && thread->joinable()) {
        if (done) *done = true;
        if (consumerReady) *consumerReady = true;
        if (cv) cv->notify_all();
        try {
            thread->join();
        } catch (...) {
            // ignore
        }
    }
}
