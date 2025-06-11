function [y] = LEuint32toIPv4str(x)
% Convert little endian uint32 to IPv4 strings.

  x1 = bitshift(bitshift(x,24),-24);
  x2 = bitshift(bitshift(x,16),-24);
  x3 = bitshift(bitshift(x, 8),-24);
  x4 = bitshift(bitshift(x, 0),-24);

  y = sprintf('%d.%d.%d.%d\n',[x4,x3,x2,x1]');

end
