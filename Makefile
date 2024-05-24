JOBS      ?= 8
MAKEFLAGS += --no-print-directory

default: all

all:
	( mkdir -p build && cd build && cmake $(CMAKE_OPTIONS) .. && cmake --build . --config Release -j${JOBS} )

debug:
	( mkdir -p build && cd build && cmake $(CMAKE_OPTIONS) .. && cmake --build . --config Debug -j${JOBS} )

install: all
	( cd build && cmake --install . --config Release )

rpm: all
	( cd build && cpack -G RPM . )

clean:
	( rm -rf build )

