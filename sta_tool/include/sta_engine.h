#pragma once
#include "timing_graph.h"
#include <algorithm>
#include <queue>
#include <stack>
#include <stdexcept>
#include <functional>
#include <queue>
#include <limits>

class STAEngine {
public:
    // ----------------------------------------------------------------
    // Main entry point: runs full STA on the graph
    // ----------------------------------------------------------------
    static STAResult analyze(TimingGraph& g) {
        topoSort(g);
        computeArrivalTimes(g);
        computeRequiredTimes(g);
        computeSlacks(g);
        return buildResult(g);
    }

private:
    // ----------------------------------------------------------------
    // Topological sort (Kahn's algorithm — detects cycles)
    // ----------------------------------------------------------------
    static void topoSort(TimingGraph& g) {
        // Build in-degree map
        std::unordered_map<std::string, int> indegree;
        for (auto& [name, node] : g.nodes)
            indegree[name] = 0;
        for (auto& [name, node] : g.nodes)
            for (const auto& succ : node.fanout)
                indegree[succ]++;

        std::queue<std::string> q;
        for (auto& [name, deg] : indegree)
            if (deg == 0) q.push(name);

        g.topo_order.clear();
        while (!q.empty()) {
            std::string cur = q.front(); q.pop();
            g.topo_order.push_back(cur);
            for (const auto& succ : g.nodes[cur].fanout) {
                if (--indegree[succ] == 0)
                    q.push(succ);
            }
        }
        if (g.topo_order.size() != g.nodes.size())
            throw std::runtime_error("Cycle detected in netlist — not a valid combinational path");
    }

    // ----------------------------------------------------------------
    // Forward pass: compute arrival times
    // AT(gate) = max over inputs of (AT(input) + delay(input)) + delay(gate)
    // ----------------------------------------------------------------
    static void computeArrivalTimes(TimingGraph& g) {
        // Init
        for (auto& [name, node] : g.nodes)
            node.arrival_time = (node.type == GateType::INPUT) ? 0.0
                              : -std::numeric_limits<double>::infinity();

        for (const auto& name : g.topo_order) {
            auto& node = g.nodes[name];
            if (node.type == GateType::INPUT) continue;

            double max_pred = 0.0;
            for (const auto& pred_name : node.fanin) {
                const auto& pred = g.nodes.at(pred_name);
                max_pred = std::max(max_pred, pred.arrival_time + pred.delay);
            }
            node.arrival_time = (node.fanin.empty() ? 0.0 : max_pred) + node.delay;
        }
    }

    // ----------------------------------------------------------------
    // Backward pass: compute required times
    // RT(gate) = min over successors of (RT(succ) - delay(succ))
    // Primary outputs: RT = clock_period
    // ----------------------------------------------------------------
    static void computeRequiredTimes(TimingGraph& g) {
        // Init
        for (auto& [name, node] : g.nodes) {
            node.required_time = (node.type == GateType::OUTPUT)
                ? g.clock_period_ps
                : std::numeric_limits<double>::max();
        }

        // Process in reverse topological order
        for (auto it = g.topo_order.rbegin(); it != g.topo_order.rend(); ++it) {
            auto& node = g.nodes[*it];

            double min_succ = std::numeric_limits<double>::max();
            for (const auto& succ_name : node.fanout) {
                const auto& succ = g.nodes.at(succ_name);
                // RT(node output) = RT(succ) - delay(succ)
                min_succ = std::min(min_succ, succ.required_time - succ.delay);
            }
            if (node.fanout.empty()) {
                // Primary output — already set
            } else {
                node.required_time = std::min(node.required_time, min_succ);
            }
        }
    }

    // ----------------------------------------------------------------
    // Slack computation
    // ----------------------------------------------------------------
    static void computeSlacks(TimingGraph& g) {
        for (auto& [name, node] : g.nodes)
            node.slack = node.required_time - node.arrival_time;
    }

    // ----------------------------------------------------------------
    // Find critical path (max arrival time at any output → trace back)
    // ----------------------------------------------------------------
    static TimingPath traceCriticalPath(TimingGraph& g, const std::string& sink) {
        TimingPath path;
        std::string cur = sink;
        path.total_delay = g.nodes[sink].arrival_time;
        path.slack       = g.nodes[sink].slack;

        // Greedily trace back through the highest-arrival-time predecessor
        while (true) {
            path.nodes.push_back(cur);
            const auto& node = g.nodes[cur];
            if (node.fanin.empty()) break;

            std::string best_pred;
            double best_at = -1e18;
            for (const auto& pred_name : node.fanin) {
                double at = g.nodes[pred_name].arrival_time + g.nodes[pred_name].delay;
                if (at > best_at) {
                    best_at  = at;
                    best_pred = pred_name;
                }
            }
            cur = best_pred;
        }
        std::reverse(path.nodes.begin(), path.nodes.end());
        return path;
    }

    // ----------------------------------------------------------------
    // Build the final STAResult
    // ----------------------------------------------------------------
    static STAResult buildResult(TimingGraph& g) {
        STAResult res;
        res.wns = std::numeric_limits<double>::max();
        res.tns = 0.0;
        res.num_violations = 0;

        // Find worst output
        std::string worst_output;
        double worst_at = -1e18;
        for (auto& [name, node] : g.nodes) {
            if (node.type == GateType::OUTPUT) {
                if (node.arrival_time > worst_at) {
                    worst_at     = node.arrival_time;
                    worst_output = name;
                }
                if (node.slack < res.wns) res.wns = node.slack;
                if (node.slack < 0) {
                    res.tns += node.slack;
                    res.num_violations++;
                }
            }
        }

        if (!worst_output.empty())
            res.critical_path = traceCriticalPath(g, worst_output);

        // Gather all output paths, sorted by slack
        for (auto& [name, node] : g.nodes) {
            if (node.type == GateType::OUTPUT)
                res.all_paths.push_back(traceCriticalPath(g, name));
        }
        std::sort(res.all_paths.begin(), res.all_paths.end(),
                  [](const TimingPath& a, const TimingPath& b){ return a.slack < b.slack; });

        return res;
    }

    // Helper for Kahn's
};

// Re-include queue for Kahn's
#include <queue>
