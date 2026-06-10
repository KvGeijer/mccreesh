#include "wrapper.hpp"

int main() {
    TwoDimStack stack{/*width=*/64, /*depth=*/1024};

    auto handle = stack.register_thread(/*thread_id=*/0);

    int x = 42;
    handle.push(&x);

    void* result = handle.pop();
}