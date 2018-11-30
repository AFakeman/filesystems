MAKE_CPPFLAGS= --std=c++17 -Wall -Werror `pkg-config fuse --cflags --libs` ${CPPFLAGS} -g

main: main.cpp Ext2Driver.cpp Ext2Driver.hpp
	g++  main.cpp Ext2Driver.cpp -o main ${MAKE_CPPFLAGS}

test: build_test
	./build_test

build_test: test.cpp Ext2Driver.cpp Ext2Driver.hpp prove.hpp
	g++ test.cpp Ext2Driver.cpp -o build_test ${MAKE_CPPFLAGS}

ext2.img:
	docker run --rm -it -v ${PWD}:/var/pwd ubuntu sh -c \
	"apt-get update && apt-get install -y genext2fs && \
	genext2fs -b 10 -d /var/pwd/to_volume /var/pwd/ext2.img"

docker_build: Dockerfile main.cpp
	docker build . -t filesystems:ext2fuse

docker_run: docker_build ext2.img
	docker run -it --rm \
		-v ${PWD}/ext2.img:/usr/src/ext2.img \
		filesystems:ext2fuse \
		sh -c "/usr/src/main ext2.img & sh"

clean:
	rm -rf *.dSYM *.o main && docker rmi filesystems:ext2fuse
