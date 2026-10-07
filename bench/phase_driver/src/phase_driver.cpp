#include <picross/picross.h>
#include <picross/picross_io.h>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <string>
#include <string_view>

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

    void change_state(std::uint32_t internal_state)
    {
        const auto now = Clock::now();
        seconds[static_cast<int>(last_bucket)] += std::chrono::duration<double>(now - last_timestamp).count();
        last_timestamp = now;
        last_bucket = state_to_phase(internal_state);
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

int main(int argc, char** argv)
{
    if (argc < 2) {
        return EXIT_FAILURE;
    }

    const std::string filepath = argv[1];
    unsigned int max_solutions = 2u;
    unsigned long long max_search_nodes = 0u;
    
    picross::SolverFlags flags;
    for (int i = 2; i < argc; i++) {
        const std::string_view arg = argv[i];

        if (arg == "--fp1") {
            flags.enable_fp1 = true;
        } else if (arg == "--dp-line-solver") {
            flags.enable_dp_line_solver = true;
        } else if (arg == "--max-search-nodes" && i + 1 < argc) {
            max_search_nodes = std::stoull(argv[i + 1]);
            i++;
        } else if (i == 2) {
            max_solutions = static_cast<unsigned int>(std::stoul(argv[i]));
        } else {
            std::cerr << "Unknown argument: " << arg << std::endl;
            return EXIT_FAILURE;
        }
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

    unsigned long long search_nodes = 0u;
    unsigned long long probing_rounds = 0u;
    std::uint32_t max_depth = 0u;
    solver->set_observer([&](picross::ObserverEvent event, const picross::Line*, const picross::ObserverData& data) {
        max_depth = std::max(max_depth, data.m_depth);
        if (event == picross::ObserverEvent::INTERNAL_STATE) {
            timer.change_state(data.m_misc_i);
            
            if (state_to_phase(data.m_misc_i) == Phase::SEARCH) { 
                search_nodes++; 
            } else if (state_to_phase(data.m_misc_i) == Phase::PROBING) { 
                probing_rounds++; 
            }
        }
    });

    if (max_search_nodes > 0u) {
        solver->set_abort_function([&]() { return search_nodes >= max_search_nodes; });
    }

    const auto wall_start = Clock::now();
    const auto result = solver->solve(input_grid, max_solutions);
    const double wall_time = std::chrono::duration<double>(Clock::now() - wall_start).count();
    timer.change_state(5);

    std::ostringstream oss;
    oss << result.status;
    const bool budget_reached = max_search_nodes > 0u && search_nodes >= max_search_nodes && result.status == picross::Solver::Status::ABORTED;
    const auto status = budget_reached ? std::string("BUDGET_REACHED") : oss.str();

    std::cout << "{";
        std::cout << "\"grid\":\"" << json_escape(input_grid.name()) << "\",";
        std::cout << "\"size\":\"" << json_escape(picross::str_input_grid_size(input_grid)) << "\",";
        std::cout << "\"status\":\"" << json_escape(status) << "\",";
        std::cout << "\"nb_solutions\":" << result.solutions.size() << ",";
        std::cout << "\"wall_time_s\":" << wall_time << ",";
        std::cout << "\"search_nodes\":" << search_nodes << ",";
        std::cout << "\"probing_rounds\":" << probing_rounds << ",";
        std::cout << "\"max_depth\":" << max_depth << ",";
        std::cout << "\"phases_s\":{";
            std::cout << "\"" << "line_solving_and_propagation" << "\":" << timer.seconds[0] << ",";
            std::cout << "\"" << "probing" << "\":" << timer.seconds[1] << ",";
            std::cout << "\"" << "search" << "\":" << timer.seconds[2] << ",";
            std::cout << "\"" << "other" << "\":" << timer.seconds[3];
        std::cout << "}";
    std::cout << "}" << std::endl;

    return EXIT_SUCCESS;
}
