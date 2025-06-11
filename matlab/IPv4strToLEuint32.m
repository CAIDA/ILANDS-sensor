function [y] = IPv4strToLEuint32(x)
% Convert IPv4 strings to little endian uint32.

  sep = x(end);
  xMat = uint32(str2num(strrep(x,'.',sep)));

  x1 = bitshift(xMat(1:4:end),24);
  x2 = bitshift(xMat(2:4:end),16);
  x3 = bitshift(xMat(3:4:end), 8);
  x4 = bitshift(xMat(4:4:end), 0);

  y =  x1 + x2 + x3 + x4;

end
