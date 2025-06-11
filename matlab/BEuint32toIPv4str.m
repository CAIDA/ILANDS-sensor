function [y] = BEuint32toIPv4str(x)
% Convert big endian uint32 to IPv4 strings.

  x1 = bitshift(bitshift(x,24),-24);
  x2 = bitshift(bitshift(x,16),-24);
  x3 = bitshift(bitshift(x, 8),-24);
  x4 = bitshift(bitshift(x, 0),-24);

  y = sprintf('%d.%d.%d.%d\n',[x1,x2,x3,x4]');

end
