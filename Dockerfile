FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && \
    apt-get install -y build-essential make python3 php-cgi && \
    rm -rf /var/lib/apt/lists/*

WORKDIR /webserv

COPY . .

RUN make

RUN mkdir -p \
    /webserv/configs/allow_methods/uploads \
    /webserv/configs/client_max_body_size/uploads \
    /webserv/configs/upload/uploads/files \
    /webserv/configs/errors/www/noindex

CMD ["bash"]
