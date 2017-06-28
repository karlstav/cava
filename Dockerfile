FROM sdt4docker/raspberry-pi-cross-compiler

RUN apt-get update && \
    DEBIAN_FRONTEND=noninteractive apt-get install -y \
        git \
        libfftw3-bin libfftw3-dev libasound2-dev libncursesw5-dev libpulse-dev \
        build-essential libtool automake \
        ;

