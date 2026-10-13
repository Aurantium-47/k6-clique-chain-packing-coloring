// Independent checker written from the report's mathematical specification.
// No search, sampling, first-step restriction, or template restriction is used.
// MSVC: cl /nologo /O2 /EHsc /std:c++17 independent_rank_check.cpp
// Formal: independent_rank_check.exe full_fast50m.bin
// Tests:  independent_rank_check.exe --self-test
// Small certificates: --fixture ROOT_KEY ROOT_RANK FILE (never a formal PASS).
// SHA-256 is intentionally checked by the enclosing audit, not by this program.
#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

using U64 = std::uint64_t;
constexpr U64 KEY_MASK = (U64(1) << 44) - 1;
constexpr U64 STATE_COUNT = 9336040560000ULL;
constexpr U64 FORMAL_RECORDS = 16646562;
constexpr U64 FORMAL_BYTES = 133172496;
constexpr int HORIZON = 72;

struct Requirement {
    bool formal = true;
    U64 root_key = 1;
    unsigned root_rank = 75;
};

unsigned rank_of(U64 w) { return unsigned(w >> 44); }
U64 key_of(U64 w) { return w & KEY_MASK; }
void require(bool b, const char* why) {
    if (!b) throw std::runtime_error(why);
}

std::vector<U64> parse(std::istream& in, U64 bytes, const Requirement& req) {
    require(bytes != 0 && bytes % 8 == 0, "empty file or incomplete 8-byte record");
    if (req.formal) require(bytes == FORMAL_BYTES, "wrong formal certificate size");
    const U64 count = bytes / 8;
    require(count <= STATE_COUNT, "more records than states");
    require(count <= std::numeric_limits<std::size_t>::max() / sizeof(U64),
            "record allocation size overflow");
    std::vector<U64> records(static_cast<std::size_t>(count));
    std::array<unsigned char, 65536> block{};
    U64 previous = 0;
    bool found_root = false;
    std::size_t pos = 0;
    while (pos < records.size()) {
        const std::size_t n = std::min(block.size() / 8, records.size() - pos);
        in.read(reinterpret_cast<char*>(block.data()), std::streamsize(n * 8));
        require(in.gcount() == std::streamsize(n * 8), "short read");
        for (std::size_t j = 0; j < n; ++j) {
            U64 w = 0;
            for (unsigned k = 0; k < 8; ++k)
                w |= U64(block[8 * j + k]) << (8 * k); // Explicit little endian.
            const U64 key = key_of(w);
            if (key == 0 || key > STATE_COUNT)
                throw std::runtime_error("invalid state key at record " + std::to_string(pos + j));
            if (key <= previous)
                throw std::runtime_error("keys not strictly increasing at record " + std::to_string(pos + j));
            if (rank_of(w) == 0)
                throw std::runtime_error("zero rank at record " + std::to_string(pos + j));
            if (key == req.root_key) {
                require(rank_of(w) == req.root_rank, "wrong root rank");
                found_root = true;
            }
            records[pos + j] = w;
            previous = key;
        }
        pos += n;
    }
    require(in.peek() == std::char_traits<char>::eof(), "trailing data");
    require(found_root, "required root absent");
    return records;
}

U64 hash_key(U64 x) {
    // Unsigned wraparound in this hash is deliberate; it is not state arithmetic.
    x ^= x >> 30; x *= 0xbf58476d1ce4e5b9ULL;
    x ^= x >> 27; x *= 0x94d049bb133111ebULL;
    return x ^ (x >> 31);
}

class RankTable {
    std::vector<U64> slots;
    std::size_t mask;
public:
    explicit RankTable(const std::vector<U64>& records) {
        std::size_t n = 2;
        while (records.size() > n / 2) {
            require(n <= std::numeric_limits<std::size_t>::max() / 2,
                    "hash slot count overflow");
            n *= 2;
        }
        require(n <= std::numeric_limits<std::size_t>::max() / sizeof(U64),
                "hash allocation size overflow");
        slots.assign(n, 0);
        mask = n - 1;
        for (U64 w : records) {
            std::size_t p = std::size_t(hash_key(key_of(w))) & mask;
            while (slots[p] != 0) p = (p + 1) & mask;
            slots[p] = w;
        }
    }
    unsigned get(U64 key) const {
        std::size_t p = std::size_t(hash_key(key)) & mask;
        while (slots[p] != 0) {
            if (key_of(slots[p]) == key) return rank_of(slots[p]);
            p = (p + 1) & mask;
        }
        return 0;
    }
};

int local_id(int a, int b) { return b * (b + 1) / 2 + a; }
int capacity(int q, int h, int r) {
    return h <= r ? 0 : 1 + (h - 1 - r) / q;
}

struct Local {
    int a = 0, b = 0, zeros = 0;
    U64 base = 0, first_delta = 0, second_delta = 0;
    std::array<unsigned char, HORIZON + 1> cap{};
};

struct Tables {
    std::array<int, 10> radix{};
    std::array<U64, 10> multiplier{};
    std::array<std::array<Local, 66>, 10> local{};
    std::array<std::uint32_t, HORIZON + 1> divides{};
    std::array<unsigned char, 1024> pop{};
    Tables() {
        U64 mult = 1;
        for (int g = 0; g < 10; ++g) {
            const int q = g + 2;
            radix[g] = q * (q + 1) / 2;
            multiplier[g] = mult;
            for (int b = 0; b < q; ++b) for (int a = 0; a <= b; ++a) {
                Local& l = local[g][local_id(a, b)];
                l.a = a; l.b = b; l.zeros = (a == 0) + (b == 0);
                const int aa = std::max(0, a - 1), bb = std::max(0, b - 1);
                const int base_id = local_id(aa, bb);
                l.base = U64(base_id) * mult;
                if (l.zeros >= 1)
                    l.first_delta = U64(local_id(bb, q - 1) - base_id) * mult;
                if (l.zeros == 2)
                    l.second_delta = U64(local_id(q - 1, q - 1) - local_id(0, q - 1)) * mult;
                for (int h = 1; h <= HORIZON; ++h)
                    l.cap[h] = static_cast<unsigned char>(capacity(q, h, aa) + capacity(q, h, bb));
            }
            mult *= U64(radix[g]);
            for (int h = q; h <= HORIZON; h += q)
                divides[h] |= std::uint32_t(3) << (2 * g);
        }
        require(mult == STATE_COUNT && mult < KEY_MASK, "incorrect state-STATE_COUNT constants");
        for (unsigned i = 1; i < pop.size(); ++i) pop[i] = pop[i >> 1] + (i & 1);
    }
    int popcount(std::uint32_t x) const { return pop[x & 1023] + pop[x >> 10]; }
};

struct Task { int group; std::uint32_t bit; U64 first, second; };
struct Horizon { int h, threshold; std::uint32_t divides; };
struct Counts {
    U64 states = 0, terminal = 0, choices = 0, by_capacity = 0, by_rank = 0;
    unsigned max_rank = 0;
    int largest_h = 0;
};

template<class F> void all_four(int n, F&& f) {
    for (int a = 0; a < n - 3; ++a)
        for (int b = a + 1; b < n - 2; ++b)
            for (int c = b + 1; c < n - 1; ++c)
                for (int d = c + 1; d < n; ++d) f(a, b, c, d);
}

Counts verify(const std::vector<U64>& records, const Tables& tab, bool progress) {
    const RankTable lookup(records);
    Counts out;
    for (U64 w : records) {
        const unsigned rank = rank_of(w);
        out.max_rank = std::max(out.max_rank, rank);
        U64 remaining = key_of(w) - 1, base_id = 0;
        std::array<const Local*, 10> locals{};
        std::array<Task, 20> available{};
        int n = 0;
        for (int g = 0; g < 10; ++g) {
            const int u = int(remaining % U64(tab.radix[g]));
            remaining /= U64(tab.radix[g]);
            const Local& l = tab.local[g][u];
            locals[g] = &l;
            base_id += l.base;
            for (int j = 0; j < l.zeros; ++j)
                available[n++] = {g, std::uint32_t(1) << (2 * g + j), l.first_delta, l.second_delta};
        }
        require(remaining == 0, "state decode overflow");
        if (n < 4) ++out.terminal; // No successor is a legitimate base case, at any positive rank.
        std::array<Horizon, HORIZON> horizons{};
        int nh = 0;
        if (n >= 4) {
            const int limit = int(std::min<unsigned>(HORIZON, rank - 1));
            for (int h = 1; h <= limit; ++h) {
                int cbase = 0;
                for (const Local* l : locals) cbase += l->cap[h];
                // C_h(successor) = C_h(base) - 4 + selected divisors of h.
                // If this threshold is nonpositive, no choice can be rejected.
                const int threshold = 4 * h + 4 - cbase;
                if (threshold > 0) horizons[nh++] = {h, threshold, tab.divides[h]};
            }
        }
        all_four(n, [&](int ai, int bi, int ci, int di) {
            ++out.choices;
            const Task& a = available[ai]; const Task& b = available[bi];
            const Task& c = available[ci]; const Task& d = available[di];
            const std::uint32_t mask = a.bit | b.bit | c.bit | d.bit;
            for (int j = 0; j < nh; ++j) {
                const Horizon& hh = horizons[j];
                if (tab.popcount(mask & hh.divides) < hh.threshold) {
                    ++out.by_capacity;
                    out.largest_h = std::max(out.largest_h, hh.h);
                    return;
                }
            }
            // Available tasks are ordered by group; a second choice in one group
            // must be adjacent and adds the increment from one selected to two.
            const U64 successor = base_id + a.first
                + (a.group == b.group ? b.second : b.first)
                + (b.group == c.group ? c.second : c.first)
                + (c.group == d.group ? d.second : d.first);
            require(successor < STATE_COUNT, "successor encoding outside STATE_COUNT");
            const unsigned next_rank = lookup.get(successor + 1);
            if (next_rank == 0 || next_rank >= rank) {
                throw std::runtime_error("uncovered transition: state_key=" + std::to_string(key_of(w))
                    + " rank=" + std::to_string(rank) + " selected_label_mask=" + std::to_string(mask)
                    + " successor_key=" + std::to_string(successor + 1)
                    + " successor_rank=" + std::to_string(next_rank));
            }
            ++out.by_rank;
        });
        ++out.states;
        if (progress && out.states % 1000000 == 0)
            std::cerr << "verified_states=" << out.states << " choices=" << out.choices << '\n';
    }
    require(out.choices == out.by_capacity + out.by_rank, "coverage accounting mismatch");
    return out;
}

std::string packed(std::initializer_list<U64> words) {
    std::string bytes;
    for (U64 w : words) for (int j = 0; j < 8; ++j) bytes.push_back(char((w >> (8 * j)) & 255));
    return bytes;
}

void self_test(const Tables& tab) {
    for (int q = 2; q <= 11; ++q) for (int h = 1; h <= HORIZON; ++h) {
        for (int r = 0; r < q; ++r) {
            int explicit_count = 0;
            for (int t = r; t < h; t += q) ++explicit_count;
            require(explicit_count == capacity(q, h, r), "capacity enumeration test");
        }
        require(capacity(q, h, 0) - capacity(q, h, q - 1) == (h % q != 0), "capacity identity test");
    }
    for (int g = 0; g < 10; ++g) {
        const int q = g + 2;
        for (int u = 0; u < tab.radix[g]; ++u) {
            const Local& l = tab.local[g][u];
            require(local_id(l.a, l.b) == u, "local decode test");
            for (int pick = 0; pick <= l.zeros; ++pick) {
                int a = std::max(l.a - 1, 0), b = std::max(l.b - 1, 0);
                if (pick >= 1) a = q - 1;
                if (pick == 2) b = q - 1;
                if (a > b) std::swap(a, b);
                U64 optimized = l.base;
                if (pick >= 1) optimized += l.first_delta;
                if (pick == 2) optimized += l.second_delta;
                require(optimized == U64(local_id(a, b)) * tab.multiplier[g], "successor delta test");
            }
        }
    }
    for (int n = 0; n <= 20; ++n) {
        U64 count = 0;
        all_four(n, [&](int a, int b, int c, int d) {
            require(a < b && b < c && c < d && d < n, "combination order test"); ++count;
        });
        const U64 expected = n < 4 ? 0 : U64(n) * (n - 1) * (n - 2) * (n - 3) / 24;
        require(count == expected, "combination count test");
    }
    const U64 root75 = (U64(75) << 44) | 1;
    const Requirement fixture{false, 1, 75};
    auto read_fixture = [&](const std::string& data, Requirement req) {
        std::istringstream in(data, std::ios::in | std::ios::binary);
        return parse(in, U64(data.size()), req);
    };
    auto reject_parse = [&](const std::string& data, Requirement req) {
        bool rejected = false;
        try { (void)read_fixture(data, req); } catch (const std::runtime_error&) { rejected = true; }
        require(rejected, "invalid parser fixture was accepted");
    };
    require(read_fixture(packed({root75}), fixture).size() == 1, "little-endian parser test");
    reject_parse("", fixture);
    reject_parse(packed({root75}) + "x", fixture);
    reject_parse(packed({root75, root75}), fixture);
    reject_parse(packed({root75 + 1, root75}), fixture);
    reject_parse(packed({U64(75) << 44}), fixture);
    reject_parse(packed({root75, (U64(1) << 44) | (STATE_COUNT + 1)}), fixture);
    reject_parse(packed({1}), fixture);
    reject_parse(packed({(U64(1) << 44) | 1}), fixture);
    reject_parse(packed({root75 + 1}), fixture);
    reject_parse(packed({root75}), Requirement{});
    // A rank-one state with no available tasks is a valid finite proof.
    const auto terminal = read_fixture(packed({(U64(1) << 44) | STATE_COUNT}), {false, STATE_COUNT, 1});
    const Counts terminal_counts = verify(terminal, tab, false);
    require(terminal_counts.terminal == 1 && terminal_counts.choices == 0, "terminal base-case test");
    // Exactly the two q=2 and two q=3 tasks are available. Its only successor
    // has no available task, so horizon one must cover this rank-two state.
    const U64 one_choice_key = STATE_COUNT - 17;
    const auto one_choice = read_fixture(packed({(U64(2) << 44) | one_choice_key}), {false, one_choice_key, 2});
    const Counts one_choice_counts = verify(one_choice, tab, false);
    require(one_choice_counts.choices == 1 && one_choice_counts.by_capacity == 1
            && one_choice_counts.largest_h == 1, "capacity-covered transition test");
    // Root rank one cannot cover even its first legal successor.
    bool rejected = false;
    try {
        const auto bad = read_fixture(packed({(U64(1) << 44) | 1}), {false, 1, 1});
        (void)verify(bad, tab, false);
    } catch (const std::runtime_error&) { rejected = true; }
    require(rejected, "rank-one nonterminal accepted");
    // The hash table must return exact ranks and distinguish absent keys.
    const RankTable hash_test({root75, (U64(2) << 44) | STATE_COUNT});
    require(hash_test.get(1) == 75 && hash_test.get(STATE_COUNT) == 2 && hash_test.get(2) == 0, "hash lookup test");
    std::cout << "{\"status\":\"PASS_SELF_TEST_ONLY\",\"capacity_formula\":true,\"capacity_identity\":true,"
                 "\"local_transitions\":true,\"all_combination_counts\":true,\"invalid_parser_cases\":10,"
                 "\"terminal_rank_one_accepted\":true,\"capacity_transition_accepted\":true,"
                 "\"nonterminal_rank_one_rejected\":true}\n";
}

U64 number(const char* s) {
    const std::string text(s);
    require(!text.empty() && text.find_first_not_of("0123456789") == std::string::npos, "invalid integer argument");
    std::size_t used = 0;
    const U64 result = std::stoull(text, &used);
    require(used == text.size(), "invalid integer argument");
    return result;
}

int main(int argc, char** argv) {
    try {
        Tables tab;
        if (argc == 2 && std::string(argv[1]) == "--self-test") { self_test(tab); return 0; }
        Requirement req;
        const char* filename = nullptr;
        if (argc == 2) filename = argv[1];
        else if (argc == 5 && std::string(argv[1]) == "--fixture") {
            req.formal = false; req.root_key = number(argv[2]);
            const U64 r = number(argv[3]);
            require(req.root_key >= 1 && req.root_key <= STATE_COUNT && r > 0 && r < (U64(1) << 20), "invalid fixture root");
            req.root_rank = unsigned(r); filename = argv[4];
        } else throw std::runtime_error("usage: checker FILE | --self-test | --fixture ROOT_KEY ROOT_RANK FILE");
        const auto start = std::chrono::steady_clock::now();
        std::ifstream in(filename, std::ios::binary | std::ios::ate);
        require(bool(in), "cannot open certificate");
        const auto end = in.tellg();
        require(end >= std::streampos(0), "cannot obtain certificate size");
        const U64 bytes = static_cast<U64>(static_cast<std::streamoff>(end));
        in.seekg(0);
        const auto records = parse(in, bytes, req);
        if (req.formal) require(records.size() == FORMAL_RECORDS, "wrong formal record count");
        const Counts out = verify(records, tab, true);
        const double seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
        std::cout << "{\"status\":\"" << (req.formal ? "PASS" : "PASS_FIXTURE_ONLY")
                  << "\",\"formal_metadata\":" << (req.formal ? "true" : "false")
                  << ",\"first_step_restricted\":false,\"certificate_bytes\":" << bytes
                  << ",\"root_key\":" << req.root_key << ",\"root_rank\":" << req.root_rank
                  << ",\"max_rank\":" << out.max_rank << ",\"verified_states\":" << out.states
                  << ",\"terminal_states\":" << out.terminal << ",\"explicitly_enumerated_choices\":" << out.choices
                  << ",\"individual_capacity_choices\":" << out.by_capacity
                  << ",\"decreasing_rank_choices\":" << out.by_rank
                  << ",\"largest_capacity_horizon_used\":" << out.largest_h
                  << ",\"seconds\":" << seconds << "}\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "FAIL: " << e.what() << '\n';
        std::cout << "{\"status\":\"FAIL\"}\n";
        return 1;
    }
}
