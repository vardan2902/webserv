FROM ubuntu:22.04

RUN apt-get update && apt-get install -y build-essential make && rm -rf /var/lib/apt/lists/*

WORKDIR /webserv

COPY . .

RUN make

CMD ["./webserv", "configs/config_basic.conf"]
