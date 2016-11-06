function [ L, EGlob, C, ELoc ] = graphProperties( varargin )
% graphProperties: compute properties of a graph from its adjacency matrix
% usage: [L,EGlob,C,ELoc] = graphProperties(A);
%
% arguments: 
%   A (nxn) - adjacency matrix of a graph G
%
%   L (scalar) - characteristic path length
%   EGlob (scalar) - global efficiency
%   C (scalar) - clustering coefficient
%   ELoc (scalar) - local efficiency
%

% author: Nathan D. Cahill
% email: nathan.cahill@rit.edu
% date: 14 Jan 2013

% get adjacency matrix from list of inputs
A = parseInputs(varargin{:});

% get number of vertices
n = size(A,1);

% shortest path distances between each node
D = graphallshortestpaths(A);

% characteristic path length
L = sum(D(:))/(n*(n-1));

% global efficiency
EGlob = (sum(sum(1./(D+eye(n)))) - n)/(n*(n-1));

% subgraphs of G induced by the neighbors of each vertex
[M,k] = subgraphs(A);

% local clustering coefficient in each subgraph
CLoc = zeros(n,1);
for i = 1:n
    CLoc(i) = sum(M{i}(:))/(numel(k{i})*(numel(k{i})-1));
end

% clustering coefficient
C = mean(CLoc);

% local efficiency of each subgraph
ELocSG = zeros(n,1);
for i = 1:n
    % distance matrix and number of vertices for current subgraph
    DSG = D(k{i},k{i});
    nSG = numel(k{i});
    % efficiency of current subgraph
    ELocSG(i) = (sum(sum(1./(DSG+eye(nSG)))) - nSG)/(nSG*(nSG-1));
end

% local efficiency of graph
ELoc = mean(ELocSG);

end

function A = parseInputs(varargin)
% parseInputs: test that input is an adjacency matrix

% check number of inputs
narginchk(1,1);

% get first input
A = varargin{1};

% test to make sure A is a square matrix
if ~isnumeric(A) || ~ismatrix(A) || size(A,1)~=size(A,2)
    error([mfilename,':ANotSquare'],'Input must be a square matrix.');
end

% test to make sure A only contains zeros and ones
if any((A~=0)&(A~=1))
    error([mfilename,':ANotValid'],...
        'Input matrix must contain only zeros and ones.');
end

% change A to sparse if necessary
if ~issparse(A)
    A = sparse(A);
end

end

function [M,k] = subgraphs(A)
% subgraphs: compute adjacency matrices for each vertex in a graph
% usage: [M,k] = subgraphs(A);
%
% arguments: 
%   A - (nxn) adjacency matrix of a graph G
%
%   M - (nx1) cell array containing adjacency matrices of the subgraphs
%       corresponding to neighbors of each vertex. For example, M{j} is 
%       the adjacency matrix of the subgraph of G corresponding to the
%       neighbors of the jth vertex of G, and k{j} is the list of vertices 
%       of G that are in the subgraph (and represent the corresponding
%       rows/columns of M{j})
%       

% author: Nathan D. Cahill
% email: nathan.cahill@rit.edu
% date: 14 Jan 2013

% number of vertices
n = size(A,1);

% initialize indices of neighboring vertices, and adjacency matrices
k = cell(n,1);
M = cell(n,1);

% loop through each vertex, determining its neighbors
for i=1:n
    
    % find indices of neighbors of vertex v_i
    k1 = find(A(i,:)); % vertices with edge beginning at v_i
    k2 = find(A(:,i)); % vertices with edge ending at v_i
    k{i} = union(k1,k2); % vertices in neighborhood of v_i
    
    % extract submatrix describing adjacency of neighbors of v_i
    M{i} = A(k{i},k{i});
    
end

end