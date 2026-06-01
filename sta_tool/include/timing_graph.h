#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <limits>
#include <stdexcept>

// ----------------------------------------------------------------
// Gate types and their propagation delays (in ps)
// ----------------------------------------------------------------
enum class GateType { INPUT, OUTPUT, AND2, OR2, NOT, NAND2, NOR2, XOR2, BUF, DFF };

struct GateDelay {
    static double get(GateType t) {
        switch (t) {
            case GateType::AND2:  return 80.0;
            case GateType::OR2:   return 90.0;
            case GateType::NAND2: return 60.0;
            case GateType::NOR2:  return 70.0;
            case GateType::NOT:   return 40.0;
            case GateType::XOR2:  return 120.0;
            case GateType::BUF:   return 30.0;
            case GateType::DFF:   return 200.0; // clock-to-Q
            default:              return 0.0;
        }
    }
    static std::string name(GateType t) {
        switch (t) {
            case GateType::AND2:  return "AND2";
            case GateType::OR2:   return "OR2";
            case GateType::NAND2: return "NAND2";
            case GateType::NOR2:  return "NOR2";
            case GateType::NOT:   return "NOT";
            case GateType::XOR2:  return "XOR2";
            case GateType::BUF:   return "BUF";
            case GateType::DFF:   return "DFF";
            case GateType::INPUT: return "INPUT";
            case GateType::OUTPUT:return "OUTPUT";
        }
        return "?";
    }
    static GateType fromString(const std::string& s) {
        if (s == "AND2")  return GateType::AND2;
        if (s == "OR2")   return GateType::OR2;
        if (s == "NAND2") return GateType::NAND2;
        if (s == "NOR2")  return GateType::NOR2;
        if (s == "NOT")   return GateType::NOT;
        if (s == "XOR2")  return GateType::XOR2;
        if (s == "BUF")   return GateType::BUF;
        if (s == "DFF")   return GateType::DFF;
        throw std::runtime_error("Unknown gate type: " + s);
    }
};

// ----------------------------------------------------------------
// Node in the timing graph
// ----------------------------------------------------------------
struct Node {
    std::string name;
    GateType    type;
    double      delay;          // intrinsic gate delay (ps)
    double      arrival_time;   // earliest time signal is valid at output
    double      required_time;  // latest time signal can arrive (from required_period)
    double      slack;          // required_time - arrival_time

    std::vector<std::string> fanin;   // input net names
    std::vector<std::string> fanout;  // output net names

    bool visited;   // for topological sort
    bool in_stack;  // for cycle detection

    Node() : delay(0), arrival_time(0), required_time(std::numeric_limits<double>::max()),
             slack(0), visited(false), in_stack(false) {}
};

// ----------------------------------------------------------------
// Timing graph (DAG of gates)
// ----------------------------------------------------------------
struct TimingGraph {
    std::unordered_map<std::string, Node> nodes;
    std::vector<std::string> topo_order;  // valid after topological sort
    double clock_period_ps = 1000.0;      // default 1 GHz

    // Helpers
    Node& getOrCreate(const std::string& name) {
        return nodes[name];
    }
    void addEdge(const std::string& from, const std::string& to) {
        nodes[from].fanout.push_back(to);
        nodes[to].fanin.push_back(from);
    }
};

// ----------------------------------------------------------------
// Analysis results
// ----------------------------------------------------------------
struct TimingPath {
    std::vector<std::string> nodes;  // gate names from source to sink
    double total_delay;              // sum of gate delays along path
    double slack;
};

struct STAResult {
    TimingPath critical_path;
    double     wns;              // worst negative slack
    double     tns;              // total negative slack
    int        num_violations;
    std::vector<TimingPath> all_paths; // top-N paths
};
