#pragma once

#include <cstddef>
#include <cstdint>
#include <stdexcept>

extern "C" {
    typedef struct mstack_file mstack_t;

    int push(mstack_t* set, int key, void* val);
    void* pop(mstack_t* set);

    mstack_t* create_stack(std::size_t num_threads,
                           std::uint64_t width,
                           std::uint64_t depth,
                           std::uint8_t k_mode,
                           std::uint64_t relaxation_bound);

    mstack_t* register_stack(mstack_t* set, int thread_id);
}

class TwoDimStack {
public:
    class Handle {
    public:
        Handle(const Handle&) = delete;
        Handle& operator=(const Handle&) = delete;

        Handle(Handle&&) noexcept = default;
        Handle& operator=(Handle&&) noexcept = default;

        void push(void* value) {
            stack_->push_impl(value);
        }

        void* pop() {
            return stack_->pop_impl();
        }

    private:
        friend class TwoDimStack;

        explicit Handle(TwoDimStack* stack) noexcept
            : stack_(stack) {}

        TwoDimStack* stack_;
    };

    TwoDimStack(std::uint64_t width, std::uint64_t depth)
        : stack_(create_stack(0, width, depth, 0, 0)) {
        if (stack_ == nullptr) {
            throw std::runtime_error("create_stack failed");
        }
    }

    TwoDimStack(const TwoDimStack&) = delete;
    TwoDimStack& operator=(const TwoDimStack&) = delete;

    TwoDimStack(TwoDimStack&&) = delete;
    TwoDimStack& operator=(TwoDimStack&&) = delete;

    Handle register_thread(int thread_id) {
        if (register_stack(stack_, thread_id) == nullptr) {
            throw std::runtime_error("register_stack failed");
        }
        return Handle{this};
    }

private:
    void push_impl(void* value) {
        if (::push(stack_, 0, value) != 1) {
            throw std::runtime_error("push failed");
        }
    }

    void* pop_impl() {
        return ::pop(stack_);
    }

    mstack_t* stack_;
};