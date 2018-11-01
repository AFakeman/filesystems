test: main
	./main

coverter.o: converter.cpp converter.hpp
	g++ -std=c++14 -Wall -Werror converter.cpp -c -o converter.o

main: converter.hpp main.cpp
	g++ -std=c++14 main.cpp converter.o -o main

