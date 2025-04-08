#include "route.hpp"

class LinkState : public BaseRouter {
   public:
    LinkState(const char *topofile, const char *messagefile) : BaseRouter(topofile, messagefile) {}
    void calculatePaths(int src) override {
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
                int new_dist = dist[src][u] + w;

                if (new_dist < dist[src][v]) {
                    dist[src][v] = new_dist;
                    prev[src][v] = u;
                    pq.push({dist[src][v], v});
                }
                // If there is a tie, choose the lowest node ID
                else if (new_dist == dist[src][v]) {
                    if (prev[src][v] == -1 || u < prev[src][v]) {
                        prev[src][v] = u;
                        pq.push({dist[src][v], v});
                    }
                }
            }
        }
    }
};

int main(int argc, char **argv) {
    if (argc != 4) {
        printf("Usage: ./linkstate topofile messagefile changesfile\n");
        return -1;
    }

    // Parse topology file
    LinkState router(argv[1], argv[2]);

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
            router.updateEdge(u, v, w);
        }
        // Run Dijkstra's algorithm for each node
        for (int i = 1; i <= router.getNumNodes(); i++) {
            router.calculatePaths(i);
        }
        for (int i = 1; i <= router.getNumNodes(); i++) {
            router.buildForwardingTable(i);
            router.printForwardingTable(i);
            router.writeForwardingTable(i, fpOut);
        }
        for (int i = 0; i < router.getNumMessages(); i++) {
            router.printMessage(i);
            router.writeMessage(i, fpOut);
        }
    } while (fscanf(fp, "%d %d %d", &u, &v, &w) != EOF);

    fclose(fp);
    fclose(fpOut);

    return 0;
}
