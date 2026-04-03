FROM ubuntu:22.04

RUN apt-get update && apt-get install -y build-essential make && rm -rf /var/lib/apt/lists/*

WORKDIR /webserv

COPY . .

RUN make

RUN mkdir -p /webserv/uploads/files

CMD ["bash"]
