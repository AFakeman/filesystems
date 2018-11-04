FROM alpine

WORKDIR /usr/src/

RUN apk add --update g++ make

ADD . .

RUN make

RUN mv ps /usr/local/bin && \
    mv lsof /usr/local/bin

CMD /usr/src/main
