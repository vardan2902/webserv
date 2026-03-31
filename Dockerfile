FROM ubuntu:22.04

RUN apt-get update && apt-get install -y build-essential make && rm -rf /var/lib/apt/lists/*

RUN mkdir -p /var/www/html && \
    cat <<EOF > /var/www/html/index.html
<!DOCTYPE html>
<html>
<head>
    <title>It works</title>
</head>
<body>
    <h1>It works!</h1>
</body>
</html>
EOF

WORKDIR /webserv

COPY . .

RUN make

CMD ["./webserv", "configs/config_basic.conf"]
