FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && \
    apt-get install -y build-essential make python3 php-cgi && \
    rm -rf /var/lib/apt/lists/*

WORKDIR /webserv

COPY . .

RUN make

RUN mkdir -p /webserv/uploads/files

CMD ["bash"]
