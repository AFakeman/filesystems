FROM ubuntu

WORKDIR /usr/src/

RUN apt-get update && apt-get install -yq gcc e2fslibs-dev pkg-config libfuse-dev

ADD . .

RUN make main

CMD /usr/src/main
