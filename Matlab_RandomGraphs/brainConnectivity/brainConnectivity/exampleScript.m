% This script provides examples of how to compute characteristic path
% length, global efficiency, local efficiency, and clustering coefficient
% for a graph given its adjacency matrix.
%
% The examples are taken from the paper "Measuring Brain Connectivity," by 
% N. D. Cahill, J. Lind, and D. A. Narayan, Bulletin of the Institute of
% Combinatorics & Its Applications, 69, pp. 68-78, September 2013.
%
% These examples use the companion function graphProperties.m.
%

%% Exercise 1
%

% construct adjacency matrix
A = [0 1 0 0 1; 1 0 1 1 1; 0 1 0 1 1; 0 1 1 0 1; 1 1 1 1 0];

% compute graph properties
[L,EGlob,C,ELoc] = graphProperties(A);

% display results
fprintf('\nGraph Properties\n');
fprintf('\tCharacteristic Path Length:\t\t%6.4f\n',L);
fprintf('\tGlobal Efficiency:\t\t\t\t%6.4f\n',EGlob);
fprintf('\tClustering Coefficient:\t\t\t%6.4f\n',C);
fprintf('\tLocal Efficiency:\t\t\t\t%6.4f\n\n',ELoc);

%% Macaque Exercise
%
% First, download the file "macaque47.mat" from the site:
%   https://sites.google.com/site/bctnet/datasets
% Save the "macaque47.mat" file into your current working directory (or
% another directory on the MATLAB search path).

% load macaque data file
load('macaque47.mat');

% adjacency matrix is stored in CIJ
% compute graph properties
[L,EGlob,C,ELoc] = graphProperties(CIJ);

% display results
fprintf('\nGraph Properties\n');
fprintf('\tCharacteristic Path Length:\t\t%6.4f\n',L);
fprintf('\tGlobal Efficiency:\t\t\t\t%6.4f\n',EGlob);
fprintf('\tClustering Coefficient:\t\t\t%6.4f\n',C);
fprintf('\tLocal Efficiency:\t\t\t\t%6.4f\n\n',ELoc);
