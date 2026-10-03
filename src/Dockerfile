FROM ubuntu:26.04
LABEL authors="kostamak"

RUN apt-get update && apt-get install -y gcc libc-dev strace && rm -rf /var/lib/apt/lists/*

WORKDIR /app

CMD ["/bin/bash"]