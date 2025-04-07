#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>  // For sleep function

#include <iostream>
#include <queue>
#include <unordered_map>
#include <vector>

using namespace std;

struct Message {
    int src;            // Source node ID
    int dest;           // Destination node ID
    char message[100];  // Message text
};

struct EdgeChange {
    int src;   // Source node ID
    int dest;  // Destination node ID
    int cost;  // Cost of the link between them
};

class LinkState {
   private:
    unordered_map<int, vector<pair<int, int>>> g;  // Graph represented as an adjacency list
    unordered_map<int, vector<int>> dist;          // Distance from the node to each other node
    unordered_map<int, vector<int>> prev;          // Previous node in the shortest path
    unordered_map<int, vector<int>> next;          // Next node in the shortest path
    vector<Message> messages;
    int num_nodes;  // Number of nodes in the graph

   public:
    LinkState(const char *topofile) {
        g.clear();
        dist.clear();
        num_nodes = 0;
        readTopologyFile(topofile);
    };
    void readTopologyFile(const char *filename);
    void printForwardingTable(int node);

    int getNumNodes();

    void addEdge(int u, int v, int w);
    void removeEdge(int u, int v);
    void dijkstra(int src);
    void buildForwardingTable(int src);
};

int LinkState::getNumNodes() {
    return num_nodes;
}

void LinkState::addEdge(int u, int v, int w) {
    g[u].push_back({v, w});
    g[v].push_back({u, w});
}

void LinkState::removeEdge(int u, int v) {
    for (auto it = g[u].begin(); it != g[u].end(); ++it) {
        if (it->first == v) {
            g[u].erase(it);
            break;
        }
    }
    for (auto it = g[v].begin(); it != g[v].end(); ++it) {
        if (it->first == u) {
            g[v].erase(it);
            break;
        }
    }
}

void LinkState::dijkstra(int src) {
    // Initialize distances
    dist[src].clear();
    prev[src].clear();
    dist[src].resize(num_nodes + 1, INT_MAX);
    prev[src].resize(num_nodes + 1, -1);

    dist[src][src] = 0;
    prev[src][src] = src;
    vector<bool> visited(num_nodes + 1, false);

    // Priority queue to select the node with the smallest distance
    priority_queue<pair<int, int>, vector<pair<int, int>>, greater<pair<int, int>>> pq;
    pq.push({0, src});  // {distance, node}
    while (!pq.empty()) {
        int u = pq.top().second;
        pq.pop();

        if (visited[u]) continue;
        visited[u] = true;

        for (auto &edge : g[u]) {
            int v = edge.first;
            int w = edge.second;

            if (dist[src][u] + w < dist[src][v]) {
                dist[src][v] = dist[src][u] + w;
                prev[src][v] = u;
                pq.push({dist[src][v], v});
            }
            // If there is a tie, choose the lowest node ID
            else if (dist[src][u] + w == dist[src][v]) {
                if (prev[src][v] == -1 || u < prev[src][v]) {
                    prev[src][v] = u;
                    pq.push({dist[src][v], v});
                }
            }
        }
    }
}

void LinkState::readTopologyFile(const char *filename) {
    /* Input format: <ID of a node> <ID of another node> <cost of the link between them>
     * Example:
     * 1 2 8
     * 2 3 3
     * 2 5 4
     * 4 1 1
     * 4 5 1
     */
    FILE *fp = fopen(filename, "r");
    if (fp == NULL) {
        printf("Error opening file %s\n", filename);
        return;
    }

    int u, v, w;
    while (fscanf(fp, "%d %d %d", &u, &v, &w) != EOF) {
        addEdge(u, v, w);
        if (u > num_nodes) {
            num_nodes = u;
        }
        if (v > num_nodes) {
            num_nodes = v;
        }
    }
    fclose(fp);
}

void LinkState::buildForwardingTable(int src) {
    next[src].clear();
    next[src].resize(num_nodes + 1, -1);

    for (int i = 1; i <= num_nodes; i++) {
        if (i == src) {
            next[src][i] = src;
            continue;
        }
        if (prev[src][i] == src) {
            next[src][i] = i;
            continue;
        }
        int cur = prev[src][i];
        while (prev[src][cur] != src) {
            cur = prev[src][cur];
        }
        next[src][i] = cur;
    }
}

// Print the forwarding table for a given node
void LinkState::printForwardingTable(int node) {
    printf("Forwarding table for node %d:\n", node);
    printf("Dest\tNext Hop\tCost\n");
    for (int i = 1; i <= num_nodes; i++) {
        printf("%d\t%d\t\t%d\n", i, next[node][i], dist[node][i]);
    }
}

int main(int argc, char **argv) {
    if (argc != 4) {
        printf("Usage: ./linkstate topofile messagefile changesfile\n");
        return -1;
    }

    // Parse topology file
    LinkState ls(argv[1]);

    // Run Dijkstra's algorithm for each node
    for (int i = 1; i <= ls.getNumNodes(); i++) {
        ls.dijkstra(i);
    }
    for (int i = 1; i <= ls.getNumNodes(); i++) {
        ls.buildForwardingTable(i);
        ls.printForwardingTable(i);
    }

    FILE *fpOut;
    fpOut = fopen("output.txt", "w");

    fclose(fpOut);

    return 0;
}
