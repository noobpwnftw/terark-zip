#include <terark/util/concurrent_cow.hpp>
#include <terark/mempool_lock_free.hpp>

#include <atomic>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <random>
#include <thread>
#include <vector>

namespace tcow = terark::cow_mman;

class CowMemPoolLike final {
public:
    static const size_t mem_alloc_fail = size_t(-1) / tcow::AlignSize;

    CowMemPoolLike(size_t fastbin_max_size, size_t reserve_bytes)
        : pool_(fastbin_max_size) {
        pool_.reserve(reserve_bytes);
    }

    size_t mem_alloc(size_t size) {
        const size_t pos = pool_.alloc(size);
        if (pos == size_t(-1)) {
            return mem_alloc_fail;
        }
        return pos / tcow::AlignSize;
    }

    void mem_free(size_t loc, size_t size) {
        pool_.sfree(loc * tcow::AlignSize, size);
    }

    void* mem_get(size_t loc) {
        return static_cast<void*>(pool_.data() + loc * tcow::AlignSize);
    }

private:
    terark::MemPool_LockFree<tcow::AlignSize> pool_;
};

struct OwnedBlock {
    size_t id;
    size_t len;
    uint32_t sig;
};

static uint32_t block_sig(size_t tid, size_t seq) {
    return uint32_t(0x9e3779b9u ^ (tid * 0x85ebca6bu) ^ (seq * 0xc2b2ae35u));
}

int main(int argc, char** argv) {
    const size_t n_threads = argc > 1 ? size_t(std::strtoul(argv[1], nullptr, 10)) : 8;
    const size_t iters_per_thread = argc > 2 ? size_t(std::strtoul(argv[2], nullptr, 10)) : 50000;
    const size_t reserve_mb = argc > 3 ? size_t(std::strtoul(argv[3], nullptr, 10)) : 256;

    CowMemPoolLike pool(/*fastbin max*/4096, reserve_mb << 20);
    std::atomic<size_t> alloc_ok{0}, alloc_fail{0}, frees{0}, verify_fail{0};

    auto worker = [&](size_t tid) {
        std::mt19937_64 rng(0xC0FFEEULL + 1315423911ULL * tid);
        std::vector<OwnedBlock> owned;
        owned.reserve(1024);

        for (size_t i = 0; i < iters_per_thread; ++i) {
            const bool do_alloc = owned.empty() || ((rng() % 100) < 65 && owned.size() < 1024);
            if (do_alloc) {
                const size_t words = (rng() % 256) + 1;
                const size_t len = words * tcow::AlignSize;
                const size_t id = pool.mem_alloc(len);
                if (id == CowMemPoolLike::mem_alloc_fail) {
                    alloc_fail.fetch_add(1, std::memory_order_relaxed);
                    continue;
                }
                uint32_t* p = static_cast<uint32_t*>(pool.mem_get(id));
                const uint32_t sig = block_sig(tid, i);
                for (size_t w = 0; w < words; ++w) {
                    p[w] = sig ^ uint32_t(w);
                }
                owned.push_back({id, len, sig});
                alloc_ok.fetch_add(1, std::memory_order_relaxed);
            } else {
                const size_t idx = size_t(rng() % owned.size());
                OwnedBlock b = owned[idx];
                owned[idx] = owned.back();
                owned.pop_back();

                uint32_t* p = static_cast<uint32_t*>(pool.mem_get(b.id));
                const size_t words = b.len / tcow::AlignSize;
                bool ok = true;
                for (size_t w = 0; w < words; ++w) {
                    if (p[w] != (b.sig ^ uint32_t(w))) {
                        ok = false;
                        break;
                    }
                }
                if (!ok) {
                    verify_fail.fetch_add(1, std::memory_order_relaxed);
                }
                pool.mem_free(b.id, b.len);
                frees.fetch_add(1, std::memory_order_relaxed);
            }
        }

        for (const OwnedBlock& b : owned) {
            uint32_t* p = static_cast<uint32_t*>(pool.mem_get(b.id));
            const size_t words = b.len / tcow::AlignSize;
            bool ok = true;
            for (size_t w = 0; w < words; ++w) {
                if (p[w] != (b.sig ^ uint32_t(w))) {
                    ok = false;
                    break;
                }
            }
            if (!ok) {
                verify_fail.fetch_add(1, std::memory_order_relaxed);
            }
            pool.mem_free(b.id, b.len);
            frees.fetch_add(1, std::memory_order_relaxed);
        }
    };

    std::vector<std::thread> threads;
    threads.reserve(n_threads);
    for (size_t t = 0; t < n_threads; ++t) {
        threads.emplace_back(worker, t);
    }
    for (auto& t : threads) {
        t.join();
    }

    std::cout
        << "threads=" << n_threads
        << " iters/thread=" << iters_per_thread
        << " alloc_ok=" << alloc_ok.load(std::memory_order_relaxed)
        << " alloc_fail=" << alloc_fail.load(std::memory_order_relaxed)
        << " frees=" << frees.load(std::memory_order_relaxed)
        << " verify_fail=" << verify_fail.load(std::memory_order_relaxed)
        << "\n";

    if (verify_fail.load(std::memory_order_relaxed) != 0) {
        std::cerr << "CowMemPool test FAILED\n";
        return 2;
    }
    std::cout << "CowMemPool test passed\n";
    return 0;
}
