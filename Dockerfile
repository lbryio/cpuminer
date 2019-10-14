#
# Dockerfile for cpuminer
# usage: docker run lbry/cpuminer --url xxxx --user xxxx --pass xxxx
# ex: docker run lbry/cpuminer --url stratum+tcp://lbc.pool.com:80 --user worker1 --pass abcdef
#
#

FROM            ubuntu:18.04

RUN             apt-get update -qq && \
                apt-get install -qqy automake libcurl4-openssl-dev git make gcc

RUN             git clone https://github.com/lbryio/cpuminer

RUN             cd cpuminer && \
                ./autogen.sh && \
                ./configure CFLAGS="-O3" && \
                make

WORKDIR         /cpuminer
ENTRYPOINT      ["./minerd"]
