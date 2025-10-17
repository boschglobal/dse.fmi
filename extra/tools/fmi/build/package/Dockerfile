# Copyright 2024 Robert Bosch GmbH
#
# SPDX-License-Identifier: Apache-2.0



# Fmi Builder
# ===========
FROM golang:bookworm AS fmi
ADD . /src
RUN cd /src && make build



# Fmi Container Image
# ===================
FROM debian:bookworm-slim
LABEL maintainer="timothy.rule@de.bosch.com";

RUN set -eux; \
	apt-get -y update; \
	apt-get -y upgrade; \
    apt-get -y install --no-install-recommends \
        bash \
        ca-certificates \
        curl \
        jq \
        less \
        vim \
    && \
    apt-get clean; \
    rm -rf /var/lib/apt/lists/*;


# Install Fmi Tool
# ================
COPY --from=fmi /src/bin/fmi /usr/local/bin/fmi


# Add License Information (from 3rd party repos)
# =======================
COPY build/stage/licenses /licenses


# Add FMI ModelC Packages
# =======================
COPY build/stage/package /package


# Setup the entry conditions
# ==========================
# Run docker with --volume parameter to map the simulation folder into /sim.
WORKDIR /sim
COPY build/package/entrypoint.sh /entrypoint.sh
RUN chmod +x /entrypoint.sh
ENTRYPOINT ["/entrypoint.sh"]
