#include "environment.h"

ToGenerator::~ToGenerator() {
    // Tell an abandoned producer to stop, then wait for it. The producer only
    // ever holds the state, never the generator, so this always runs on the
    // consumer's thread and the join is safe.
    if (state) {
        {
            std::lock_guard<std::mutex> lock(state->mtx);
            state->done = true;
            state->consumerReady = true;
        }
        state->cv.notify_all();
    }
    if (thread && thread->joinable()) {
        try {
            thread->join();
        } catch (...) {
            // ignore
        }
    }
}
