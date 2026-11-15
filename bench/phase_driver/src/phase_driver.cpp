#include <picross/picross.h>
#include <picross/picross_io.h>
#include <picross/picross_stats.h>

#include <sys/resource.h>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

using Clock = std::chrono::steady_clock;

enum class Phase {
    LINE_SOLVING_AND_PROPAGATION = 0,
    PROBING = 1,
    SEARCH = 2,
    OTHER = 3
};
constexpr int NUM_PHASES = 4;

Phase state_to_phase(std::uint32_t internal_state)
{
    switch (internal_state) {
        case 0: case 1: case 2:
            return Phase::LINE_SOLVING_AND_PROPAGATION;
        case 3:
            return Phase::PROBING;
        case 4:
            return Phase::SEARCH;
        default:
            return Phase::OTHER;
    }
}

struct PhaseTimer
{
    Clock::time_point last_timestamp = Clock::now();
    Phase last_bucket = Phase::OTHER;
    double seconds[NUM_PHASES] = { 0.0, 0.0, 0.0, 0.0 };
    bool last_in_branch = false;
    double root_seconds = 0.0;
    double branch_seconds = 0.0;

    void change_state(std::uint32_t internal_state, std::uint32_t depth = 0u)
    {
        const auto now = Clock::now();
        const double elapsed = std::chrono::duration<double>(now - last_timestamp).count();
        seconds[static_cast<int>(last_bucket)] += elapsed;

        if (last_in_branch) {
            branch_seconds += elapsed;
        } else {
            root_seconds += elapsed;
        }

        last_timestamp = now;
        last_bucket = state_to_phase(internal_state);
        last_in_branch = depth > 0u;
    }
};

std::string json_escape(std::string_view s)
{
    std::string out;
    out.reserve(s.size());
    for (char c : s)
    {
        if (c == '"' || c == '\\') { out += '\\'; }
        out += c;
    }
    return out;
}

double cpu_seconds(const rusage& usage)
{
    return static_cast<double>(usage.ru_utime.tv_sec) + static_cast<double>(usage.ru_utime.tv_usec) * 1e-6 + static_cast<double>(usage.ru_stime.tv_sec) + static_cast<double>(usage.ru_stime.tv_usec) * 1e-6;
}

std::string solution_rows_json(const picross::OutputGrid& grid)
{
    std::string out = "[";
    for (std::size_t y = 0; y < grid.height(); y++) {
        out += y > 0 ? ",\"" : "\"";
        for (std::size_t x = 0; x < grid.width(); x++) {
            const auto tile = grid.get_tile(static_cast<picross::Line::Index>(x), static_cast<picross::Line::Index>(y));
            out += tile == picross::Tile::FILLED ? '1' : (tile == picross::Tile::EMPTY ? '0' : '?');
        }
        out += "\"";
    }
    return out + "]";
}

int main(int argc, char** argv)
{
    if (argc < 2) {
        std::cerr << "Usage: phase_driver <puzzle.non> [max_solutions] [--max-solutions N] [--timeout S] [--max-search-nodes N] [--no-observer] [solver flags]" << std::endl;
        return EXIT_FAILURE;
    }

    const std::string filepath = argv[1];
    unsigned int max_solutions = 2u;
    unsigned long long max_search_nodes = 0u;
    double timeout_s = 0.0;
    bool use_observer = true;

    picross::SolverFlags flags;
    for (int i = 2; i < argc; i++) {
        const std::string_view arg = argv[i];

        if (arg == "--fp1") {
            flags.enable_fp1 = true;
        } else if (arg == "--fp2") {
            flags.enable_fp2 = true;
        } else if (arg == "--pixel-branching") {
            flags.enable_pixel_branching = true;
        } else if (arg == "--dp-line-solver") {
            flags.enable_dp_line_solver = true;
        } else if (arg == "--freedom") {
            flags.enable_freedom_line_solver = true;
        } else if (arg == "--bitboard") {
            flags.enable_bitboard = true;
        } else if (arg == "--incremental-propagation") {
            flags.incremental_propagation = true;
        } else if (arg == "--no-observer") {
            use_observer = false;
        } else if (arg == "--max-search-nodes" && i + 1 < argc) {
            max_search_nodes = std::stoull(argv[++i]);
        } else if (arg == "--max-solutions" && i + 1 < argc) {
            max_solutions = static_cast<unsigned int>(std::stoul(argv[++i]));
        } else if (arg == "--timeout" && i + 1 < argc) {
            timeout_s = std::stod(argv[++i]);
        } else if (i == 2) {
            max_solutions = static_cast<unsigned int>(std::stoul(argv[i]));
        } else {
            std::cerr << "Unknown argument: " << arg << std::endl;
            return EXIT_FAILURE;
        }
    }

    if (flags.enable_fp1 && flags.enable_fp2) {
        std::cerr << "Options --fp1 and --fp2 are mutually exclusive." << std::endl;
        return EXIT_FAILURE;
    }

    bool had_error = false;
    const picross::io::ErrorHandler err_handler = [&had_error](picross::io::ErrorCodeT code, std::string_view msg) {
        std::cerr << picross::io::str_error_code(code) << ": " << msg << std::endl;
        had_error = true;
    };

    const auto grids = picross::io::parse_input_file_non_format(filepath, err_handler);
    if (had_error || grids.empty()) {
        return EXIT_FAILURE;
    }
    const picross::InputGrid& input_grid = grids[0].m_input_grid;

    PhaseTimer timer;
    const auto solver = picross::get_ref_solver(flags);

    picross::GridStats stats;
    if (use_observer) {
        solver->set_stats(stats);
    }

    unsigned long long search_nodes = 0u;
    unsigned long long probing_rounds = 0u;
    std::uint32_t max_depth = 0u;
    if (use_observer) {
        solver->set_observer([&](picross::ObserverEvent event, const picross::Line*, const picross::ObserverData& data) {
            max_depth = std::max(max_depth, data.m_depth);
            if (event == picross::ObserverEvent::INTERNAL_STATE) {
                timer.change_state(data.m_probing ? 3u : data.m_misc_i, data.m_depth);
                if (data.m_probing) { 
                    return; 
                }

                if (state_to_phase(data.m_misc_i) == Phase::SEARCH) {
                    search_nodes++;
                } else if (state_to_phase(data.m_misc_i) == Phase::PROBING) {
                    probing_rounds++;
                }
            }
        });
    }

    const auto wall_start = Clock::now();
    const auto deadline = wall_start + std::chrono::duration_cast<Clock::duration>(std::chrono::duration<double>(timeout_s));
    bool timed_out = false;
    if (max_search_nodes > 0u || timeout_s > 0.0) {
        solver->set_abort_function([&]() {
            if (max_search_nodes > 0u && search_nodes >= max_search_nodes) { 
                return true; 
            }
            
            if (timeout_s > 0.0 && Clock::now() >= deadline) { 
                timed_out = true; 
                return true; 
            }
            
            return false;
        });
    }

    rusage usage_start{};
    getrusage(RUSAGE_SELF, &usage_start);
    const auto result = solver->solve(input_grid, max_solutions);
    const double wall_time = std::chrono::duration<double>(Clock::now() - wall_start).count();
    rusage usage_end{};
    getrusage(RUSAGE_SELF, &usage_end);
    timer.change_state(5);

    std::ostringstream oss;
    oss << result.status;
    std::string status = oss.str();
    if (result.status == picross::Solver::Status::ABORTED) {
        if (timed_out) {
            status = "TIMEOUT";
        } else if (max_search_nodes > 0u && search_nodes >= max_search_nodes) {
            status = "BUDGET_REACHED";
        }
    }

    std::cout << "{";
        std::cout << "\"grid\":\"" << json_escape(input_grid.name()) << "\",";
        std::cout << "\"size\":\"" << json_escape(picross::str_input_grid_size(input_grid)) << "\",";
        std::cout << "\"status\":\"" << json_escape(status) << "\",";
        std::cout << "\"nb_solutions\":" << result.solutions.size() << ",";
        std::cout << "\"solutions\":[";
        for (std::size_t i = 0; i < result.solutions.size(); i++) {
            if (i > 0) { std::cout << ","; }
            std::cout << "{\"partial\":" << (result.solutions[i].partial ? "true" : "false") << ",\"rows\":" << solution_rows_json(result.solutions[i].grid) << "}";
        }
        std::cout << "],";
        std::cout << "\"wall_time_s\":" << wall_time << ",";
        std::cout << "\"cpu_time_s\":" << (cpu_seconds(usage_end) - cpu_seconds(usage_start)) << ",";
        std::cout << "\"peak_rss_kb\":" << usage_end.ru_maxrss << ",";
        std::cout << "\"observer\":" << (use_observer ? "true" : "false") << ",";
        std::cout << "\"search_nodes\":" << search_nodes << ",";
        std::cout << "\"probing_rounds\":" << probing_rounds << ",";
        std::cout << "\"max_depth\":" << max_depth << ",";
        std::cout << "\"line_solves\":{";
            std::cout << "\"linear\":" << stats.nb_single_line_linear_reduction << ",";
            std::cout << "\"linear_w_change\":" << stats.nb_single_line_linear_reduction_w_change << ",";
            std::cout << "\"full\":" << stats.nb_single_line_full_reduction << ",";
            std::cout << "\"full_w_change\":" << stats.nb_single_line_full_reduction_w_change << ",";
            std::cout << "\"probing_calls\":" << stats.nb_probing_calls << ",";
            std::cout << "\"branching_calls\":" << stats.nb_branching_calls;
        std::cout << "},";
        std::cout << "\"phases_s\":{";
            std::cout << "\"" << "line_solving_and_propagation" << "\":" << timer.seconds[0] << ",";
            std::cout << "\"" << "probing" << "\":" << timer.seconds[1] << ",";
            std::cout << "\"" << "search" << "\":" << timer.seconds[2] << ",";
            std::cout << "\"" << "other" << "\":" << timer.seconds[3];
        std::cout << "},";
        std::cout << "\"depth_split_s\":{";
            std::cout << "\"root\":" << timer.root_seconds << ",";
            std::cout << "\"in_branches\":" << timer.branch_seconds;
        std::cout << "}";
    std::cout << "}" << std::endl;

    return EXIT_SUCCESS;
}
