#include <unordered_set>
#include "timing_graph.h"
#include "parser.h"
#include "sta_engine.h"
#include <iostream>
#include <iomanip>
#include <fstream>
#include <sstream>

// ----------------------------------------------------------------
// Pretty-print the timing report
// ----------------------------------------------------------------
void printReport(const TimingGraph& g, const STAResult& res) {
    const int W = 60;
    std::string bar(W, '=');
    std::string dashes(W, '-');

    std::cout << "\n" << bar << "\n";
    std::cout << "  Static Timing Analysis Report\n";
    std::cout << "  Clock period: " << g.clock_period_ps << " ps  ("
              << std::fixed << std::setprecision(2)
              << (1000.0 / g.clock_period_ps) << " GHz)\n";
    std::cout << bar << "\n";

    // Per-node arrival times
    std::cout << "\nNode timing table:\n";
    std::cout << std::left
              << std::setw(16) << "Name"
              << std::setw(10) << "Type"
              << std::setw(10) << "Delay(ps)"
              << std::setw(12) << "Arrival(ps)"
              << std::setw(12) << "Required(ps)"
              << "Slack(ps)\n";
    std::cout << dashes << "\n";

    for (const auto& name : g.topo_order) {
        const auto& n = g.nodes.at(name);
        double req = (n.required_time == std::numeric_limits<double>::max())
                     ? g.clock_period_ps : n.required_time;
        std::string slack_str = std::to_string((int)n.slack);
        if (n.slack < 0) slack_str = "*** " + slack_str + " VIOLATION";
        std::cout << std::left
                  << std::setw(16) << n.name
                  << std::setw(10) << GateDelay::name(n.type)
                  << std::setw(10) << (int)n.delay
                  << std::setw(12) << (int)n.arrival_time
                  << std::setw(12) << (int)req
                  << slack_str << "\n";
    }

    // Critical path
    std::cout << "\n" << bar << "\n";
    std::cout << "  Critical Path\n";
    std::cout << bar << "\n";
    std::cout << "  Total path delay: " << (int)res.critical_path.total_delay << " ps\n";
    std::cout << "  Path slack:       " << (int)res.critical_path.slack << " ps\n";
    std::cout << "  Path: ";
    for (size_t i = 0; i < res.critical_path.nodes.size(); i++) {
        if (i) std::cout << " → ";
        std::cout << res.critical_path.nodes[i];
    }
    std::cout << "\n";

    // Summary
    std::cout << "\n" << bar << "\n";
    std::cout << "  Summary\n";
    std::cout << bar << "\n";
    std::cout << "  Worst Negative Slack (WNS): " << (int)res.wns << " ps";
    if (res.wns < 0) std::cout << "  *** TIMING VIOLATED";
    std::cout << "\n";
    std::cout << "  Total Negative Slack (TNS): " << (int)res.tns << " ps\n";
    std::cout << "  Timing violations:          " << res.num_violations << "\n";
    std::cout << bar << "\n\n";

    // All paths (sorted by slack)
    if (res.all_paths.size() > 1) {
        std::cout << "All output paths (sorted by slack):\n";
        std::cout << std::left << std::setw(8) << "Slack"
                  << std::setw(10) << "Delay" << "Path\n";
        std::cout << dashes << "\n";
        for (const auto& p : res.all_paths) {
            std::cout << std::setw(8) << (int)p.slack
                      << std::setw(10) << (int)p.total_delay;
            for (size_t i = 0; i < p.nodes.size(); i++) {
                if (i) std::cout << "→";
                std::cout << p.nodes[i];
            }
            std::cout << "\n";
        }
    }
}

// ----------------------------------------------------------------
// Generate DOT graph for Graphviz visualisation
// ----------------------------------------------------------------
void writeDot(const TimingGraph& g, const STAResult& res, const std::string& out_file) {
    // Build a set of critical path nodes for highlighting
    std::unordered_set<std::string> cp_nodes(
        res.critical_path.nodes.begin(), res.critical_path.nodes.end());

    std::ofstream f(out_file);
    f << "digraph netlist {\n";
    f << "  rankdir=LR;\n";
    f << "  node [fontname=\"monospace\" fontsize=10];\n\n";

    for (const auto& [name, node] : g.nodes) {
        bool on_cp = cp_nodes.count(name) > 0;
        bool viol  = node.slack < 0;

        std::string color = on_cp  ? "#FF6B6B" :
                            viol   ? "#FFD700" : "#E8F5E9";
        std::string shape = (node.type == GateType::INPUT)  ? "invtriangle" :
                            (node.type == GateType::OUTPUT) ? "triangle"    : "box";

        f << "  \"" << name << "\" ["
          << "label=\"" << name << "\\n" << GateDelay::name(node.type)
          << "\\nAT=" << (int)node.arrival_time << "ps"
          << "\\nSlk=" << (int)node.slack << "ps\""
          << " shape=" << shape
          << " style=filled fillcolor=\"" << color << "\""
          << "];\n";
    }

    f << "\n";
    for (const auto& [name, node] : g.nodes) {
        for (const auto& succ : node.fanout) {
            bool critical = cp_nodes.count(name) && cp_nodes.count(succ);
            f << "  \"" << name << "\" -> \"" << succ << "\"";
            if (critical) f << " [color=\"#CC0000\" penwidth=2]";
            f << ";\n";
        }
    }
    f << "}\n";
    std::cout << "DOT graph written to: " << out_file << "\n";
    std::cout << "Render with: dot -Tpng " << out_file << " -o netlist.png\n";
}

// ----------------------------------------------------------------
// main
// ----------------------------------------------------------------
int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: sta_tool <netlist.net> [clock_period_ps] [output.dot]\n";
        std::cerr << "  netlist.net      — gate-level netlist file\n";
        std::cerr << "  clock_period_ps  — optional override (default from file or 1000ps)\n";
        std::cerr << "  output.dot       — optional Graphviz output file\n\n";
        std::cerr << "Example: ./sta_tool tests/adder.net 500\n";
        return 1;
    }

    std::string netlist_file = argv[1];
    std::string dot_file;

    try {
        TimingGraph g = NetlistParser::parse(netlist_file);

        if (argc >= 3) {
            g.clock_period_ps = std::stod(argv[2]);
        }
        if (argc >= 4) {
            dot_file = argv[3];
        }

        STAResult res = STAEngine::analyze(g);
        printReport(g, res);

        if (!dot_file.empty())
            writeDot(g, res, dot_file);

        return (res.num_violations > 0) ? 2 : 0;

    } catch (const std::exception& e) {
        std::cerr << "ERROR: " << e.what() << "\n";
        return 1;
    }
}
