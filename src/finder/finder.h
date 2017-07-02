#pragma once
#include <stddef.h>
#include "../algorithm.h"
#include "../cube.h"
#include "../formula.h"

typedef struct Finder Finder;
typedef struct FinderWorker FinderWorker;
typedef struct Insertion Insertion;

struct Finder {
    size_t algorithm_count;
    const Algorithm** algorithm_list;
    int corner_cycle_index[6 * 24 * 24];
    int edge_cycle_index[10 * 24 * 24];
    bool change_corner;
    bool change_edge;
    Formula scramble;
    Cube scramble_cube;
    Cube inverse_scramble_cube;
    size_t fewest_moves;
    size_t solution_count;
    size_t solution_capacity;
    FinderWorker* solution_list;
};

struct FinderWorker {
    Finder* finder;
    size_t depth;
    size_t solving_step_capacity;
    Insertion* solving_step;
};

struct Insertion {
    Formula partial_solution;
    size_t insert_place;
    const Formula *insertion;
};

Finder* FinderConstruct(
    Finder* finder,
    size_t algorithm_count,
    const Algorithm** algorithm_list,
    const Formula* scramble
);
void FinderDestroy(Finder* finder);

FinderWorker* FinderWorkerConstruct(FinderWorker* worker, Finder* finder);
void FinderWorkerDestroy(FinderWorker* worker);

void FinderWorkerSearch(
    FinderWorker* worker,
    int corner_cycles, int edge_cycles,
    size_t begin, size_t end
);

Insertion* InsertionConstruct(Insertion* insertion, const Formula* formula);
void InsertionDestroy(Insertion* insertion);