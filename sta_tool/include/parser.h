#pragma once
#include "timing_graph.h"
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <iostream>

// ----------------------------------------------------------------
// Netlist format (.net):
//
//   # comment
//   clock_period 1000       # ps (optional, default 1000)
//   input  A B C            # primary inputs
//   output Z                # primary outputs
//   gate  <type> <name>  <in1> [<in2>]  ->  <out>
//
// Example:
//   input  A B
//   output Z
//   gate AND2 g1  A B  ->  n1
//   gate NOT  g2  n1   ->  Z
// ----------------------------------------------------------------

class NetlistParser {
public:
    static TimingGraph parse(const std::string& filename) {
        std::ifstream f(filename);
        if (!f.is_open())
            throw std::runtime_error("Cannot open netlist: " + filename);

        TimingGraph g;
        std::string line;
        int lineno = 0;

        while (std::getline(f, line)) {
            ++lineno;
            // Strip comments
            auto comment = line.find('#');
            if (comment != std::string::npos) line = line.substr(0, comment);
            std::istringstream ss(line);
            std::string keyword;
            if (!(ss >> keyword)) continue; // blank line

            if (keyword == "clock_period") {
                ss >> g.clock_period_ps;

            } else if (keyword == "input") {
                std::string name;
                while (ss >> name) {
                    auto& n = g.getOrCreate(name);
                    n.name  = name;
                    n.type  = GateType::INPUT;
                    n.delay = 0.0;
                    n.arrival_time = 0.0;
                }

            } else if (keyword == "output") {
                std::string name;
                while (ss >> name) {
                    auto& n = g.getOrCreate(name);
                    n.name  = name;
                    n.type  = GateType::OUTPUT;
                    n.delay = 0.0;
                    // required time will be set to clock_period during analysis
                }

            } else if (keyword == "gate") {
                // gate <TYPE> <instance_name> <in...> -> <out>
                std::string type_str, inst_name;
                if (!(ss >> type_str >> inst_name))
                    throw std::runtime_error("Malformed gate at line " + std::to_string(lineno));

                GateType gtype = GateDelay::fromString(type_str);

                // Collect tokens until "->"
                std::vector<std::string> inputs;
                std::string tok;
                while (ss >> tok) {
                    if (tok == "->") break;
                    inputs.push_back(tok);
                }

                std::string out_net;
                if (!(ss >> out_net))
                    throw std::runtime_error("Missing output net at line " + std::to_string(lineno));

                // Create the gate node (keyed by instance name)
                auto& gnode = g.getOrCreate(inst_name);
                gnode.name  = inst_name;
                gnode.type  = gtype;
                gnode.delay = GateDelay::get(gtype);

                // Wire: input nets → gate node → output net
                for (const auto& in_net : inputs) {
                    // Ensure input net node exists
                    if (g.nodes.find(in_net) == g.nodes.end()) {
                        // Implicit wire (from another gate's output)
                        auto& wn = g.getOrCreate(in_net);
                        wn.name = in_net;
                        wn.type = GateType::BUF; // placeholder until the driving gate is seen
                        wn.delay = 0.0;
                    }
                    g.addEdge(in_net, inst_name);
                }

                // Ensure output net node exists
                if (g.nodes.find(out_net) == g.nodes.end()) {
                    auto& wn = g.getOrCreate(out_net);
                    wn.name = out_net;
                    wn.type = GateType::BUF;
                    wn.delay = 0.0;
                }
                g.addEdge(inst_name, out_net);

            } else {
                std::cerr << "Warning: unknown keyword '" << keyword
                          << "' at line " << lineno << "\n";
            }
        }
        return g;
    }
};
