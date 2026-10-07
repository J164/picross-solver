#include <catch_amalgamated.hpp>
#include <picross/picross.h>
#include <utils/test_helpers.h>
#include <utils/text_io.h>

#include "binomial.h"
#include "line.h"
#include "line_alternatives.h"
#include "line_constraint.h"
#include "line_span.h"

#include <algorithm>
#include <cstddef>
#include <random>
#include <utility>
#include <vector>


namespace picross {
namespace {
    inline constexpr unsigned int LINE_INDEX = 0;

    binomial::Cache& get_binomial()
    {
        static binomial::Cache binomial;
        return binomial;
    }

    LineAlternatives::Reduction full_reduction(const LineConstraint& constraint, const Line& known_tiles)
    {
        return LineAlternatives(constraint, known_tiles, get_binomial()).full_reduction();
    }

    LineAlternatives::Reduction linear_reduction(const LineConstraint& constraint, const Line& known_tiles)
    {
        return LineAlternatives(constraint, known_tiles, get_binomial()).linear_reduction();
    }

    bool dp_matches_baseline(const InputGrid::Constraint& segments, const Line& known_tiles) {
        const LineConstraint constraint(Line::ROW, segments);
        const auto baseline = LineAlternatives(constraint, known_tiles, get_binomial()).full_reduction();
        const auto dp = LineAlternatives(constraint, known_tiles, get_binomial()).full_reduction_dp();
        
        return dp.nb_alternatives == baseline.nb_alternatives && ((baseline.nb_alternatives == 0 && dp.nb_alternatives == 0) || (baseline.reduced_line == dp.reduced_line && baseline.is_fully_reduced == dp.is_fully_reduced));
    }

    void all_constraints(std::size_t size, InputGrid::Constraint& prefix, std::vector<InputGrid::Constraint>& out) {
        out.push_back(prefix);
        const std::size_t used = prefix.empty() ? 0u : static_cast<std::size_t>(compute_min_line_size(prefix)) + 1u;
        
        for (std::size_t len = 1u; used + len <= size; len++) {
            prefix.push_back(static_cast<unsigned int>(len));
            all_constraints(size, prefix, out);
            prefix.pop_back();
        }
    }
}

TEST_CASE("full_reduction_of_a_simple_use_case", "[line_alternatives]")
{
    const LineConstraint constraint(Line::ROW, { 1, 1 });
    {
        const auto known_tiles = build_line_from("???", Line::ROW, LINE_INDEX);
        const auto reduction = full_reduction(constraint, known_tiles);
        CHECK(reduction.reduced_line == build_line_from("#.#", Line::ROW, LINE_INDEX));
        CHECK(reduction.nb_alternatives == 1);
        CHECK(reduction.is_fully_reduced);
    }
    {
        const auto known_tiles = build_line_from("????", Line::ROW, LINE_INDEX);
        const auto reduction = full_reduction(constraint, known_tiles);
        CHECK(reduction.reduced_line == build_line_from("????", Line::ROW, LINE_INDEX));
        CHECK(reduction.nb_alternatives == 3);
        CHECK(reduction.is_fully_reduced);
    }
    {
        const auto known_tiles = build_line_from("#???", Line::ROW, LINE_INDEX);
        const auto reduction = full_reduction(constraint, known_tiles);
        CHECK(known_tiles + reduction.reduced_line == build_line_from("#.??", Line::ROW, LINE_INDEX));
        CHECK(reduction.nb_alternatives == 2);
        CHECK(reduction.is_fully_reduced);
    }
    {
        const auto known_tiles = build_line_from("#..?", Line::ROW, LINE_INDEX);
        const auto reduction = full_reduction(constraint, known_tiles);
        CHECK(known_tiles + reduction.reduced_line == build_line_from("#..#", Line::ROW, LINE_INDEX));
        CHECK(reduction.nb_alternatives == 1);
    }
    {
        const auto known_tiles = build_line_from("??#?", Line::ROW, LINE_INDEX);
        const auto reduction = full_reduction(constraint, known_tiles);
        CHECK(known_tiles + reduction.reduced_line == build_line_from("#.#.", Line::ROW, LINE_INDEX));
        CHECK(reduction.nb_alternatives == 1);
        CHECK(reduction.is_fully_reduced);
    }
}

TEST_CASE("full_reduction_with_a_long_segment_of_ones", "[line_alternatives]")
{
    const LineConstraint constraint(Line::ROW, { 6, 1 });
    {
        const auto known_tiles = build_line_from('?', 8, Line::ROW, LINE_INDEX);
        const auto reduction = full_reduction(constraint, known_tiles);
        CHECK(reduction.reduced_line == build_line_from("######.#", Line::ROW, LINE_INDEX));
        CHECK(reduction.nb_alternatives == 1);
        CHECK(reduction.is_fully_reduced);
    }
    {
        const auto known_tiles = build_line_from("??????#??", Line::ROW, LINE_INDEX);
        CHECK(known_tiles.size() == 9);
        const auto reduction = full_reduction(constraint, known_tiles);
        CHECK(known_tiles + reduction.reduced_line == build_line_from(".######.#", Line::ROW, LINE_INDEX));
        CHECK(reduction.nb_alternatives == 1);
        CHECK(reduction.is_fully_reduced);
    }
    {
        const auto known_tiles = build_line_from('?', 10, Line::ROW, LINE_INDEX);
        CHECK(known_tiles.size() == 10);
        const auto reduction = full_reduction(constraint, known_tiles);
        CHECK(reduction.reduced_line == build_line_from("??####????", Line::ROW, LINE_INDEX));
        CHECK(reduction.nb_alternatives == 6);
        CHECK(reduction.is_fully_reduced);
    }
    {
        const auto known_tiles = build_line_from("??????#???", Line::ROW, LINE_INDEX);
        CHECK(known_tiles.size() == 10);
        const auto reduction = full_reduction(constraint, known_tiles);
        CHECK(known_tiles + reduction.reduced_line == build_line_from(".?#####???", Line::ROW, LINE_INDEX));
        CHECK(reduction.nb_alternatives == 3);
        CHECK(reduction.is_fully_reduced);
    }
}

TEST_CASE("full_reduction_contradictory_use_case_a", "[line_alternatives]")
{
    const LineConstraint constraint(Line::ROW, { 1 });
    {
        const auto known_tiles = build_line_from("..", Line::ROW, LINE_INDEX);
        const auto reduction = full_reduction(constraint, known_tiles);
        CHECK(reduction.reduced_line.type() == known_tiles.type());
        CHECK(reduction.reduced_line.index() == known_tiles.index());
        CHECK(reduction.reduced_line.size() == known_tiles.size());
        CHECK(reduction.nb_alternatives == 0);
    }
}

TEST_CASE("full_reduction_contradictory_use_case_b", "[line_alternatives]")
{
    const LineConstraint constraint(Line::ROW, { 1, 1 });
    {
        const auto known_tiles = build_line_from("?#?", Line::ROW, LINE_INDEX);
        const auto reduction = full_reduction(constraint, known_tiles);
        CHECK(reduction.reduced_line.type() == known_tiles.type());
        CHECK(reduction.reduced_line.index() == known_tiles.index());
        CHECK(reduction.reduced_line.size() == known_tiles.size());
        CHECK(reduction.nb_alternatives == 0);
    }
    {
        const auto known_tiles = build_line_from("##??", Line::ROW, LINE_INDEX);
        const auto reduction = full_reduction(constraint, known_tiles);
        CHECK(reduction.reduced_line.type() == known_tiles.type());
        CHECK(reduction.reduced_line.index() == known_tiles.index());
        CHECK(reduction.reduced_line.size() == known_tiles.size());
        CHECK(reduction.nb_alternatives == 0);
    }
    {
        const auto known_tiles = build_line_from("#...", Line::ROW, LINE_INDEX);
        const auto reduction = full_reduction(constraint, known_tiles);
        CHECK(reduction.reduced_line.type() == known_tiles.type());
        CHECK(reduction.reduced_line.index() == known_tiles.index());
        CHECK(reduction.reduced_line.size() == known_tiles.size());
        CHECK(reduction.nb_alternatives == 0);
    }
    {
        const auto known_tiles = build_line_from(".#.", Line::ROW, LINE_INDEX);
        const auto reduction = full_reduction(constraint, known_tiles);
        CHECK(reduction.reduced_line.type() == known_tiles.type());
        CHECK(reduction.reduced_line.index() == known_tiles.index());
        CHECK(reduction.reduced_line.size() == known_tiles.size());
        CHECK(reduction.nb_alternatives == 0);
    }
}

TEST_CASE("reduction_contradictory_use_case_c", "[line_alternatives]")
{
    const LineConstraint constraint(Line::ROW, { 3 });
    const auto known_tiles = build_line_from("????????.#", Line::ROW, LINE_INDEX);
    {
        const auto reduction = linear_reduction(constraint, known_tiles);
        CHECK(reduction.nb_alternatives == 0);
    }
    {
        const auto reduction = full_reduction(constraint, known_tiles);
        CHECK(reduction.nb_alternatives == 0);
    }
}

TEST_CASE("reduction_contradictory_use_case_d", "[line_alternatives]")
{
    const LineConstraint constraint(Line::ROW, { 3 });
    const auto known_tiles = build_line_from("????####.", Line::ROW, LINE_INDEX);
    {
        const auto reduction = linear_reduction(constraint, known_tiles);
        CHECK(reduction.nb_alternatives == 0);
    }
    {
        const auto reduction = full_reduction(constraint, known_tiles);
        CHECK(reduction.nb_alternatives == 0);
    }
}

TEST_CASE("reduction_contradictory_use_case_e", "[line_alternatives]")
{
    const LineConstraint constraint(Line::ROW, { 3, 2 });
    const auto known_tiles = build_line_from("????###.#...????", Line::ROW, LINE_INDEX);
    {
        const auto reduction = linear_reduction(constraint, known_tiles);
        CHECK(reduction.nb_alternatives == 0);
    }
    {
        const auto reduction = full_reduction(constraint, known_tiles);
        CHECK(reduction.nb_alternatives == 0);
    }
}

TEST_CASE("reduction_of_a_completed_line", "[line_alternatives]")
{
    const LineConstraint constraint(Line::ROW, { 4, 1, 3, 1 });
    const auto known_tiles = build_line_from("..####....#...###.#...", Line::ROW, LINE_INDEX);
    {
        const auto reduction = linear_reduction(constraint, known_tiles);
        CHECK(known_tiles + reduction.reduced_line == known_tiles);
        CHECK(reduction.nb_alternatives == 1);
        CHECK(reduction.is_fully_reduced);
    }
    {
        const auto reduction = full_reduction(constraint, known_tiles);
        CHECK(known_tiles + reduction.reduced_line == known_tiles);
        CHECK(reduction.nb_alternatives == 1);
        CHECK(reduction.is_fully_reduced);
    }
}

TEST_CASE("linear_reduction_of_a_trivially_solvable_line", "[line_alternatives]")
{
    const LineConstraint constraint(Line::ROW, { 4, 1, 2, 1, 3, 1 });
    const auto min_line_size = constraint.min_line_size();
    const Line known_tiles = build_line_from('?', min_line_size, Line::ROW, LINE_INDEX);
    const auto reduction = linear_reduction(constraint, known_tiles);
    CHECK(known_tiles + reduction.reduced_line == build_line_from("####.#.##.#.###.#", Line::ROW, LINE_INDEX));
    CHECK(reduction.nb_alternatives == 1);
    CHECK(reduction.is_fully_reduced);
}

TEST_CASE("full_reduction_of_a_trivially_solvable_line", "[line_alternatives]")
{
    const LineConstraint constraint(Line::ROW, { 4, 1, 2, 1, 3, 1 });
    const auto min_line_size = constraint.min_line_size();
    const Line known_tiles = build_line_from('?', min_line_size, Line::ROW, LINE_INDEX);
    const auto reduction = full_reduction(constraint, known_tiles);
    CHECK(known_tiles + reduction.reduced_line == build_line_from("####.#.##.#.###.#", Line::ROW, LINE_INDEX));
    CHECK(reduction.nb_alternatives == 1);
    CHECK(reduction.is_fully_reduced);
}

// Test based on the puzzle webpbn-10810.non "Centerpiece"
TEST_CASE("centerpiece-webpbn-10810-row-8", "[line_alternatives]")
{
    const LineConstraint constraint(Line::ROW, { 2,4,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,4,2 });
    const auto min_line_size = constraint.min_line_size();
    CHECK(min_line_size == 51);

    const Line known_tiles = build_line_from("##.####.????.????.????.????.????.????.????.????.????.####.##", Line::ROW, 8);
    LineAlternatives line_alternatives(constraint, known_tiles, get_binomial());
    const auto full_reduction = line_alternatives.full_reduction();
    CHECK(known_tiles + full_reduction.reduced_line == known_tiles);
    CHECK(full_reduction.nb_alternatives == 19683);
    CHECK(full_reduction.is_fully_reduced);
}

// Test based on the puzzle webpbn-10810.non "Centerpiece"
TEST_CASE("centerpiece-webpbn-10810-row-9", "[line_alternatives]")
{
    const LineConstraint constraint(Line::ROW, { 1,1,1,2,2,2,2,2,2,2,2,2,1,1,1 });
    const auto min_line_size = constraint.min_line_size();
    CHECK(min_line_size == 38);

    const Line known_tiles = build_line_from(".#.#..#.?????????.????.????.????.????.????.?????????.#..#.#.", Line::ROW, 9);
    LineAlternatives line_alternatives(constraint, known_tiles, get_binomial());
    const auto full_reduction = line_alternatives.full_reduction();
    CHECK(known_tiles + full_reduction.reduced_line == known_tiles);
    CHECK(full_reduction.nb_alternatives == 123147);
    CHECK(full_reduction.is_fully_reduced);
}

TEST_CASE("line_holes", "[line_alternatives]")
{
    const auto known_tiles = build_line_from("..????###?#.??##??.?..??######?##..#..????##.??##?", Line::ROW, LINE_INDEX);
    CHECK(known_tiles.size() == 50);
    CHECK(line_holes(known_tiles)         == std::vector<LineHole>{ { 2, 9 }, { 12, 6 }, { 19, 1 }, { 22, 11 }, { 35, 1 }, { 38, 6 }, { 45, 5 } });
    CHECK(line_holes(known_tiles, 5)      == std::vector<LineHole>{ { 5, 6 }, { 12, 6 }, { 19, 1 }, { 22, 11 }, { 35, 1 }, { 38, 6 }, { 45, 5 } });
    CHECK(line_holes(known_tiles, 12)     == std::vector<LineHole>{           { 12, 6 }, { 19, 1 }, { 22, 11 }, { 35, 1 }, { 38, 6 }, { 45, 5 } });
    CHECK(line_holes(known_tiles, 12, 35) == std::vector<LineHole>{           { 12, 6 }, { 19, 1 }, { 22, 11 } });
}

TEST_CASE("find_segments_range", "[line_alternatives]")
{
    {
        const LineConstraint constraint(Line::ROW, { 3, 4, 4 });
        const auto known_tiles = build_line_from("..???.?????.??.?.????????", Line::ROW, LINE_INDEX);
        CHECK(known_tiles.size() == 25);
        const auto [success, ranges] = LineAlternatives(constraint, known_tiles, get_binomial()).find_segments_range();
        CHECK(success);
        CHECK(ranges == std::vector<SegmentRange>{ { 2, 2 }, { 6, 7 }, { 17, 21 } });
    }
    {
        const LineConstraint constraint(Line::ROW, { 3, 4, 2 });
        const auto known_tiles = build_line_from('?', constraint.min_line_size(), Line::ROW, LINE_INDEX);
        const auto [success, ranges] = LineAlternatives(constraint, known_tiles, get_binomial()).find_segments_range();
        CHECK(success);
        CHECK(ranges.size() == 3);
        CHECK(std::all_of(ranges.cbegin(), ranges.cend(), [](const auto& r) { return r.m_leftmost_index == r.m_rightmost_index; }));
    }
    {
        const LineConstraint constraint(Line::ROW, { 3, 2, 3 });
        const auto known_tiles = build_line_from("??????...??????", Line::ROW, LINE_INDEX);
        CHECK(known_tiles.size() == 15);
        const auto [success, ranges] = LineAlternatives(constraint, known_tiles, get_binomial()).find_segments_range();
        CHECK(success);
        CHECK(ranges == std::vector<SegmentRange>{ { 0, 3 }, { 4, 9 }, { 9, 12 } });
    }
}

TEST_CASE("linear_reduction_basic_use_cases", "[line_alternatives]")
{
    const LineConstraint constraint(Line::ROW, { 2, 2, 2 });
    {
        const auto known_tiles = build_line_from('?', 8, Line::ROW, LINE_INDEX);
        const auto reduction = linear_reduction(constraint, known_tiles);
        CHECK(reduction.reduced_line == build_line_from("##.##.##", Line::ROW, LINE_INDEX));
        CHECK(reduction.nb_alternatives == 1);
        CHECK(reduction.is_fully_reduced);
    }
    {
        const auto known_tiles = build_line_from("??????#??", Line::ROW, LINE_INDEX);
        CHECK(known_tiles.size() == 9);
        const auto reduction = linear_reduction(constraint, known_tiles);
        CHECK(known_tiles + reduction.reduced_line == build_line_from("##.##.##.", Line::ROW, LINE_INDEX));
        CHECK(reduction.nb_alternatives == 1);
        CHECK(reduction.is_fully_reduced);
    }
    {
        const auto known_tiles = build_line_from("??????.??", Line::ROW, LINE_INDEX);
        CHECK(known_tiles.size() == 9);
        const auto reduction = linear_reduction(constraint, known_tiles);
        CHECK(known_tiles + reduction.reduced_line == build_line_from("?#??#?.##", Line::ROW, LINE_INDEX));
        CHECK(reduction.nb_alternatives == 4);
        CHECK(reduction.is_fully_reduced == false);
    }
    {
        const auto known_tiles = build_line_from('?', 9, Line::ROW, LINE_INDEX);
        const auto reduction = linear_reduction(constraint, known_tiles);
        CHECK(reduction.reduced_line == build_line_from("?#??#??#?", Line::ROW, LINE_INDEX));
        CHECK(reduction.nb_alternatives == 4);
        CHECK(reduction.is_fully_reduced == false);
    }
    {
        const auto known_tiles = build_line_from("?????.?.?.?.?.?.??", Line::ROW, LINE_INDEX);
        const auto reduction = linear_reduction(constraint, known_tiles);
        CHECK(known_tiles + reduction.reduced_line == build_line_from("##.##...........##", Line::ROW, LINE_INDEX));
        CHECK(reduction.nb_alternatives == 1);
        CHECK(reduction.is_fully_reduced);
    }
    {
        const auto known_tiles = build_line_from("???????.?.?.?.?.?.????", Line::ROW, LINE_INDEX);
        const auto reduction = linear_reduction(constraint, known_tiles);
        CHECK(known_tiles + reduction.reduced_line == build_line_from("???????...........????", Line::ROW, LINE_INDEX));
        CHECK(reduction.nb_alternatives == 27);
        CHECK(reduction.is_fully_reduced == false);
    }
}

TEST_CASE("linear_reduction_symetrical_use_case", "[line_alternatives]")
{
    const LineConstraint constraint(Line::ROW, { 3, 3, 3 });
    {
        const auto known_tiles = build_line_from('?', 11, Line::ROW, LINE_INDEX);
        const auto reduction = linear_reduction(constraint, known_tiles);
        CHECK(reduction.reduced_line == build_line_from("###.###.###", Line::ROW, LINE_INDEX));
        CHECK(reduction.nb_alternatives == 1);
        CHECK(reduction.is_fully_reduced);
    }
    {
        const auto known_tiles = build_line_from('?', 12, Line::ROW, LINE_INDEX);
        const auto reduction = linear_reduction(constraint, known_tiles);
        CHECK(reduction.reduced_line == build_line_from("?##??##??##?", Line::ROW, LINE_INDEX));
        CHECK(reduction.nb_alternatives == 4);
        CHECK(reduction.is_fully_reduced == false);
    }
    {
        const auto known_tiles = build_line_from("???#????????", Line::ROW, LINE_INDEX);
        CHECK(known_tiles.size() == 12);
        const auto reduction = linear_reduction(constraint, known_tiles);
        CHECK(known_tiles + reduction.reduced_line == build_line_from(".###.###.###", Line::ROW, LINE_INDEX));
        CHECK(reduction.nb_alternatives == 1);
        CHECK(reduction.is_fully_reduced);
    }
    {
        const auto known_tiles = build_line_from("???#?????????", Line::ROW, LINE_INDEX);
        CHECK(known_tiles.size() == 13);
        const auto reduction = linear_reduction(constraint, known_tiles);
        CHECK(known_tiles + reduction.reduced_line == build_line_from(".?##??##??##?", Line::ROW, LINE_INDEX));
        CHECK(reduction.nb_alternatives == 8);
        CHECK_FALSE(reduction.is_fully_reduced);
    }
}

TEST_CASE("linear_reduction_with_a_long_segment_of_ones", "[line_alternatives]")
{
    const LineConstraint constraint(Line::ROW, { 6, 1 });
    {
        const auto known_tiles = build_line_from('?', 8, Line::ROW, LINE_INDEX);
        const auto reduction = linear_reduction(constraint, known_tiles);
        CHECK(reduction.reduced_line == build_line_from("######.#", Line::ROW, LINE_INDEX));
        CHECK(reduction.nb_alternatives == 1);
        CHECK(reduction.is_fully_reduced);
    }
    {
        const auto known_tiles = build_line_from("??????#??", Line::ROW, LINE_INDEX);
        CHECK(known_tiles.size() == 9);
        const auto reduction = linear_reduction(constraint, known_tiles);
        CHECK(known_tiles + reduction.reduced_line == build_line_from(".######.#", Line::ROW, LINE_INDEX));
        CHECK(reduction.nb_alternatives == 1);
        CHECK(reduction.is_fully_reduced);
    }
    {
        const auto known_tiles = build_line_from('?', 10, Line::ROW, LINE_INDEX);
        CHECK(known_tiles.size() == 10);
        const auto reduction = linear_reduction(constraint, known_tiles);
        CHECK(reduction.reduced_line == build_line_from("??####????", Line::ROW, LINE_INDEX));
        CHECK(reduction.nb_alternatives == 6);
        CHECK(reduction.is_fully_reduced == false);
    }
    {
        const auto known_tiles = build_line_from("??????#???", Line::ROW, LINE_INDEX);
        CHECK(known_tiles.size() == 10);
        const auto reduction = linear_reduction(constraint, known_tiles);
        CHECK(known_tiles + reduction.reduced_line == build_line_from(".?#####???", Line::ROW, LINE_INDEX));
        CHECK(reduction.nb_alternatives == 4);
        CHECK(reduction.is_fully_reduced == false);
    }
}

TEST_CASE("linear_reduction_almost_completed_line", "[line_alternatives]")
{
    const LineConstraint constraint(Line::ROW, { 3, 4 });
    {
        const auto known_tiles = build_line_from("###.????..####", Line::ROW, LINE_INDEX);
        const auto reduction = linear_reduction(constraint, known_tiles);
        CHECK(known_tiles + reduction.reduced_line == build_line_from("###.......####", Line::ROW, LINE_INDEX));
        CHECK(reduction.nb_alternatives == 1);
        CHECK(reduction.is_fully_reduced);
    }
    {
        const auto known_tiles = build_line_from("###.??????####", Line::ROW, LINE_INDEX);
        const auto reduction = linear_reduction(constraint, known_tiles);
        CHECK(known_tiles + reduction.reduced_line == build_line_from("###.......####", Line::ROW, LINE_INDEX));
        CHECK(reduction.nb_alternatives == 1);
        CHECK(reduction.is_fully_reduced);
    }
    {
        const auto known_tiles = build_line_from("###??????.####", Line::ROW, LINE_INDEX);
        const auto reduction = linear_reduction(constraint, known_tiles);
        CHECK(known_tiles + reduction.reduced_line == build_line_from("###.......####", Line::ROW, LINE_INDEX));
        CHECK(reduction.nb_alternatives == 1);
        CHECK(reduction.is_fully_reduced);
    }
    {
        const auto known_tiles = build_line_from("###???????####", Line::ROW, LINE_INDEX);
        const auto reduction = linear_reduction(constraint, known_tiles);
        CHECK(known_tiles + reduction.reduced_line == build_line_from("###.......####", Line::ROW, LINE_INDEX));
        CHECK(reduction.nb_alternatives == 1);
        CHECK(reduction.is_fully_reduced);
    }
}

TEST_CASE("linear_reduction_non_regression_a", "[line_alternatives]")
{
    // An assertion occured on ROW 47 of puzzle webpbn-03541-sign.non
    const LineConstraint constraint(Line::ROW, { 5, 4, 3 });
    {
        // There is no solution to this line solving
        const auto known_tiles = build_line_from("........???????????????????????????????????#?###??##.?????..", Line::ROW, 47);
        const auto reduction = linear_reduction(constraint, known_tiles);
        CHECK(reduction.nb_alternatives == 0);
        CHECK(reduction.is_fully_reduced == false);
    }
}

TEST_CASE("linear_vs_full_reduction", "[line_alternatives]")
{
    constexpr auto MAX = binomial::overflowValue();
    {
        const LineConstraint constraint(Line::COL, { 7, 6, 9, 3, 3, 4, 4 });   // Example from tiger.non
        const auto known_tiles      = build_line_from("..????###?#???##???????######?###?#??????##???##??", Line::COL, 34);
        const auto expected_delta_f = build_line_from("?????#???????#?????.###??????.???.?##..???????????", Line::COL, 34);
        const auto expected_delta_l = build_line_from("???????????????????.###??????.???.?##..???????????", Line::COL, 34);
        {
            const auto reduction = full_reduction(constraint, known_tiles);
            CHECK(known_tiles + reduction.reduced_line == known_tiles + expected_delta_f);
            CHECK(reduction.nb_alternatives == 24);
            CHECK(reduction.is_fully_reduced);
        }
        {
            const auto reduction = linear_reduction(constraint, known_tiles);
            CHECK(known_tiles + reduction.reduced_line == known_tiles + expected_delta_l);
            CHECK(reduction.nb_alternatives == 144);
            CHECK(reduction.is_fully_reduced == false);
        }
    }
    {
        const LineConstraint constraint(Line::COL, { 6, 4, 5, 8, 1, 1, 1, 4, 1, 2 });   // Example from tiger.non
        const auto known_tiles      = build_line_from("...??####?#?##?????#????#######?#.#.?????##???????", Line::COL, 35);
        const auto expected_delta_f = build_line_from("?????????????????##???.#???????.??????????????????", Line::COL, 35);
        const auto expected_delta_l = build_line_from("??????????????????#???.#???????.??????????????????", Line::COL, 35);
        {
            const auto reduction = full_reduction(constraint, known_tiles);
            CHECK(known_tiles + reduction.reduced_line == known_tiles + expected_delta_f);
            CHECK(reduction.nb_alternatives == 100);
            CHECK(reduction.is_fully_reduced);
        }
        {
            const auto reduction = linear_reduction(constraint, known_tiles);
            CHECK(known_tiles + reduction.reduced_line == known_tiles + expected_delta_l);
            CHECK(reduction.nb_alternatives == 3003);
            CHECK(reduction.is_fully_reduced == false);
        }
    }
    {
        const LineConstraint constraint(Line::COL, { 15, 2, 2 });   // Example from tiger.non
        const auto known_tiles      = build_line_from("????????????????????????????#####?????????????????", Line::COL, 71);
        const auto expected_delta   = build_line_from("..................????????????????????????????????", Line::COL, 71);
        {
            const auto reduction = full_reduction(constraint, known_tiles);
            CHECK(known_tiles + reduction.reduced_line == known_tiles + expected_delta);
            CHECK(reduction.nb_alternatives == 363);
            CHECK(reduction.is_fully_reduced);
        }
        {
            const auto reduction = linear_reduction(constraint, known_tiles);
            CHECK(known_tiles + reduction.reduced_line == known_tiles + expected_delta);
            CHECK(reduction.nb_alternatives == 560);
            CHECK(reduction.is_fully_reduced == false);
        }
    }
    {
        const LineConstraint constraint(Line::ROW, { 13, 2, 3, 2, 4, 2, 3, 11 });   // Example from tiger.non
        const auto known_tiles      = build_line_from(".......?????########?????????????????##????????????????????..#??########???", Line::ROW, 39);
        const auto expected_delta   = build_line_from("??????????????????????????????????????????????????????????????##????????...", Line::ROW, 39);
        {
            const auto reduction = full_reduction(constraint, known_tiles);
            CHECK(known_tiles + reduction.reduced_line == known_tiles + expected_delta);
            CHECK(reduction.nb_alternatives == 81024);
            CHECK(reduction.is_fully_reduced);
        }
        {
            const auto reduction = linear_reduction(constraint, known_tiles);
            CHECK(known_tiles + reduction.reduced_line == known_tiles + expected_delta);
            CHECK(reduction.nb_alternatives == 4292145);
            CHECK(reduction.is_fully_reduced == false);
        }
    }
    {
        const LineConstraint constraint(Line::ROW, { 6, 6, 1, 7 });   // Example from webpbn-04645-marylin.non
        const auto known_tiles      = build_line_from("??????????????????????????????????????##???#??????", Line::ROW, 42);
        const auto expected_delta_f = build_line_from("?????????????????????????????????????????#????????", Line::ROW, 42);
        {
            const auto reduction = full_reduction(constraint, known_tiles);
            CHECK(known_tiles + reduction.reduced_line == known_tiles + expected_delta_f);
            CHECK(reduction.nb_alternatives == 4352);
            CHECK(reduction.is_fully_reduced);
        }
        {
            const auto reduction = linear_reduction(constraint, known_tiles);
            CHECK(known_tiles + reduction.reduced_line == known_tiles);
            CHECK(reduction.nb_alternatives == 31465);
            CHECK(reduction.is_fully_reduced == false);
        }
    }
    {
        const LineConstraint constraint(Line::ROW, { 3, 4, 4 });   // Example from webpbn-04645-marylin.non
        const auto known_tiles      = build_line_from("?????????.#???????????????????????????????????????", Line::ROW, 11);
        const auto expected_delta_f = build_line_from("???????????##?????????????????????????????????????", Line::ROW, 11);
        {
            const auto reduction = full_reduction(constraint, known_tiles);
            CHECK(known_tiles + reduction.reduced_line == known_tiles + expected_delta_f);
            CHECK(reduction.nb_alternatives == 633);
            CHECK(reduction.is_fully_reduced);
        }
        {
            const auto reduction = linear_reduction(constraint, known_tiles);
            CHECK(known_tiles + reduction.reduced_line == known_tiles);
            CHECK(reduction.nb_alternatives == 9504);
            CHECK(reduction.is_fully_reduced == false);
        }
    }
    {
        const LineConstraint constraint(Line::ROW, { 9, 4, 9 });    // Example from webpbn-00065-mum.non
        const auto known_tiles      = build_line_from("?????????????#??##??#?????????????", Line::ROW, 22);
        const auto expected_delta_f = build_line_from("???????????##????????##???????????", Line::ROW, 22);
        {
            const auto reduction = full_reduction(constraint, known_tiles);
            CHECK(known_tiles + reduction.reduced_line == known_tiles + expected_delta_f);
            CHECK(reduction.nb_alternatives == 9);
            CHECK(reduction.is_fully_reduced);
        }
        {
            const auto reduction = linear_reduction(constraint, known_tiles);
            CHECK(known_tiles + reduction.reduced_line == known_tiles);
            CHECK(reduction.nb_alternatives == 286);
            CHECK(reduction.is_fully_reduced == false);
        }
    }
    {
        const LineConstraint constraint(Line::COL, { 1, 7, 2, 16, 1, 1 });      // Example from webpbn-00065-mum.non
        //                                             #.#######.##.......
        //                                             ......#.#######.##.
        const auto known_tiles      = build_line_from("???????????????????##########???????????", Line::COL, 13);
        const auto expected_delta_f = build_line_from("????????#???????????????????????????????", Line::COL, 13);
        {
            const auto reduction = full_reduction(constraint, known_tiles);
            CHECK(known_tiles + reduction.reduced_line == known_tiles + expected_delta_f);
            CHECK(reduction.nb_alternatives == 1596);
            CHECK(reduction.is_fully_reduced);
        }
        {
            const auto reduction = linear_reduction(constraint, known_tiles);
            CHECK(known_tiles + reduction.reduced_line == known_tiles);
            CHECK(reduction.nb_alternatives == 1716);
            CHECK(reduction.is_fully_reduced == false);
        }
    }
    {
        const LineConstraint constraint(Line::COL, { 1, 2, 1, 3, 1, 1, 6, 1, 1, 1, 1 });    // Example from webpbn-00065-mum.non
        const auto known_tiles      = build_line_from("?????????????????????#??###?????.???????", Line::COL, 18);
        const auto expected_delta   = build_line_from("????????????????????.???????????????????", Line::COL, 18);
        {
            const auto reduction = full_reduction(constraint, known_tiles);
            CHECK(known_tiles + reduction.reduced_line == known_tiles + expected_delta);
            CHECK(reduction.nb_alternatives == 120648);
            CHECK(reduction.is_fully_reduced);
        }
        {
            const auto reduction = linear_reduction(constraint, known_tiles);
            CHECK(known_tiles + reduction.reduced_line == known_tiles + expected_delta);
            CHECK(reduction.nb_alternatives == 705432);
            CHECK(reduction.is_fully_reduced == false);
        }
    }
    {
        const LineConstraint constraint(Line::COL, { 5, 2, 4, 6 });     // Example from webpbn-03541-sign.non
        const auto known_tiles      = build_line_from("..#####.??????#.??????????????????????????????????", Line::COL, 6);
        const auto expected_delta_f = build_line_from("??????????.??#????????????????????????????????????", Line::COL, 6);
        {
            const auto reduction = full_reduction(constraint, known_tiles);
            CHECK(known_tiles + reduction.reduced_line == known_tiles + expected_delta_f);
            CHECK(reduction.nb_alternatives == 329);
            CHECK(reduction.is_fully_reduced);
        }
        {
            const auto reduction = linear_reduction(constraint, known_tiles);
            CHECK(known_tiles + reduction.reduced_line == known_tiles);
            CHECK(reduction.nb_alternatives == 3625);
            CHECK(reduction.is_fully_reduced == false);
        }
    }
    {
        const LineConstraint constraint(Line::COL, { 2, 3, 3, 3, 2, 4, 2, 2, 3 });    // Example from webpbn-03541-sign.non
        const auto known_tiles      = build_line_from(".????..#??????.???????????????????????????????????", Line::COL, 39);
        const auto expected_delta_f = build_line_from("????????#?????????????????????????????????????????", Line::COL, 39);
        {
            const auto reduction = full_reduction(constraint, known_tiles);
            CHECK(known_tiles + reduction.reduced_line == known_tiles + expected_delta_f);
            CHECK(reduction.nb_alternatives == 216523);
            CHECK(reduction.is_fully_reduced);
        }
        {
            const auto reduction = linear_reduction(constraint, known_tiles);
            CHECK(known_tiles + reduction.reduced_line == known_tiles);
            CHECK(reduction.nb_alternatives == 3124550);
            CHECK(reduction.is_fully_reduced == false);
        }
    }
    {
        const LineConstraint constraint(Line::ROW, { 3, 1, 4, 4, 4, 1, 2, 2, 5, 5, 2, 3, 1, 1, 2 });    // Example from raccoon_17403.txt
        const auto known_tiles      = build_line_from("????????????###..???#....#???????????#??#???????????????????????????????????????", Line::ROW, 2);
        const auto expected_delta_f = build_line_from("??????????.?????????????????????????????????????????????????????????????????????", Line::ROW, 2);
        {
            const auto reduction = full_reduction(constraint, known_tiles);
            CHECK(known_tiles + reduction.reduced_line == known_tiles + expected_delta_f);
            CHECK(reduction.nb_alternatives == 41834376);
            CHECK(reduction.is_fully_reduced);
        }
        {
            const auto reduction = linear_reduction(constraint, known_tiles);
            CHECK(known_tiles + reduction.reduced_line == known_tiles);
            CHECK(reduction.nb_alternatives == MAX);
            CHECK(reduction.is_fully_reduced == false);
        }
    }
    {
        const LineConstraint constraint(Line::ROW, { 4, 1, 1, 5, 6, 6, 1, 1, 1, 2, 2, 3, 2, 5, 1, 4 }); // Example from raccoon_17403.txt
        const auto known_tiles      = build_line_from("???????????.#####...######..?????????#??????????????????????????????????????????", Line::ROW, 1);
        const auto expected_delta_f = build_line_from("???#????????????????????????????????????????????????????????????????????????????", Line::ROW, 1);
        {
            const auto reduction = full_reduction(constraint, known_tiles);
            CHECK(known_tiles + reduction.reduced_line == known_tiles + expected_delta_f);
            CHECK(reduction.nb_alternatives == 31058220);
            CHECK(reduction.is_fully_reduced);
        }
        {
            const auto reduction = linear_reduction(constraint, known_tiles);
            CHECK(known_tiles + reduction.reduced_line == known_tiles);
            CHECK(reduction.nb_alternatives == MAX);
            CHECK(reduction.is_fully_reduced == false);
        }
    }
}

TEST_CASE("full_reduction_dp_matches_baseline_on_all_short_lines", "[line_alternatives]") {
    static constexpr Tile TILES[] = { Tile::UNKNOWN, Tile::EMPTY, Tile::FILLED };
    for (std::size_t size = 1u; size <= 8u; size++) {
        std::vector<InputGrid::Constraint> constraints;
        InputGrid::Constraint prefix;
        all_constraints(size, prefix, constraints);
        
        std::size_t patterns = 1u;
        for (std::size_t i = 0u; i < size; i++) { 
            patterns *= 3u; 
        }
        
        for (std::size_t pattern = 0u; pattern < patterns; pattern++) {
            Line known_tiles(Line::ROW, LINE_INDEX, size);
            
            std::size_t code = pattern;
            for (std::size_t i = 0u; i < size; i++) { 
                known_tiles[i] = TILES[code % 3u]; 
                code /= 3u;
            }
            
            for (const auto& segments : constraints) {
                CHECK(dp_matches_baseline(segments, known_tiles));
            }
        }
    }
}

TEST_CASE("full_reduction_dp_matches_baseline_on_random_lines", "[line_alternatives]")
{
    std::mt19937 rng(598u);
    std::uniform_int_distribution<std::size_t> random_size(9u, 60u);
    std::uniform_real_distribution<double> unit(0.0, 1.0);
    
    for (int trial = 0; trial < 20000; trial++) {
        const std::size_t size = random_size(rng);
        const double density = unit(rng);
        const double reveal = unit(rng) * 0.6;
        const bool consistent = unit(rng) < 0.8;

        std::vector<bool> solution;
        InputGrid::Constraint segments;
        for (std::size_t i = 0u; i < size; i++) { 
            const bool fill = unit(rng) < density;
            solution.push_back(fill);
            if (fill) {
                if (i == 0u || !solution[i - 1u]) {
                    segments.push_back(0u);
                }

                segments[segments.size() - 1]++;
            } 
        }
        
        Line known_tiles(Line::ROW, LINE_INDEX, size);
        for (std::size_t i = 0u; i < size; i++) {
            if (unit(rng) < reveal) {
                const bool filled = consistent ? solution[i] : unit(rng) < 0.5;
                known_tiles[i] = filled ? Tile::FILLED : Tile::EMPTY;
            }
        }

        CHECK(dp_matches_baseline(segments, known_tiles));
    }
}

} // namespace picross
