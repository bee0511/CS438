#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

class DistanceVector {
   private:
    unordered_map<int, vector<pair<int, int>>> g;  // Graph represented as an adjacency list
    unordered_map<int, vector<int>> dist;          // Distance from the node to each other node
    unordered_map<int, vector<int>> prev;          // Previous node in the shortest path
    unordered_map<int, vector<int>> next;          // Next node in the shortest path
    vector<Message> messages;
    int num_nodes;  // Number of nodes in the graph

   public:
    DistanceVector(const char *topofile, const char *messagefile) {
        g.clear();
        dist.clear();
        num_nodes = 0;
        readTopologyFile(topofile);
        readMessageFile(messagefile);
    };
    void readTopologyFile(const char *filename);
    void readMessageFile(const char *filename);

    void printForwardingTable(int node);
    void printMessage(int index);

    void writeForwardingTable(int node, FILE *fp);
    void writeMessage(int index, FILE *fp);

    int getNumNodes();
    int getNumMessages();

    void addEdge(int u, int v, int w);
    void removeEdge(int u, int v);
    void updateEdge(int u, int v, int w);

    void BellmanFord(int src);
    void buildForwardingTable(int src);
    vector<int> getPath(int src, int dest);
};

int DistanceVector::getNumNodes() {
    return num_nodes;
}
int DistanceVector::getNumMessages() {
    return messages.size();
}

void DistanceVector::addEdge(int u, int v, int w) {
    g[u].push_back({v, w});
    g[v].push_back({u, w});
}

void DistanceVector::removeEdge(int u, int v) {
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

void DistanceVector::updateEdge(int u, int v, int w) {
    removeEdge(u, v);
    if (w != -999) {
        addEdge(u, v, w);
    }
}

void DistanceVector::BellmanFord(int src) {
    // Initialize distances and previous nodes
    dist[src].clear();
    prev[src].clear();
    dist[src].resize(num_nodes + 1, INT_MAX);
    prev[src].resize(num_nodes + 1, -1);

    dist[src][src] = 0;

    for (int i = 1; i <= num_nodes - 1; i++) {
        for (auto it : g) {
            int u = it.first;
            for (auto edge : it.second) {
                int v = edge.first;
                int w = edge.second;
                if (dist[src][u] != INT_MAX && dist[src][u] + w < dist[src][v]) {
                    dist[src][v] = dist[src][u] + w;
                    prev[src][v] = u;
                } else if (dist[src][u] != INT_MAX && dist[src][u] + w == dist[src][v]) {
                    // If the distance is the same, prefer the smaller node ID
                    if (prev[src][v] == -1 || u < prev[src][v]) {
                        prev[src][v] = u;
                    }
                }
            }
        }
    }
}

void DistanceVector::readTopologyFile(const char *filename) {
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

void DistanceVector::readMessageFile(const char *filename) {
    /* Input format: <source node ID> <dest node ID> <message text>
     * Example:
     * 2 1 here is a message from 2 to 1
     */
    FILE *fp = fopen(filename, "r");
    if (fp == NULL) {
        printf("Error opening file %s\n", filename);
        return;
    }

    int src, dest;
    char message[100];
    while (fscanf(fp, "%d %d %[^\n]", &src, &dest, message) != EOF) {
        Message msg;
        msg.src = src;
        msg.dest = dest;
        strncpy(msg.message, message, sizeof(msg.message));
        messages.push_back(msg);
    }
    fclose(fp);
}

void DistanceVector::buildForwardingTable(int src) {
    next[src].clear();
    next[src].resize(num_nodes + 1, -1);

    for (int i = 1; i <= num_nodes; i++) {
        if (dist[src][i] == INT_MAX) {
            next[src][i] = -1;  // No path found
            continue;
        }
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
void DistanceVector::printForwardingTable(int node) {
    printf("Forwarding table for node %d:\n", node);
    printf("Dest\tNext Hop\tCost\n");
    for (int i = 1; i <= num_nodes; i++) {
        if (dist[node][i] == INT_MAX) {
            continue;
        }
        printf("%d\t%d\t\t%d\n", i, next[node][i], dist[node][i]);
    }
}

/*
    Print the message at the given index with the format:
    from <x> to <y> cost <path_cost> hops <hop1> <hop2> <...> message <message>
    if the path is not found, print:
    from <x> to <y> costinfinite hops unreachable message <message>
*/
void DistanceVector::printMessage(int index) {
    if (index < 0 || index >= messages.size()) {
        printf("Invalid message index\n");
        return;
    }
    Message msg = messages[index];

    int src = msg.src;
    int dest = msg.dest;
    int path_cost = dist[src][dest];
    vector<int> path = getPath(src, dest);
    if (path.empty()) {
        printf("from %d to %d cost infinite hops unreachable message %s\n", src, dest, msg.message);
        return;
    }
    printf("from %d to %d cost %d hops ", src, dest, path_cost);
    for (int i = 0; i < path.size(); i++) {
        printf("%d ", path[i]);
    }
    printf("message %s\n", msg.message);
}

vector<int> DistanceVector::getPath(int src, int dest) {
    vector<int> path;
    if (dist[src][dest] == INT_MAX) {
        return path;  // No path found
    }
    int cur = src;
    while (cur != dest) {
        path.push_back(cur);
        cur = next[cur][dest];
    }
    // Do not record the last node
    return path;
}

void DistanceVector::writeForwardingTable(int node, FILE *fp) {
    for (int i = 1; i <= num_nodes; i++) {
        if (dist[node][i] == INT_MAX) {
            continue;
        }
        fprintf(fp, "%d %d %d\n", i, next[node][i], dist[node][i]);
    }
}

void DistanceVector::writeMessage(int index, FILE *fp) {
    if (index < 0 || index >= messages.size()) {
        fprintf(fp, "Invalid message index\n");
        return;
    }
    Message msg = messages[index];

    int src = msg.src;
    int dest = msg.dest;
    int path_cost = dist[src][dest];
    vector<int> path = getPath(src, dest);
    if (path.empty()) {
        fprintf(fp, "from %d to %d cost infinite hops unreachable message %s\n", src, dest, msg.message);
        return;
    }
    fprintf(fp, "from %d to %d cost %d hops ", src, dest, path_cost);
    for (int i = 0; i < path.size(); i++) {
        fprintf(fp, "%d ", path[i]);
    }
    fprintf(fp, "message %s\n", msg.message);
}

int main(int argc, char **argv) {
    if (argc != 4) {
        printf("Usage: ./DistanceVector topofile messagefile changesfile\n");
        return -1;
    }

    // Parse topology file
    DistanceVector ls(argv[1], argv[2]);

    // Open the changes file
    // File format: <ID of a node> <ID of another node> <cost of the link between them>
    FILE *fp = fopen(argv[3], "r");
    if (fp == NULL) {
        printf("Error opening file %s\n", argv[3]);
        return -1;
    }
    FILE *fpOut;
    fpOut = fopen("output.txt", "w");
    if (fpOut == NULL) {
        printf("Error opening file output.txt\n");
        fclose(fp);
        return -1;
    }

    int u = -1;
    int v = -1;
    int w = -1;
    do {
        if (u != -1 && v != -1 && w != -1) {
            // Update the edge in the graph
            ls.updateEdge(u, v, w);
        }
        for (int i = 1; i <= ls.getNumNodes(); i++) {
            ls.BellmanFord(i);
        }
        for (int i = 1; i <= ls.getNumNodes(); i++) {
            ls.buildForwardingTable(i);
            ls.printForwardingTable(i);
            ls.writeForwardingTable(i, fpOut);
        }
        for (int i = 0; i < ls.getNumMessages(); i++) {
            ls.printMessage(i);
            ls.writeMessage(i, fpOut);
        }
    } while (fscanf(fp, "%d %d %d", &u, &v, &w) != EOF);

    fclose(fp);
    fclose(fpOut);

    return 0;
}
