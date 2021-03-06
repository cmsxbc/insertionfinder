#pragma once
#include <cstddef>
#include <cstdint>
#include <array>
#include <atomic>
#include <limits>
#include <utility>
#include <vector>
#include <insertionfinder/algorithm.hpp>
#include <insertionfinder/case.hpp>
#include <insertionfinder/cube.hpp>
#include <insertionfinder/twist.hpp>

namespace InsertionFinder {
    namespace FinderStatus {
        constexpr std::byte success {0};
        constexpr std::byte parity_algorithms_needed {1};
        constexpr std::byte corner_cycle_algorithms_needed {2};
        constexpr std::byte edge_cycle_algorithms_needed {4};
        constexpr std::byte center_algorithms_needed {8};
        constexpr std::byte full = parity_algorithms_needed
            | corner_cycle_algorithms_needed | edge_cycle_algorithms_needed
            | center_algorithms_needed;
    };

    class Finder {
    protected:
        struct CycleStatus {
            bool parity;
            std::int8_t corner_cycles;
            std::int8_t edge_cycles;
            std::int8_t placement;
            CycleStatus() = default;
            CycleStatus(bool parity, int corner_cycles, int edge_cycles, Rotation placement):
                parity(parity), corner_cycles(corner_cycles), edge_cycles(edge_cycles), placement(placement) {}
        };
    public:
        struct Insertion {
            Algorithm skeleton;
            std::size_t insert_place;
            const Algorithm* insertion;
            explicit Insertion(
                const Algorithm& skeleton,
                std::size_t insert_place = 0,
                const Algorithm* insertion = nullptr
            ): skeleton(skeleton), insert_place(insert_place), insertion(insertion) {}
            explicit Insertion(
                Algorithm&& skeleton,
                std::size_t insert_place = 0,
                const Algorithm* insertion = nullptr
            ): skeleton(std::move(skeleton)), insert_place(insert_place), insertion(insertion) {}
        };
        struct Solution {
            std::vector<Insertion> insertions;
            std::size_t cancellation = 0;
            Solution(const std::vector<Insertion>& insertions): insertions(insertions) {}
            Solution(std::vector<Insertion>&& insertions): insertions(std::move(insertions)) {}
        };
        struct Result {
            std::byte status = FinderStatus::full;
            std::int64_t duration;
        };
        struct SearchParams {
            std::size_t search_target;
            double parity_multiplier;
            std::size_t max_threads;
        };
    protected:
        const Algorithm scramble;
        const std::vector<Algorithm> skeletons;
        const std::vector<Case>& cases;
        std::atomic<std::size_t> fewest_moves = std::numeric_limits<std::size_t>::max();
        std::vector<Solution> solutions;
        Result result;
        int parity_multiplier = 3;
        bool verbose = false;
    protected:
        int corner_cycle_index[6 * 24 * 24];
        int edge_cycle_index[10 * 24 * 24];
        int center_index[24];
        bool change_parity = false;
        bool change_corner = false;
        bool change_edge = false;
        bool change_center = false;
        const Cube scramble_cube;
        const Cube inverse_scramble_cube;
    public:
        Finder(const Algorithm& scramble, const std::vector<Algorithm>& skeletons, const std::vector<Case>& cases):
            scramble(scramble), skeletons(skeletons), cases(cases),
            scramble_cube(Cube() * scramble), inverse_scramble_cube(Cube::inverse(this->scramble_cube)) {
            this->init();
        }
        Finder(const Algorithm& scramble, std::vector<Algorithm>&& skeletons, const std::vector<Case>& cases):
            scramble(scramble), skeletons(std::move(skeletons)), cases(cases),
            scramble_cube(Cube() * scramble), inverse_scramble_cube(Cube::inverse(this->scramble_cube)) {
            this->init();
        }
        Finder(const Algorithm& scramble, const Algorithm& skeleton, const std::vector<Case>& cases):
            scramble(scramble), skeletons({skeleton}), cases(cases),
            scramble_cube(Cube() * scramble), inverse_scramble_cube(Cube::inverse(this->scramble_cube)) {
            this->init();
        }
        virtual ~Finder() = default;
    public:
        void search(const SearchParams& params);
    protected:
        void init();
        virtual void search_core(const SearchParams& params) = 0;
    public:
        std::size_t get_fewest_moves() const noexcept {
            return this->fewest_moves;
        }
        const std::vector<Solution>& get_solutions() const noexcept {
            return this->solutions;
        }
        Result get_result() const noexcept {
            return this->result;
        }
    public:
        void set_verbose(bool verbose = true) noexcept {
            this->verbose = verbose;
        }
    public:
        int get_total_cycles(bool parity, int corner_cycles, int edge_cycles, Rotation placement) {
            int center_cycles = Cube::center_cycles[placement];
            return (center_cycles > 1 ? 0 : parity * this->parity_multiplier)
                + (corner_cycles + edge_cycles + center_cycles) * 2;
        }
    };
};
