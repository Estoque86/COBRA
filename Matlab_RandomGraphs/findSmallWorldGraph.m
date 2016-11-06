function [G, L, CC, kAvg, kMin, kMax, numEdg] = findSmallWorldGraph(n,k,p)

% INPUT
% n = number of nodes;
% m = number of nearest-neighbours to connect;
% p = probability of adding a shortcut in a given row;
%
% OUTPUT
% G = adj matrix;
% L = Characteristic path length;
% CC = Mean Clustering Coefficient;
% kAvg = Mean Degree of a Node;
% kMin = Min Degree of a Node;
% kMax = Max Degree of a Node;
% numEdg = Number of Edges.


finish = false;
while~finish
    G = smallw(n, k, p);
    G = full(G);
    if isconnected(G)
        finish = true;
    end
end

% Find the characteristic path length 

[L, EGlob, C, ELoc] = graphProperties(G);

% Find the cluster coefficient of the graph

[CC,C,D] = clustcoeff(G);

% Find the average, max and min degree, and numEdges

kAvg = average_degree(G);
kMax = max(sum(G,2));
kMin = min(sum(G,2));

numEdg = numedges(G);

end