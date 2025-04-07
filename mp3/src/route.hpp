#ifndef ROUTE_HPP
#define ROUTE_HPP

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <iostream>
#include <unordered_map>
#include <vector>
#include <queue>

using namespace std;

struct Message {
    int src;            // Source node ID
    int dest;           // Destination node ID
    char message[100];  // Message text
};

class BaseRouter {
   protected:
    unordered_map<int, vector<pair<int, int>>> g;  // Graph represented as an adjacency list
    unordered_map<int, vector<int>> dist;          // Distance from the node to each other node
    unordered_map<int, vector<int>> prev;          // Previous node in the shortest path
    unordered_map<int, vector<int>> next;          // Next node in the shortest path
    vector<Message> messages;
    int num_nodes;  // Number of nodes in the graph

   public:
    BaseRouter(const char* topofile, const char* messagefile) {
        g.clear();
        dist.clear();
        num_nodes = 0;
        readTopologyFile(topofile);
        readMessageFile(messagefile);
    };
    virtual ~BaseRouter() {}

    void readTopologyFile(const char* filename);
    void readMessageFile(const char* filename);
    void writeForwardingTable(int node, FILE *fp);
    void writeMessage(int index, FILE *fp);

    // Virtual function to be implemented by derived classes
    virtual void calculatePaths(int src) = 0;

    void buildForwardingTable(int src);
    void printForwardingTable(int node);
    void printMessage(int index);
    vector<int> getPath(int src, int dest);

    int getNumNodes() { return num_nodes; }
    int getNumMessages() { return messages.size(); }

    void addEdge(int u, int v, int w);
    void removeEdge(int u, int v);
    void updateEdge(int u, int v, int w);
};

#endif