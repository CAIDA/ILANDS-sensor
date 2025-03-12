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
%   Set SUBRANGE = 1 to process subranges or set SUBRANGE = 0
%   Set IPrangeFile to point to a GraphBLAS tar file of diagonal matrices for selecting ranges
%   Start Matlab (https://www.mathworks.com/) or Octave (https://octave.org/) and type:
%     GraphChallenge_analyses_subrange
%
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
tab = char(9); nl = char(10); q = '''';  % Set common characters

nIPv4 = 2^32;                            % Max IPv4 value

% Set file with a list of matrix files
fMatrixFileList = getenv('GC_FILELIST');

% Set analysis table file
fAnalysisTable = '/data/Pipeline/Step0_Raw/data/GraphChallenge_analyses_subrange-select.tsv';

SUBRANGE = 1;                            % 1 = Do subrange analysis
RANGEFILES = 1;                          % 1 = Write subrange tar files.

Aempty = GrB(nIPv4,nIPv4);               % Create empty traffic matrix

% Set names of columns for analysis table.
analyzeCols = 'n_packets,n_links,n_sources,n_destinations,max_packets,max_source_packets,max_fan_out,max_destination_packets,max_fan_in,';

if SUBRANGE
  %IPrangeFile = 'IPlist/NonRoutableBogonIPList-LE-2.tar';   % IP Ranges Little Endian (LE)
  IPrangeFile = '/data/Pipeline/Step0_Raw/data/iplists/LocalIPList-4.tar';   % IP Ranges Big Endian (BE)

  % Read in IP ranges.
  AsrcRange = readTar(IPrangeFile);                  % IP Ranges
  AsrcRange{end+1} = Aempty;                         % Init "everythine else" range

  % Create sum of ranges for subtracting off to get the "everything else" range
  for i=1:(length(AsrcRange)-1)
    AsrcRange{end} = AsrcRange{end} + AsrcRange{i};
  end
  AdestRange = AsrcRange;                            % Use same source and dest ranges

  NsrcRange = length(AsrcRange);                     % Number of source ranges
  NdestRange = length(AdestRange);                   % Number of dest ranges
end

matrixFileList = StrFileRead(fMatrixFileList);       % Read list of file names
NmatrixFile = NumStr(matrixFileList);                % Number of total files to process

Asum = Aempty;                                       % Init global sum matrix
Atable = Assoc('','','');                            % Init analysis table

% Get first file name to use as the analysis table row name
firstFullFileName = StrSubindFilt(matrixFileList,1);
[firstFileDir, firstFileName, firstFileExt] = fileparts(firstFullFileName(1:end-1));

% Set filename for subrange matrix tar output if RANGEFILES = 1 above.
if RANGEFILES
  fTarFileName = fullfile(filesep, 'data', 'tar', 'range', 'tmp', [firstFileName '.tar']);
end

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

  if SUBRANGE                                        % Perform sub range analysis
  for i=1:NsrcRange                                  % Loop over each source range

    AsumSrc = AsrcRange{i}*Asum;                     % Select sources
    if (i == NsrcRange)
      AsumSrc = GrB.prune(Asum - AsumSrc);           % Select "everything else" sources
    end

    for j=1:NdestRange                               % Loop over each dest range

      AsumSrcDest = AsumSrc*AdestRange{j};           % Selct dests
      if (j == NdestRange)
        AsumSrcDest = GrB.prune(AsumSrc - AsumSrcDest);  % Select "everthing else" dests
      end

      if RANGEFILES
        fGrbFileName = [num2str(i) '_' num2str(j) '.grb'];
        fGrbFilePath = fullfile(filesep, 'data', 'tar', 'tmp', fGrbFileName);
        AsumSrcDestBlob = GrB.serialize(AsumSrcDest);
      
        grbfd = fopen(fGrbFilePath, 'w');
        fwrite(grbfd, AsumSrcDestBlob, 'uint8');
        fclose(grbfd);
      
        command = ['tar --directory=/data/tar/tmp -rf ' fTarFileName ' ' fGrbFileName];
        system(command);
      end % RANGEFILES

      clear AsumSrcDestBlob 

      sub_n_packets{i,j}= sum(AsumSrcDest,'all');                  % Number of packets
      sub_n_links{i,j} = GrB.nonz(AsumSrcDest,'all');              % Unique links
      sub_n_sources{i,j}  = GrB.nonz(AsumSrcDest,'row');           % Number of unique sources
      sub_n_destinations{i,j}  = GrB.nonz(AsumSrcDest,'col');      % Number of unique dests
      sub_max_packets{i,j} = max(AsumSrcDest,[],'all');            % Max packets on a link
      sub_max_source_packets{i,j} = max(sum(AsumSrcDest,2));       % Max packets from a source
      sub_max_fan_out{i,j} = max(sum(sign(AsumSrcDest),2));        % Max dests a source sends to
      sub_max_destination_packets{i,j} = max(sum(AsumSrcDest,1));  % Max packets to a dest
      sub_max_fan_in{i,j} = max(sum(sign(AsumSrcDest),1));         % Max sources a dest receives

      % Add to analysis table
      sub_analyzeCols = CatStr(CatStr('sub,','_',analyzeCols),'_',[num2str(i) '_' num2str(j) ',']);
      sub_analyzeVals = double([sub_n_packets{i,j} sub_n_links{i,j} sub_n_sources{i,j} sub_n_destinations{i,j} ...
         sub_max_packets{i,j} sub_max_source_packets{i,j} sub_max_fan_out{i,j} ... 
         sub_max_destination_packets{i,j} sub_max_fan_in{i,j}].');
      Atable = Atable + Assoc([firstFileName ','],sub_analyzeCols,sub_analyzeVals);

    end  % j
  end  % i
  end % SUBRANGE
analyze_time = toc;                                 % End analyze_time timer

if RANGEFILES
  command = ['rm -f /data/tar/tmp/*.grb'];
  system(command);
end % RANGEFILES

% Compute analyze rate and add to analysis table
analyze_rate = n_packets/analyze_time;
Atable = Atable + Assoc([firstFileName ','],'analyze_time,analyze_rate,',[analyze_time analyze_rate].');
disp(['Analyze time: ' num2str(analyze_time) ', Rate: ' num2str(double(analyze_rate))]);

Assoc2CSV(Atable,nl,tab,fAnalysisTable);                  % Save analysis table to .tsv file
