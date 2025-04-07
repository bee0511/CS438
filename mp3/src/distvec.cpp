#include "route.hpp"

class DistanceVector : public BaseRouter {
   public:
    DistanceVector(const char *topofile, const char *messagefile) : BaseRouter(topofile, messagefile) {}
    void calculatePaths(int src) override {
        // Initialize distances and previous nodes
        dist[src].clear();
        prev[src].clear();
        dist[src].resize(num_nodes + 1, INT_MAX);
        prev[src].resize(num_nodes + 1, -1);

        dist[src][src] = 0;

        for (int i = 1; i <= num_nodes - 1; i++) {
            for (auto it : g) {
                int u = it.first;
                if (dist[src][u] == INT_MAX) continue;  // Skip unreachable nodes
                for (auto edge : it.second) {
                    int v = edge.first;
                    int w = edge.second;
                    if (dist[src][u] + w < dist[src][v]) {
                        dist[src][v] = dist[src][u] + w;
                        prev[src][v] = u;
                    } else if (dist[src][u] + w == dist[src][v]) {
                        // If the distance is the same, prefer the smaller node ID
                        if (prev[src][v] == -1 || u < prev[src][v]) {
                            prev[src][v] = u;
                        }
                    }
                }
            }
        }
    }
};

int main(int argc, char **argv) {
    if (argc != 4) {
        printf("Usage: ./DistanceVector topofile messagefile changesfile\n");
        return -1;
    }

    // Parse topology file
    DistanceVector router(argv[1], argv[2]);

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
        // Run Bellman-Ford algorithm for each node
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
