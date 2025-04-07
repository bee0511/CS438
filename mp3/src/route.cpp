#include "route.hpp"

void BaseRouter::addEdge(int u, int v, int w) {
    g[u].push_back({v, w});
    g[v].push_back({u, w});
}

void BaseRouter::removeEdge(int u, int v) {
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

void BaseRouter::updateEdge(int u, int v, int w) {
    removeEdge(u, v);
    if (w != -999) {
        addEdge(u, v, w);
    }
}

void BaseRouter::readTopologyFile(const char *filename) {
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

void BaseRouter::readMessageFile(const char *filename) {
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

void BaseRouter::buildForwardingTable(int src) {
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
void BaseRouter::printForwardingTable(int node) {
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
void BaseRouter::printMessage(int index) {
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

vector<int> BaseRouter::getPath(int src, int dest) {
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

void BaseRouter::writeForwardingTable(int node, FILE *fp) {
    for (int i = 1; i <= num_nodes; i++) {
        if (dist[node][i] == INT_MAX) {
            continue;
        }
        fprintf(fp, "%d %d %d\n", i, next[node][i], dist[node][i]);
    }
}

void BaseRouter::writeMessage(int index, FILE *fp) {
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