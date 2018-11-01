FROM alpine

WORKDIR /usr/src/

RUN apk add --update g++ make

ADD main.cpp .
ADD Makefile .

RUN make

CMD /usr/src/main
