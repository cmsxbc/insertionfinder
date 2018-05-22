#pragma once
#include <cstdint>
#include <array>
#include <vector>
#include <utility>
#include <algorithm.hpp>
#include <case.hpp>
#include <cube.hpp>

namespace InsertionFinder {
    class Finder {
    protected:
        struct CycleStatus {
            bool parity;
            int corner_cycles;
            int edge_cycles;
            int placement;
        };
        struct Insertion {
            Algorithm skeleton;
            std::size_t insert_place;
            const Algorithm* insertion;
        };
        struct Solution {
            std::vector<Insertion> insertions;
            std::size_t cancellation;
        };
        enum class Status {
            SUCCESS,
            SUCCESS_SOLVED,
            FAILURE_PARITY_ALGORITHMS_NEEDED,
            FAILURE_CORNER_CYCLE_ALGORITHMS_NEEDED,
            FAILURE_EDGE_CYCLE_ALGORITHMS_NEEDED,
            FAILURE_CENTER_ALGORITHMS_NEEDED
        };
        struct Result {
            Status status;
            std::int64_t duration;
        };
    protected:
        const std::vector<Case> cases;
        const Algorithm scramble;
        const Algorithm skeleton;
        std::size_t fewest_moves;
        std::vector<Solution> solutions;
        Result result;
    protected:
        std::array<int, 7 * 24 * 11 * 24> parity_index;
        std::array<int, 6 * 24 * 24> corner_cycle_index;
        std::array<int, 10 * 24 * 24> edge_cycle_index;
        std::array<int, 24> center_index;
        bool change_parity;
        bool change_corner;
        bool change_edge;
        bool change_center;
        Cube scramble_cube;
        Cube inverse_scramble_cube;
    public:
        Finder(
            const Algorithm& scramble, const Algorithm& skeleton,
            const std::vector<Case>& cases
        );
    public:
        void search(std::size_t max_threads);
        virtual Status search_core(std::size_t max_threads) = 0;
    };
};