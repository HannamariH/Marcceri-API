FROM node:lts-alpine3.14
#ENTRYPOINT ["bin/usemarcon"]

COPY . build
WORKDIR usemarcon

RUN chown -R node:node /build \
    && apk add -U --no-cache gcc \
    && apk add --no-cache --virtual .build-deps \
    sudo autoconf perl automake libtool make g++ file \
    && sudo -u node sh -c 'cd /build/ && rm aclocal.m4 && aclocal \
    && ./buildconf.sh' \
    && sudo -u node sh -c 'cd /build/pcre && chmod +x configure depcomp \
    Detrail install-sh perltest.pl PrepareRelease RunGrepTest RunTest \
    132html && ./configure --enable-utf8 --enable-unicode-properties \
    --disable-shared --disable-cpp && make' \
    && sudo -u node sh -c 'cd /build && chmod +x configure install-sh \
    mkinstalldirs && ./configure --prefix=/usemarcon && \
    make' \
    && sh -c 'cd /build && make install' \
    && apk del .build-deps \
    && rm -rf /build tmp/* /var/cache/apk/*

COPY usemarcon-conversion-rules /usemarcon
RUN mkdir /usemarcon/uploads

WORKDIR /usr/src/app

COPY package*.json ./
RUN npm install
COPY index.js ./
COPY .env ./
COPY config.json ./
CMD [ "node", "index.js" ]
EXPOSE 3000

