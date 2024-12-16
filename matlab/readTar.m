function Anet = readTar(tarFile)
    tf = fopen(tarFile, 'r');

    if tf == -1
        fprintf('Error opening tar file: %s.\n', tarFile);
        return;
    end

    tfdata = fread(tf, '*uint8');
    fclose(tf);

    tarSize = size(tfdata,1);
    Anet = cell(0,1);
    offset = 0;
    numFiles = 0;

    while offset + 512 < tarSize
        fileName = strtrim(char(tfdata(offset+1:offset+100)));

        % is this the end of the archive?
        if all(fileName == 0)
            break;
        end

        % did we find something?
        if isempty(fileName)
            fprintf('Error getting filename for file %d.\n', numFiles+1);
            break;
        end

        % tar saves size at position 125 in octal, as character
        [fileSize, ret] = sscanf(char(tfdata(offset+125:offset+135)), "%011o");

        if ret ~= 1
            fprintf('Error getting filesize for file %d.\n', numFiles+1);
            break;
        end

        offset = offset + 512;                                     % move our pointer
        blob = tfdata(offset+1:offset+fileSize);                   % serialized blob should be here, we hope
        numFiles = numFiles + 1;

        try
            Anet{numFiles} = GrB.deserialize(blob);                % let's see if it is
        catch
            fprintf('Error deserializing %2d @ %10d (size %10d)\n', numFiles, offset, fileSize);
            break;
        end
 
        offset = offset + fileSize;                                % move our pointer to the next file
        aligned = mod(offset, 512);

        if aligned ~= 0
           offset = offset + 512 - aligned;                        % pad out to 512 byte alignment
        end        
    end
end

