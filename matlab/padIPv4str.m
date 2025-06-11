function [y] = padIPv4str(x)
% Inserts leading 0 into IPv4 strings.

  sep = x(end);
  xMat = uint32(str2num(strrep(x,'.',sep)));

  x1 = xMat(1:4:end);
  x2 = xMat(2:4:end);
  x3 = xMat(3:4:end);
  x4 = xMat(4:4:end);

  y = sprintf(['%3.3d.%3.3d.%3.3d.%3.3d' sep],[x1,x2,x3,x4]');

end
