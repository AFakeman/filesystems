CARGS=-Wall -Werror --std=c++14

all: lsof ps

lsof: lsof.cpp proc.hpp proc.o
	g++ lsof.cpp proc.o ${CARGS} -o lsof

ps: ps.cpp proc.hpp proc.o
	g++ ps.cpp proc.o ${CARGS} -o ps

proc.o: proc.cpp proc.hpp
	g++ proc.cpp ${CARGS} -c -o proc.o

build_docker:
	docker build . -t ps-lsof

run_docker_ps: build_docker
	docker run --rm -it ps-lsof ps

run_docker_lsof: build_docker
	docker run --rm -it ps-lsof lsof

clean:
	rm lsof ps *.o
