%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
% Script to sum and analyze GrB network traffic matrices.
% Notation and formulas based on:
%
% Anonymized Network Sensing Graph Challenge
%   http://graphchallenge.mit.edu/challenges
%
% Dependencies:
%   Install https://github.com/DrTimothyAldenDavis/GraphBLAS
%   Install https://github.com/Accla/d4m
%
% Usage:
%   Set fMatrixFileList to point to a file with a list of GraphBLAS tar files, such as:
%     https://graphchallenge.s3.amazonaws.com/network2024/20240507-142247.grb.tar
%   Set fAnalysisTable to where results should be written
%   Start Matlab (https://www.mathworks.com/) or Octave (https://octave.org/) and type:
%     GraphChallenge_analyses
%
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
tab = char(9); nl = char(10); q = '''';  % Set common characters

nIPv4 = 2^32;                            % Max IPv4 value

% Set file with a list of matrix files
fMatrixFileList = '../../Step2_Ingest/data/random_PCAP_GrB_anon/random_PCAP_GrB_anon_filelist.txt';

% Set analysis table file
fAnalysisTable = '../data/GraphChallenge_analyses_subrange.tsv';


Aempty = GrB(nIPv4,nIPv4);               % Create empty traffic matrix

% Set names of columns for analysis table.
analyzeCols = 'n_packets,n_links,n_sources,n_destinations,max_packets,max_source_packets,max_fan_in,max_destination_packets,max_fan_in,';

matrixFileList = StrFileRead(fMatrixFileList);       % Read list of file names
NmatrixFile = NumStr(matrixFileList);                % Number of total files to process

Asum = Aempty;                                       % Init global sum matrix
Atable = Assoc('','','');                            % Init analysis table

% Get first file name to use as the analysis table row name
firstFullFileName = StrSubindFilt(matrixFileList,1);
[firstFileDir, firstFileName, firstFileExt] = fileparts(firstFullFileName(1:end-1));

tic;                                                 % Start update_time timer

for iFile = 1:NmatrixFile                            % Loop over each file
%for iFile = 1:4                                      % Uncomment shorter loop for debugging

  iFileFullName = StrSubindFilt(matrixFileList,iFile);      % Get file name
  disp(iFileFullName(1:end-1))
  Anet = readTar(iFileFullName(1:end-1));            % Read matrix file

  NmatrixPerFile = length(Anet);                     % Number of matrices per file

  AnetSum = Aempty;                                  % Initialize file sum matrix
  for iMatrix=1:NmatrixPerFile
    AnetSum = AnetSum + Anet{iMatrix};               % Sum submatrices
  end


  Asum = Asum + AnetSum;                             % Add to global sum


end % iFile

update_time = toc;                                   % End update_time timer

tic; % analyze_time                                  % Start analysis_time timer

  n_packets = sum(Asum,'all');                       % Total packets

  % Compute update rate and add to analysis table
  update_rate = double(n_packets/update_time);
  Atable = Atable + Assoc([firstFileName ','],'update_time,update_rate,',[update_time update_rate].');
  disp(['Update time: ' num2str(update_time) ', Rate: ' num2str(update_rate)]);

  n_links = GrB.nonz(Asum,'all');                    % Unique links
  n_sources = GrB.nonz(Asum,'row');                  % Number of unique sources
  n_destinations = GrB.nonz(Asum,'col');             % Number of unique dests
  max_packets = max(Asum,[],'all');                  % Max packets on a link
  max_source_packets = max(sum(Asum,2));             % Max packets from a source
  max_fan_out = max(sum(sign(Asum),2));              % Max dests a source sends to
  max_destination_packets = max(sum(Asum,1));        % Max packets to a dest
  max_fan_in = max(sum(sign(Asum),1));               % Max sources a dest receives

  % Add to analysis table
  analyzeVals = double([n_packets n_links n_sources n_destinations ...
    max_packets max_source_packets max_fan_out max_destination_packets max_fan_in].');
  Atable = Atable + Assoc([firstFileName ','],analyzeCols,analyzeVals);

analyze_time = toc;                                 % End analyze_time timer

% Compute analyze rate and add to analysis table
analyze_rate = n_packets/analyze_time;
Atable = Atable + Assoc([firstFileName ','],'analyze_time,analyze_rate,',[analyze_time analyze_rate].');
disp(['Analyze time: ' num2str(analyze_time) ', Rate: ' num2str(double(analyze_rate))]);

Assoc2CSV(Atable,nl,tab,fAnalysisTable);                  % Save analysis table to .tsv file
