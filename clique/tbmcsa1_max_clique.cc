/* vim: set sw=4 sts=4 et foldmethod=syntax : */

#include <clique/tbmcsa1_max_clique.hh>
#include <clique/degree_sort.hh>
#include <clique/colourise.hh>

#include <2d-stack/wrapper.hpp>
#include <termination_detection.hpp>

#include <boost/thread.hpp>

#include <algorithm>
#include <list>
#include <functional>
#include <vector>
#include <thread>
#include <future>
#include <condition_variable>
#include <atomic>
#include <memory>
#include <numeric>
#include <utility>

using namespace clique;

namespace
{
    template <unsigned size_>
    struct QueueItem
    {
        FixedBitSet<size_> c;
        FixedBitSet<size_> p;
        unsigned cn;
    };

    /**
     * Here we only implement using an atomic for the incumbent.
     */
    struct AtomicBestAnywhere
    {
        std::atomic<unsigned> value;

        AtomicBestAnywhere()
        {
            value.store(0, std::memory_order_seq_cst);
        }

        bool update(unsigned v)
        {
            while (true) {
                unsigned cur_v = value.load(std::memory_order_seq_cst);
                if (v > cur_v) {
                    if (value.compare_exchange_strong(cur_v, v, std::memory_order_seq_cst))
                        return true;
                }
                else
                    return false;
            }
        }

        unsigned get()
        {
            return value.load(std::memory_order_relaxed);
        }
    };

    /**
     * We've possibly found a new best. Update best_anywhere and our local
     * result, and do any necessary printing.
     */
    template <unsigned size_>
    auto found_possible_new_best(const FixedBitGraph<size_> & graph, const std::vector<int> & o,
            const FixedBitSet<size_> & c, int c_popcount,
            const MaxCliqueParams & params, MaxCliqueResult & result, AtomicBestAnywhere & best_anywhere) -> void
    {
        if (best_anywhere.update(c_popcount)) {
            result.size = c_popcount;
            result.members.clear();
            for (int i = 0 ; i < graph.size() ; ++i)
                if (c.test(i))
                    result.members.insert(o[i]);
            if (params.print_candidates)
                print_candidate(params, result.size);
        }
    }

    /**
     * Bound function.
     */
    auto bound(unsigned c_popcount, unsigned cn, const MaxCliqueParams & params, AtomicBestAnywhere & best_anywhere) -> bool
    {
        unsigned best_anywhere_value = best_anywhere.get();
        return (c_popcount + cn <= best_anywhere_value || best_anywhere_value >= params.stop_after_finding);
    }

    template <unsigned size_>
    auto push_work(TwoDimStack::Handle & stack, FixedBitSet<size_> && c, FixedBitSet<size_> && p, unsigned cn) -> void
    {
        stack.push(new QueueItem<size_>{ std::move(c), std::move(p), cn });
    }

    template <unsigned size_>
    auto process_item(
            const FixedBitGraph<size_> & graph,
            const std::vector<int> & o,
            TwoDimStack::Handle & stack,
            QueueItem<size_> & item,
            MaxCliqueResult & result,
            const MaxCliqueParams & params,
            AtomicBestAnywhere & best_anywhere) -> void
    {
        if (params.abort.load())
            return;

        ++result.nodes;

        unsigned best_anywhere_value = best_anywhere.get();
        if (best_anywhere_value >= params.stop_after_finding || item.cn <= best_anywhere_value)
            return;
        ++result.processed_nodes;

        // get our coloured vertices
        std::array<unsigned, size_ * bits_per_word> p_order, colours;
        colourise<size_>(graph, item.p, p_order, colours);

        const auto c_popcount = item.c.popcount();

        // for each v in p... (v comes later)
        for (int n = static_cast<int>(item.p.popcount()) - 1 ; n >= 0 ; --n) {

            // bound, timeout or early exit?
            if (params.abort.load())
                return;
            if (bound(c_popcount, colours[n], params, best_anywhere))
                break;

            auto v = p_order[n];

            // consider taking v
            FixedBitSet<size_> child_c = item.c;
            child_c.set(v);
            const auto child_c_popcount = c_popcount + 1;

            // filter p to contain vertices adjacent to v
            FixedBitSet<size_> new_p = item.p;
            graph.intersect_with_row(v, new_p);

            if (new_p.empty()) {
                found_possible_new_best(graph, o, child_c, child_c_popcount, params, result, best_anywhere);
            }
            else
            {
                const auto cn = child_c_popcount + colours[n];
                push_work<size_>(stack, std::move(child_c), std::move(new_p), cn);
            }

            // now consider not taking v
            item.p.unset(v);
        }
    }

    template <unsigned size_>
    auto max_clique(const FixedBitGraph<size_> & graph, const std::vector<int> & o, const MaxCliqueParams & params) -> MaxCliqueResult
    {
        const auto n_threads = std::max(1u, params.n_threads);
        const auto stack_depth = std::max(1u, params.stack_depth);
        TwoDimStack work_stack{ 2 * n_threads, stack_depth };
        termination_detection::TerminationDetection termination{ static_cast<int>(n_threads) };

        MaxCliqueResult result; // global result
        std::mutex result_mutex;

        AtomicBestAnywhere best_anywhere; // global incumbent
        best_anywhere.update(params.initial_bound);

        std::list<std::thread> threads; // workers

        for (unsigned i = 0 ; i < n_threads ; ++i) {
            threads.push_back(std::thread([&, i] {
                        auto start_time = std::chrono::steady_clock::now(); // local start time
                        auto stack = work_stack.register_thread(static_cast<int>(i));

                        MaxCliqueResult tr; // local result

                        if (0 == i) {
                            FixedBitSet<size_> c; // current candidate clique
                            c.resize(graph.size());

                            FixedBitSet<size_> p; // potential additions
                            p.resize(graph.size());
                            p.set_all();

                            const auto cn = c.popcount() + p.popcount();
                            push_work<size_>(stack, std::move(c), std::move(p), cn);
                        }

                        while (termination.repeat([&] {
                                    if (params.abort.load())
                                        return false;

                                    std::unique_ptr<QueueItem<size_> > args{ static_cast<QueueItem<size_> *>(stack.pop()) };
                                    if (! args)
                                        return false;

                                    process_item<size_>(graph, o, stack, *args, tr, params, best_anywhere);

                                    if (! params.abort.load())
                                        ++tr.top_nodes_done;

                                    return true;
                                    })) {
                        }

                        auto overall_time = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start_time);

                        // merge results
                        {
                            std::unique_lock<std::mutex> guard(result_mutex);
                            result.merge(tr);
                            result.times.push_back(overall_time);
                            result.thread_processed_nodes.push_back(tr.processed_nodes);
                        }
                        }));
        }

        // wait until they're done, and clean up threads
        for (auto & t : threads)
            t.join();

        auto cleanup_stack = work_stack.register_thread(static_cast<int>(n_threads));
        while (void * item = cleanup_stack.pop())
            delete static_cast<QueueItem<size_> *>(item);

        return result;
    }

    template <unsigned size_>
    auto tbmcsa1(const Graph & graph, const MaxCliqueParams & params) -> MaxCliqueResult
    {
        std::vector<int> o(graph.size()); // vertex ordering
        std::iota(o.begin(), o.end(), 0);
        degree_sort(graph, o);

        // re-encode graph as a bit graph
        FixedBitGraph<size_> bit_graph;
        bit_graph.resize(graph.size());

        for (int i = 0 ; i < graph.size() ; ++i)
            for (int j = 0 ; j < graph.size() ; ++j)
                if (graph.adjacent(o[i], o[j]))
                    bit_graph.add_edge(i, j);

        // go!
        return max_clique(bit_graph, o, params);
    }
}

auto clique::tbmcsa1_max_clique(const Graph & graph, const MaxCliqueParams & params) -> MaxCliqueResult
{
    /* This is pretty horrible: in order to avoid dynamic allocation, select
     * the appropriate specialisation for our graph's size. */
    static_assert(max_graph_words == 256, "Need to update here if max_graph_size is changed.");
    if (graph.size() < bits_per_word)
        return tbmcsa1<1>(graph, params);
    else if (graph.size() < 2 * bits_per_word)
        return tbmcsa1<2>(graph, params);
    else if (graph.size() < 4 * bits_per_word)
        return tbmcsa1<4>(graph, params);
    else if (graph.size() < 8 * bits_per_word)
        return tbmcsa1<8>(graph, params);
    else if (graph.size() < 16 * bits_per_word)
        return tbmcsa1<16>(graph, params);
    else if (graph.size() < 32 * bits_per_word)
        return tbmcsa1<32>(graph, params);
    else if (graph.size() < 64 * bits_per_word)
        return tbmcsa1<64>(graph, params);
    else if (graph.size() < 128 * bits_per_word)
        return tbmcsa1<128>(graph, params);
    else if (graph.size() < 256 * bits_per_word)
        return tbmcsa1<256>(graph, params);
    else
        throw GraphTooBig();
}
