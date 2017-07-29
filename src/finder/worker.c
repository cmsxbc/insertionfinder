#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include "../algorithm/algorithm.h"
#include "../cube/cube.h"
#include "../formula/formula.h"
#include "../utils/memory.h"
#include "finder.h"


typedef FinderWorker Worker;

static void SearchLastParity(Worker* worker, size_t begin, size_t end);
static void SearchLastCornerCycle(Worker* worker, size_t begin, size_t end);
static void SearchLastEdgeCycle(Worker* worker, size_t begin, size_t end);

static void TryInsertion(
    Worker* worker,
    size_t insert_place,
    const Cube* state,
    bool swapped,
    bool parity, int corner_cycles, int edge_cycles
);
static void TryLastInsertion(Worker* worker, size_t insert_place, int index);

static void PushInsertion(Worker* worker, const Formula* insert_result);
static void PopInsertion(Worker* worker);

static void SolutionFound(
    Worker* worker,
    size_t insert_place,
    const Algorithm* algorithm
);
static void UpdateFewestMoves(Worker* worker, size_t moves);

static bool BitCountLessThan2(uint32_t n);

static bool NotSearched(
    const Formula* formula,
    size_t index,
    size_t new_begin,
    bool swappable
);
static bool ContinueSearching(const Worker* worker, const Formula* formula);


void FinderWorkerConstruct(
    Worker* worker,
    Finder* finder,
    const Formula* skeleton
) {
    worker->finder = finder;
    worker->depth = 0;
    worker->solving_step_capacity = 8;
    worker->solving_step = MALLOC(Insertion, worker->solving_step_capacity);
    FormulaDuplicate(&worker->solving_step[0].skeleton, skeleton);
}

void FinderWorkerDestroy(Worker* worker) {
    Insertion* begin = worker->solving_step;
    Insertion* end = begin + worker->depth + 1;
    for (Insertion* p = begin; p < end; ++p) {
        FormulaDestroy(&p->skeleton);
    }
    free(worker->solving_step);
}


void FinderWorkerSearch(
    Worker* worker,
    bool parity, int corner_cycles, int edge_cycles,
    size_t begin, size_t end
) {
    if (parity && corner_cycles == 0 && edge_cycles == 0) {
        SearchLastParity(worker, begin, end);
        return;
    } else if (!parity && corner_cycles == 1 && edge_cycles == 0) {
        SearchLastCornerCycle(worker, begin, end);
        return;
    } else if (!parity && corner_cycles == 0 && edge_cycles == 1) {
        SearchLastEdgeCycle(worker, begin, end);
        return;
    }

    const Finder* finder = worker->finder;
    Cube state = identity_cube;
    for (size_t insert_place = begin; insert_place <= end; ++insert_place) {
        Formula* skeleton = &worker->solving_step[worker->depth].skeleton;
        if (insert_place == begin) {
            CubeRangeTwistFormula(
                &state,
                skeleton,
                insert_place, skeleton->length,
                finder->change_corner, finder->change_edge,
                false
            );
            CubeTwistCube(
                &state,
                &finder->scramble_cube,
                finder->change_corner, finder->change_edge
            );
            CubeRangeTwistFormula(
                &state,
                skeleton,
                0, insert_place,
                finder->change_corner, finder->change_edge,
                false
            );
        } else {
            int move = skeleton->move[insert_place - 1];
            CubeTwistMoveBefore(
                &state,
                inverse_move_table[move],
                finder->change_corner, finder->change_edge
            );
            CubeTwistMove(
                &state,
                move,
                finder->change_corner, finder->change_edge
            );
        }
        TryInsertion(
            worker,
            insert_place,
            &state,
            false,
            parity, corner_cycles, edge_cycles
        );

        skeleton = &worker->solving_step[worker->depth].skeleton;
        if (FormulaSwappable(skeleton, insert_place)) {
            FormulaSwapAdjacent(skeleton, insert_place);
            const int moves[2] = {
                skeleton->move[insert_place - 1],
                skeleton->move[insert_place]
            };
            Cube swapped_state = identity_cube;
            CubeTwistMove(
                &swapped_state,
                moves[1],
                finder->change_corner, finder->change_edge
            );
            CubeTwistMove(
                &swapped_state,
                inverse_move_table[moves[0]],
                finder->change_corner, finder->change_edge
            );
            CubeTwistCube(
                &swapped_state,
                &state,
                finder->change_corner, finder->change_edge
            );
            CubeTwistMove(
                &swapped_state,
                moves[0],
                finder->change_corner, finder->change_edge
            );
            CubeTwistMove(
                &swapped_state,
                inverse_move_table[moves[1]],
                finder->change_corner, finder->change_edge
            );
            TryInsertion(
                worker,
                insert_place,
                &swapped_state,
                true,
                parity, corner_cycles, edge_cycles
            );
            FormulaSwapAdjacent(
                &worker->solving_step[worker->depth].skeleton,
                insert_place
            );
        }
    }
}


void SearchLastParity(Worker* worker, size_t begin, size_t end) {
    const Finder* finder = worker->finder;
    for (size_t insert_place = begin; insert_place <= end; ++insert_place) {
        Formula* skeleton = &worker->solving_step[worker->depth].skeleton;
        int index;
        if (insert_place == begin) {
            Cube state = identity_cube;
            CubeRangeTwistFormula(
                &state,
                skeleton,
                0, insert_place,
                true, true,
                true
            );
            CubeTwistCube(&state, &finder->inverse_scramble_cube, true, true);
            CubeRangeTwistFormula(
                &state,
                skeleton,
                insert_place, skeleton->length,
                true, true,
                true
            );
            index = CubeParityIndex(&state);
        } else {
            index = CubeParityNextIndex(
                index,
                skeleton->move[insert_place - 1]
            );
        }
        TryLastInsertion(
            worker,
            insert_place,
            finder->parity_index[index]
        );

        skeleton = &worker->solving_step[worker->depth].skeleton;
        if (FormulaSwappable(skeleton, insert_place)) {
            FormulaSwapAdjacent(skeleton, insert_place);
            int swapped_index = CubeParityNextIndex(
                CubeParityNextIndex(
                    index,
                    skeleton->move[insert_place - 1]
                ),
                inverse_move_table[skeleton->move[insert_place]]
            );
            TryLastInsertion(
                worker,
                insert_place,
                finder->parity_index[swapped_index]
            );
            FormulaSwapAdjacent(
                &worker->solving_step[worker->depth].skeleton,
                insert_place
            );
        }
    }
}

void SearchLastCornerCycle(Worker* worker, size_t begin, size_t end) {
    const Finder* finder = worker->finder;
    for (size_t insert_place = begin; insert_place <= end; ++insert_place) {
        Formula* skeleton = &worker->solving_step[worker->depth].skeleton;
        int index;
        if (insert_place == begin) {
            Cube state = identity_cube;
            CubeRangeTwistFormula(
                &state,
                skeleton,
                0, insert_place,
                true, false,
                true
            );
            CubeTwistCube(&state, &finder->inverse_scramble_cube, true, false);
            CubeRangeTwistFormula(
                &state,
                skeleton,
                insert_place, skeleton->length,
                true, false,
                true
            );
            index = CubeCorner3CycleIndex(&state);
        } else {
            index = CubeCornerNext3CycleIndex(
                index,
                skeleton->move[insert_place - 1]
            );
        }
        TryLastInsertion(
            worker,
            insert_place,
            finder->corner_cycle_index[index]
        );

        skeleton = &worker->solving_step[worker->depth].skeleton;
        if (FormulaSwappable(skeleton, insert_place)) {
            FormulaSwapAdjacent(skeleton, insert_place);
            int swapped_index = CubeCornerNext3CycleIndex(
                CubeCornerNext3CycleIndex(
                    index,
                    skeleton->move[insert_place - 1]
                ),
                inverse_move_table[skeleton->move[insert_place]]
            );
            TryLastInsertion(
                worker,
                insert_place,
                finder->corner_cycle_index[swapped_index]
            );
            FormulaSwapAdjacent(
                &worker->solving_step[worker->depth].skeleton,
                insert_place
            );
        }
    }
}

void SearchLastEdgeCycle(Worker* worker, size_t begin, size_t end) {
    const Finder* finder = worker->finder;
    for (size_t insert_place = begin; insert_place <= end; ++insert_place) {
        Formula* skeleton = &worker->solving_step[worker->depth].skeleton;
        int index;
        if (insert_place == begin) {
            Cube state = identity_cube;
            CubeRangeTwistFormula(
                &state,
                skeleton,
                0, insert_place,
                false, true,
                true
            );
            CubeTwistCube(&state, &finder->inverse_scramble_cube, false, true);
            CubeRangeTwistFormula(
                &state,
                skeleton,
                insert_place, skeleton->length,
                false, true,
                true
            );
            index = CubeEdge3CycleIndex(&state);
        } else {
            index = CubeEdgeNext3CycleIndex(
                index,
                skeleton->move[insert_place - 1]
            );
        }
        TryLastInsertion(worker, insert_place, finder->edge_cycle_index[index]);

        skeleton = &worker->solving_step[worker->depth].skeleton;
        if (FormulaSwappable(skeleton, insert_place)) {
            FormulaSwapAdjacent(skeleton, insert_place);
            int swapped_index = CubeEdgeNext3CycleIndex(
                CubeEdgeNext3CycleIndex(
                    index,
                    skeleton->move[insert_place - 1]
                ),
                inverse_move_table[skeleton->move[insert_place]]
            );
            TryLastInsertion(
                worker,
                insert_place,
                finder->edge_cycle_index[swapped_index]
            );
            FormulaSwapAdjacent(
                &worker->solving_step[worker->depth].skeleton,
                insert_place
            );
        }
    }
}


void TryInsertion(
    Worker* worker,
    size_t insert_place,
    const Cube* state,
    bool swapped,
    bool parity, int corner_cycles, int edge_cycles
) {
    const Finder* finder = worker->finder;
    worker->solving_step[worker->depth].insert_place = insert_place;
    uint32_t insert_place_mask[2];
    FormulaGetInsertPlaceMask(
        &worker->solving_step[worker->depth].skeleton,
        insert_place,
        insert_place_mask
    );
    uint32_t mask = CubeMask(state);
    for (size_t i = 0; i < finder->algorithm_count; ++i) {
        const Algorithm* algorithm = finder->algorithm_list[i];
        if (BitCountLessThan2(mask & algorithm->mask)) {
            continue;
        }
        bool corner_changed = algorithm->mask & 0xff000;
        bool edge_changed = algorithm->mask & 0xfff;
        Cube cube;
        if (!CubeTwistPositive(
            &cube,
            state, &algorithm->state,
            corner_changed, edge_changed
        )) {
            continue;
        }
        bool parity_new = parity ^ algorithm->parity;
        int corner_cycles_new =
            corner_changed ? CubeCornerCycles(&cube) : corner_cycles;
        int edge_cycles_new =
            edge_changed ? CubeEdgeCycles(&cube) : edge_cycles;
        if (!parity_new && corner_cycles_new == 0 && edge_cycles_new == 0) {
            SolutionFound(worker, insert_place, algorithm);
        } else if (
            corner_cycles_new + edge_cycles_new + parity_new
            < corner_cycles + edge_cycles + parity
        ) {
            for (size_t j = 0; j < algorithm->size; ++j) {
                Insertion* insertion = &worker->solving_step[worker->depth];
                const Formula* skeleton = &insertion->skeleton;
                insertion->insertion = &algorithm->formula_list[j];
                if (!FormulaInsertIsWorthy(
                    skeleton,
                    insert_place,
                    insertion->insertion,
                    insert_place_mask,
                    finder->fewest_moves
                )) {
                    continue;
                }
                Formula formula;
                size_t new_begin = FormulaInsert(
                    skeleton,
                    insert_place,
                    insertion->insertion,
                    &formula
                );
                if (
                    !NotSearched(skeleton, insert_place, new_begin, swapped)
                    || !ContinueSearching(worker, &formula)
                ) {
                    FormulaDestroy(&formula);
                    continue;
                }
                PushInsertion(worker, &formula);
                FinderWorkerSearch(
                    worker,
                    parity_new, corner_cycles_new, edge_cycles_new,
                    new_begin, formula.length
                );
                PopInsertion(worker);
            }
        }
    }
}

void TryLastInsertion(Worker* worker, size_t insert_place, int index) {
    if (index == -1) {
        return;
    }
    const Finder* finder = worker->finder;
    const Algorithm* algorithm = finder->algorithm_list[index];
    worker->solving_step[worker->depth].insert_place = insert_place;
    uint32_t insert_place_mask[2];
    FormulaGetInsertPlaceMask(
        &worker->solving_step[worker->depth].skeleton,
        insert_place,
        insert_place_mask
    );
    for (size_t i = 0; i < algorithm->size; ++i) {
        Insertion* insertion = &worker->solving_step[worker->depth];
        insertion->insertion = &algorithm->formula_list[i];
        if (!FormulaInsertIsWorthy(
            &insertion->skeleton,
            insert_place,
            insertion->insertion,
            insert_place_mask,
            finder->fewest_moves
        )) {
            continue;
        }
        PushInsertion(worker, NULL);
        UpdateFewestMoves(
            worker,
            worker->solving_step[worker->depth].skeleton.length
        );
        PopInsertion(worker);
    }
}


void PushInsertion(Worker* worker, const Formula* insert_result) {
    if (++worker->depth == worker->solving_step_capacity) {
        REALLOC(
            worker->solving_step,
            Insertion,
            worker->solving_step_capacity <<= 1
        );
    }
    Formula* formula = &worker->solving_step[worker->depth].skeleton;
    if (insert_result) {
        memcpy(formula, insert_result, sizeof(Formula));
    } else {
        const Insertion* previous = &worker->solving_step[worker->depth - 1];
        FormulaInsert(
            &previous->skeleton,
            previous->insert_place,
            previous->insertion,
            formula
        );
    }
}

void PopInsertion(Worker* worker) {
    FormulaDestroy(&worker->solving_step[worker->depth--].skeleton);
}


void SolutionFound(
    Worker* worker,
    size_t insert_place,
    const Algorithm* algorithm
) {
    const Finder* finder = worker->finder;
    worker->solving_step[worker->depth].insert_place = insert_place;
    uint32_t insert_place_mask[2];
    FormulaGetInsertPlaceMask(
        &worker->solving_step[worker->depth].skeleton,
        insert_place,
        insert_place_mask
    );
    for (size_t i = 0; i < algorithm->size; ++i) {
        Insertion* insertion = &worker->solving_step[worker->depth];
        insertion->insertion = &algorithm->formula_list[i];
        if (!FormulaInsertIsWorthy(
            &insertion->skeleton,
            insert_place,
            insertion->insertion,
            insert_place_mask,
            finder->fewest_moves
        )) {
            continue;
        }
        PushInsertion(worker, NULL);
        UpdateFewestMoves(
            worker,
            worker->solving_step[worker->depth].skeleton.length
        );
        PopInsertion(worker);
    }
}

void UpdateFewestMoves(Worker* worker, size_t moves) {
    Finder* finder = worker->finder;
    if (moves > finder->fewest_moves) {
        return;
    }
    pthread_mutex_lock(&finder->mutex);

    size_t depth = worker->depth;
    const Insertion* worker_steps = worker->solving_step;
    if (moves <= finder->fewest_moves) {
        if (moves < finder->fewest_moves) {
            finder->fewest_moves = moves;
            for (size_t i = 0; i < finder->solution_count; ++i) {
                FinderWorkerDestroy(&finder->solution_list[i]);
            }
            finder->solution_count = 0;
        }

        if (finder->solution_count == finder->solution_capacity) {
            REALLOC(
                finder->solution_list,
                Worker,
                finder->solution_capacity <<= 1
            );
        }
        Worker* answer = &finder->solution_list[finder->solution_count++];
        answer->depth = depth;
        answer->solving_step_capacity = depth + 1;
        answer->solving_step = MALLOC(Insertion, worker->solving_step_capacity);
        Insertion* answer_steps = answer->solving_step;
        for (size_t i = 0; i < depth; ++i) {
            FormulaDuplicate(
                &answer_steps[i].skeleton,
                &worker_steps[i].skeleton
            );
            answer_steps[i].insert_place = worker_steps[i].insert_place;
            answer_steps[i].insertion = worker_steps[i].insertion;
        }
        FormulaDuplicate(
            &answer_steps[depth].skeleton,
            &worker_steps[depth].skeleton
        );
    }

    pthread_mutex_unlock(&finder->mutex);
}


bool BitCountLessThan2(uint32_t n) {
    return (n & (n - 1)) == 0;
}


bool NotSearched(
    const Formula* formula,
    size_t insert_place,
    size_t new_begin,
    bool swappable
) {
    if (
        swappable || insert_place < 2
        || FormulaSwappable(formula, insert_place - 1)
    ) {
        return new_begin >= insert_place;
    } else {
        return new_begin >= insert_place - 1;
    }
}

bool ContinueSearching(const Worker* worker, const Formula* formula) {
    return formula->length <= worker->finder->fewest_moves;
}
