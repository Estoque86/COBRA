function [G, L, CC, kAvg, kMin, kMax] = findRandomGraph(n,m)

% INPUT
% n = number of nodes;
% m = number of edges;
%
% OUTPUT
% G = adj matrix;
% L = Characteristic path length;
% CC = Mean Clustering Coefficient;
% kAvg = Mean Degree of a Node;
% kMin = Min Degree of a Node;
% kMax = Max Degree of a Node;

finish = false;
while~finish
    G = erdrey(n, m);
    G = full(G);
    if isconnected(G)
        finish = true;
    end
end

% Find the characteristic path length 

[L, EGlob, C, ELoc] = graphProperties(G);

% Find the cluster coefficient of the graph

[CC,C,D] = clustcoeff(G);

% Find the average, min and max degree

kAvg = average_degree(G);
kMax = max(sum(G,2));
kMin = min(sum(G,2));

end